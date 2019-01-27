#include <stdio.h>
#include <stdint.h>
#include "ST7735.h"
#include "driverlib.h"
#include <misc.h>

char pin[4];
char pin_check[4];
int enter[1000];
int pressed;
uint8_t sensor;
volatile int idleSeconds;
uint8_t timeout;

uint8_t keypad_getkey(void)                            // assumes port 4 bits 0-3 are connected to rows
{
    MAP_WDT_A_clearTimer();
    uint8_t row;                                      // bits 4,5,6 are connected to columns
    uint8_t col;
    const char column_select[] = {0x10, 0x20, 0x40};  // one column is active

    for (col = 0; col < 3; col++)                     // Activates one column at a time, read the input to see which column
    {
        P4->DIR &= ~0xF0;                             // disable all columns
        P4->DIR |= column_select[col];                // enable one column at a time
        P4->OUT &= ~column_select[col];               // drive the active column low
        __delay_cycles(10);                           // wait for signal to settle
        row = P4->IN & 0x0F;                          // read all rows

        while (!(P4IN & BIT0) | !(P4IN & BIT1) |  !(P4IN & BIT2) | !(P4IN & BIT3))
        {
            idleSeconds = 0;
            MAP_WDT_A_clearTimer();
        }

        P4->OUT |= column_select[col];                // drive the active column high

        if (row != 0x0F) break;                       // if one of the input is low,
                                                      // some key is pressed.
    }

    P4->OUT |= 0xF0;                                  // drive all columns high before disable
    P4->DIR &= ~0xF0;                                 // disable all columns

    if (col == 3)
        return 0;                                     // if we get here, no key is pressed

                                                      // gets here when one of the columns has key pressed,
                                                      // check which row it is
    MAP_WDT_A_clearTimer();

    if (row == 0x0E) return col + 1;                 // key in row 0
    if (row == 0x0D) return 3 + col + 1;             // key in row 1
    if (row == 0x0B) return 6 + col + 1;             // key in row 2
    if (row == 0x07) return 9 + col + 1;            // key in row 3
    return 0;                                       // just to be safe
}

void create_pin(void)
{
    ST7735_DrawString(1, 7, "Please enter new PIN", ST7735_WHITE);

    int i = -1;
    int j = 0;

    //pin[1] = 0;

    do
    {
        MAP_WDT_A_clearTimer();
        pressed = keypad_getkey();                      //test input from keypad

        while (pressed == 0) //make sure that no button is pressed before taking input
        {
            MAP_WDT_A_clearTimer();
            pressed = keypad_getkey();                            //read keypad

            if (pressed && (pressed != 12) && (pressed != 10)) //if a number key was pressed
            {
                i++;            //increment the bit in the numbers entered array
                enter[i] = pressed; //save the digit entered into the numbers entered array

                if (pressed == 11)                    //if the 0 key was pressed
                {
                    pressed = 0;
                    enter[i] = 0; //save a 0 into the designated bit of the array
                }
                ST7735_DrawChar(((i * 8) % 120) + 1, (90 + 10 * (i / 15)),
                                pressed + '0', ST7735_WHITE, 0, 1); //print that value

                pressed = 1;

                SysTick_delay(10);           //wait 10ms for debouncing purposed
                break;
            }

            else if (pressed == 12)                    //if the pound key was pressed
            {
                if (i < 3)              //if the user entered less than 4 digits
                {

                    break;                                   //break out of loop
                }
                //if at least 4 digits were entered
                for (j = 0; j < 4; j++)
                {
                    pin[j] = enter[i - 3]; //save the last 4 digits entered from the enter array into the pin array
                    i++;                  //this is how the 4 digit pin is saved
                }

                i = -1;                          //reset i to its original value
                break;                                       //break out of loop
            }

            if (timeout)
            {
                break;
            }
        }
        if (timeout)
        {
            mainScreen_unarm();
        }
    }
    while (i != -1); //keep running through this loop until i was reset, meaning the user terminated their pin entry
}

void check_pin(void)
{
    MAP_WDT_A_clearTimer();
    int i = -1;
    int j = 0;

    ST7735_DrawString(2, 1, "Enter current PIN", ST7735_WHITE);
    if(sensor == 4){
        ST7735_DrawString(5,0,"FIRE ALARM", ST7735_RED);
        ST7735_DrawString(2,2,"to silence alarm", ST7735_WHITE);
        ST7735_DrawString(2,3,"when safe.", ST7735_WHITE);
    }
    do
    {
        MAP_WDT_A_clearTimer();
        pressed = keypad_getkey();                      //test input from keypad

        while (pressed == 0) //make sure that no button is pressed before taking input
        {
            MAP_WDT_A_clearTimer();
            pressed = keypad_getkey();                            //read keypad

            if (pressed && (pressed != 12) && (pressed != 10)) //if a number key was pressed
            {
                i++;            //increment the bit in the numbers entered array
                enter[i] = pressed; //save the digit entered into the numbers entered array

                if (pressed == 11)                    //if the 0 key was pressed
                {
                    pressed = 0;
                    enter[i] = 0;                        //save 0 into the array
                }

                ST7735_DrawChar(((i * 8) % 120) + 1, (30 + 10 * (i / 15)),
                                pressed + '0', ST7735_WHITE, 0, 1); //print that value
                pressed = 1;

                SysTick_delay(10);                   //wait 10 ms as a debouncer
                break;
            }
            else if (pressed == 12)               //if the pound key was pressed
            {
                if (i < 3)              //if the user entered less than 4 digits
                {
                    break;                                   //break out of loop
                }
                //if at least 4 digits were entered
                for (j = 0; j < 4; j++)
                {
                    pin_check[j] = enter[i - 3]; //save the last 4 digits entered from the enter array into the pin array
                    i++;                  //this is how the 4 digit pin is saved
                }

                i = -1;                          //reset i to its original value
                break;                                       //break out of loop
            }
            if (timeout)
            {
                break;
            }
        }
        if (timeout)
        {
            mainScreen_unarm();
        }
    }
    while (i != -1); //keep running through this loop until i was reset, meaning the user terminated their pin entry
}

void Port_Init(void)
{
    MAP_WDT_A_clearTimer();
    P4SEL0 &= ~0xFF;
    P4SEL1 &= ~0xFF;                                //configure P4 as GPIO
    P4DIR &= ~0xFF;                                 //make P4 inputs
    P4REN |= 0xFF;                                  //enable pull resistors on P4
    P4OUT |= 0xFF;                                  // P4 is a pull-up


    P8DIR |= (BIT5 | BIT6 | BIT7 ); //make P8.5 thru 8.7 and 9.0 outputs for stepper motor
    P9DIR |= BIT0;
    P2DIR |= (BIT5 | BIT6);

    P2->SEL0 |= BIT4;                               // Setup the P2.4 to be an output for the alarm.  This should drive a sounder.
    P2->SEL1 &= ~BIT4;
    P2->DIR |= BIT4;
    P2->OUT &=~ BIT4;

    P1->DIR |= BIT0;                                //On board red LED
    P1->OUT |= BIT0;

    P1SEL0 &= ~BIT7;
    P1SEL1 &= ~BIT7;
    P1DIR &= ~BIT7;
    P1SEL0 &= ~BIT6;
        P1SEL1 &= ~BIT6;
        P1DIR &= ~BIT6;
}
