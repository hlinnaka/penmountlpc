//
// Driver modified by Elmar Hanlhofer development@plop.at
// http://www.plop.at
//
// 20050617 added init sequence and more debug messages
//

#include <linux/input.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>

#include <asm/irq.h>
#include <asm/io.h>


#define DRIVER_DESC     "Penmount LPC touchscreen driver"

MODULE_AUTHOR("Heikki Linnakangas <heikki.linnakangas@iki.fi>");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");


#define PENMOUNT_PORT 0x0338
#define PENMOUNT_IRQ 6
#define PENMOUNT_INITMAX 7

#define TIMEOUT 2000000

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
    input_report_key(&penmount_dev, BTN_TOUCH, !(touch & 0x40));
    input_sync(&penmount_dev);
//        printk(KERN_ERR "penmountlpc.c: key\n");

  }
}

static irqreturn_t penmount_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
  poll_penmount();

  return IRQ_HANDLED;
}

static int __init penmount_init(void)
{

  unsigned short initWPorts[]={0x33c,0x33c,0x33c,0x338,0x33c,0x33c,0x33c};
  unsigned short initRPorts[]={0x33c,0x33c,0x33c,0x338,0x33c,0x338,0x338};

  unsigned char initWData[]={0xf8,0xfb,0xf0,0xf2,0xf1,0xf9,0xf2};
  unsigned char initRData[]={0x09,0x09,0x09,0xf0,0x09,0xf9,0xf2};

  int initDataCount=0;
  int timeout;


 
    for (initDataCount=0;initDataCount<PENMOUNT_INITMAX;initDataCount++) {
     printk(KERN_ERR "penmountlpc.c: count %d\n", initDataCount);
	outb_p(initWData[initDataCount],initWPorts[initDataCount]);
	timeout=0;
	while ((timeout++<TIMEOUT) && (initRData[initDataCount]!=inb_p(initRPorts[initDataCount]))) {}
	if (timeout>TIMEOUT) {	
	  printk(KERN_ERR "penmountlpc.c: INIT TIMEOUT\n");
	  return -EBUSY;
        }
    }


 if (request_irq(PENMOUNT_IRQ, penmount_interrupt, 0, "penmountlpc", NULL)) {
    printk(KERN_ERR "penmountlpc.c: Can't allocate irq %d\n", PENMOUNT_IRQ);
    return -EBUSY;
  }


  init_input_dev(&penmount_dev);

  input_set_abs_params(&penmount_dev, ABS_X, 0, 128*7, 4, 0);
  input_set_abs_params(&penmount_dev, ABS_Y, 0, 128*7, 4, 0);

  penmount_dev.evbit[0] = BIT(EV_ABS) | BIT(EV_KEY) | BIT(EV_KEY);
  penmount_dev.absbit[LONG(ABS_X)] = BIT(ABS_X);
  penmount_dev.absbit[LONG(ABS_Y)] |= BIT(ABS_Y);
  penmount_dev.absbit[LONG(ABS_PRESSURE)] |= BIT(ABS_PRESSURE);
  penmount_dev.keybit[LONG(BTN_TOOL_FINGER)] |= BIT(BTN_TOOL_FINGER);

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
