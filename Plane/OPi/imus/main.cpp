#include <iostream>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <chrono>
#include <ctime>
#include <future>
#include <termios.h>
#include <cmath>
#include <cstring>
#include <string>
#include "MahonyAHRS.cpp"

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

string Convert(float number)
{
	ostringstream buff;
	buff << number;
	return buff.str();
}

/*


		MPU9250

	
*/
#define PWR_MGMT_1 0x6B
#define USER_CTRL 0x6A
#define AK8963_ST1 0x02
#define SMPLRT_DIV 0x19
#define CONFIG 0x1A
#define GYRO_CONFIG 0x1B
#define ACCEL_CONFIG 0x1C
#define ACCEL_CONFIG_2 0x1D
#define INT_PIN_CFG 0x37
#define AK8963_CNTL1 0x0A
#define AK8963_ASAX 0x10
class mpu
{

public:
	int bus;
	bool ready;
	int file;
	int mpu_file;
	char *filename = "/dev/i2c-1";
	char *mpu_filename = "/dev/i2c-1";
	float mres = 4912.0 / 32760.0;
	float magXc;
	float magYc;
	float magZc;
	float magX;
	float magY;
	float magZ;
	float accX;
	float accY;
	float accZ;
	float gyroX;
	float gyroY;
	float gyroZ;
	float gyroXoff;
	float gyroYoff;
	float gyroZoff;
	int st1;
	int st2;

	mpu(int a) : bus(a)
	{
	}

	int init()
	{
		file = open(filename, O_RDWR);
		mpu_file = open(mpu_filename, O_RDWR);
		if (ioctl(file, I2C_SLAVE, 0x0c) < 0)
		{
			exit(1);
		}
		if (ioctl(mpu_file, I2C_SLAVE, 0x68) < 0)
		{
			exit(1);
		}
		//MPU CONIG
		i2c_write(mpu_file, PWR_MGMT_1, 0x00);
		usleep(100000);
		i2c_write(mpu_file, PWR_MGMT_1, 0x01);
		usleep(100000);
		i2c_write(mpu_file, CONFIG, 0x00);
		i2c_write(mpu_file, SMPLRT_DIV, 0x00);
		i2c_write(mpu_file, GYRO_CONFIG, 0x00);
		i2c_write(mpu_file, ACCEL_CONFIG, 0x00);
		i2c_write(mpu_file, ACCEL_CONFIG_2, 0x00);
		i2c_write(mpu_file, INT_PIN_CFG, 0x02);
		usleep(100000);
		i2c_write(mpu_file, USER_CTRL, 0x00);
		usleep(100000);

		//AK CONFIG
		i2c_write(file, AK8963_CNTL1, 0x00);
		usleep(100000);
		i2c_write(file, AK8963_CNTL1, 0x0F);
		usleep(100000);
		__u8 buf[3];
		buf[0] = i2c_read(file, AK8963_ASAX);
		buf[1] = i2c_read(file, AK8963_ASAX + 1);
		buf[2] = i2c_read(file, AK8963_ASAX + 2);
		magXc = (buf[0] - 128) / 256.0 + 1.0;
		magYc = (buf[1] - 128) / 256.0 + 1.0;
		magZc = (buf[2] - 128) / 256.0 + 1.0;
		i2c_write(file, AK8963_CNTL1, 0x00);
		usleep(100000);
		i2c_write(file, AK8963_CNTL1, 0x16);
	}
	void read_raw()
	{

		std::clock_t c_start = std::clock();
		auto t_start = std::chrono::high_resolution_clock::now();
		std::async(std::launch::async, read_mag());
		std::clock_t c_end = std::clock();
		auto t_end = std::chrono::high_resolution_clock::now();
		cout << std::chrono::duration<double, std::micro>(t_end - t_start).count()
			 << " ms\t";
		c_start = std::clock();
		t_start = std::chrono::high_resolution_clock::now();
		std::async(std::launch::async, read_accel());
		c_end = std::clock();
		t_end = std::chrono::high_resolution_clock::now();
		cout << std::chrono::duration<double, std::micro>(t_end - t_start).count()
			 << " ms\t";
		c_start = std::clock();
		t_start = std::chrono::high_resolution_clock::now();
		std::async(std::launch::async, read_gyro());
		c_end = std::clock();
		t_end = std::chrono::high_resolution_clock::now();
		cout << std::chrono::duration<double, std::micro>(t_end - t_start).count()
			 << " ms\n";
	}
	void *read_mag()
	{
		st1 = i2c_read(file, AK8963_ST1);
		if (st1 & 0x01)
		{
			int16_t buf[7];
			buf[0] = i2c_read(file, 0x03);
			buf[1] = i2c_read(file, 0x04);
			buf[2] = i2c_read(file, 0x05);
			buf[3] = i2c_read(file, 0x06);
			buf[4] = i2c_read(file, 0x07);
			buf[5] = i2c_read(file, 0x08);
			buf[6] = i2c_read(file, 0x09);
			st2 = buf[6];
			if (buf[6] == 16)
			{
				magX = dataConv(buf[0], buf[1]);
				magY = dataConv(buf[2], buf[3]);
				magZ = dataConv(buf[4], buf[5]);

				magX = magX * mres * magXc;
				magY = magY * mres * magYc;
				magZ = magZ * mres * magZc;
			}
		}
	}
	void *read_accel()
	{
		int16_t buf[6];
		buf[0] = i2c_read(mpu_file, 0x3B);
		buf[1] = i2c_read(mpu_file, 0x3C);
		buf[2] = i2c_read(mpu_file, 0x3D);
		buf[3] = i2c_read(mpu_file, 0x3E);
		buf[4] = i2c_read(mpu_file, 0x3F);
		buf[5] = i2c_read(mpu_file, 0x40);
		accX = dataConv(buf[1], buf[0]) / 16384.0;
		accY = dataConv(buf[3], buf[2]) / 16384.0;
		accZ = dataConv(buf[5], buf[4]) / 16384.0;
	}
	void *read_gyro()
	{
		int16_t buf[6];
		buf[0] = i2c_read(mpu_file, 0x43);
		buf[1] = i2c_read(mpu_file, 0x44);
		buf[2] = i2c_read(mpu_file, 0x45);
		buf[3] = i2c_read(mpu_file, 0x46);
		buf[4] = i2c_read(mpu_file, 0x47);
		buf[5] = i2c_read(mpu_file, 0x48);
		gyroX = dataConv(buf[1], buf[0]) / 131.0;
		gyroY = dataConv(buf[3], buf[2]) / 131.0;
		gyroZ = dataConv(buf[5], buf[4]) / 131.0;
	}
};

