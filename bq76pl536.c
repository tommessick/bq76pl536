/*
  bq76pl536.c

  Copyright Tom Messick, 2012
  Copyright Scott Ellis, 2010

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/*
  Driver for the Texas Instruments bq76pl536 battery monitor IC.
  This version has only been tested on a Beaglebone board.

  Creates device /dev/bq76pl536.c
  Each read from the device gets data in the following format:
  All data is 8 bits.
  Voltage count       How many voltage measurements to follow
  Voltage 1           Range 0-255 - 0.02 volts/unit - 0-5.10 volts
  Voltage 2
     .
     .
     .
  Voltage n
  Chip count          How many Groups of chip paramters to follow
  Each chip returns
    Temperature 1     Signed degrees Celsius
    Temperature 2
    Status            Chip status
    Fault             Fault register
    Alert             Alert register
    Undervoltage      Undervoltage register
    Overvoltage       Overvoltage register
  CRC                 CRC-8, poly = x^8 + x^2 + x^1 + x^0, init = 0
                      CRC source available at flac.sourceforge.net
                      or google for "0x00, 0x07, 0x0E, 0x09"
*/
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/spi/spi.h>
#include <linux/string.h>
#include <linux/crc8.h>
#include <asm/uaccess.h>
#include "bq76pl536.h"

#define SPI_BUFF_SIZE	50
#define USER_BUFF_SIZE	128

#define SPI_BUS 2
#define SPI_BUS_CS1 0
#define SPI_BUS_SPEED 100000
#define MAX_BQ_DEVICES 32
#define MAX_XFER 10
#define CRC_TABLE_SIZE 256
#define CELL_MISSING_THRESHOLD 1000
typedef struct cell
{
	int chip;
	int index;
} cell_t;

cell_t *cells = 0;
int total_cell_count;

static void bq_prepare_spi_message(void);

