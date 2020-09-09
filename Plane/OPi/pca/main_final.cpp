#include <iostream>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>

#include <termios.h>
#include <cmath>
#include <cstring>
#include <string>

extern "C"
{
#include <linux/i2c-dev.h>
#include <i2c/smbus.h>
}
#include <iomanip>
using namespace std;

int16_t dataConv(int16_t data1, int16_t data2)
{
    int16_t value = data1 | (data2 << 8);
    if (value & (1 << 16 - 1))
    {
        value -= (1 << 16);
    }
    return value;
}
void i2c_write(int privfile, __u8 reg_address, __u8 val)
{
    char buf[2];

    buf[0] = reg_address;
    buf[1] = val;

    if (write(privfile, buf, 2) != 2)
    {
        printf("Error, unable to write to i2c device\n");
        exit(1);
    }
}

char i2c_read(int privfile, uint8_t reg_address)
{
    char buf[1];
    buf[0] = reg_address;

    if (write(privfile, buf, 1) != 1)
    {
        printf("Error, unable to write to i2c device\n");
        exit(1);
    }
    if (read(privfile, buf, 1) != 1)
    {
        printf("Error, unable to read from i2c device\n");
        exit(1);
    }

    return buf[0];
}

int16_t i2c_readword(int privfile, uint8_t MSB, uint8_t LSB)
{

    uint8_t msb = i2c_read(privfile, MSB);

    uint8_t lsb = i2c_read(privfile, LSB);

    return ((int16_t)msb << 8) + lsb;
}

#define MODE1 0x00                    //Mode  register  1
#define MODE2 0x01                    //Mode  register  2
#define SUBADR1 0x02                  //I2C-bus subaddress 1
#define SUBADR2 0x03                  //I2C-bus subaddress 2
#define SUBADR3 0x04                  //I2C-bus subaddress 3
#define ALLCALLADR 0x05               //LED All Call I2C-bus address
#define LED0 0x6                      //LED0 start register
#define LED0_ON_L 0x6                 //LED0 output and brightness control byte 0
#define LED0_ON_H 0x7                 //LED0 output and brightness control byte 1
#define LED0_OFF_L 0x8                //LED0 output and brightness control byte 2
#define LED0_OFF_H 0x9                //LED0 output and brightness control byte 3
#define LED_MULTIPLYER 4              // For the other 15 channels
#define ALLLED_ON_L 0xFA              //load all the LEDn_ON registers, byte 0 (turn 0-7 channels on)
#define ALLLED_ON_H 0xFB              //load all the LEDn_ON registers, byte 1 (turn 8-15 channels on)
#define ALLLED_OFF_L 0xFC             //load all the LEDn_OFF registers, byte 0 (turn 0-7 channels off)
#define ALLLED_OFF_H 0xFD             //load all the LEDn_OFF registers, byte 1 (turn 8-15 channels off)
#define PRE_SCALE 0xFE                //prescaler for output frequency
#define MAX_PWM_RES 4096              //Resolution 4096=12bit 分辨率，按2的阶乘计算，12bit为4096
#define CLOCK_FREQ 27000000.0         //25MHz default osc clock
#define PCA9685_DEFAULT_I2C_ADDR 0x40 // default i2c address for pca9685 默认i2c地址为0x40
#define PCA9685_DEFAULT_I2C_BUS 1

char *filename = "/dev/i2c-1";
int file;

void set_pwm_freq(int freq)
{
    uint8_t prescale = (CLOCK_FREQ / MAX_PWM_RES / freq) + 0.5;
    uint8_t oldmode = i2c_read(file, MODE1);
    uint8_t newmode = (oldmode & 0x7F) | 0x10; //sleep
    i2c_write(file, MODE1, newmode);           // go to sleep
    i2c_write(file, PRE_SCALE, prescale);
    i2c_write(file, MODE1, oldmode);
    usleep(100000);
    i2c_write(file, MODE1, oldmode | 0xA1);
}

void pca_reset()
{
    i2c_write(file, MODE2, 0x04);
    i2c_write(file, MODE1, 0x01);
    usleep(100000);
    i2c_write(file, MODE1, 0x00);
}

class pca
{

public:
    float mres = 4912.0 / 32760.0;
    int st1;
    int st2;
    int min_pwm;
    int max_pwm;
    int max_deg;
    int pca_bus;

    pca(int a, int b, int c, int d) : pca_bus(a), min_pwm(b), max_pwm(c), max_deg(d)
    {
    }

    void set_pwm(int on_value, int off_value)
    {
        i2c_write(file, LED0_ON_L + LED_MULTIPLYER * pca_bus, on_value & 0xFF);
        i2c_write(file, LED0_ON_H + LED_MULTIPLYER * pca_bus, on_value >> 8);
        i2c_write(file, LED0_OFF_L + LED_MULTIPLYER * pca_bus, off_value & 0xFF);
        i2c_write(file, LED0_OFF_H + LED_MULTIPLYER * pca_bus, off_value >> 8);
    }
    void rotate_deg(double deg)
    {
        if (deg < 0)
        {
            set_pwm(0, min_pwm);
        }
        else if (deg > max_deg)
        {
            set_pwm(0, max_pwm);
        }
        else
        {
            int calc_pwm = min_pwm + deg * (max_pwm - min_pwm) / max_deg;
            set_pwm(0, calc_pwm);
        }
    }
};

int main()
{
    file = open(filename, O_RDWR);
    if (ioctl(file, I2C_SLAVE, 0x40) < 0)
    {
        exit(1);
    }
    pca_reset();
    usleep(100000);
    set_pwm_freq(50);
    pca pca1(0, 70, 500, 180);
    pca pca2(1, 70, 500, 180);
    usleep(100000);
    while (true)
    {
        pca1.rotate_deg(0);
        pca2.rotate_deg(0);
        usleep(1000000);
        pca1.rotate_deg(180);
        pca2.rotate_deg(180);
        usleep(1000000);
    }
    pca_reset();
    return 0;
}