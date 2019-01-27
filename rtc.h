#ifndef _rtcH_
#define _rtcH_

void write_RTC(unsigned char data[8]);
void readHistory_RTC(unsigned char data_in[80], uint8_t armed, uint8_t sensor);
void I2C_init(void);
void read_RTC(unsigned char data_in[8]);

#endif
