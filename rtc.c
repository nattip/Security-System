
#include <stdio.h>
#include <stdint.h>
#include "ST7735.h"
#include "driverlib.h"

uint8_t alarmFlag;
uint8_t armFlag;
unsigned char info_out[8];
int pressed;
uint8_t sensor;
uint8_t armed;
uint8_t timeout;

void I2C_init(void)
{
    MAP_WDT_A_clearTimer();
    const eUSCI_I2C_MasterConfig i2cConfig = {EUSCI_B_I2C_CLOCKSOURCE_SMCLK,        // SMCLK Clock Source
                                              12000000,                              // SMCLK = 3MHz
                                              EUSCI_B_I2C_SET_DATA_RATE_400KBPS,    // Desired I2C Clock of 400khz
                                              0,                                    // No byte counter threshold
                                              EUSCI_B_I2C_NO_AUTO_STOP              // No Autostop
                                              };

                                                                                // Set Pin 6.4, 6.5 to input Primary Module Function,
                                                                                // P6.4 is UCB1SDA, P6.5 is UCB1SCL
    MAP_GPIO_setAsPeripheralModuleFunctionInputPin(GPIO_PORT_P6,
    GPIO_PIN4 + GPIO_PIN5, GPIO_PRIMARY_MODULE_FUNCTION);
                                                                                    // Initializing I2C Master (see description in Driver Lib for
                                                                                    // proper configuration options)
    MAP_I2C_initMaster(EUSCI_B1_BASE, &i2cConfig);

    MAP_I2C_setSlaveAddress(EUSCI_B1_BASE, 0x68);                                  // Specify slave address

    MAP_I2C_setMode(EUSCI_B1_BASE, EUSCI_B_I2C_TRANSMIT_MODE);                      // Set Master in transmit mode

    MAP_I2C_enableModule(EUSCI_B1_BASE);                                            // Enable I2C Module to start operations
}

void readHistory_RTC(unsigned char data_in[80], uint8_t armed, uint8_t sensor)
{
    MAP_WDT_A_clearTimer();

    MAP_I2C_setMode(EUSCI_B1_BASE, EUSCI_B_I2C_TRANSMIT_MODE);                  // Set Master in transmit mode

    while (MAP_I2C_isBusBusy(EUSCI_B1_BASE));                                   // Wait for bus release, ready to write

    MAP_I2C_masterSendSingleByte(EUSCI_B1_BASE,0);                              // set pointer to beginning of RTC registers

    while (MAP_I2C_isBusBusy(EUSCI_B1_BASE));                                   // Wait for bus release

    MAP_I2C_setMode(EUSCI_B1_BASE, EUSCI_B_I2C_RECEIVE_MODE);                   // Set Master in receive mode

    while (MAP_I2C_isBusBusy(EUSCI_B1_BASE));                                   // Wait for bus release, ready to receive

    int i;
    if (alarmFlag)
    {
        for (i = 0; i < 7; i++)
        {
            data_in[i] = MAP_I2C_masterReceiveSingleByte(EUSCI_B1_BASE); //reads each of the values into data in array
        }
        data_in[7] = sensor;
    }

    if (armFlag)
    {
        for (i = 40; i < 47; i++)
        {
            data_in[i] = MAP_I2C_masterReceiveSingleByte(EUSCI_B1_BASE); //reads each of the values into data in array
        }
        data_in[47] = armed;
    }
}

void write_RTC(unsigned char data[8])
{
    MAP_WDT_A_clearTimer();

    MAP_I2C_setMode(EUSCI_B1_BASE, EUSCI_B_I2C_TRANSMIT_MODE);                      // Set Master in transmit mode

    while (MAP_I2C_isBusBusy(EUSCI_B1_BASE));                                      // Wait for bus release, ready to write

    MAP_I2C_masterSendMultiByteStart(EUSCI_B1_BASE,0);                            // set pointer to beginning of data array

    int i;
    for(i=0; i<6; i++)
    {
        MAP_WDT_A_clearTimer();
        MAP_I2C_masterSendMultiByteNext(EUSCI_B1_BASE, data[i]);                //write each successive piece of data to successive bits on an array
    }

    MAP_I2C_masterSendMultiByteFinish(EUSCI_B1_BASE, data[i]);                 // write to year location and send stop signal
}