static int cells_per_device[MAX_BQ_DEVICES+1] =
{
	/* The first element in this array is not used. The chain of
	   devices is numbered starting with 1. Adding an extra
	   element here gets rid of +1 and -i in the code.
	 */
	0,
	4, 4, 4, 3, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

/* Configure the pack when loading the driver. The driver looks at what is
   attached so this is not critical
*/
static int devices_used = 4;

module_param_array(cells_per_device, int, &devices_used, S_IRUGO);

const char this_driver_name[] = "bq76pl536";

struct bq_control {
	struct spi_message msg;
	struct spi_transfer xfer[MAX_XFER];
	u8 *tx_buff;
	u8 *rx_buff;
};

static struct bq_control bq_ctl;
static int xfer_index = 0;
static int byte_index = 0;

struct bq_dev {
	struct semaphore spi_sem;
	struct semaphore fop_sem;
	dev_t devt;
	struct cdev cdev;
	struct class *class;
	struct spi_device *spi_device;
	char *user_buff;
	u8 test_data;
};

static struct bq_dev bq_dev;

static u8 *crc8_table = 0;

static int writeRegister(u8 address, u8 reg, u8 data)
{
	u8 command;
	u8* addr;

	if (xfer_index >= MAX_XFER)
	{
		dev_alert(&bq_dev.spi_device->dev,
			  "Transfer index overflow\n");
		// TODO: Look for a better return code
		return -EFAULT;
	}

	xfer_index++;

	pr_devel("%s: write reg(%x %x) = %x\n",
		 this_driver_name, address, reg, data);

	bq_ctl.xfer[xfer_index].cs_change = 1;
	bq_ctl.xfer[xfer_index].tx_buf = &bq_ctl.tx_buff[byte_index];
	bq_ctl.xfer[xfer_index].rx_buf = 0;
	bq_ctl.xfer[xfer_index].len = 4;

	// Shift the address over and add the write bit;
	command = address << 1;
	command |= 1;
	addr = &bq_ctl.tx_buff[byte_index];
	bq_ctl.tx_buff[byte_index++] = command;
	bq_ctl.tx_buff[byte_index++] = reg;
	bq_ctl.tx_buff[byte_index++] = data;
	bq_ctl.tx_buff[byte_index++] = crc8(crc8_table, addr, 3, 0);

	spi_message_add_tail(&bq_ctl.xfer[xfer_index], &bq_ctl.msg);

	return 0;
}

/*
  Read a register or a register pair.
  This will terminate and run the current chain of writes.
*/
int readRegister(u8 address, u8 reg, int count)
{
	u8 command;
	u8 *result;
	u8 crc;
	int status;
	int val;

	if ((count != 1) && (count != 2))
	{
		dev_alert(&bq_dev.spi_device->dev,
			  "readRegister: count is %d, must be 1,2\n", count);
		return -EFAULT;
	}

	xfer_index++;

	if (xfer_index >= MAX_XFER)
	{
		dev_alert(&bq_dev.spi_device->dev,
			  "Transfer index overflow\n");
		return -EFAULT;
	}

	bq_ctl.xfer[xfer_index].cs_change = 1;
	bq_ctl.xfer[xfer_index].tx_buf = &bq_ctl.tx_buff[byte_index];
	bq_ctl.xfer[xfer_index].rx_buf = &bq_ctl.rx_buff[byte_index];
	bq_ctl.xfer[xfer_index].len = 4+count;

	/* Shift the address over and leave zero for read bit; */
	command = address << 1;
	bq_ctl.tx_buff[byte_index++] = command;
	bq_ctl.tx_buff[byte_index++] = reg;
	bq_ctl.tx_buff[byte_index++] = count;
	result = &bq_ctl.rx_buff[byte_index];
	/* Read the registers. There is no need to pad with zeros.
	   The values are not read by the chip
	*/
	byte_index += count;

	/* Read the CRC */
	byte_index++;

	spi_message_add_tail(&bq_ctl.xfer[xfer_index], &bq_ctl.msg);

	status = spi_sync(bq_dev.spi_device, &bq_ctl.msg);
	if (status != 0)
	{
		dev_alert(&bq_dev.spi_device->dev,
			  "read status = %x\n", status);
		return status;
	}

	crc = crc8(crc8_table, (u8*)bq_ctl.xfer[xfer_index].tx_buf, 3, 0);
	crc = crc8(crc8_table, (u8*)bq_ctl.xfer[xfer_index].rx_buf+3, count, crc);
	if(crc != *(char*)(bq_ctl.xfer[xfer_index].rx_buf+count+3))
	{
		dev_alert(&bq_dev.spi_device->dev,
			  "CRC error %x != %x\n", crc,
			  *(char*)(bq_ctl.xfer[xfer_index].rx_buf+count+3));
		return -EFAULT;
	}

	if (count == 1)
		val = *result;
	else
		val = *result<<8|*(result+1);

	pr_devel("read reg(%x %x) = %x\n", address, reg, val);

	return val;
}

//TODO: rename
int get_voltages(u8* p)
{
	int i;
	int temp;
	int size;
	int status;
	int tries = 0;
	u8* save = p;

	/* Start the ADC */
	bq_prepare_spi_message();
	writeRegister(BROADCAST, ADC_CONVERT, AC_CONV);
	status = spi_sync(bq_dev.spi_device, &bq_ctl.msg);
	if (status != 0)
	{
		dev_alert(&bq_dev.spi_device->dev,
			  "start conversion status = %x\n",
			  status);
		return 0;
	}

	/* Wait until the conversions are done. By the time we read
	   the first chip the others are done also
	*/
	do
	{
		bq_prepare_spi_message();
		temp = readRegister(1, DEVICE_STATUS, 1);
		dev_info(&bq_dev.spi_device->dev,
			 "Wait status = %x tries = %d\n", temp, tries);
		if (tries++ > 5)
		{
			dev_info(&bq_dev.spi_device->dev,
				 "Giving up\n");
			return 0;
		}

	} while ((temp & DRDY) == 0);

	*p++ = total_cell_count;

	for(i=0; i<total_cell_count; i++)
	{
		bq_prepare_spi_message();
		temp = readRegister(cells[i].chip, cells[i].index, 2);
		/* scale differently than the data sheet
		   Make 0-5.10 volts fit in one byte (0-255)
		*/
		*p++ = ((temp * 6250) / 327660);
	}

	*p++ = devices_used;

	for(i=1; i<devices_used+1; i++)
	{
		//xxx
		*p++ = cells_per_device[i];

		bq_prepare_spi_message();
		temp = readRegister(i, TEMPERATURE1, 2);
		dev_alert(&bq_dev.spi_device->dev,
				  "%d raw temperature = %x %d\n", i, temp, temp);
		//TODO: constants
		temp -= 2048;
		temp /= 120;
		*p++ = temp;
		bq_prepare_spi_message();
		temp = readRegister(i, TEMPERATURE2, 2);
		dev_alert(&bq_dev.spi_device->dev,
				  "%d raw temperature = %x %d\n", i, temp, temp);
		//TODO: constants
		temp -= 2048;
		temp /= 120;
		*p++ = temp;

		bq_prepare_spi_message();
		*p++ = readRegister(i, DEVICE_STATUS, 1);
		*p++ = readRegister(i, FAULT_STATUS, 1);
		*p++ = readRegister(i, ALERT_STATUS, 1);
		*p++ = readRegister(i, CUV_FAULT, 1);
		*p++ = readRegister(i, COV_FAULT, 1);
	}
	size = p - save;
	*p++ = crc8(crc8_table, save, size, 0);
	/* +1 to include the crc */
	return size+1;
}


int write_defaults(void)
{
	int status;

	bq_prepare_spi_message();
	/* Enable all cells & thermistors */
	writeRegister(BROADCAST, ADC_CONTROL, AC_CELL_SEL_6 | AC_TS1 | AC_TS2);

	/* Connect the thermistors to REG50 */
	writeRegister(BROADCAST, IO_CONTROL, TS1 | TS2);

	writeRegister(BROADCAST, SHDW_CTRL, SC_ENABLE);

	/* Start the ADC. This is needed because we read the voltages
	   to discover which cells are present
	*/
	writeRegister(BROADCAST, ADC_CONVERT, AC_CONV);

	// High voltage = 3.5V
	writeRegister(BROADCAST, SHDW_CTRL, SC_ENABLE);
	writeRegister(BROADCAST, CONFIG_COV, COV_350);

	// Low voltage = 3.0V
	writeRegister(BROADCAST, SHDW_CTRL, SC_ENABLE);
	writeRegister(BROADCAST, CONFIG_CUV, COV_300);

	// High voltage timer = 100ms
	writeRegister(BROADCAST, SHDW_CTRL, SC_ENABLE);
	writeRegister(BROADCAST, CONFIG_COVT, CC_USMS | 1);

	status = spi_sync(bq_dev.spi_device, &bq_ctl.msg);
	if (status != 0)
	{
		dev_alert(&bq_dev.spi_device->dev,
			  "write_defaults status = %x\n",
			  status);
	}

	return status;
}

void cov(int address)
{
	int cov;

	cov = readRegister(address, COV_FAULT, 1);
	dev_info(&bq_dev.spi_device->dev, "cov = %x\n", cov);
	cov = readRegister(address, CONFIG_COV, 1);
	dev_info(&bq_dev.spi_device->dev, "config cov = %x\n", cov);
}

void get_fault(u8 address)
{
	int fault;

	fault = readRegister(address, FAULT_STATUS, 1);
	writeRegister(address, FAULT_STATUS, fault);
	writeRegister(address, FAULT_STATUS, 0);
	dev_info(&bq_dev.spi_device->dev, "fault = %x\n", fault);
	if (fault & FS_POR)
	{
		dev_info(&bq_dev.spi_device->dev, "Power on\n");
	}
	if (fault & FS_COV)
	{
		dev_info(&bq_dev.spi_device->dev, "Cell over voltage\n");
		cov(address);
	}
}

void get_alert(int address)
{
	int alert;
	int address_reg;

	alert = readRegister(address, ALERT_STATUS, 1);
	writeRegister(address, ALERT_STATUS, alert);
	writeRegister(address, ALERT_STATUS, 0);
	dev_info(&bq_dev.spi_device->dev, "Alert = %x\n", alert);
	if ((alert & AS_AR) == 0)
	{
		address_reg = readRegister(address, ADDRESS_CONTROL, 1);
		dev_info(&bq_dev.spi_device->dev, "Address register = %x\n",
			 address_reg);
	}
}

int get_chip_status(int address)
{
	int val = 0;

	bq_prepare_spi_message();
	val = readRegister(address, DEVICE_STATUS, 1);
	if (val < 0)
	{
		return (int) val;
	}
	dev_info(&bq_dev.spi_device->dev,
		 "Chip %d status = %x\n", address, val);

	if ((val & DS_ADDR_RQST) == 0)
	{
		dev_alert(&bq_dev.spi_device->dev,
			  "Address not assigned!!!!!!!!!\n");
	}
	if (val & DS_FAULT)
	{
		get_fault(address);
	}
	if (val & DS_ALERT)
	{
		get_alert(address);
	}
	return val;
}

/*
  search_pack - Discover all the bq76PL536 chips in the system.

  This is right from the flow chart in the data sheet.

  The RESET_COMMAND only resets chips that are addressable plus one extra
  chip. This code will find and reset chips that somehow have bogus addresses.
*/

u8 search_pack (void)
{
	u8  look_for = 0;
	u8  n;
	int verify;
	int status;

	bq_prepare_spi_message();
	do
	{
		status = writeRegister(BROADCAST, RESET, RESET_COMMAND);
		if (status != 0)
		{
			return status;
		}
		look_for++;
		n=0;
		do
		{
			n++;
			status = writeRegister(DISCOVERY_ADDR,
					       ADDRESS_CONTROL, n);
			if (status != 0)
			{
				return status;
			}
			verify = readRegister(n, ADDRESS_CONTROL, 1);
			if (verify < 0)
			{
				return n-1;
			}
			if (verify != (n | AC_ADDR_RQST))
			{
				dev_alert(&bq_dev.spi_device->dev,
					  "search_pack: %x != %x\n",
					  verify, (n | AC_ADDR_RQST));
				return n-1;
			}
			bq_prepare_spi_message();
		} while (n < look_for);
	} while (n < devices_used);
	return n;
}


static void bq_prepare_spi_message(void)
{
	spi_message_init(&bq_ctl.msg);

	memset(bq_ctl.rx_buff, 0, SPI_BUFF_SIZE);

	xfer_index = -1;
	byte_index = 0;
}

static ssize_t bq_read(struct file *filp, char __user *buff, size_t count,
			loff_t *offp)
{
	size_t len;
	ssize_t status = 0;

	if (!buff)
		return -EFAULT;

	if (*offp > 0)
		return 0;

	if (down_interruptible(&bq_dev.fop_sem))
		return -ERESTARTSYS;

	len = get_voltages(bq_dev.user_buff);

	if (len == 0)
	{
		status = 0;
	}
	else
	{
		if (len < count)
			count = len;

		if (copy_to_user(buff, bq_dev.user_buff, count)) {
			dev_alert(&bq_dev.spi_device->dev,
				  "bq_read(): copy_to_user() failed\n");
			status = -EFAULT;
		} else {
			*offp += count;
			status = count;
		}

	}

	up(&bq_dev.fop_sem);

	return status;
}

static int bq_open(struct inode *inode, struct file *filp)
{
	int status = 0;

	if (down_interruptible(&bq_dev.fop_sem))
		return -ERESTARTSYS;

	if (!bq_dev.user_buff) {
		bq_dev.user_buff = kmalloc(USER_BUFF_SIZE, GFP_KERNEL);
		if (!bq_dev.user_buff)
			status = -ENOMEM;
	}

	up(&bq_dev.fop_sem);

	return status;
}

static int bq_probe(struct spi_device *spi_device)
{
	int count = 0;
	int cell_index;
	int chip_cell_count;
	int i;
	int j;
	int temp;
	int retval = -EFAULT;

	if (down_interruptible(&bq_dev.spi_sem))
		return -EBUSY;

	bq_dev.spi_device = spi_device;

	bq_ctl.tx_buff = kmalloc(SPI_BUFF_SIZE, GFP_KERNEL | GFP_DMA);
	if (!bq_ctl.tx_buff) {
		retval = -ENOMEM;
		goto bq_probe_error;
	}

	bq_ctl.rx_buff = kmalloc(SPI_BUFF_SIZE, GFP_KERNEL | GFP_DMA);
	if (!bq_ctl.rx_buff) {
		retval = -ENOMEM;
		goto bq_probe_error;
	}

	crc8_table = kmalloc(CRC_TABLE_SIZE, GFP_KERNEL);
	if (!crc8_table) {
		retval = -ENOMEM;
		goto bq_probe_error;
	}

	/* CRC-8, poly = x^8 + x^2 + x^1 + x^0, init = 0
	   See crc8.h for translation of poly to constant 7
	*/
	crc8_populate_msb(crc8_table, 7);

	count = search_pack();
	if (count == devices_used)
	{
 		dev_info(&bq_dev.spi_device->dev,
			  "Found %d chips\n", count);
	}
	else
	{
		/* This is the only effect of not correctly configuring the
		   pack definition when loading the driver
		*/
		dev_alert(&bq_dev.spi_device->dev,
			  "Expected %d chips found %d\n", devices_used, count);
		devices_used = count;
	}


	retval = write_defaults();
	for(i=1; i<count+1; i++)
		get_chip_status(i);
	/* Count the cells */
	total_cell_count = 0;
	for(i=1; i<count+1; i++)
	{
		chip_cell_count = 0;
		// TODO: make sure all valid cells are 1-x
		// TODO: set the valid cell mask
		for(j=VCELL1; j<=VCELL6; j+=2)
		{
			bq_prepare_spi_message();
			temp = readRegister(i, j, 2);
			dev_alert(&bq_dev.spi_device->dev,
				  "voltage %d\n", temp);
			if (temp > CELL_MISSING_THRESHOLD)
			{
				total_cell_count++;
				chip_cell_count++;
			}
		}
		if (cells_per_device[i] != chip_cell_count)
		{
			/* This is the only effect of not correctly
			   configuring the pack definition when
			   loading the driver
			*/
			dev_alert(&bq_dev.spi_device->dev,
				  " Chip %d expected %d cells found %d\n",
				  i, cells_per_device[i], chip_cell_count);
			cells_per_device[i] = chip_cell_count;
		}
	}

	cells = kmalloc(total_cell_count*sizeof(cell_t), GFP_KERNEL);
	if (!cells) {
		retval = -ENOMEM;
		goto bq_probe_error;
	}

	/* Fill in the cell array */
	cell_index = 0;
	for(i=1; i<count+1; i++)
	{
		for(j=VCELL1; j<=VCELL6; j+=2)
		{
			bq_prepare_spi_message();
			temp = readRegister(i, j, 2);
			if (temp > CELL_MISSING_THRESHOLD)
			{
				cells[cell_index].chip = i;
				cells[cell_index++].index = j;
			}
		}
	}

	dev_info(&bq_dev.spi_device->dev,
			  "Total cells = %d\n", total_cell_count);

	up(&bq_dev.spi_sem);
 bq_probe_error:
	if (retval != 0)
	{
		if (bq_ctl.tx_buff)
			kfree(bq_ctl.tx_buff);

		if (bq_ctl.rx_buff)
			kfree(bq_ctl.rx_buff);

		if (crc8_table)
			kfree(crc8_table);

		if (cells)
			kfree(cells);
	}

	return retval;
}

static int bq_remove(struct spi_device *spi_device)
{
	if (down_interruptible(&bq_dev.spi_sem))
		return -EBUSY;

	bq_dev.spi_device = NULL;

	if (bq_ctl.tx_buff)
		kfree(bq_ctl.tx_buff);

	if (bq_ctl.rx_buff)
		kfree(bq_ctl.rx_buff);

	if (crc8_table)
		kfree(crc8_table);

	if (cells)
		kfree(cells);

	up(&bq_dev.spi_sem);

	return 0;
}

static int __init add_bq_device_to_bus(void)
{
	struct spi_master *spi_master;
	struct spi_device *spi_device;
	struct device *pdev;
	char buff[64];
	int status = 0;

	spi_master = spi_busnum_to_master(SPI_BUS);
	if (!spi_master) {
		printk(KERN_ALERT
		       "%s: spi_busnum_to_master(%d) returned NULL\n",
		       this_driver_name, SPI_BUS);
		printk(KERN_ALERT "Missing modprobe omap2_mcspi?\n");
		return -1;
	}

	spi_device = spi_alloc_device(spi_master);
	if (!spi_device) {
		put_device(&spi_master->dev);
		printk(KERN_ALERT "%s: spi_alloc_device() failed\n",
		       this_driver_name);
		return -1;
	}

	spi_device->chip_select = SPI_BUS_CS1;

	/* Check whether this SPI bus.cs is already claimed */
	snprintf(buff, sizeof(buff), "%s.%u",
			dev_name(&spi_device->master->dev),
			spi_device->chip_select);

	pdev = bus_find_device_by_name(spi_device->dev.bus, NULL, buff);
 	if (pdev) {
		/* We are not going to use this spi_device, so free it */
		spi_dev_put(spi_device);

		/*
		  There is already a device configured for this bus.cs
		  It is okay if it us, otherwise complain and fail.
		*/
		if (pdev->driver && pdev->driver->name &&
		    strcmp(this_driver_name, pdev->driver->name)) {
			printk(KERN_ALERT
			       "Driver [%s] already registered for %s\n",
			       this_driver_name, buff);
			status = -1;
		}
	} else {
		spi_device->max_speed_hz = SPI_BUS_SPEED;
		spi_device->mode = SPI_MODE_1;
		spi_device->bits_per_word = 8;
		spi_device->irq = -1;
		spi_device->controller_state = NULL;
		spi_device->controller_data = NULL;
		strlcpy(spi_device->modalias, this_driver_name, SPI_NAME_SIZE);

		status = spi_add_device(spi_device);
		if (status < 0) {
			spi_dev_put(spi_device);
			dev_alert(&bq_dev.spi_device->dev,
				  "spi_add_device() failed: %d\n", status);
		}
	}

	put_device(&spi_master->dev);

	return status;
}

static struct spi_driver bq_driver = {
	.driver = {
		.name =	this_driver_name,
		.owner = THIS_MODULE,
	},
	.probe = bq_probe,
	.remove = __devexit_p(bq_remove),
};

static int __init bq_init_spi(void)
{
	int error;

	error = spi_register_driver(&bq_driver);
	if (error < 0) {
		printk(KERN_ALERT "%s spi_register_driver() failed %d\n",
		       this_driver_name, error);
		goto bq_init_error;
	}

	error = add_bq_device_to_bus();
	if (error < 0) {
		printk(KERN_ALERT "%s: add_bq_to_bus() failed\n",
		       this_driver_name);
		spi_unregister_driver(&bq_driver);
		goto bq_init_error;
	}

	return 0;

bq_init_error:

	if (bq_ctl.tx_buff) {
		kfree(bq_ctl.tx_buff);
		bq_ctl.tx_buff = 0;
	}

	if (bq_ctl.rx_buff) {
		kfree(bq_ctl.rx_buff);
		bq_ctl.rx_buff = 0;
	}

	return error;
}

static const struct file_operations bq_fops = {
	.owner =	THIS_MODULE,
	.read = 	bq_read,
	.open =		bq_open,
};

static int __init bq_init_cdev(void)
{
	int error;

	bq_dev.devt = MKDEV(0, 0);

	error = alloc_chrdev_region(&bq_dev.devt, 0, 1, this_driver_name);
	if (error < 0) {
		printk(KERN_ALERT "%s: alloc_chrdev_region() failed: %d \n",
		       this_driver_name, error);
		return -1;
	}

	cdev_init(&bq_dev.cdev, &bq_fops);
	bq_dev.cdev.owner = THIS_MODULE;

	error = cdev_add(&bq_dev.cdev, bq_dev.devt, 1);
	if (error) {
		printk(KERN_ALERT "%s: cdev_add() failed: %d\n",
		       this_driver_name, error);
		unregister_chrdev_region(bq_dev.devt, 1);
		return -1;
	}

	return 0;
}

static int __init bq_init_class(void)
{
	bq_dev.class = class_create(THIS_MODULE, this_driver_name);

	if (!bq_dev.class) {
		printk(KERN_ALERT "%s: class_create() failed\n",
		       this_driver_name);
		return -1;
	}

	if (!device_create(bq_dev.class, NULL, bq_dev.devt, NULL,
			this_driver_name)) {
		printk(KERN_ALERT "device_create(..., %s) failed\n",
			this_driver_name);
		class_destroy(bq_dev.class);
		return -1;
	}

	return 0;
}

static int __init bq_init(void)
{
	int i;

	/* Validate the configuration data
	   Don't register the device if this is not valid
	*/
	// TODO: is this needed???
	for (i=1; i<devices_used+1; i++) {
		if ((cells_per_device[i] != 3) &&
		    (cells_per_device[i] != 4) &&
		    (cells_per_device[i] != 5) &&
		    (cells_per_device[i] != 6)) {
			printk(KERN_ALERT
	 		       "%s: cellsPerDevice %d is not 3..6\n",
			       this_driver_name, cells_per_device[i]);
			goto fail_1;
		}
	}

	memset(&bq_dev, 0, sizeof(bq_dev));
	memset(&bq_ctl, 0, sizeof(bq_ctl));

	sema_init(&bq_dev.spi_sem, 1);
	sema_init(&bq_dev.fop_sem, 1);

	if (bq_init_cdev() < 0)
		goto fail_1;

	if (bq_init_class() < 0)
		goto fail_2;

	if (bq_init_spi() < 0)
		goto fail_3;

	return 0;

fail_3:
	device_destroy(bq_dev.class, bq_dev.devt);
	class_destroy(bq_dev.class);

fail_2:
	cdev_del(&bq_dev.cdev);
	unregister_chrdev_region(bq_dev.devt, 1);

fail_1:
	return -1;
}
module_init(bq_init);

static void __exit bq_exit(void)
{
	spi_unregister_device(bq_dev.spi_device);
	spi_unregister_driver(&bq_driver);

	device_destroy(bq_dev.class, bq_dev.devt);
	class_destroy(bq_dev.class);

	cdev_del(&bq_dev.cdev);
	unregister_chrdev_region(bq_dev.devt, 1);

	if (bq_dev.user_buff)
		kfree(bq_dev.user_buff);
}
module_exit(bq_exit);

MODULE_AUTHOR("Scott Ellis");
MODULE_DESCRIPTION("Driver for Texas Instruments BQ76PL536");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.2");

