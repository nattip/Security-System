/*
 * -------------------------------------------
 *    MSP432 DriverLib - v3_21_00_05 
 * -------------------------------------------
 */

/****************************************
 * Written by Natalie Tipton and Seth Scheib
 * EGR 326 (902)
 * Dr. Nabeeh Kandalaft
 * EGR326_Project
 ****************************************/

#include "driverlib.h"                                  //include libraries
#include <stdint.h>
#include <stdbool.h>
#include <ST7735.h>
#include <flash.h>
#include <keypad.h>
#include <misc.h>

uint8_t historyData[80];                                //initialize arrays
char pin[4];
char pin_check[4];
int enter[1000];

int pressed;                                            //initialize variables
uint8_t alarmFlag;
uint8_t armFlag;
uint8_t armed;
uint8_t sensor;
int dutyCycle = 0;
uint8_t timeout;
unsigned char temperature;
volatile int PWMseconds;
volatile int idleSeconds;
static volatile uint16_t curADCResult;

void mainScreen_unarm(void);                            //function declarations
void mainScreen_arm(void);
void options(void);
void pinEntry(void);
void checkSensors(void);
void alert(void);
void silenceAlarm(void);
uint16_t map(uint16_t curADCResult);
void InitATimer(void);
void PWMInit(void);
void checkTemp(void);

Timer_A_PWMConfig pwmConfig1 =                   //alarm frequency declarations
        {
        TIMER_A_CLOCKSOURCE_SMCLK,
          TIMER_A_CLOCKSOURCE_DIVIDER_64, 320,
          TIMER_A_CAPTURECOMPARE_REGISTER_1,
          TIMER_A_OUTPUTMODE_RESET_SET, 250 };

Timer_A_PWMConfig pwmConfig2 = {
                                 TIMER_A_CLOCKSOURCE_SMCLK,
                                 TIMER_A_CLOCKSOURCE_DIVIDER_32, 320,
                                 TIMER_A_CAPTURECOMPARE_REGISTER_1,
                                 TIMER_A_OUTPUTMODE_RESET_SET, 250 };

int main(void)
{

    MAP_WDT_A_holdTimer();                             //Stop Watchdog
    Clock_Init48MHz();                 //initialize clocks
    I2C_init();                                        //initialize I2C
    Port_Init();                                 //initialize pins
    SysTick_Init();                                   //initialize SysTick timer
    ST7735_InitR(INITR_REDTAB);                        //initialize LCD screen

    readHistory_Flash();                 //read flash history to populate arrays
    readPIN_Flash();
    PWMInit();
    armed = historyData[47]; //set alarm status to most recent state before reset

    InitATimer();                           //initialize timer a

    MAP_SysCtl_setWDTTimeoutResetType(SYSCTL_SOFT_RESET); //initialized WDT to soft reset the program when timeout occurs
    MAP_WDT_A_initWatchdogTimer(WDT_A_CLOCKSOURCE_ACLK, //sets WDT to ACLK and 512k iterations of ACLK before timeout
                                WDT_A_CLOCKITERATIONS_32K); //creates ~4second delay before timeout
    MAP_WDT_A_startTimer();

    __enable_irq();                     //enable global interrupt

    NVIC->ISER[0] = 1 << ((TA1_N_IRQn) & 31); //from sample code for timer a interrupt

    splash_screen();
    SysTick_delay(3000);

    while (1)                                          //continuous loop
    {
        if (armed == 0)                               //if the system is unarmed
            mainScreen_unarm(); //show mainscreen with appropriate options for this case

        if (armed == 1)                                 //if the system is armed
            mainScreen_arm();        //show main screen with appropriate options
    }
}