void read_RTC(unsigned char data_in[19])
{
    MAP_WDT_A_clearTimer();

    MAP_I2C_setMode(EUSCI_B1_BASE, EUSCI_B_I2C_TRANSMIT_MODE);                 // Set Master in transmit mode

    while (MAP_I2C_isBusBusy(EUSCI_B1_BASE));                                  // Wait for bus release, ready to write

    MAP_I2C_masterSendSingleByte(EUSCI_B1_BASE,0);                             // set pointer to beginning of RTC registers

    while (MAP_I2C_isBusBusy(EUSCI_B1_BASE));                                  // Wait for bus release

    MAP_I2C_setMode(EUSCI_B1_BASE, EUSCI_B_I2C_RECEIVE_MODE);                  // Set Master in receive mode

    while (MAP_I2C_isBusBusy(EUSCI_B1_BASE));                                  // Wait for bus release, ready to receive

    int i;
    for(i=0; i<19; i++)
    {
        MAP_WDT_A_clearTimer();
        data_in[i]=MAP_I2C_masterReceiveSingleByte(EUSCI_B1_BASE);                  //reads each of the values into data in array
    }
}

unsigned char get_year(unsigned char data[8])
{
    MAP_WDT_A_clearTimer();

    int yearTens;
    int yearOnes;
    int flag = 0;
    unsigned char tempString[2];

    ST7735_DrawString(1, 1, "Enter year, 2 digits", ST7735_WHITE);
    pressed = keypad_getkey();                  //test input from keypad

    while (pressed == 0 || flag == 0) //make sure that no button is pressed before taking input
    {
        MAP_WDT_A_clearTimer();
        pressed = keypad_getkey();              //obtains new keypad entry

        if (pressed && (pressed != 10) && (pressed != 12)) //if user presses a key
        {
            if (pressed == 11)                 //indicate that an 11 means 0 key
                pressed = 0;
            yearTens = pressed;      //set the tens place of the year to pressed
            flag = 1;
            sprintf(tempString, "%d", yearTens);
            ST7735_DrawString(1, 2, tempString, ST7735_YELLOW); //displays the current tempterature

            pressed = 1;      //make pressed 1 incase it was set back to 0 above
        }
        else
            //if the key was not pressed set pressed to zero so the while loop will continue
            pressed = 0;

        if (timeout)
        {
            mainScreen_unarm();
        }

    }

    if (flag == 1)
    {
        pressed = keypad_getkey();                  //test input from keypad

        while (pressed == 0) //make sure that no button is pressed before taking input
        {
            MAP_WDT_A_clearTimer();
            pressed = keypad_getkey();              //obtains new keypad entry

            if (pressed && (pressed != 10) && (pressed != 12)) //if a second key was pressed
            {
                if (pressed == 11)             //indicate that an 11 means 0 key
                    pressed = 0;
                yearOnes = pressed; //second key press becomes the ones place of the year
                sprintf(tempString, "%d", yearOnes);
                ST7735_DrawString(2, 2, tempString, ST7735_YELLOW); //displays the current tempterature

                pressed = 1;  //make pressed 1 incase it was set back to 0 above
            }
            else
                //if the key was not pressed set pressed to zero so the while loop will continue
                pressed = 0;

            if (timeout)
            {
                mainScreen_unarm();
            }
        }
    }
    pressed = 0;
    return yearOnes | yearTens << 4; //return the decimal encoded hex value by shifting the tens place 4
}

/******************************************
 * Obtain the month from user input on a keypad
 * Written by Natalie Tipton
 * 9/30/17
 ******************************************/

