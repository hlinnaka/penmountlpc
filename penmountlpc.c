//==========================================================================
// eventdevice driver for penmount touchscreen
// Licence GPL
//
// Driver written by Heikki Linnakangas <heikki.linnakangas@iki.fi>
// Driver modified by Elmar Hanlhofer development@plop.at http://www.plop.at
// Driver modified by Christoph Pittracher <pitt@segfault.info>
//
// Changelog:
//
//   20080529 fixed small 2.6.24 issues by Elmar Hanlhofer
//   20060113 fixed small 2.6.15 issues by Christoph Pittracher
//   20050902 cleanup, irq data check by Christoph Pittracher
//   20050624 init sequence and timeout optimized by Elmar Hanlhofer
//   20050622 some evbit fixes by Christoph Pittracher
//   20050617 added init sequence and more debug messages by Elmar Hanlhofer
// 
//==========================================================================

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
#define PENMOUNT_INITMAX 5
#define TIMEOUT 25000

struct input_dev* penmount_dev;

static void poll_penmount(void) {
  unsigned short xa, xb, ya, yb;
  unsigned char touch, flag;

  flag = inb_p(PENMOUNT_PORT + 4);
  if (flag) {
    touch = inb_p(PENMOUNT_PORT);
    xa = inb_p(PENMOUNT_PORT);
    inb_p(PENMOUNT_PORT + 4);
    xb = inb_p(PENMOUNT_PORT);
    inb_p(PENMOUNT_PORT + 4);
    ya = inb_p(PENMOUNT_PORT);
    inb_p(PENMOUNT_PORT + 4);
    yb = inb_p(PENMOUNT_PORT);

    if ((touch | 0x40) != 0xFF || (xa & 0xF8) != 0 || (ya & 0xF8) != 0 || (xb & 0x80) != 0 || (yb & 0x80) != 0)
      return;

    input_report_abs(penmount_dev, ABS_X, xa * 128 + xb);
    input_report_abs(penmount_dev, ABS_Y, ya * 128 + yb);
    input_report_key(penmount_dev, BTN_TOUCH, (touch & 0x40) != 0);
    input_sync(penmount_dev);
//    printk(KERN_INFO "penmount (%d|%d) %s\n", xa*128+xb, ya*128+yb, (touch & 0x40) ? "press" : "release");
  }
}

//static irqreturn_t penmount_interrupt(int irq, void *dummy, struct pt_regs *fp) {
static irqreturn_t penmount_interrupt(int irq, void *dummy) {
  poll_penmount();
  return IRQ_HANDLED;
}

static int __init penmount_init(void) {
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
      printk(KERN_ERR "penmountlpc.c: timeout\n");
      return -EPERM;
    }
  }

  outb_p(initWData[PENMOUNT_INITMAX], initPorts[PENMOUNT_INITMAX]);

  if (request_irq(PENMOUNT_IRQ, penmount_interrupt, 0, "penmountlpc", NULL))
    return -EBUSY;

  penmount_dev = input_allocate_device();
  if (!penmount_dev)
    return -ENOMEM;

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
  penmount_dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH); 
  
  input_register_device(penmount_dev);
  printk(KERN_ERR "penmountlpc.c: init finished\n");

  return 0;
}

static void __exit penmount_exit(void) {
  input_unregister_device(penmount_dev);
  free_irq(PENMOUNT_IRQ, NULL);
  printk(KERN_ERR "penmountlpc.c: driver removed\n");
}

module_init(penmount_init);
module_exit(penmount_exit);