int main()
{
	printf("Bon dia\n");
	mpu mpu1(0);
	mpu1.init();
	Mahony filter;
	int serial = open("/dev/ttyS0", O_RDWR);
	struct termios config;
	tcgetattr(serial, &config);
	cfsetispeed(&config, B115200);
	tcsetattr(serial, TCSANOW, &config);
	while (true)
	{

		std::clock_t c_start = std::clock();
		auto t_start = std::chrono::high_resolution_clock::now();
		mpu1.read_raw();
		std::clock_t c_end = std::clock();
		auto t_end = std::chrono::high_resolution_clock::now();
		cout << std::chrono::duration<double, std::micro>(t_end - t_start).count()
			 << " ms\n"; /*
		cout
			<< '\r'
			<< std::setw(10) << std::setfill(' ') << mpu1.accX << '\t'
			<< std::setw(10) << std::setfill(' ') << mpu1.accY << '\t'
			<< std::setw(10) << std::setfill(' ') << mpu1.accZ << '\t'
			<< std::setw(10) << std::setfill(' ') << mpu1.gyroX << '\t'
			<< std::setw(10) << std::setfill(' ') << mpu1.gyroY << '\t'
			<< std::setw(10) << std::setfill(' ') << mpu1.gyroZ << '\t'
			<< std::setw(10) << std::setfill(' ') << mpu1.magX << '\t'
			<< std::setw(10) << std::setfill(' ') << mpu1.magY << '\t'
			<< std::setw(10) << std::setfill(' ') << mpu1.magZ << '\t'
			<< std::flush;*/
	}
	return 0;
}