void mainScreen_unarm(void)
{
    uint8_t currentTime[19]; //array for the data read from the RTC to be saved into
    char timeString[20], tempString[11];                //strings to be printed
    char dayMap[] = { 'S', 'M', 'T', 'W', 'R', 'F', 'S' }; //map of the numbered day of the week to corresponding letter
    pressed = 0;                             //start with no input on the keypad

    P2OUT &= ~BIT6;                         //turn red LED off
    P2OUT |= BIT5;                          //turn green LED on

    ST7735_FillScreen(0);                               //fill screen black

    while (1)                                            //continuous loop
    {
        MAP_WDT_A_clearTimer();
        timeout = 0;            //don't let system time out while in main screen

        if (armed == 1)
            break;    //if the system is actually armed, break out of while loop

        map(curADCResult);                      //adjust adaptive brightness level

        read_RTC(currentTime);                    //read time/date info from RTC
        sprintf(timeString,
                "%.2x:%.2x:%.2x %c %x/%x/%x", //turns the time/date info into a printable character string
                currentTime[2], currentTime[1], currentTime[0],
                dayMap[currentTime[3] - 1], currentTime[5], currentTime[4],
                currentTime[6]);
        ST7735_DrawString(0, 0, timeString, ST7735_WHITE); //displays the current time and date

        temperature = currentTime[17] * 9 / 5 + 32; //converts temperature from celcius to fahrenheit
        sprintf(tempString, "%d deg F", temperature); //turns the temp info into a printable string
        ST7735_DrawString(0, 1, tempString, ST7735_YELLOW); //displays the current temperature

        //Door sensor indicator
        if (P1IN & BIT6)                       //if the door sensor is triggered
            ST7735_FillRect(0, 27, ST7735_TFTWIDTH, 13, ST7735_RED); //displays red status bar
        else
            //if sensor not triggered
            ST7735_FillRect(0, 27, ST7735_TFTWIDTH, 13, ST7735_GREEN); //displays green status bar
        ST7735_DrawString(8, 3, "Door", 0); //denotes status bar is for the door sensor

        //window sensor indicator
        //TO DO: change pin
        if (P5IN & BIT0)                     //if the window sensor is triggered
            ST7735_FillRect(0, 47, ST7735_TFTWIDTH, 13, ST7735_RED); //displays red status bar
        else
            //if sensor not triggered
            ST7735_FillRect(0, 47, ST7735_TFTWIDTH, 13, ST7735_GREEN); //displays green status bar
        ST7735_DrawString(7, 5, "Window", 0); //denotes status is of window sensor

        //safe sensor indicator
        //TO DO: change pin
//        if (P1IN & BIT7)                       //if the safe sensor is triggered
//            ST7735_FillRect(0, 67, ST7735_TFTWIDTH, 13, ST7735_RED); //displays red status bar
//        else
//            //if sensor not triggered
//            ST7735_FillRect(0, 67, ST7735_TFTWIDTH, 13, ST7735_GREEN); //displays green status bar
//        ST7735_DrawString(8, 7, "Safe", 0);   //denotes status is of safe sensor

        if (P6IN & BIT0)                       //if the safe sensor is triggered
            ST7735_FillRect(0, 67, ST7735_TFTWIDTH, 13, ST7735_RED); //displays red status bar
        else
            //if sensor not triggered
            ST7735_FillRect(0, 67, ST7735_TFTWIDTH, 13, ST7735_GREEN); //displays green status bar
        ST7735_DrawString(7, 7, "Motion", 0); //denotes status is of safe sensor

        //system status indicator
        ST7735_FillRect(0, 87, ST7735_TFTWIDTH, 13, ST7735_GREEN); //displays red status for unprotected and unarmed system
        ST7735_DrawString(7, 9, "Disarmed", 0);

        //Options
        ST7735_DrawString(1, 13, "Press # to arm/lock", ST7735_WHITE); //tells users how to arm/lock their system
        ST7735_DrawString(1, 14, "Press * for Options", ST7735_WHITE); //tells users how to view more options

        checkTemp();

        pressed = keypad_getkey();                               //checks keypad
        if (pressed == 10)           //if user presses *, go to the options menu
            options();                             //checks keypad
        else if (pressed == 12) //if user pressed #, allow them to enter their pin
            pinEntry();
    }
}

