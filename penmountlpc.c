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


#define DRIVER_DESC     "Penmount LPC touchscreen driver"

MODULE_AUTHOR("Heikki Linnakangas <heikki.linnakangas@iki.fi>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");


#define PENMOUNT_PORT 0x0338
#define PENMOUNT_IRQ 6
#define PENMOUNT_INITMAX 5

#define TIMEOUT 25000

struct input_dev penmount_dev;

static void poll_penmount(void) {
  unsigned short addr = PENMOUNT_PORT;
  unsigned char flag;
  unsigned short xa,xb,ya,yb;
  unsigned char touch;

  flag = inb_p(addr+4);

  if(flag) {
    touch = inb_p(addr);
    xa = inb_p(addr);
    flag = inb_p(addr+4);
    xb = inb_p(addr);
    flag = inb_p(addr+4);
    ya = inb_p(addr);
    flag = inb_p(addr+4);
    yb = inb_p(addr);

    input_report_abs(&penmount_dev, ABS_X, xa*128+xb);
    input_report_abs(&penmount_dev, ABS_Y, ya*128+yb);
    input_report_key(&penmount_dev, BTN_LEFT, touch & 0x40);
    input_sync(&penmount_dev);
//    printk(KERN_INFO "penmount (%d|%d) %s\n", xa*128+xb, ya*128+yb, (touch & 0x40) ? "press" : "release");

  }
}

static irqreturn_t penmount_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
  poll_penmount();

  return IRQ_HANDLED;
}

static int __init penmount_init(void)
{

  unsigned short initPorts[]={+4,+4,+4,+0,+4,+4};
    
  unsigned char initWData[]={0xf8,0xfb,0xf0,0xf2,0xf1,0xf9};
  unsigned char initRData[]={0x09,0x09,0x09,0xf0,0x09};
    
  unsigned short addr = PENMOUNT_PORT;	

  int initDataCount=0;
  int timeout;
    

  for (initDataCount=0;initDataCount<PENMOUNT_INITMAX;initDataCount++) {
        outb_p(initWData[initDataCount],initPorts[initDataCount]+addr);
        timeout=0;
        while ((timeout++<TIMEOUT) && 
	       (initRData[initDataCount]!=inb_p(initPorts[initDataCount]+addr))) 
	  udelay(10);
	
	if (timeout>TIMEOUT) {
	    printk(KERN_ERR "penmountlpc.c: timeout\n");
	    return -EPERM;
	}
  }

  outb_p(initWData[PENMOUNT_INITMAX],initPorts[PENMOUNT_INITMAX]+addr);



  if (request_irq(PENMOUNT_IRQ, penmount_interrupt, 0, "penmountlpc", NULL)) {
    return -EBUSY;
  }


  init_input_dev(&penmount_dev);

  input_set_abs_params(&penmount_dev, ABS_X, 0, 128*7, 4, 0);
  input_set_abs_params(&penmount_dev, ABS_Y, 0, 128*7, 4, 0);

  penmount_dev.evbit[0] = BIT(EV_ABS) | BIT(EV_KEY);
  penmount_dev.absbit[LONG(ABS_X)] = BIT(ABS_X);
  penmount_dev.absbit[LONG(ABS_Y)] |= BIT(ABS_Y);
  penmount_dev.keybit[LONG(BTN_LEFT)] |= BIT(BTN_LEFT);
  
  input_register_device(&penmount_dev);

  printk(KERN_ERR "penmountlpc.c: init finished\n");

  return 0;
}

static void __exit penmount_exit(void)
{
  input_unregister_device(&penmount_dev);
  free_irq(PENMOUNT_IRQ, NULL);
  printk(KERN_ERR "penmountlpc.c: driver removed\n");
}

module_init(penmount_init);
module_exit(penmount_exit);

