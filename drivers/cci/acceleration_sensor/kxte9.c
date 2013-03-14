/*
 * KXTE9 accelerometer driver
 *
 * Copyright (C) 2009 Kionix, Inc.
 *     Written by Chris Hudson <chudson@kionix.com>
 * Copyright (c) 2010 Yamaha Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */
 /*
 *  KXTE9   accelerometer driver
 *  the Copyright (c) is claimed by Yamaha
 *  the location of code is .../kernel/drivers/cci/acceleration_sensor
 *  for the convience of mangement. 
 */
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <linux/rtc.h>
#include <linux/earlysuspend.h>

#define KXTE9_VERSION "1.1.0"
#define KXTE9_NAME    "kxte9"

/* for debugging */
#define DEBUG 0
#define DEBUG_DELAY 0
#define TRACE_FUNC() pr_debug(KXTE9_NAME ": <trace> %s()\n", __FUNCTION__)

/*
 * Default parameters
 */
#define KXTE9_DEFAULT_DELAY               100
#define KXTE9_MAX_DELAY                   2000

/*
 * Registers
 */
#define KXTE9_WHO_AM_I_REG                0x0f
#define KXTE9_WHO_AM_I                    0x00

#define KXTE9_TILT_POS_CUR_REG            0x10
#define KXTE9_TILT_POS_PRE_REG            0x11
#define KXTE9_TILT_POS_LE                 0x20
#define KXTE9_TILT_POS_RI                 0x10
#define KXTE9_TILT_POS_DO                 0x08
#define KXTE9_TILT_POS_UP                 0x04
#define KXTE9_TILT_POS_FD                 0x02
#define KXTE9_TILT_POS_FU                 0x01

#define KXTE9_ACC_REG                     0x12

#define KXTE9_SOFT_RESET_REG              0x1d
#define KXTE9_SOFT_RESET_MASK             0x80
#define KXTE9_SOFT_RESET_SHIFT            7

#define KXTE9_POWER_CONTROL_REG           0x1b
#define KXTE9_POWER_CONTROL_MASK          0x80
#define KXTE9_POWER_CONTROL_SHIFT         7

#define KXTE9_BANDWIDTH_REG               0x1b
#define KXTE9_BANDWIDTH_MASK              0x18
#define KXTE9_BANDWIDTH_SHIFT             3
#define KXTE9_BANDWIDTH_1HZ               0
#define KXTE9_BANDWIDTH_3HZ               1
#define KXTE9_BANDWIDTH_10HZ              2
#define KXTE9_BANDWIDTH_40HZ              3

#if DEBUG
#define T_RESP                            0x0c
#define WHO_AM_I                          0x0f
#define TILT_POS_CUR                      0x10
#define TILT_POS_PRE                      0x11
#define XOUT                              0x12
#define YOUT                              0x13
#define ZOUT                              0x14
#define INT_SRC_REG1                      0x16
#define INT_SRC_REG2                      0x17
#define STATUS_REG                        0x18
#define INT_REL                           0x1a
#define CTRL_REG1                         0x1b
#define CTRL_REG2                         0x1C
#define CTRL_REG3                         0x1D
#define INT_CTRL_REG1                     0x1E
#define INT_CTRL_REG2                     0x1F
#define TILT_TIMER                        0x28
#define WUF_TIMER                         0x29
#define B2S_TIMER                         0x2A
#define WUF_THRESH1                       0x5A
#define B2S_THRESH1                       0x5B
#endif /* DEBUG */

//HuaPu: for logging and debugging
#define mo 1
#define to 0
#define ko 0

//HuaPu: begin / 20101105 / BugID:970 [KB62][Froyo][G-sensor] acceleration_sensor, geomagnetic_sensor and orientation_sensor fixing.
#define earlySuspend 1
struct i2c_client *kxte9_client;
//yiyiing_song: begin / 20101116 / tk15524  remove unnecessary log
//static int readCount = 0;
//yiyiing_song : end
static bool suspendStatus = false; /*if in suspend status is true, else false*/
//HuaPu: end
/*
 * Acceleration measurement
 */
#define KXTE9_RESOLUTION                  1024