void mainScreen_arm(void)
{
    uint8_t currentTime[19]; //array for current time/date info to be saved from RTC
    char timeString[20], tempString[10]; //strings for time/date info to be printed
    char dayMap[] = { 'S', 'M', 'T', 'W', 'R', 'F', 'S' }; //maps which number corresponds to which day of the week
    pressed = 0;                             //start with no input on the keypad

    P2OUT &= ~BIT5;                 //turns off green LED
    P2OUT |= BIT6;                      //turns on red LED

    ST7735_FillScreen(0);                                   //fills screen black

    while (1)                                                  //continuous loop
    {
        MAP_WDT_A_clearTimer();
        timeout = 0;               //doesnt allow a timeout while in main screen

        if (armed == 0)
            break;    //if the system is actually armed, break out of while loop

        map(curADCResult);                          //update adaptive brightness
        checkSensors();                            //check all sensors

        if (armed == 0)
            break; //if the alarm went off and user disabled system, the code will return to here, but it is no longer armed

        read_RTC(currentTime);                           //read the current time
        sprintf(timeString, "%.2x:%.2x:%.2x %c %x/%x/%x",
                currentTime[2], //turns current time/date into printable characters
                currentTime[1], currentTime[0], dayMap[currentTime[3] - 1],
                currentTime[5], currentTime[4], currentTime[6]);
        ST7735_DrawString(0, 0, timeString, ST7735_WHITE); //displays the current time and date

        temperature = currentTime[17] * 9 / 5 + 32; //converts celcius temp into fahrenheit
        sprintf(tempString, "%.2d deg F", temperature); //turns the temp info into a printable string
        ST7735_DrawString(0, 1, tempString, ST7735_YELLOW); //displays the current temperature

        //Door sensor indicator
        if (P1IN & BIT6)                //if the hall effect sensor is triggered
            ST7735_FillRect(0, 27, ST7735_TFTWIDTH, 13, ST7735_RED); //displays a red status
        else
            //if not triggered
            ST7735_FillRect(0, 27, ST7735_TFTWIDTH, 13, ST7735_GREEN); //displays a green status
        ST7735_DrawString(8, 3, "Door", 0); //denotes this sensor status is of the door

        //window sensor indicator
        //TO DO: change pin
        if (P5IN & BIT0)                     //if the window sensor is triggered
            ST7735_FillRect(0, 47, ST7735_TFTWIDTH, 13, ST7735_RED); //display red status bar
        else
            //if sensor not triggered
            ST7735_FillRect(0, 47, ST7735_TFTWIDTH, 13, ST7735_GREEN); //display green status bar
        ST7735_DrawString(7, 5, "Window", 0); //denotes this sensor status is of the window

        //safe sensor indicator
        //TO DO: change pin
//        if (P1IN & BIT7)                       //if the safe sensor is triggered
//            ST7735_FillRect(0, 67, ST7735_TFTWIDTH, 13, ST7735_RED); //display red status bar
//        else
//            //if sensor not triggered
//            ST7735_FillRect(0, 67, ST7735_TFTWIDTH, 13, ST7735_GREEN); //display green status bar
//        ST7735_DrawString(8, 7, "Safe", 0); //denotes this sensor status is of the safe

        if (P6IN & BIT0)                       //if the safe sensor is triggered
            ST7735_FillRect(0, 67, ST7735_TFTWIDTH, 13, ST7735_RED); //displays red status bar
        else
            //if sensor not triggered
            ST7735_FillRect(0, 67, ST7735_TFTWIDTH, 13, ST7735_GREEN); //displays green status bar
        ST7735_DrawString(7, 7, "Motion", 0); //denotes status is of safe sensor

        //system status indicator
        ST7735_FillRect(0, 87, ST7735_TFTWIDTH, 13, ST7735_RED); //shows system is armed and safe with a green status bar
        ST7735_DrawString(8, 9, "Armed", 0);

        //options
        ST7735_DrawString(1, 13, "Press # to", ST7735_WHITE); //tells users how to disarm and unlock their system
        ST7735_DrawString(1, 14, "disarm/unlock", ST7735_WHITE);

        checkTemp();

        pressed = keypad_getkey();                               //checks keypad
        if (pressed == 12)                                   //if user pressed #
            pinEntry();      //go to the pin entry function to disarm the system
    }

}

