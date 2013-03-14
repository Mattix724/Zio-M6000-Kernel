/*
 * gsensor.h
 *
 *  Created on: 13- 1月-09
 *      Author: oscar
 */

#ifndef GSENSOR_H_
#define GSENSOR_H_

#define EXT_GS_WAKUP_GPIO  18

static void gsensor_wakeup(struct work_struct *);
static irqreturn_t gsensor_irqhandler(int , void *);

static int init_gsensor_wakeup_irq(void);
static int config_gsensor_gpio(void);

static void __exit kb60_gsensor_wakeup_exit(void);
static int __init kb60_gsensor_wakeup_init(void);
#endif /* GSENSOR_H_ */
