/*
 * eventdevice driver for Penmount LPC touchscreen, that comes with
 * Dialogue Flybook notebooks.
 *
 * Changelog:
 *
 *   20080810 moved to version control at http://penmountlpc.googlecode.com/
 *   20080801 modified to support flybook V5 touchscreen by Tobias Wiese
 *   20080529 fixed small 2.6.24 issues by Elmar Hanlhofer
 *   20060113 fixed small 2.6.15 issues by Christoph Pittracher
 *   20050902 cleanup, irq data check by Christoph Pittracher
 *   20050624 init sequence and timeout optimized by Elmar Hanlhofer
 *   20050622 some evbit fixes by Christoph Pittracher
 *   20050617 added init sequence and more debug messages by Elmar Hanlhofer
 *   20050213 initial version by Heikki Linnakangas
 * 
 * Copyright 2005-2008 Heikki Linnakangas
 * Copyright 2005,2008 Elmar Hanlhofer
 * Copyright 2005-2006 Christoph Pittracher
 * Copyright 2008      Tobias Wiese
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 */

#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/delay.h>

#include <asm/irq.h>
#include <asm/io.h>

MODULE_AUTHOR("Heikki Linnakangas <heikki.linnakangas@iki.fi>");
MODULE_DESCRIPTION("Penmount LPC touchscreen driver");
MODULE_LICENSE("GPL");

#define PENMOUNT_PORT 0x0338
#define PENMOUNT_IRQ 6
#define TIMEOUT 25000

struct input_dev* penmount_dev;

static void poll_penmount(void)
{
	unsigned short xa, xb, ya, yb;
	unsigned char touch;
	udelay(1000);
	touch = inb_p(PENMOUNT_PORT);
	udelay(1000);
	xa = inb_p(PENMOUNT_PORT);
	udelay(1000);
	xb = inb_p(PENMOUNT_PORT);
	udelay(1000);
	ya = inb_p(PENMOUNT_PORT);
	udelay(1000);
	yb = inb_p(PENMOUNT_PORT);

	if ((touch == 0xBF || touch == 0xFF) /* Press or release */
	    && xa < 8 && xb < 128 && ya < 8 && yb < 128) {
		//printk(KERN_INFO "penmount (%d|%d) %s\n", xb, yb, (touch & 0x40) ? "press" : "release");

		input_report_abs(penmount_dev, ABS_X, xa * 128 + xb);
		input_report_abs(penmount_dev, ABS_Y, ya * 128 + yb);
		input_report_key(penmount_dev, BTN_TOUCH, (touch & 0x40) != 0);
		// TODO: Use BTN_LEFT here for evdev support
		input_sync(penmount_dev);
	}
}

static irqreturn_t penmount_interrupt(int irq, void *dummy)
{
	poll_penmount();
	return IRQ_HANDLED;
}

static int penmount_detect_init(void)
{
#define PENMOUNT_INITMAX 5
	unsigned short initPorts[] = { PENMOUNT_PORT+4, PENMOUNT_PORT+4, PENMOUNT_PORT+4, PENMOUNT_PORT, PENMOUNT_PORT+4, PENMOUNT_PORT+4 };
	unsigned char initWData[] =  { 0xf8, 0xfb, 0xf0, 0xf2, 0xf1, 0xf9 };
	unsigned char initRData[] =  { 0x09, 0x09, 0x09, 0xf0, 0x09 };
	int initDataCount = 0;
	int timeout;

	for (initDataCount = 0; initDataCount < PENMOUNT_INITMAX; initDataCount++) {
		outb_p(initWData[initDataCount], initPorts[initDataCount]);
		timeout = TIMEOUT;
		while (timeout-- && initRData[initDataCount] != inb_p(initPorts[initDataCount]))
			udelay(10);

		if (timeout == 0) {
			printk(KERN_ERR "penmountlpc.c: timeout during initialization\n");
			return -ENODEV;
		}
	}

	outb_p(initWData[PENMOUNT_INITMAX], initPorts[PENMOUNT_INITMAX]);

	return 0;
}

static int __init penmount_init(void)
{
	int err;
	if ((err = request_irq(PENMOUNT_IRQ, penmount_interrupt, 0, "penmountlpc", NULL)))
		return err;

	if (!request_region(PENMOUNT_PORT, 8, "penmountlpc")) {
		free_irq(PENMOUNT_IRQ, NULL);
		return -EBUSY;
	}

	if ((err = penmount_detect_init())) {
		release_region(PENMOUNT_PORT, 8);
		free_irq(PENMOUNT_IRQ, NULL);
		return err;
	}

	penmount_dev = input_allocate_device();
	if (!penmount_dev)
	{
		release_region(PENMOUNT_PORT, 8);
		free_irq(PENMOUNT_IRQ, NULL);
		return -ENOMEM;
	}

	penmount_dev->name = "PenmountLPC TouchScreen";
	penmount_dev->id.bustype = BUS_ISA;
	penmount_dev->id.vendor = 0;
	penmount_dev->id.product = 0;
	penmount_dev->id.version = 0;

	input_set_abs_params(penmount_dev, ABS_X, 0, 128 * 7, 4, 0);
	input_set_abs_params(penmount_dev, ABS_Y, 0, 128 * 7, 4, 0);

	penmount_dev->evbit[0] = BIT(EV_ABS) | BIT(EV_KEY);
	penmount_dev->absbit[BIT_WORD(ABS_X)] = BIT(ABS_X);
	penmount_dev->absbit[BIT_WORD(ABS_Y)] |= BIT(ABS_Y);
	// TODO: Use BTN_LEFT here for evdev
	penmount_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH); 
	
	err = input_register_device(penmount_dev);
	if (err)
	{
		input_free_device(penmount_dev);
		release_region(PENMOUNT_PORT, 8);
		free_irq(PENMOUNT_IRQ, NULL);
		return err;
	}

	printk(KERN_ERR "penmountlpc.c: init finished\n");
	return 0;
}

static void __exit penmount_exit(void)
{
	input_unregister_device(penmount_dev);
	release_region(PENMOUNT_PORT, 8);
	free_irq(PENMOUNT_IRQ, NULL);
	printk(KERN_ERR "penmountlpc.c: driver removed\n");
}

module_init(penmount_init);
module_exit(penmount_exit);