void options(void)
{
    ST7735_FillScreen(0);                                    //fill screen black

    while (1)                                                  //continuous loop
    {
        MAP_WDT_A_clearTimer();
        ST7735_DrawString(1, 1, "OPTIONS:", ST7735_WHITE); //prints available options to LCD with the key press to access them
        ST7735_DrawString(1, 4, "1 - Change time/date", ST7735_WHITE);
        ST7735_DrawString(1, 6, "2 - Change PIN", ST7735_WHITE);
        ST7735_DrawString(1, 8, "3 - View past alarms", ST7735_WHITE);
        ST7735_DrawString(1, 10, "4 - View past arming", ST7735_WHITE);
        ST7735_DrawString(1, 14, "Press # to go back", ST7735_WHITE);

        checkTemp();

        pressed = keypad_getkey();                               //checks keypad
        if (pressed == 1)                                    //if user pressed 1
        {
            MAP_WDT_A_clearTimer();
            checkTemp();
            change_timedate();                  //go to change_timedate function
            ST7735_InitR(INITR_REDTAB); //initialize LCD screen to override weird cursor changes
        }

        else if (pressed == 2)                               //if user pressed 2
        {                                  //will allow them to change their pin
            MAP_WDT_A_clearTimer();
            checkTemp();
            ST7735_FillScreen(0);                        //fill the screen black
            pin_check[1] = 0;
            check_pin();                                 //check the current pin
            if ((pin[0] == pin_check[0]) && (pin[1] == pin_check[1]) //if correct entry of current pin is confirmed
                    && (pin[2] == pin_check[2]) && (pin[3] == pin_check[3]))
            {
                create_pin();  //call function to allow them to create a new pin
                writePIN_Flash();            //write the new pin to flash memory
                readPIN_Flash(); //read pin from flash memory to make sure it was correctly saved
                printPIN(); //print newly saved pin from flash memory to the LCD
                SysTick_delay(4000);                         //wait four seconds
                ST7735_FillScreen(0);                        //fill screen black
            }
            else                      //if current pin was not entered correctly
            {
                ST7735_DrawString(3, 10, "Incorrect PIN", ST7735_WHITE); //let user know of incorrect entry
                SysTick_delay(4000);                         //wait four seconds
                break;                               //go back to options screen
            }
        }
        else if (pressed == 3)                               //if user pressed 3
        {                        //will allow them to view last 5 alarm triggers
            MAP_WDT_A_clearTimer();
            checkTemp();
            ST7735_FillScreen(0);                           //fills screen black
            readAlarmHistory_Flash(); //reads the alarm history from flash memory
            ST7735_DrawString(1, 1, "ALARM HISTORY:", ST7735_WHITE); //prints the time and date data read from flash
            printAlarmHistory();  //shows last five time/dates of alarm triggers
            ST7735_DrawString(1, 14, "Press # to go back", ST7735_WHITE); //shows user how to leave the history screen
            pressed = 0;                    //sets value of pressed back to zero
            while (pressed != 12) //screen will stay on the history screen until user presses #
            {
                MAP_WDT_A_clearTimer();
                pressed = keypad_getkey();                       //checks keypad
                if (timeout)
                    break;
            }
            ST7735_FillScreen(0);     //after user pressed #, fills screen black
        }
        else if (pressed == 4)                               //if user pressed 4
        {                     //will allow them to view last 5 arm/disarm events
            MAP_WDT_A_clearTimer();
            checkTemp();
            ST7735_FillScreen(0);                           //fills screen black
            readArmHistory_Flash();    //reads the arm history from flash memory
            ST7735_DrawString(1, 1, "ARM/DISARM HISTORY:", ST7735_WHITE); //prints the time and date data read from flash
            printArmHistory(); //shows last five time/dates of arm/disarm events
            ST7735_DrawString(1, 14, "Press # to go back", ST7735_WHITE); //shows user how to leave the history screen
            pressed = 0;                    //sets value of pressed back to zero
            while (pressed != 12) //screen will stay on the history screen until user presses #
            {
                MAP_WDT_A_clearTimer();
                pressed = keypad_getkey();                       //checks keypad
                if (timeout)
                    break;
            }
            ST7735_FillScreen(0);     //after user pressed #, fills screen black
        }
        else if (pressed == 12)                              //if user pressed #
        {
            pressed = 0;                                //resets pressed to zero
            break; //breaks out of continuous while loop to return to main screen
        }
        if (timeout) //if user has been on options menu for one minute without pressing a button,
            break;   //break back to main menu
    }

    ST7735_FillScreen(0);                                   //fills screen black
}