/* ABS axes parameter range [um/s^2] (for input event) */
#define GRAVITY_EARTH                     9806550
#define ABSMIN_2G                         (-GRAVITY_EARTH * 2)
#define ABSMAX_2G                         (GRAVITY_EARTH * 2)

// YiYiing: begin / 20101109 / fixing tk15127 [Maps]the display will flash when launching the "Compass mode" from the street view
// fixing tk 15118[Labyrinth Lite] two small astragals are flashing all the time at setting animation
//HuaPu: begin / 20101122 /BugID: 1041 [KB62][G-sensor]Rotation Problem
//the window size=4 is a sutiable choice of liding window
#define BUF_SIZE 4  //window size=4  sliding window
//HuaPu: end

int buf_x[BUF_SIZE]={0};
int buf_y[BUF_SIZE]={0};
int buf_z[BUF_SIZE]={0};

int sum_x=0;
int sum_y=0;
int sum_z=0;

int buf_index=0;
// YiYiing: end

struct acceleration {
    int x;
    int y;
    int z;
};

/*
 * Output data rate
 */
struct kxte9_odr {
        unsigned long delay;            /* min delay (msec) in the range of ODR */
        u8 odr;                         /* bandwidth register value */
};

static const struct kxte9_odr kxte9_odr_table[] = {
    {25,    KXTE9_BANDWIDTH_40HZ},
    {100,   KXTE9_BANDWIDTH_10HZ},
    {333,   KXTE9_BANDWIDTH_3HZ},
    {1000,  KXTE9_BANDWIDTH_1HZ},
};

/*
 * Transformation matrix for chip mounting position
 */
static const int kxte9_position_map[][3][3] = {
    {{ 1,  0,  0}, { 0,  1,  0}, { 0,  0,  1}}, /* top/upper-left */
    {{ 0,  1,  0}, {-1,  0,  0}, { 0,  0,  1}}, /* top/upper-right */
    {{-1,  0,  0}, { 0, -1,  0}, { 0,  0,  1}}, /* top/lower-right */
    {{ 0, -1,  0}, { 1,  0,  0}, { 0,  0,  1}}, /* top/lower-left */
    {{-1,  0,  0}, { 0,  1,  0}, { 0,  0, -1}}, /* bottom/upper-left */
    {{ 0, -1,  0}, {-1,  0,  0}, { 0,  0, -1}}, /* bottom/upper-right */
    {{ 1,  0,  0}, { 0, -1,  0}, { 0,  0, -1}}, /* bottom/lower-right */
    {{ 0,  1,  0}, { 1,  0,  0}, { 0,  0, -1}}, /* bottom/lower-left */
};

/*
 * driver private data
 */
struct kxte9_data {
    atomic_t enable;                /* attribute value */
    atomic_t delay;                 /* attribute value */
    atomic_t position;              /* attribute value */
    struct acceleration last;       /* last measured data */
    struct mutex enable_mutex;
    struct mutex data_mutex;
    struct i2c_client *client;
    struct input_dev *input;
    struct delayed_work work;
//HuaPu: begin / 20101105 / BugID:970 [KB62][Froyo][G-sensor] acceleration_sensor, geomagnetic_sensor and orientation_sensor fixing.
#if earlySuspend
struct early_suspend early_suspend;
#endif
//HuaPu: end
#if DEBUG
    int suspend;
#endif
};

#define delay_to_jiffies(d) ((d)?msecs_to_jiffies(d):1)
#define actual_delay(d)     (jiffies_to_msecs(delay_to_jiffies(d)))

