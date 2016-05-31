/*****************************************************************************************
 * @file   main.c
 * @Author Ankit (ankit4970@gmail.com)
 * @date   May 27, 2016
 * @brief  Raspberry Pi is interfaced with MCP4970N(I2C-RTC from Microchip)
 * @ingroup	Raspberry_I2C
 * Detailed description of file.
 ****************************************************************************************/

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <linux/fs.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <assert.h>
#include <string.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>



#define  RTC_ADDRESS     	0x6F  	///<  0b01101111
#define  ADDR_SEC          	0x00  	///<  address of SECONDS register
#define  ADDR_MIN          	0x01   	///<  address of MINUTES register
#define  ADDR_HOUR         	0x02   	///<  address of HOURS register
#define  ADDR_DAY          	0x03   	///<  address of DAY OF WK register
#define  ADDR_STAT         	0x03   	///<  address of STATUS register
#define  ADDR_DATE         	0x04   	///<  address of DATE register
#define  ADDR_MNTH         	0x05   	///<  address of MONTH register
#define  ADDR_YEAR         	0x06   	///<  address of YEAR register
#define  ADDR_CTRL         	0x07   	///<  address of CONTROL register
#define  ADDR_CAL          	0x08   	///<  address of CALIB register
#define  ADDR_ULID         	0x09   	///<  address of UNLOCK ID register
#define  START_32KHZ  		0x80    ///<  start crystal: ST = b7 (ADDR_SEC)
#define  LP           		0x20 	///<  mask for the leap year bit(MONTH REG)
#define  HOUR_12      		0x40   	///<  12 hours format   (ADDR_HOUR)
#define  PM           		0x20  	///<  post-meridian bit (ADDR_HOUR)
#define  OUT_PIN      		0x80  	///<  = b7 (ADDR_CTRL)
#define  SQWE         		0x40   	///<  SQWE = b6 (ADDR_CTRL)
#define  ALM_NO       		0x00  	///<  no alarm activated        (ADDR_CTRL)
#define  ALM_0        		0x10  	///<  ALARM0 is       activated (ADDR_CTRL)
#define  ALM_1        		0x20  	///<  ALARM1 is       activated (ADDR_CTRL)
#define  ALM_01       		0x30   	///<  both alarms are activated (ADDR_CTRL)
#define  MFP_01H      		0x00   	///<  MFP = SQVAW(01 HERZ)      (ADDR_CTRL)
#define  MFP_04K      		0x01   	///<  MFP = SQVAW(04 KHZ)       (ADDR_CTRL)
#define  MFP_08K      		0x02  	///<  MFP = SQVAW(08 KHZ)       (ADDR_CTRL)
#define  MFP_32K      		0x03   	///<  MFP = SQVAW(32 KHZ)       (ADDR_CTRL)
#define  MFP_64H      		0x04   	///<  MFP = SQVAW(64 HERZ)      (ADDR_CTRL)
#define  ALMx_POL     		0x80   	///<  polarity of MFP on alarm  (ADDR_ALMxCTL)
#define  ALMxC_SEC    		0x00   	///<  ALARM compare on SEC      (ADDR_ALMxCTL)
#define  ALMxC_MIN    		0x10    ///<  ALARM compare on MIN      (ADDR_ALMxCTL)
#define  ALMxC_HR     		0x20   	///<  ALARM compare on HOUR     (ADDR_ALMxCTL)
#define  ALMxC_DAY    		0x30   	///<  ALARM compare on DAY      (ADDR_ALMxCTL)
#define  ALMxC_DAT    		0x40   	///<  ALARM compare on DATE     (ADDR_ALMxCTL)
#define  ALMxC_ALL    		0x70   	///<  ALARM compare on all param(ADDR_ALMxCTL)
#define  ALMx_IF      		0x08    ///<  MASK of the ALARM_IF      (ADDR_ALMxCTL)
#define  OSCON        		0x20    ///<  state of the oscillator(running or not)
#define  VBATEN       		0x08    ///<  enable battery for back-up


/*
--------------------------------------------------------------------------------------------------
 i2c_smbus_access
 -------------------------------------------------------------------------------------------------
 * @func 		i2c_smbus_access
 * @brief		Access i2c bus
 * @param[in]	devBus		: I2C bus dev entry ("/dev/i2c-0" or "/dev/i2c-1" entry)
 * 				devAddress  : I2C slave address
 * 				file  		Device File
 * 				read_write	Read/Write mode
 * 				size		Size of the data transfer
 * 				data 		pointer to i2c_smbus_data union
 * @return 		Opened File Descriptor
***************************************************************************************************/
static int i2c_smbus_access(int file, char read_write, __u8 command, int size, union i2c_smbus_data *data)
{
	struct i2c_smbus_ioctl_data args;

	args.read_write = read_write;
	args.command = command;
	args.size = size;
	args.data = data;

	return ioctl(file,I2C_SMBUS,&args);
}