void pinEntry(void)
{
    MAP_WDT_A_clearTimer();
    checkTemp();
    ST7735_FillScreen(0);                                   //fills screen black
    int i;
    if (armed == 1)                                     //if the system is armed
    {
        MAP_WDT_A_clearTimer();
        pin_check[1] = 0; //resets a bit of pin_check so that the last store pin_check value is not the same as the current pin
        check_pin();                                           //user enters pin
        if ((pin[0] == pin_check[0]) && (pin[1] == pin_check[1]) //if pin entered correctly
                && (pin[2] == pin_check[2]) && (pin[3] == pin_check[3]))
        {
            armed = 0;                        //sends system to a disarmed state
            armFlag = 1; //sets flag so current time will be saved to arm/disarm events in flash
            readHistory_RTC(historyData, armed, sensor);    //read the RTC
            writeHistory_Flash(); //saves current time/date to past 5 arm/disarm events in flash
            ST7735_DrawString(4, 10, "Correct PIN", ST7735_WHITE); //tells user they entered their pin correctly
            ST7735_DrawString(3, 11, "Unlocking door...", ST7735_WHITE); //tells user the door is unlocking
                                                                         //turn stepper motor to unlock door

            for (i = 0; i < 128; i++) //cycle through enough times to make a quarter spin
            {                               //turn clockwise
                P9OUT ^= BIT0;              //turns on the first coil
                SysTick_delay(5);           //small delay to let the motor move
                P9OUT ^= BIT0;              //shut first coil off
                P8OUT ^= BIT5;              //turn second coil on
                SysTick_delay(5);           //small delay
                P8OUT ^= BIT5;              //shut second coil off
                P8OUT ^= BIT6;              //turn third coil on
                SysTick_delay(5);           //small delay
                P8OUT ^= BIT6;              //shut third coil off
                P8OUT ^= BIT7;              //turn fourth coil on
                SysTick_delay(5);           //small delay
                P8OUT ^= BIT7;              //shut fourth coil off
            }
        }
        else                                        //if pin entered incorrectly
        {
            ST7735_DrawString(3, 10, "Incorrect PIN", ST7735_WHITE); //tells user they entered their pin incorrectly
            SysTick_delay(4000); //waits 4 seconds before returning to main screen
        }
    }
    else                                              //if the system is unarmed
    {
        MAP_WDT_A_clearTimer();
        pin_check[1] = 0; //resets a bit of pin_check so that the last store pin_check value is not the same as the current pin
        check_pin();                                           //user enters pin
        if ((pin[0] == pin_check[0]) && (pin[1] == pin_check[1]) //if pin entered correctly
                && (pin[2] == pin_check[2]) && (pin[3] == pin_check[3]))
        {
            if ((P5IN & BIT0) || (P1IN & BIT6)) //TO DO: change this to when anything is open once in house
            {
                ST7735_DrawString(1, 10, "Secure all openings", ST7735_WHITE); //tell user that something is open
                ST7735_DrawString(3, 11, "and try again.", ST7735_WHITE); //and they must close it before arming system
                SysTick_delay(5000);
            }
            else                          //if everything in the house if secure
            {
                armed = 1;                      //sends system to an armed state
                armFlag = 1; //sets flag so current time will be saved to arm/disarm events in flash
                readHistory_RTC(historyData, armed, sensor);        //read RTC
                writeHistory_Flash(); //saves current time/date to past 5 arm/disarm events in flash
                ST7735_DrawString(4, 10, "Correct PIN", ST7735_WHITE); //tells user they entered their pin correctly
                ST7735_DrawString(3, 11, "Locking Door...", ST7735_WHITE); //tells user the door is locking
                                                                           //turn stepper motor to lock door

                for (i = 0; i < 128; i++) //cycle through enough times to make a 360 spin
                {                               //turn counterclockwise
                    P8OUT ^= BIT7;              //turns on the first coil
                    SysTick_delay(5);        //small delay to let the motor move
                    P8OUT ^= BIT7;              //shut first coil off
                    P8OUT ^= BIT6;              //turn second coil on
                    SysTick_delay(5);           //small delay
                    P8OUT ^= BIT6;              //shut second coil off
                    P8OUT ^= BIT5;              //turn third coil on
                    SysTick_delay(5);           //small delay
                    P8OUT ^= BIT5;              //shut third coil off
                    P9OUT ^= BIT0;              //turn fourth coil on
                    SysTick_delay(5);           //small delay
                    P9OUT ^= BIT0;              //shut fourth coil off
                }
            }
        }
        else                                        //if pin entered incorrectly
        {
            ST7735_DrawString(3, 10, "Incorrect PIN", ST7735_WHITE); //tells user they entered their pin incorrectly
            SysTick_delay(4000); //waits 4 seconds before returning to main screen
        }
    }
    armFlag = 0;                          //resets flag for saving flash history
    ST7735_FillScreen(0);                                    //fill screen black
}