unsigned char get_month(unsigned char data[8])
{
    int monthTens;
    int monthOnes;
    int flag = 0;
    unsigned char tempString[2];

    ST7735_DrawString(1, 3, "Enter month, 2 digits", ST7735_WHITE);
    pressed = keypad_getkey();                          //test input from keypad

    while (pressed == 0 || flag == 0) //make sure that no button is pressed before taking input
    {
        MAP_WDT_A_clearTimer();
        pressed = keypad_getkey();                    //obtains new keypad entry

        if (pressed && (pressed < 2 || pressed == 11)) //if user presses a key that would be valid as first digit of a month
        {
            if (pressed == 11)                 //indicate that an 11 means 0 key
                pressed = 0;
            monthTens = pressed;
            flag = 1;
            sprintf(tempString, "%d", monthTens);
            ST7735_DrawString(1, 4, tempString, ST7735_YELLOW); //displays the current tempterature

            pressed = 1;      //make pressed 1 incase it was set back to 0 above
        }
        else
            //if the key was not pressed set pressed to zero so the while loop will continue
            pressed = 0;

        if (timeout)
        {
            mainScreen_unarm();
        }
    }

    if (flag == 1)
    {
        pressed = keypad_getkey();                      //test input from keypad

        while (pressed == 0) //make sure that no button is pressed before taking input
        {
            MAP_WDT_A_clearTimer();
            pressed = keypad_getkey();                //obtains new keypad entry

            if (pressed && (pressed != 10) && (pressed != 12)) //if a second key was pressed
            {
                if (pressed == 11)             //indicate that an 11 means 0 key
                    pressed = 0;
                monthOnes = pressed;
                sprintf(tempString, "%d", monthOnes);
                ST7735_DrawString(2, 4, tempString, ST7735_YELLOW); //displays the current tempterature

                pressed = 1;  //make pressed 1 incase it was set back to 0 above
            }
            else
                //if the key was not pressed set pressed to zero so the while loop will continue
                pressed = 0;

            if (timeout)
            {
                mainScreen_unarm();
            }
        }
    }
    pressed = 0;
    return monthOnes | monthTens << 4; //return the decimal encoded hex value by shifting the tens place 4
}

/******************************************
 * Obtain the date from user input on a keypad
 * Written by Natalie Tipton
 * 9/30/17
 ******************************************/

unsigned char get_date(unsigned char data[8])
{
    int dateTens;
    int dateOnes;
    int flag = 0;
    unsigned char tempString[2];

    ST7735_DrawString(1, 5, "Enter date, 2 digits", ST7735_WHITE);
    pressed = keypad_getkey();                          //test input from keypad

    while (pressed == 0 || flag == 0) //make sure that no button is pressed before taking input
    {
        MAP_WDT_A_clearTimer();
        pressed = keypad_getkey();                    //obtains new keypad entry

        if (pressed && (pressed < 4 || pressed == 11)) //if user presses a key that would be valid as first digit
        {
            if (pressed == 11)                 //indicate that an 11 means 0 key
                pressed = 0;
            dateTens = pressed;
            flag = 1;
            sprintf(tempString, "%d", dateTens);
            ST7735_DrawString(1, 6, tempString, ST7735_YELLOW); //displays the current tempterature

            pressed = 1;      //make pressed 1 incase it was set back to 0 above
        }
        else
            //if the key was not pressed set pressed to zero so the while loop will continue
            pressed = 0;

        if (timeout)
        {
            mainScreen_unarm();
        }
    }
    if (flag == 1)
    {
        pressed = keypad_getkey();                      //test input from keypad

        while (pressed == 0) //make sure that no button is pressed before taking input
        {
            MAP_WDT_A_clearTimer();
            pressed = keypad_getkey();                //obtains new keypad entry

            if (pressed && (pressed != 10) && (pressed != 12)) //if a second key was pressed
            {
                if (pressed == 11)             //indicate that an 11 means 0 key
                    pressed = 0;
                dateOnes = pressed;
                sprintf(tempString, "%d", dateOnes);
                ST7735_DrawString(2, 6, tempString, ST7735_YELLOW); //displays the current tempterature

                pressed = 1;  //make pressed 1 incase it was set back to 0 above
            }
            else
                //if the key was not pressed set pressed to zero so the while loop will continue
                pressed = 0;

            if (timeout)
            {
                mainScreen_unarm();
            }
        }
    }
    pressed = 0;
    return dateOnes | dateTens << 4; //return the decimal encoded hex value by shifting the tens place 4
}

/******************************************
 * Obtain the day of the week from user
 *      input on a keypad
 * Written by Natalie Tipton
 * 9/30/17
 ******************************************/