/*
--------------------------------------------------------------------------------------------------
 i2CSetupDevice
 -------------------------------------------------------------------------------------------------
 * @func 		i2CSetupDevice
 * @brief		setup i2c device
 * @param[in]	devBus		: I2C bus dev entry ("/dev/i2c-0" or "/dev/i2c-1" entry)
 * 				devAddress  : I2C slave address
 * @return 		Opened File Descriptor
***************************************************************************************************/
static int i2CSetupDevice(const char *devBus, int devAddress)
{
	int fd = 0;

	if ((fd = open (devBus, O_RDWR)) < 0)
	{
		printf("Unable to open I2C device: %s\n", strerror(errno));
		return -1;
	}

	if (ioctl (fd, I2C_SLAVE, devAddress) < 0)
	{
		printf("Unable to select I2C device: %s\n", strerror(errno)) ;
		return -1;
	}

	return fd ;
}

/*
--------------------------------------------------------------------------------------------------
 i2c_smbus_read_byte
 -------------------------------------------------------------------------------------------------
 * @brief		read data byte from previous selected I2C device
 * @param[in]	file  	:	Device File
 * @pre			Device file must be open
 * @return 		Byte Data
**************************************************************************************************/
static int i2c_smbus_read_byte(int file)
{
	union i2c_smbus_data data;
	
	if (i2c_smbus_access(file,I2C_SMBUS_READ,0,I2C_SMBUS_BYTE,&data))
	{
		return -1;
	}
	else
	{		
		return 0x0FF & data.byte;
	}
}

/*
--------------------------------------------------------------------------------------------------
 i2c_smbus_write_byte_data
 -------------------------------------------------------------------------------------------------
 * @brief		write data byte to previously selected I2C device
 * @param[in]	file  		: Device File
 * 				command		: Command
 * 				value  		: Value to be written
 * @pre			Device file must be open
 * @return 		Byte Data
**************************************************************************************************/
static int i2c_smbus_write_byte_data(int file, __u8 command, __u8 value)
{
	union i2c_smbus_data data;
	data.byte = value;
	return i2c_smbus_access(file, I2C_SMBUS_WRITE, command, I2C_SMBUS_BYTE_DATA, &data);
}

/*
--------------------------------------------------------------------------------------------------
 i2c_smbus_read_byte_data
 -------------------------------------------------------------------------------------------------
 * @brief		Reas data byte from previously selected I2C device
 * @param[in]	file  		: Device File
 * 				command		: Command
 * @pre			Device file must be open
 * @return 		Byte Data
**************************************************************************************************/
static int i2c_smbus_read_byte_data(int file, __u8 command)
{
	union i2c_smbus_data data;

	if (i2c_smbus_access(file, I2C_SMBUS_READ, command, I2C_SMBUS_BYTE_DATA, &data))
	{
		return -1;
	}
	else
	{
		return data.byte;
	}
}

/*
--------------------------------------------------------------------------------------------------
 main
 -------------------------------------------------------------------------------------------------
 * @brief		Main entry
 * 				Calls to i2CSetupDevice with RTC address
 * 				Enables RTC Oscillator and then reads minute,hour and second register 
 * @return 		None
**************************************************************************************************/
int main(int argc, char *argv[])
{
	int sec =0,hr =0,min =0;
	int fd=0,ret =0;

	do
	{
		// Setup I2C device
		fd = i2CSetupDevice("/dev/i2c-1", RTC_ADDRESS);
		if(fd < 0)
		{
			printf("Unable to open i2c device file \n");
			break;
		}

		// Enable RTC Oscillator
		ret = i2c_smbus_write_byte_data(fd , ADDR_SEC, (START_32KHZ));
		if(ret < 0)
		{
			printf("Error writing data\n");
			break;
		}

		while(1)
		{
			// Read data from RTC

			sec = (i2c_smbus_read_byte_data(fd, ADDR_SEC) & ~(1<<7));
			min = i2c_smbus_read_byte_data(fd, ADDR_MIN);
			hr = i2c_smbus_read_byte_data(fd, ADDR_HOUR);

			printf(" Time : %.2d:%.2d:%.2x\n",hr,min,sec);
			usleep(1000000);
	 	}
	}while(0);


    if(fd > 0)
    {
    	close(fd);
    }

    return 0;
}