void checkSensors(void)
{
    MAP_WDT_A_clearTimer();
    checkTemp();
    if (P1IN & BIT6) //if door
    {

        sensor = 0;         //set which sensor was triggered for saving to flash
        alarmFlag = 1;      //set flag so flash knows an alarm event occurred
        readHistory_RTC(historyData, armed, sensor);    //read RTC
        writeHistory_Flash(); //saves current time/date to past 5 alarm events in flash
        silenceAlarm();  //call function to have user enter pin to silence alarm
        MAP_WDT_A_clearTimer();
    }
    else if (P5IN & BIT0) //if window
    {
        sensor = 1;         //set which sensor was triggered for saving to flash
        alarmFlag = 1;      //set flag so flash knows an alarm event occurred
        readHistory_RTC(historyData, armed, sensor);      //read RTC
        writeHistory_Flash(); //saves current time/date to past 5 alarm events in flash
        silenceAlarm();  //call function to have user enter pin to silence alarm
        MAP_WDT_A_clearTimer();
    }
//    else if (P1IN & BIT7) //if safe
//    {
//        sensor = 2;         //set which sensor was triggered for saving to flash
//        alarmFlag = 1;      //set flag so flash knows an alarm event occurred
//        readHistory_RTC(historyData, armed, sensor);      //read RTC
//        writeHistory_Flash(); //saves current time/date to past 5 alarm events in flash
//        silenceAlarm();  //call function to have user enter pin to silence alarm
//        MAP_WDT_A_clearTimer();
//    }
    if (P6IN & BIT0) //if PIR
    {
        sensor = 3;         //set which sensor was triggered for saving to flash
        alarmFlag = 1;      //set flag so flash knows an alarm event occurred
        readHistory_RTC(historyData, armed, sensor);      //read RTC
        writeHistory_Flash(); //saves current time/date to past 5 alarm events in flash
        silenceAlarm();  //call function to have user enter pin to silence alarm
        MAP_WDT_A_clearTimer();
    }
}