/* register access functions */
#define kxte9_read_bits(p,r) \
    ((i2c_smbus_read_byte_data((p)->client, r##_REG) & r##_MASK) >> r##_SHIFT)
#define kxte9_update_bits(p,r,v) \
    i2c_smbus_write_byte_data((p)->client, r##_REG, \
                              ((i2c_smbus_read_byte_data((p)->client,r##_REG) & ~r##_MASK) | ((v) << r##_SHIFT)))

//HuaPu: begin / 20101105 / BugID:970 [KB62][Froyo][G-sensor] acceleration_sensor, geomagnetic_sensor and orientation_sensor fixing.
#if earlySuspend
static void kxte9_early_suspend(struct early_suspend *h);
static void kxte9_late_resume(struct early_suspend *h);
#endif
//HuaPu: end

/*
 * Device dependant operations
 */
static int kxte9_power_up(struct kxte9_data *kxte9)
{
    kxte9_update_bits(kxte9, KXTE9_POWER_CONTROL, 1);

    return 0;
}

static int kxte9_power_down(struct kxte9_data *kxte9)
{
    kxte9_update_bits(kxte9, KXTE9_POWER_CONTROL, 0);

    return 0;
}

static int kxte9_hw_init(struct kxte9_data *kxte9)
{
    kxte9_power_down(kxte9);

    /* software reset */
    kxte9_update_bits(kxte9, KXTE9_SOFT_RESET, 1);
    while (kxte9_read_bits(kxte9, KXTE9_SOFT_RESET)) {
        msleep(1);
    }

    return 0;
}

static int kxte9_get_enable(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct kxte9_data *kxte9 = i2c_get_clientdata(client);

    return atomic_read(&kxte9->enable);
}

static void kxte9_set_enable(struct device *dev, int enable)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct kxte9_data *kxte9 = i2c_get_clientdata(client);
    int delay = atomic_read(&kxte9->delay);
    mutex_lock(&kxte9->enable_mutex);
//HuaPu: begin / 20101105 / BugID:970 [KB62][Froyo][G-sensor] acceleration_sensor, geomagnetic_sensor and orientation_sensor fixing.
   if (enable && !suspendStatus) {                   /* enable if state will be changed */
//HuaPu: end
        if (!atomic_cmpxchg(&kxte9->enable, 0, 1)) {
	kxte9_power_up(kxte9);
	schedule_delayed_work(&kxte9->work, delay_to_jiffies(delay) + 1);
        }
    }else{                        /* disable if state will be changed */
	if (atomic_cmpxchg(&kxte9->enable, 1, 0)) {
	cancel_delayed_work_sync(&kxte9->work);
	kxte9_power_down(kxte9);
        }
    }
    atomic_set(&kxte9->enable, enable);

    mutex_unlock(&kxte9->enable_mutex);

}

static int kxte9_get_delay(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct kxte9_data *kxte9 = i2c_get_clientdata(client);

    return atomic_read(&kxte9->delay);
}

static void kxte9_set_delay(struct device *dev, int delay)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct kxte9_data *kxte9 = i2c_get_clientdata(client);
    u8 odr;
    int i;

    /* determine optimum ODR */
    for (i = 1; (i < ARRAY_SIZE(kxte9_odr_table)) &&
             (actual_delay(delay) >= kxte9_odr_table[i].delay); i++)
        ;
    odr = kxte9_odr_table[i-1].odr;
    atomic_set(&kxte9->delay, delay);

    mutex_lock(&kxte9->enable_mutex);

    if (kxte9_get_enable(dev)) {
        cancel_delayed_work_sync(&kxte9->work);
        kxte9_update_bits(kxte9, KXTE9_BANDWIDTH, odr);
        schedule_delayed_work(&kxte9->work, delay_to_jiffies(delay) + 1);
    } else {
        kxte9_power_up(kxte9);
        kxte9_update_bits(kxte9, KXTE9_BANDWIDTH, odr);
        kxte9_power_down(kxte9);
    }

    mutex_unlock(&kxte9->enable_mutex);
}

static int kxte9_get_position(struct device *dev)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct kxte9_data *kxte9 = i2c_get_clientdata(client);

    return atomic_read(&kxte9->position);
}

static void kxte9_set_position(struct device *dev, int position)
{
    struct i2c_client *client = to_i2c_client(dev);
    struct kxte9_data *kxte9 = i2c_get_clientdata(client);

    atomic_set(&kxte9->position, position);
}

static int kxte9_measure(struct kxte9_data *kxte9, struct acceleration *accel)
{
    struct i2c_client *client = kxte9->client;
    u8 buf[3];
    int raw[3], data[3];
    int pos = atomic_read(&kxte9->position);
    long long g;
    int i, j;
#if DEBUG_DELAY
    struct timespec t;
#endif

#if DEBUG_DELAY
    getnstimeofday(&t);
#endif

    /* read acceleration data */
    if (i2c_smbus_read_i2c_block_data(client, KXTE9_ACC_REG, 3, buf) != 3) {
        dev_err(&client->dev,
                "I2C block read error: addr=0x%02x, len=%d\n",
                KXTE9_ACC_REG, 3);
            raw[0] = 0;
            raw[1] = 0;
            raw[2] = 0;
    } else {
        raw[0] = ((int)(buf[0] >> 2) - 32) << 6;
        raw[1] = ((int)(buf[1] >> 2) - 32) << 6;
        raw[2] = ((int)(buf[2] >> 2) - 32) << 6;
    }

    /* for X, Y, Z axis */
    for (i = 0; i < 3; i++) {
        /* coordinate transformation */
        data[i] = 0;
        for (j = 0; j < 3; j++) {
            data[i] += raw[j] * kxte9_position_map[pos][i][j];
        }
        /* normalization */
        g = (long long)data[i] * GRAVITY_EARTH / KXTE9_RESOLUTION;
        data[i] = g;
    }

    dev_dbg(&client->dev, "raw(%5d,%5d,%5d) => norm(%8d,%8d,%8d)\n",
            raw[0], raw[1], raw[2], data[0], data[1], data[2]);

#if DEBUG_DELAY
    dev_info(&client->dev, "%ld.%lds:raw(%5d,%5d,%5d) => norm(%8d,%8d,%8d)\n", t.sec, t.tv_nsec,
             raw[0], raw[1], raw[2], data[0], data[1], data[2]);
#endif
// YiYiing: begin / 20101109 / fixing tk15127 [Maps]the display will flash when launching the "Compass mode" from the street view
// fixing tk 15118[Labyrinth Lite] two small astragals are flashing all the time at setting animation

// circular buffer
    sum_x-=buf_x[buf_index];
    buf_x[buf_index]=data[0];
    sum_x+=buf_x[buf_index];

    sum_y-=buf_y[buf_index];
    buf_y[buf_index]=data[1];
    sum_y+=buf_y[buf_index];

    sum_z-=buf_z[buf_index];
    buf_z[buf_index]=data[2];
    sum_z+=buf_z[buf_index];

    buf_index=(buf_index+1)%BUF_SIZE;

    accel->x=(sum_x/BUF_SIZE);
    accel->y=(sum_y/BUF_SIZE);
    accel->z=(sum_z/BUF_SIZE);

#if 0
    accel->x = data[0];
    accel->y = data[1];
    accel->z = data[2];
#endif

// YiYiing: end
    return 0;
}

static void kxte9_work_func(struct work_struct *work)
{
    struct kxte9_data *kxte9 = container_of((struct delayed_work *)work, struct kxte9_data, work);
    struct acceleration accel;
    unsigned long delay = delay_to_jiffies(atomic_read(&kxte9->delay));

    kxte9_measure(kxte9, &accel);

    input_report_abs(kxte9->input, ABS_X, accel.x);
    input_report_abs(kxte9->input, ABS_Y, accel.y);
    input_report_abs(kxte9->input, ABS_Z, accel.z);
    input_sync(kxte9->input);

    mutex_lock(&kxte9->data_mutex);
    kxte9->last = accel;
    mutex_unlock(&kxte9->data_mutex);

    schedule_delayed_work(&kxte9->work, delay);
//yiyiing_song: begin / 20101116 / tk15524  remove unnecessary log
#if 0
    //HuaPu: for print data
    if(readCount >= 100){  //print data every 20 sec.
	printk(KERN_INFO "HuaPu>>>kxte9_work_func>>>(x,y,z) = (%d, %d, %d)\n", accel.x, accel.y, accel.z);
	readCount = 0;
    }else{
	readCount++;
    }
#endif
//yiyiing_song : end
}

/*
 * Input device interface
 */
static int kxte9_input_init(struct kxte9_data *kxte9)
{
    struct input_dev *dev;
    int err;

    dev = input_allocate_device();
    if (!dev) {
        return -ENOMEM;
    }
    dev->name = "accelerometer";
    dev->id.bustype = BUS_I2C;

    input_set_capability(dev, EV_ABS, ABS_MISC);
    input_set_abs_params(dev, ABS_X, ABSMIN_2G, ABSMAX_2G, 0, 0);
    input_set_abs_params(dev, ABS_Y, ABSMIN_2G, ABSMAX_2G, 0, 0);
    input_set_abs_params(dev, ABS_Z, ABSMIN_2G, ABSMAX_2G, 0, 0);
    input_set_drvdata(dev, kxte9);

    err = input_register_device(dev);
    if (err < 0) {
        input_free_device(dev);
        return err;
    }
    kxte9->input = dev;

    return 0;
}

static void kxte9_input_fini(struct kxte9_data *kxte9)
{
    struct input_dev *dev = kxte9->input;

    input_unregister_device(dev);
    input_free_device(dev);
}

/*
 * sysfs device attributes
 */
static ssize_t kxte9_enable_show(struct device *dev,
                                 struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", kxte9_get_enable(dev));
}

static ssize_t kxte9_enable_store(struct device *dev,
                                  struct device_attribute *attr,
                                  const char *buf, size_t count)
{
    unsigned long enable = simple_strtoul(buf, NULL, 10);

    if ((enable == 0) || (enable == 1)) {
        kxte9_set_enable(dev, enable);
    }

    return count;
}

static ssize_t kxte9_delay_show(struct device *dev,
                                struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", kxte9_get_delay(dev));
}

static ssize_t kxte9_delay_store(struct device *dev,
                                 struct device_attribute *attr,
                                 const char *buf, size_t count)
{
    unsigned long delay = simple_strtoul(buf, NULL, 10);

    if (delay > KXTE9_MAX_DELAY) {
        delay = KXTE9_MAX_DELAY;
    }

    kxte9_set_delay(dev, delay);

   return count;
}

static ssize_t kxte9_position_show(struct device *dev,
                                   struct device_attribute *attr, char *buf)
{
    return sprintf(buf, "%d\n", kxte9_get_position(dev));
}

static ssize_t kxte9_position_store(struct device *dev,
                                    struct device_attribute *attr,
                                    const char *buf, size_t count)
{
    unsigned long position;

    position = simple_strtoul(buf, NULL,10);
    if ((position >= 0) && (position <= 7)) {
        kxte9_set_position(dev, position);
    }

    return count;
}

static ssize_t kxte9_wake_store(struct device *dev,
                                struct device_attribute *attr,
                                const char *buf, size_t count)
{
    struct input_dev *input = to_input_dev(dev);
    static atomic_t serial = ATOMIC_INIT(0);

    input_report_abs(input, ABS_MISC, atomic_inc_return(&serial));

    return count;
}

static ssize_t kxte9_data_show(struct device *dev,
                               struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct kxte9_data *kxte9 = input_get_drvdata(input);
    struct acceleration accel;

    mutex_lock(&kxte9->data_mutex);
    accel = kxte9->last;
    mutex_unlock(&kxte9->data_mutex);

    return sprintf(buf, "%d %d %d\n", accel.x, accel.y, accel.z);
}

#if DEBUG
static ssize_t kxte9_debug_reg_show(struct device *dev,
                                     struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct kxte9_data *kxte9 = input_get_drvdata(input);
    struct i2c_client *client = kxte9->client;
    ssize_t count = 0;
    u8 reg;

    reg = i2c_smbus_read_byte_data(client, T_RESP);
    count += sprintf(&buf[count], "%02x: %d\n", T_RESP, reg);
    reg = i2c_smbus_read_byte_data(client, WHO_AM_I);
    count += sprintf(&buf[count], "%02x: %d\n", WHO_AM_I, reg);
    reg = i2c_smbus_read_byte_data(client, TILT_POS_CUR);
    count += sprintf(&buf[count], "%02x: %d\n", TILT_POS_CUR, reg);
    reg = i2c_smbus_read_byte_data(client, TILT_POS_PRE);
    count += sprintf(&buf[count], "%02x: %d\n", TILT_POS_PRE, reg);
    reg = i2c_smbus_read_byte_data(client, XOUT);
    count += sprintf(&buf[count], "%02x: %d\n", XOUT, reg);
    reg = i2c_smbus_read_byte_data(client, YOUT);
    count += sprintf(&buf[count], "%02x: %d\n", YOUT, reg);
    reg = i2c_smbus_read_byte_data(client, ZOUT);
    count += sprintf(&buf[count], "%02x: %d\n", ZOUT, reg);
    reg = i2c_smbus_read_byte_data(client, INT_SRC_REG1);
    count += sprintf(&buf[count], "%02x: %d\n", INT_SRC_REG1, reg);
    reg = i2c_smbus_read_byte_data(client, INT_SRC_REG2);
    count += sprintf(&buf[count], "%02x: %d\n", INT_SRC_REG2, reg);
    reg = i2c_smbus_read_byte_data(client, STATUS_REG);
    count += sprintf(&buf[count], "%02x: %d\n", STATUS_REG, reg);
    reg = i2c_smbus_read_byte_data(client, INT_REL);
    count += sprintf(&buf[count], "%02x: %d\n", INT_REL, reg);
    reg = i2c_smbus_read_byte_data(client, CTRL_REG1);
    count += sprintf(&buf[count], "%02x: %d\n", CTRL_REG1, reg);
    reg = i2c_smbus_read_byte_data(client, CTRL_REG2);
    count += sprintf(&buf[count], "%02x: %d\n", CTRL_REG2, reg);
    reg = i2c_smbus_read_byte_data(client, CTRL_REG3);
    count += sprintf(&buf[count], "%02x: %d\n", CTRL_REG3, reg);
    reg = i2c_smbus_read_byte_data(client, INT_CTRL_REG1);
    count += sprintf(&buf[count], "%02x: %d\n", INT_CTRL_REG1, reg);
    reg = i2c_smbus_read_byte_data(client, INT_CTRL_REG2);
    count += sprintf(&buf[count], "%02x: %d\n", INT_CTRL_REG2, reg);
    reg = i2c_smbus_read_byte_data(client, TILT_TIMER);
    count += sprintf(&buf[count], "%02x: %d\n", TILT_TIMER, reg);
    reg = i2c_smbus_read_byte_data(client, WUF_TIMER);
    count += sprintf(&buf[count], "%02x: %d\n", WUF_TIMER, reg);
    reg = i2c_smbus_read_byte_data(client, B2S_TIMER);
    count += sprintf(&buf[count], "%02x: %d\n", B2S_TIMER, reg);
    reg = i2c_smbus_read_byte_data(client, WUF_THRESH1);
    count += sprintf(&buf[count], "%02x: %d\n", WUF_THRESH1, reg);
    reg = i2c_smbus_read_byte_data(client, B2S_THRESH1);
    count += sprintf(&buf[count], "%02x: %d\n", B2S_THRESH1, reg);

    return count;
}

static int kxte9_suspend(struct i2c_client *client, pm_message_t mesg);
static int kxte9_resume(struct i2c_client *client);

static ssize_t kxte9_debug_suspend_show(struct device *dev,
                                        struct device_attribute *attr, char *buf)
{
    struct input_dev *input = to_input_dev(dev);
    struct kxte9_data *kxte9 = input_get_drvdata(input);

    return sprintf(buf, "%d\n", kxte9->suspend);
}

static ssize_t kxte9_debug_suspend_store(struct device *dev,
                                         struct device_attribute *attr,
                                         const char *buf, size_t count)
{
    struct input_dev *input = to_input_dev(dev);
    struct kxte9_data *kxte9 = input_get_drvdata(input);
    struct i2c_client *client = kxte9->client;
    unsigned long suspend = simple_strtoul(buf, NULL, 10);

    if (suspend) {
        pm_message_t msg;
        kxte9_suspend(client, msg);
    } else {
        kxte9_resume(client);
    }

    return count;
}
#endif /* DEBUG */

static DEVICE_ATTR(enable, S_IRUGO|S_IWUSR|S_IWGRP,
                   kxte9_enable_show, kxte9_enable_store);
static DEVICE_ATTR(delay, S_IRUGO|S_IWUSR|S_IWGRP,
                   kxte9_delay_show, kxte9_delay_store);
static DEVICE_ATTR(position, S_IRUGO|S_IWUSR,
                   kxte9_position_show, kxte9_position_store);
static DEVICE_ATTR(wake, S_IWUSR|S_IWGRP, NULL, kxte9_wake_store);
static DEVICE_ATTR(data, S_IRUGO,
                   kxte9_data_show, NULL);

#if DEBUG
static DEVICE_ATTR(debug_reg, S_IRUGO,
                   kxte9_debug_reg_show, NULL);
static DEVICE_ATTR(debug_suspend, S_IRUGO|S_IWUSR,
                   kxte9_debug_suspend_show, kxte9_debug_suspend_store);
#endif /* DEBUG */

static struct attribute *kxte9_attributes[] = {
    &dev_attr_enable.attr,
    &dev_attr_delay.attr,
    &dev_attr_position.attr,
    &dev_attr_wake.attr,
    &dev_attr_data.attr,
#if DEBUG
    &dev_attr_debug_reg.attr,
    &dev_attr_debug_suspend.attr,
#endif /* DEBUG */
    NULL
};

static struct attribute_group kxte9_attribute_group = {
    .attrs = kxte9_attributes
};

/*
 * I2C client
 */
static int kxte9_detect(struct i2c_client *client, struct i2c_board_info *info)
{
    int id;

    id = i2c_smbus_read_byte_data(client, KXTE9_WHO_AM_I_REG);
    if (id != KXTE9_WHO_AM_I)
        return -ENODEV;

    return 0;
}


static int kxte9_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    struct kxte9_data *kxte9;
//HuaPu: begin / 20101105 / BugID:970 [KB62][Froyo][G-sensor] acceleration_sensor, geomagnetic_sensor and orientation_sensor fixing.
    kxte9_client = client;
//HuaPu: end

    int err;

    TRACE_FUNC();

    /* setup private data */
    kxte9 = kzalloc(sizeof(struct kxte9_data), GFP_KERNEL);
    if (!kxte9) {
        err = -ENOMEM;
        goto error_0;
    }

    mutex_init(&kxte9->enable_mutex);
    mutex_init(&kxte9->data_mutex);

    /* setup i2c client */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        err = -ENODEV;
        goto error_1;
    }

    i2c_set_clientdata(client, kxte9);
    kxte9->client = client;

    /* detect and init hardware */
    if ((err = kxte9_detect(client, NULL))) {
        goto error_1;
    }
    dev_info(&client->dev, "%s found\n", id->name);

    kxte9_hw_init(kxte9);
    kxte9_set_delay(&client->dev, KXTE9_DEFAULT_DELAY);
    kxte9_set_position(&client->dev, CONFIG_INPUT_ACCELERATION_SENSOR_POSITION);

//HuaPu: begin / 20101105 / BugID:970 [KB62][Froyo][G-sensor] acceleration_sensor, geomagnetic_sensor and orientation_sensor fixing.
#if earlySuspend
	kxte9->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING + 1;
	kxte9->early_suspend.suspend = kxte9_early_suspend;
	kxte9->early_suspend.resume = kxte9_late_resume;
	register_early_suspend(&kxte9->early_suspend);
#endif
//HuaPu: end

    /* setup driver interfaces */
    INIT_DELAYED_WORK(&kxte9->work, kxte9_work_func);

    err = kxte9_input_init(kxte9);
    if (err < 0) {
        goto error_1;
    }

    err = sysfs_create_group(&kxte9->input->dev.kobj, &kxte9_attribute_group);
    if (err < 0) {
        goto error_2;
    }

    return 0;

error_2:
    kxte9_input_fini(kxte9);
error_1:
    kfree(kxte9);
error_0:
    return err;
}

static int kxte9_remove(struct i2c_client *client)
{
    struct kxte9_data *kxte9 = i2c_get_clientdata(client);

    kxte9_set_enable(&client->dev, 0);

    sysfs_remove_group(&kxte9->input->dev.kobj, &kxte9_attribute_group);
    kxte9_input_fini(kxte9);
//HuaPu: begin / 20101105 / BugID:970 [KB62][Froyo][G-sensor] acceleration_sensor, geomagnetic_sensor and orientation_sensor fixing.
#if earlySuspend
	unregister_early_suspend(&kxte9->early_suspend);
#endif
//HuaPu: end

    kfree(kxte9);

    return 0;
}
//HuaPu: begin / 20101105 / BugID:970 [KB62][Froyo][G-sensor] acceleration_sensor, geomagnetic_sensor and orientation_sensor fixing.
#if earlySuspend
static void kxte9_early_suspend(struct early_suspend *h)
{
	struct kxte9_data *kxte9;
	struct i2c_client *client = kxte9_client;
	kxte9 = container_of(h, struct kxte9_data, early_suspend);
	suspendStatus = true;

	printk(KERN_INFO "HuaPu>>>kxte9_early_suspend\n");


	mutex_lock(&kxte9->enable_mutex);

	 if (kxte9_get_enable(&client->dev)) {
	cancel_delayed_work_sync(&kxte9->work);
	kxte9_power_down(kxte9);
	 }

	mutex_unlock(&kxte9->enable_mutex);

}
#endif

#if earlySuspend
static void kxte9_late_resume(struct early_suspend *h)
{
	struct kxte9_data *kxte9;
	struct i2c_client *client =  kxte9_client;;
	kxte9 = container_of(h, struct kxte9_data, early_suspend);
	int delay = atomic_read(&kxte9->delay);
	suspendStatus = false;


	printk(KERN_INFO "HuaPu>>>kxte9_late_resume\n");

	kxte9_hw_init(kxte9);
	kxte9_set_delay(&client->dev, delay);

	mutex_lock(&kxte9->enable_mutex);

	if (kxte9_get_enable(&client->dev)) {
	kxte9_power_up(kxte9);
	schedule_delayed_work(&kxte9->work, delay_to_jiffies(delay) + 1);
	}

	mutex_unlock(&kxte9->enable_mutex);
}
#endif
//HuaPu: end
static int kxte9_suspend(struct i2c_client *client, pm_message_t mesg)
{
    struct kxte9_data *kxte9 = i2c_get_clientdata(client);

    mutex_lock(&kxte9->enable_mutex);

    if (kxte9_get_enable(&client->dev)) {
        cancel_delayed_work_sync(&kxte9->work);
        kxte9_power_down(kxte9);
    }

#if DEBUG
    kxte9->suspend = 1;
#endif

    mutex_unlock(&kxte9->enable_mutex);

    return 0;
}

static int kxte9_resume(struct i2c_client *client)
{
    struct kxte9_data *kxte9 = i2c_get_clientdata(client);
    int delay = atomic_read(&kxte9->delay);

    kxte9_hw_init(kxte9);
    kxte9_set_delay(&client->dev, delay);

    mutex_lock(&kxte9->enable_mutex);

    if (kxte9_get_enable(&client->dev)) {
        kxte9_power_up(kxte9);
        schedule_delayed_work(&kxte9->work, delay_to_jiffies(delay) + 1);
    }

#if DEBUG
    kxte9->suspend = 0;
#endif

    mutex_unlock(&kxte9->enable_mutex);

    return 0;
}

static const struct i2c_device_id kxte9_id[] = {
    {KXTE9_NAME, 0},
    {},
};

MODULE_DEVICE_TABLE(i2c, kxte9_id);

static struct i2c_driver kxte9_driver = {
    .driver = {
        .name = KXTE9_NAME,
        .owner = THIS_MODULE,
    },
    .probe = kxte9_probe,
    .remove = kxte9_remove,
    .suspend = kxte9_suspend,
    .resume = kxte9_resume,
    .id_table = kxte9_id,
};

static int __init kxte9_init(void)
{
    return i2c_add_driver(&kxte9_driver);
}

static void __exit kxte9_exit(void)
{
    i2c_del_driver(&kxte9_driver);
}

module_init(kxte9_init);
module_exit(kxte9_exit);

MODULE_DESCRIPTION("KXTE9 accelerometer driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(KXTE9_VERSION);
