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
  }
}

static irqreturn_t penmount_interrupt(int irq, void *dummy, struct pt_regs *fp)
{
  poll_penmount();

  return IRQ_HANDLED;
}

static int __init penmount_init(void)
{
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

  return 0;
}

static void __exit penmount_exit(void)
{
  input_unregister_device(&penmount_dev);
  free_irq(PENMOUNT_IRQ, NULL);
}

module_init(penmount_init);
module_exit(penmount_exit);