void silenceAlarm(void)
{
    int i;
    ST7735_FillScreen(0);                                   //fills screen black
    while (alarmFlag)                              //while an alarm is going off
    {
        MAP_WDT_A_clearTimer();
        pin_check[1] = 0; //resets a bit of pin_check so that the last store pin_check value is not the same as the current pin
        check_pin();                                           //user enters pin
        if ((pin[0] == pin_check[0]) && (pin[1] == pin_check[1]) //if pin entered correctly
                && (pin[2] == pin_check[2]) && (pin[3] == pin_check[3]))
        {
            MAP_WDT_A_clearTimer();
            alarmFlag = 0; //sets alarmflag to 0 so it doesnt save to flash anymore and interrupt stops alarming
            armed = 0;      //puts system in disarmed state
            armFlag = 1; //sets arm flag so flash will save current time as an arming/disarming event
            readHistory_RTC(historyData, armed, sensor);        //read RTC
            writeHistory_Flash(); //saves current time/date to past 5 arm/disarm events in flash
            armFlag = 0; //sets arm flag to 0 so flash will not save another arming/disarming event when unneccessary
            ST7735_DrawString(4, 10, "Correct PIN", ST7735_WHITE); //tells user they entered their pin correctly
            for (i = 0; i < 128; i++) //cycle through enough times to make a quarter spin
                       {                               //turn clockwise
                           P9OUT ^= BIT0;              //turns on the first coil
                           SysTick_delay(5);           //small delay to let the motor move
                           P9OUT ^= BIT0;              //shut first coil off
                           P8OUT ^= BIT5;              //turn second coil on
                           SysTick_delay(5);           //small delay
                           P8OUT ^= BIT5;              //shut second coil off
                           P8OUT ^= BIT6;              //turn third coil on
                           SysTick_delay(5);           //small delay
                           P8OUT ^= BIT6;              //shut third coil off
                           P8OUT ^= BIT7;              //turn fourth coil on
                           SysTick_delay(5);           //small delay
                           P8OUT ^= BIT7;              //shut fourth coil off
                       }
        }
        else
        {
            MAP_WDT_A_clearTimer();
            ST7735_DrawString(4, 10, "Incorrect PIN", ST7735_WHITE); //tells user they entered their pin correctly
            SysTick_delay(1000);        //delay one second
            ST7735_FillScreen(0);                           //fills screen black
        }
    }

}

uint16_t map(uint16_t curADCResult)
{
    MAP_WDT_A_clearTimer();
    int output;
    int input_range = 17000; //sets range of possible input values from the photocell
    int output_range = 100; //sets range of duty cycle that the input must be mapped to

    output = (curADCResult * output_range) / input_range; //maps input to the dutycycle range
    output = (output - 100) * (-1); //using a pnp transistor so inverts the mapped output
    return output; //returns the mapped value to be used for the dutycycle of the PWM
}

void InitATimer(void)       //configure timer a to create a one second interrupt
{
    TIMER_A1->CTL = TIMER_A_CTL_TASSEL_1 |  //sets it to a clock
            TIMER_A_CTL_MC_2 |             // in continuous mode
            TIMER_A_CTL_CLR |              // Clear TAR
            TIMER_A_CTL_IE;                // Enable timer overflow interrupt
}