unsigned char get_day(unsigned char data[8])
{
    int day;
    int flag;
    unsigned char tempString[2];

    ST7735_DrawString(1, 7, "Enter day, 1 digit", ST7735_WHITE);
    pressed = keypad_getkey();                          //test input from keypad

    while (pressed == 0 || flag == 0) //make sure that no button is pressed before taking input
    {
        MAP_WDT_A_clearTimer();
        pressed = keypad_getkey();

        if (pressed && (pressed < 8 && pressed > 0)) //if user presses a key that would be valid as first digit
        {
            if (pressed == 11)                 //indicate that an 11 means 0 key
                pressed = 0;
            day = pressed;
            flag = 1;
            sprintf(tempString, "%d", day);
            ST7735_DrawString(1, 8, tempString, ST7735_YELLOW); //displays the current tempterature

            pressed = 1;      //make pressed 1 incase it was set back to 0 above
        }
        else
            //if the key was not pressed set pressed to zero so the while loop will continue
            pressed = 0;

        if (timeout)
        {
            mainScreen_unarm();
        }
    }
    return day;
}

/******************************************
 * Obtain the hour from user input on a keypad
 * Written by Natalie Tipton
 * 9/30/17
 ******************************************/

unsigned char get_hour(unsigned char data[8])
{
    int hourTens;
    int hourOnes;
    int flag = 0;
    unsigned char tempString[2];

    ST7735_DrawString(1, 9, "Enter hour, 2 digits", ST7735_WHITE);
    pressed = keypad_getkey();                          //test input from keypad

    while (pressed == 0 || flag == 0) //make sure that no button is pressed before taking input
    {
        MAP_WDT_A_clearTimer();
        pressed = keypad_getkey();                    //obtains new keypad entry

        if (pressed && (pressed < 3 || pressed == 11)) //if user presses a key that would be valid as first digit
        {
            if (pressed == 11)                 //indicate that an 11 means 0 key
                pressed = 0;
            hourTens = pressed;
            flag = 1;
            sprintf(tempString, "%d", hourTens);
            ST7735_DrawString(1, 10, tempString, ST7735_YELLOW); //displays the current tempterature

            pressed = 1;      //make pressed 1 incase it was set back to 0 above
        }
        else
            //if the key was not pressed set pressed to zero so the while loop will continue
            pressed = 0;

        if (timeout)
        {
            mainScreen_unarm();
        }
    }
    if (flag == 1)
    {
        pressed = keypad_getkey();                      //test input from keypad

        while (pressed == 0) //make sure that no button is pressed before taking input
        {
            MAP_WDT_A_clearTimer();
            pressed = keypad_getkey();                //obtains new keypad entry

            if (pressed && (pressed != 10) && (pressed != 12)) //if a second key was pressed
            {
                if (pressed == 11)             //indicate that an 11 means 0 key
                    pressed = 0;
                hourOnes = pressed;
                sprintf(tempString, "%d", hourOnes);
                ST7735_DrawString(2, 10, tempString, ST7735_YELLOW); //displays the current tempterature

                pressed = 1;  //make pressed 1 incase it was set back to 0 above
            }
            else
                //if the key was not pressed set pressed to zero so the while loop will continue
                pressed = 0;

            if (timeout)
            {
                mainScreen_unarm();
            }
        }
    }
    pressed = 0;
    return hourOnes | hourTens << 4; //return the decimal encoded hex value by shifting the tens place 4
}

/******************************************
 * Obtain the minute from user input on a keypad
 * Written by Natalie Tipton
 * 9/30/17
 ******************************************/

unsigned char get_minute(unsigned char data[8])
{
    int minuteTens;
    int minuteOnes;
    int flag = 0;
    unsigned char tempString[2];

    ST7735_DrawString(1, 11, "Enter min, 2 digits", ST7735_WHITE);
    pressed = keypad_getkey();                          //test input from keypad

    while (pressed == 0 || flag == 0) //make sure that no button is pressed before taking input
    {
        MAP_WDT_A_clearTimer();
        pressed = keypad_getkey();                    //obtains new keypad entry

        if (pressed && (pressed < 6 || pressed == 11)) //if user presses a key that would be valid as first digit
        {
            if (pressed == 11)                 //indicate that an 11 means 0 key
                pressed = 0;
            minuteTens = pressed;
            flag = 1;
            sprintf(tempString, "%d", minuteTens);
            ST7735_DrawString(1, 12, tempString, ST7735_YELLOW); //displays the current tempterature

            pressed = 1;      //make pressed 1 incase it was set back to 0 above
        }
        else
            //if the key was not pressed set pressed to zero so the while loop will continue
            pressed = 0;

        if (timeout)
        {
            mainScreen_unarm();
        }
    }
    if (flag == 1)
    {
        pressed = keypad_getkey();                      //test input from keypad

        while (pressed == 0) //make sure that no button is pressed before taking input
        {
            MAP_WDT_A_clearTimer();
            pressed = keypad_getkey();                //obtains new keypad entry

            if (pressed && (pressed != 10) && (pressed != 12)) //if a second key was pressed
            {
                if (pressed == 11)             //indicate that an 11 means 0 key
                    pressed = 0;
                minuteOnes = pressed;
                sprintf(tempString, "%d", minuteOnes);
                ST7735_DrawString(2, 12, tempString, ST7735_YELLOW); //displays the current tempterature

                pressed = 1;  //make pressed 1 incase it was set back to 0 above
            }
            else
                //if the key was not pressed set pressed to zero so the while loop will continue
                pressed = 0;

            if (timeout)
            {
                mainScreen_unarm();
            }
        }
    }
    pressed = 0;
    return minuteOnes | minuteTens << 4; //return the decimal encoded hex value by shifting the tens place 4
}

