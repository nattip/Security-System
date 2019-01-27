
#include <stdio.h>
#include <stdint.h>
#include "ST7735.h"
#include "driverlib.h"
//#include "..\inc\msp432p401r.h"

#define HISTORY 0x000200000                        //flash memory macros
#define PINLOCATION  0x000202000

int pressed;                                            //keypad pressed variable
uint8_t alarmFlag;
uint8_t armFlag;
uint8_t historyData[80];
char pin[4];
int enter[1000];

void writeHistory_Flash(void)
{
    MAP_WDT_A_clearTimer();
    int i;
    uint8_t* addr_pointer;

    if(alarmFlag)
    {
        MAP_WDT_A_clearTimer();
        addr_pointer = HISTORY + 4;     // point to address in flash for saving data
        for (i = 8; i < 40; i++)                     //Saves 4 most recent times
        {
            historyData[i] = *addr_pointer++;
        }
    }

    if(armFlag)
    {
        MAP_WDT_A_clearTimer();
        addr_pointer = HISTORY + 44;     // point to address in flash for saving data
        for (i = 48; i < 80; i++)                    //Saves 4 most recent times
        {
            historyData[i] = *addr_pointer++;
        }
    }

    MAP_FlashCtl_unprotectSector(FLASH_INFO_MEMORY_SPACE_BANK0, FLASH_SECTOR0);         // Unprotecting Info Bank 0, Sector 0

    while (!MAP_FlashCtl_eraseSector(HISTORY));                                   // Erase the flash sector starting HISTORY.
    while (!MAP_FlashCtl_programMemory(historyData,(void*) HISTORY + 4, 80));       // Program the flash with the new data.
                                                                                      // leave first 4 bytes unprogrammed

    MAP_FlashCtl_protectSector(FLASH_INFO_MEMORY_SPACE_BANK0, FLASH_SECTOR0);        // Setting the sector back to protected
}

void readHistory_Flash(void)
{
    MAP_WDT_A_clearTimer();
    int i;
    uint8_t* addr_pointer;


    addr_pointer = HISTORY + 4;                            // point to address in flash for saving data

    for (i = 0; i < 80; i++)                                    //Reads all previous times
    {
        historyData[i] = *addr_pointer++;
    }
}

void readAlarmHistory_Flash(void)
{
    MAP_WDT_A_clearTimer();
    int i;
    uint8_t* addr_pointer;


    addr_pointer = HISTORY + 4;                            // point to address in flash for saving data

    for (i = 0; i < 40; i++)                                    //Reads all previous times
    {
        historyData[i] = *addr_pointer++;
    }
}

void readArmHistory_Flash(void)
{
    MAP_WDT_A_clearTimer();
    int i;
    uint8_t* addr_pointer;


    addr_pointer = HISTORY + 44;                            // point to address in flash for saving data

    for (i = 40; i < 80; i++)                                    //Reads all previous times
    {
        historyData[i] = *addr_pointer++;
    }
}

void writePIN_Flash(void)
{
    MAP_WDT_A_clearTimer();
    int i;
    uint8_t* addr_pointer;                                                               //read info from RTC

    addr_pointer = PINLOCATION + 4;                                                   // point to address in flash for saving data

    MAP_FlashCtl_unprotectSector(FLASH_INFO_MEMORY_SPACE_BANK1, FLASH_SECTOR0);       // Unprotecting Info Bank 0, Sector 0

    while (!MAP_FlashCtl_eraseSector(PINLOCATION));                                  // Erase the flash sector starting HISTORY.
    while (!MAP_FlashCtl_programMemory(pin,(void*) PINLOCATION + 4, 40));      // Program the flash with the new data.
                                                                                     // leave first 4 bytes unprogrammed

    MAP_FlashCtl_protectSector(FLASH_INFO_MEMORY_SPACE_BANK1, FLASH_SECTOR0);        // Setting the sector back to protected
}

void readPIN_Flash(void)
{
    MAP_WDT_A_clearTimer();
    int i;
    uint8_t* addr_pointer;

    addr_pointer = PINLOCATION + 4;                                                 // point to address in flash for saving data

    for (i = 0; i < 4; i++)                                                         //Reads all previous times
    {
        pin[i] = *addr_pointer++;
    }
}

void printPIN(void)
{
    MAP_WDT_A_clearTimer();
    char dataOut [30];

    sprintf(dataOut, "PIN set to: %d%d%d%d", pin[0], pin[1], pin[2], pin[3]);
    ST7735_DrawString(1,13,dataOut,ST7735_WHITE);
}

void printHistory(void)
{
    MAP_WDT_A_clearTimer();
    int i;
    char dataOut [60];

    for(i=0;i<10;i++)                                            //Outputs data to screen
        {
            sprintf(dataOut, "Time: %.2x:%.2x:%.2x", historyData[(i*8)+2],historyData[(i*8)+1],historyData[(i*8)+0]);
            ST7735_DrawString(1,i+4,dataOut,ST7735_WHITE);
        }
}


void printAlarmHistory(void)
{
    MAP_WDT_A_clearTimer();
    int i;
    char dataOut [60];
    char SensorMap[] = {'D','W','C','M','F'};

    for(i=0;i<5;i++)                                            //Outputs data to screen
        {
            sprintf(dataOut, "%.2x:%.2x:%.2x %x/%x/%x %c", historyData[(i*8)+2],historyData[(i*8)+1],historyData[(i*8)+0],historyData[(i*8)+5],historyData[(i*8)+4],historyData[(i*8)+6], SensorMap[historyData[(i*8)+7]]);
            ST7735_DrawString(1,i+4,dataOut,ST7735_WHITE);
        }
}


void printArmHistory(void)
{
    MAP_WDT_A_clearTimer();
    int i;
    char dataOut [60];
    char ADMap[] = {'D','A'};

    for(i=5;i<10;i++)                                            //Outputs data to screen
        {
        sprintf(dataOut, "%.2x:%.2x:%.2x %x/%x/%x %c", historyData[(i*8)+2],historyData[(i*8)+1],historyData[(i*8)+0],historyData[(i*8)+5],historyData[(i*8)+4],historyData[(i*8)+6], ADMap[historyData[(i*8)+7]]);
            ST7735_DrawString(1,i,dataOut,ST7735_WHITE);
        }
}