void TA1_N_IRQHandler(void)
{
    TIMER_A1->CTL &= ~TIMER_A_CTL_IFG;    // Clear timer overflow interrupt flag

    if (alarmFlag)   //if the system is armed and an alarm is oging off
    {
        P2->DIR |= BIT4;                    //turn on buzzer

        if ((PWMseconds % 2) == 0)   //if the seconds count is on an even number
        {
            MAP_Timer_A_generatePWM(TIMER_A0_BASE, &pwmConfig1); //sound first frequency
        }
        if ((PWMseconds % 2) == 1)    //if the seconds count is on an odd number
        {
            MAP_Timer_A_generatePWM(TIMER_A0_BASE, &pwmConfig2); //sound second frequency
        }

        P2->OUT ^= BIT5;                        // Toggle LED
    }
    else                 //if the system is unarmed or an alarm is not going off
    {
        P2->DIR &= ~BIT4;                   //turn off buzzer
        //P2->OUT |= BIT5;                   //turn off LED TO DO: different LED P2 RGB

        if (idleSeconds > 58)       //if the system has been idle for one minute
        {
            idleSeconds = 0;                //reset idle seconds counter
            timeout = 1;                 //flag the system that it has timed out
        }
    }

    PWMseconds++; //increment seconds for altering buzzer frequency/flashing LED
    idleSeconds++;                              //increment idle seconds counter

    curADCResult = MAP_ADC14_getResult(ADC_MEM0); //finds the current converted ambient light result
    TIMER_A0->CCR[4] = map(curADCResult); //sends result to change the PWM of the LCD to change brightness

    MAP_ADC14_toggleConversionTrigger(); //triggers the conversion flag to start a new conversion
}

void checkTemp(void)
{
    MAP_WDT_A_clearTimer();
    uint8_t currentTime[20];

    read_RTC(currentTime);                    //read time/date info from RTC
    temperature = currentTime[17] * 9 / 5 + 32; //converts celcius temp into fahrenheit
    if(temperature >= 110)
    {
        alarmFlag = 1;
        sensor = 4;
        readHistory_RTC(historyData, armed, sensor);    //read RTC
        writeHistory_Flash(); //saves current time/date to past 5 alarm events in flash
        silenceAlarm();  //call function to have user enter pin to silence alarm
    }
}

void PWMInit(void)
{
    MAP_WDT_A_clearTimer();
    MAP_PCM_setPowerState(PCM_AM_LDO_VCORE1);            // Setting DCO to 48MHz
    MAP_CS_setDCOCenteredFrequency(CS_DCO_FREQUENCY_48);

    MAP_FPU_enableModule();     // Enabling the FPU for floating point operation
    MAP_FPU_enableLazyStacking();

    MAP_ADC14_enableModule();                     // Initializing ADC (MCLK/1/4)
    MAP_ADC14_initModule(ADC_CLOCKSOURCE_MCLK, ADC_PREDIVIDER_1, ADC_DIVIDER_4,
                         0);

    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P5,
                                                   GPIO_PIN5, // Configuring GPIOs (5.5 A0)
            GPIO_TERTIARY_MODULE_FUNCTION);

    MAP_ADC14_configureSingleSampleMode(ADC_MEM0, true); // Configuring ADC Memory
    MAP_ADC14_configureConversionMemory(ADC_MEM0, ADC_VREFPOS_AVCC_VREFNEG_VSS,
    ADC_INPUT_A0,
                                        false);

    MAP_ADC14_enableSampleTimer(ADC_MANUAL_ITERATION); // Configuring Sample Timer

    MAP_ADC14_enableConversion();                // Enabling/Toggling Conversion
    MAP_ADC14_toggleConversionTrigger();

    P2->SEL0 |= BIT7;                         //set P2.7 as special function I/O
    P2->SEL1 &= ~BIT7;
    P2->DIR |= BIT7;                                     //set P2.7 as an output

    TIMER_A0->CCR[0] = 100 - 1;                                  //Period of PWM
    TIMER_A0->CCR[4] = dutyCycle; //sets the duty cycle of the PWM to a variable
    TIMER_A0->CCTL[4] = TIMER_A_CCTLN_OUTMOD_7; //sets timer a to mode seven reset/set
    TIMER_A0->CTL = TIMER_A_CTL_SSEL1 | TIMER_A_CTL_MC_1 | TIMER_A_CTL_CLR; //small clock, up mode, clear timer a register
}