/******************************************
 * Obtain the second from user input on a keypad
 * Written by Natalie Tipton
 * 9/30/17
 ******************************************/

unsigned char get_second(unsigned char data[8])
{
    int secondTens;
    int secondOnes;
    int flag = 0;
    unsigned char tempString[2];

    ST7735_DrawString(1, 13, "Enter sec, 2 digits", ST7735_WHITE);
    pressed = keypad_getkey();                          //test input from keypad

    while (pressed == 0 || flag == 0) //make sure that no button is pressed before taking input
    {
        MAP_WDT_A_clearTimer();
        pressed = keypad_getkey();                    //obtains new keypad entry

        if (pressed && (pressed < 6 || pressed == 11)) //if user presses a key that would be valid as first digit
        {
            if (pressed == 11)                 //indicate that an 11 means 0 key
                pressed = 0;
            secondTens = pressed;
            flag = 1;
            sprintf(tempString, "%d", secondTens);
            ST7735_DrawString(1, 14, tempString, ST7735_YELLOW); //displays the current tempterature

            pressed = 1;      //make pressed 1 incase it was set back to 0 above
        }
        else
            //if the key was not pressed set pressed to zero so the while loop will continue
            pressed = 0;

        if (timeout)
        {
            mainScreen_unarm();
        }
    }
    if (flag == 1)
    {
        pressed = keypad_getkey();                      //test input from keypad

        while (pressed == 0) //make sure that no button is pressed before taking input
        {
            MAP_WDT_A_clearTimer();
            pressed = keypad_getkey();                //obtains new keypad entry

            if (pressed && (pressed != 10) && (pressed != 12)) //if a second key was pressed
            {
                if (pressed == 11)             //indicate that an 11 means 0 key
                    pressed = 0;
                secondOnes = pressed;
                sprintf(tempString, "%d", secondOnes);
                ST7735_DrawString(2, 14, tempString, ST7735_YELLOW); //displays the current tempterature

                pressed = 1;  //make pressed 1 incase it was set back to 0 above
            }
            else
                //if the key was not pressed set pressed to zero so the while loop will continue
                pressed = 0;

            if (timeout)
            {
                mainScreen_unarm();
            }
        }
    }
    pressed = 0;
       return secondOnes | secondTens << 4;                     //return the decimal encoded hex value by shifting the tens place 4
}

void change_timedate(void)
{
    MAP_WDT_A_clearTimer();
    ST7735_FillScreen(0);

    info_out[6] = get_year(info_out);                          //retrieve date and time info from keypad
    MAP_WDT_A_clearTimer();
    info_out[5] = get_month(info_out);
    MAP_WDT_A_clearTimer();
    info_out[4] = get_date(info_out);
    MAP_WDT_A_clearTimer();
    info_out[3] = get_day(info_out);
    MAP_WDT_A_clearTimer();
    info_out[2] = get_hour(info_out);
    MAP_WDT_A_clearTimer();
    info_out[1] = get_minute(info_out);
    MAP_WDT_A_clearTimer();
    info_out[0] = get_second(info_out);
    MAP_WDT_A_clearTimer();
    write_RTC(info_out);                              //write original info to RTC
    read_RTC(info_out);                               //read to avoid an original printing of all zeroes

    //ST7735_FillScreen(0);


}
