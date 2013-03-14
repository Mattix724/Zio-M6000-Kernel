/* Copyright (c) 2008-2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/workqueue.h>
#include <linux/diagchar.h>
#include <mach/usbdiag.h>
#include <mach/msm_smd.h>
#include "diagmem.h"
#include "diagchar.h"
#include "diagfwd.h"
#include "diagchar_hdlc.h"

#ifdef CONFIG_SLATE_TEST
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/semaphore.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/vmalloc.h> //[Bug 249]Using dynamic memory allocation for Slate screen capture test.
#include <linux/input.h>
extern struct semaphore msm_fb_ioctl_ppp_sem;
#endif


MODULE_DESCRIPTION("Diag Char Driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");

int diag_debug_buf_idx;
unsigned char diag_debug_buf[1024];

//#ifdef CONFIG_SLATE_TEST
//extern void send_down_event(int x, int y);
//extern void send_up_event(int x, int y);
//extern void send_move_event(int x, int y);
//#endif
/* Number of maximum USB requests that the USB layer should handle at
   one time. */
#define MAX_DIAG_USB_REQUESTS 12
static unsigned int buf_tbl_size = 8; /*Number of entries in table of buffers */

#define CHK_OVERFLOW(bufStart, start, end, length) \
((bufStart <= start) && (end - start >= length)) ? 1 : 0

#ifdef CONFIG_SLATE_TEST
struct input_dev *ts_input_dev;

void send_down_event(int x, int y)
{
	input_report_abs(ts_input_dev, ABS_X, x);
	input_report_abs(ts_input_dev, ABS_Y, y);
	input_report_key(ts_input_dev, BTN_TOUCH, !!255);
	input_sync(ts_input_dev);
}

void send_up_event(int x, int y)
{
	input_report_abs(ts_input_dev, ABS_X, x);
	input_report_abs(ts_input_dev, ABS_Y, y);
	input_report_key(ts_input_dev, BTN_TOUCH, !!0);
	input_sync(ts_input_dev);
}

void send_move_event(int x, int y)
{
	input_report_abs(ts_input_dev, ABS_X, x);
	input_report_abs(ts_input_dev, ABS_Y, y);
	input_report_key(ts_input_dev, BTN_TOUCH, !!255);
	input_sync(ts_input_dev);
}
#endif

void __diag_smd_send_req(int context)
{
	void *buf;

	if (driver->ch && (!driver->in_busy)) {
		int r = smd_read_avail(driver->ch);

	if (r > USB_MAX_IN_BUF) {
		if (r < MAX_BUF_SIZE) {
				printk(KERN_ALERT "\n diag: SMD sending in "
					   "packets upto %d bytes", r);
				driver->usb_buf_in = krealloc(
					driver->usb_buf_in, r, GFP_KERNEL);
		} else {
			printk(KERN_ALERT "\n diag: SMD sending in "
				 "packets more than %d bytes", MAX_BUF_SIZE);
			return;
		}
	}
		if (r > 0) {

			buf = driver->usb_buf_in;
			if (!buf) {
				printk(KERN_INFO "Out of diagmem for a9\n");
			} else {
				APPEND_DEBUG('i');
				if (context == SMD_CONTEXT)
					smd_read_from_cb(driver->ch, buf, r);
				else
					smd_read(driver->ch, buf, r);
				APPEND_DEBUG('j');
				driver->usb_write_ptr->length = r;
				driver->in_busy = 1;
				diag_device_write(buf, MODEM_DATA);
			}
		}
	}
}

int diag_device_write(void *buf, int proc_num)
{
	int i, err = 0;

	if (driver->logging_mode == USB_MODE) {
		if (proc_num == APPS_DATA) {
			driver->usb_write_ptr_svc = (struct diag_request *)
			(diagmem_alloc(driver, sizeof(struct diag_request),
				 POOL_TYPE_USB_STRUCT));
			driver->usb_write_ptr_svc->length = driver->used;
			driver->usb_write_ptr_svc->buf = buf;
			err = diag_write(driver->usb_write_ptr_svc);
		} else if (proc_num == MODEM_DATA) {
				driver->usb_write_ptr->buf = buf;
#ifdef DIAG_DEBUG
				printk(KERN_INFO "writing data to USB,"
						 " pkt length %d \n",
				       driver->usb_write_ptr->length);
				print_hex_dump(KERN_DEBUG, "Written Packet Data"
					       " to USB: ", 16, 1, DUMP_PREFIX_
					       ADDRESS, buf, driver->
					       usb_write_ptr->length, 1);
#endif
			err = diag_write(driver->usb_write_ptr);
		} else if (proc_num == QDSP_DATA) {
			driver->usb_write_ptr_qdsp->buf = buf;
			err = diag_write(driver->usb_write_ptr_qdsp);
		}
		APPEND_DEBUG('k');
	} else if (driver->logging_mode == MEMORY_DEVICE_MODE) {
		if (proc_num == APPS_DATA) {
			for (i = 0; i < driver->poolsize_usb_struct; i++)
				if (driver->buf_tbl[i].length == 0) {
					driver->buf_tbl[i].buf = buf;
					driver->buf_tbl[i].length =
								 driver->used;
#ifdef DIAG_DEBUG
					printk(KERN_INFO "\n ENQUEUE buf ptr"
						   " and length is %x , %d\n",
						   (unsigned int)(driver->buf_
				tbl[i].buf), driver->buf_tbl[i].length);
#endif
					break;
				}
		}
		for (i = 0; i < driver->num_clients; i++)
			if (driver->client_map[i] == driver->logging_process_id)
				break;
		if (i < driver->num_clients) {
			driver->data_ready[i] |= MEMORY_DEVICE_LOG_TYPE;
			wake_up_interruptible(&driver->wait_q);
		} else
			return -EINVAL;
	} else if (driver->logging_mode == NO_LOGGING_MODE) {
		if (proc_num == MODEM_DATA) {
			driver->in_busy = 0;
			queue_work(driver->diag_wq, &(driver->
							diag_read_smd_work));
		} else if (proc_num == QDSP_DATA) {
			driver->in_busy_qdsp = 0;
			queue_work(driver->diag_wq, &(driver->
						diag_read_smd_qdsp_work));
		}
		err = -1;
	}
    return err;
}

void __diag_smd_qdsp_send_req(int context)
{
	void *buf;

	if (driver->chqdsp && (!driver->in_busy_qdsp)) {
		int r = smd_read_avail(driver->chqdsp);

		if (r > USB_MAX_IN_BUF) {
			printk(KERN_INFO "diag dropped num bytes = %d\n", r);
			return;
		}
		if (r > 0) {
			buf = driver->usb_buf_in_qdsp;
			if (!buf) {
				printk(KERN_INFO "Out of diagmem for q6\n");
			} else {
				APPEND_DEBUG('l');
				if (context == SMD_CONTEXT)
					smd_read_from_cb(
						driver->chqdsp, buf, r);
				else
					smd_read(driver->chqdsp, buf, r);
				APPEND_DEBUG('m');
				driver->usb_write_ptr_qdsp->length = r;
				driver->in_busy_qdsp = 1;
				diag_device_write(buf, QDSP_DATA);
			}
		}

	}
}

static void diag_print_mask_table(void)
{
/* Enable this to print mask table when updated */
#ifdef MASK_DEBUG
	int first;
	int last;
	uint8_t *ptr = driver->msg_masks;
	int i = 0;

	while (*(uint32_t *)(ptr + 4)) {
		first = *(uint32_t *)ptr;
		ptr += 4;
		last = *(uint32_t *)ptr;
		ptr += 4;
		printk(KERN_INFO "SSID %d - %d\n", first, last);
		for (i = 0 ; i <= last - first ; i++)
			printk(KERN_INFO "MASK:%x\n", *((uint32_t *)ptr + i));
		ptr += ((last - first) + 1)*4;

	}
#endif
}

static void diag_update_msg_mask(int start, int end , uint8_t *buf)
{
	int found = 0;
	int first;
	int last;
	uint8_t *ptr = driver->msg_masks;
	uint8_t *ptr_buffer_start = &(*(driver->msg_masks));
	uint8_t *ptr_buffer_end = &(*(driver->msg_masks)) + MSG_MASK_SIZE;

	mutex_lock(&driver->diagchar_mutex);
	/* First SSID can be zero : So check that last is non-zero */

	while (*(uint32_t *)(ptr + 4)) {
		first = *(uint32_t *)ptr;
		ptr += 4;
		last = *(uint32_t *)ptr;
		ptr += 4;
		if (start >= first && start <= last) {
			ptr += (start - first)*4;
			if (end <= last)
				if (CHK_OVERFLOW(ptr_buffer_start, ptr,
						  ptr_buffer_end,
						  (((end - start)+1)*4)))
					memcpy(ptr, buf , ((end - start)+1)*4);
				else
					printk(KERN_CRIT "Not enough"
							 " buffer space for"
							 " MSG_MASK\n");
			else
				printk(KERN_INFO "Unable to copy"
						 " mask change\n");

			found = 1;
			break;
		} else {
			ptr += ((last - first) + 1)*4;
		}
	}
	/* Entry was not found - add new table */
	if (!found) {
		if (CHK_OVERFLOW(ptr_buffer_start, ptr, ptr_buffer_end,
				  8 + ((end - start) + 1)*4)) {
			memcpy(ptr, &(start) , 4);
			ptr += 4;
			memcpy(ptr, &(end), 4);
			ptr += 4;
			memcpy(ptr, buf , ((end - start) + 1)*4);
		} else
			printk(KERN_CRIT " Not enough buffer"
					 " space for MSG_MASK\n");
	}
	mutex_unlock(&driver->diagchar_mutex);
	diag_print_mask_table();

}

static void diag_update_event_mask(uint8_t *buf, int toggle, int num_bits)
{
	uint8_t *ptr = driver->event_masks;
	uint8_t *temp = buf + 2;

	mutex_lock(&driver->diagchar_mutex);
	if (!toggle)
		memset(ptr, 0 , EVENT_MASK_SIZE);
	else
		if (CHK_OVERFLOW(ptr, ptr,
				 ptr+EVENT_MASK_SIZE,
				  num_bits/8 + 1))
			memcpy(ptr, temp , num_bits/8 + 1);
		else
			printk(KERN_CRIT "Not enough buffer space "
					 "for EVENT_MASK\n");
	mutex_unlock(&driver->diagchar_mutex);
}

static void diag_update_log_mask(uint8_t *buf, int num_items)
{
	uint8_t *ptr = driver->log_masks;
	uint8_t *temp = buf;

	mutex_lock(&driver->diagchar_mutex);
	if (CHK_OVERFLOW(ptr, ptr, ptr + LOG_MASK_SIZE,
				  (num_items+7)/8))
		memcpy(ptr, temp , (num_items+7)/8);
	else
		printk(KERN_CRIT " Not enough buffer space for LOG_MASK\n");
	mutex_unlock(&driver->diagchar_mutex);
}

static void diag_update_pkt_buffer(unsigned char *buf)
{
	unsigned char *ptr = driver->pkt_buf;
	unsigned char *temp = buf;

	mutex_lock(&driver->diagchar_mutex);
	if (CHK_OVERFLOW(ptr, ptr, ptr + PKT_SIZE, driver->pkt_length))
		memcpy(ptr, temp , driver->pkt_length);
	else
		printk(KERN_CRIT " Not enough buffer space for PKT_RESP\n");
	mutex_unlock(&driver->diagchar_mutex);
}

void diag_update_userspace_clients(unsigned int type)
{
	int i;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i] != 0)
			driver->data_ready[i] |= type;
	wake_up_interruptible(&driver->wait_q);
	mutex_unlock(&driver->diagchar_mutex);
}

void diag_update_sleeping_process(int process_id)
{
	int i;

	mutex_lock(&driver->diagchar_mutex);
	for (i = 0; i < driver->num_clients; i++)
		if (driver->client_map[i] == process_id) {
			driver->data_ready[i] |= PKT_TYPE;
			break;
		}
	wake_up_interruptible(&driver->wait_q);
	mutex_unlock(&driver->diagchar_mutex);
}

#ifdef CONFIG_SLATE_TEST

#define BAD_CMD 0x13
#define BAD_PARA 0x14
#define BAD_LEN 0x15
#define PEN_CMD 0x4B
#define PEN_SUBCMD 0x10
#define PEN_SUBSYS 0x0010
#define PEN_DOWN 0x000F
#define PEN_MOVE 0x0010
#define PEN_UP 0x0011
#define SCR_CMD 0x22

struct pen_handle {
unsigned char cmd_code;
unsigned char subcmd_code;
unsigned short subsys_code;
unsigned short event_type;
unsigned short x_pos;
unsigned short y_pos;
unsigned short z_pos;
unsigned int reserve;
};

struct scrcap_handle_rsp {
char cmd_code;
char unused[106];
char data_flag;
char lcd_data[2048];
};

struct bmpformat {
	int fsize;
	int reverse;
	int bitmapoff;
	int bitmapinfo;
	int width;
	int height;
	short planes;
	short bpp;
	int comp;
	int size;
	int hres;
	int vres;
	int ucolor;
	int icolor;
};

//B [Bug 249]Using dynamic memory allocation for Slate screen capture test.
//static char IMGBUFBMP[480*800*2+54]={0};
//static char IMGBUF[480*800*2]={0};
static char* IMGBUFBMP = NULL;
static char *IMGBUF = NULL;
//E [Bug 249]Using dynamic memory allocation for Slate screen capture test.
struct timer_list diagtimer;
int cap_flag = 0;
int cmd_flag = 0;
int segment = 0;
int last = 0;
char buf_copy[2200];
char buf_hdl[2200];

void diag_timeout(unsigned long data)
{
	if(cmd_flag == 1) {
		cmd_flag = 0;
		cap_flag = 0;
//		printk("%s ...\n",__func__);
	}
}

int diag_write_data(char *buf, int len)
{
	struct diag_send_desc_type send = { NULL, NULL, DIAG_STATE_START, 0 };
	struct diag_hdlc_dest_type enc = { NULL, NULL, 0 };
	int payload_size = len;
	int ret,err,used;

	memset(buf_copy,0,2200);
	memset(buf_hdl,0,2200);
	memcpy(buf_copy, buf, payload_size);

	send.state = DIAG_STATE_START;
	send.pkt = buf_copy;
	send.last = (void *)(buf_copy + payload_size - 1);
	send.terminate = 1;

	enc.dest = buf_hdl;
	enc.dest_last = (void *)(buf_hdl + payload_size + 7);

	diag_hdlc_encode(&send, &enc);

	used = (uint32_t) enc.dest - (uint32_t) buf_hdl;

	memset(buf_copy,0,2200);

	driver->usb_write_ptr->buf = buf_hdl;
	driver->usb_write_ptr->length = used;
	err = diag_write(driver->usb_write_ptr);

	if (err) {
		/*Free the buffer right away if write failed */
		ret = -EIO;
		goto fail_free_hdlc;
	}
	return 0;

fail_free_hdlc:
	return ret;
}


int diag_pen_handle(char *buf, int len)
{
	struct pen_handle *pen_rsp;
	char *fail_pen_rsp;
	char *rspbuf;
	unsigned short x,y;

	pen_rsp = (struct pen_handle *)buf;
	if( buf[0] != PEN_CMD )
		return 0;
	fail_pen_rsp = kzalloc(len+1, GFP_KERNEL);

	if( len != sizeof(struct pen_handle) ) {
		return 0;
	}
	if( (buf[1] != PEN_SUBCMD) || (buf[2] != PEN_SUBSYS) ) {
		return 0;
	}

	x = pen_rsp->x_pos;
	y = pen_rsp->y_pos;
	if( (x > 480) || (y > 800) ) {
		return 0;
	}

	switch(pen_rsp->event_type)
	{
		case PEN_DOWN:
			send_down_event(x,y);
			break;
		case PEN_MOVE:
			send_move_event(x,y);
			break;
		case PEN_UP:
			send_up_event(x,y);
			break;
		default:
			return 0;
	}
#if 0
	printk("cmd_code = %x\n",pen_rsp->cmd_code);
	printk("subcmd_code = %x\n",pen_rsp->subcmd_code);
	printk("subsys_code = %x\n",pen_rsp->subsys_code);
	printk("event_type = %x\n",pen_rsp->event_type);
	printk("x_pos = %x\n",pen_rsp->x_pos);
	printk("y_pos = %x\n",pen_rsp->y_pos);
	printk("z_pos = %x\n",pen_rsp->z_pos);
	printk("reserve = %x\n",pen_rsp->reserve);
#endif
	rspbuf = (char*)buf;
	diag_write_data(rspbuf, len);

	kfree(fail_pen_rsp);
	return 1;
}

int diag_scrcap_handle(char *buf, int len)
{
	struct scrcap_handle_rsp scrcap_rsp;
	char cmd_code = 0;
	unsigned short x,y,w,h;
	struct fb_info *info = registered_fb[0];
	u32 __iomem *src = NULL;
	unsigned char *imgbuf = NULL;
	unsigned long total_size;
	int i,j;
	unsigned long p = 0;
	int count = (info->var.xres) * (info->var.yres) * 2;
	char *bitmap = NULL;
	struct bmpformat *bmphdr = NULL;
	char *rspbuf = NULL;
	int ver = 0;	
//B [Bug 249]Using dynamic memory allocation for Slate screen capture test.
	char bhdr[54];
	//int k;
	unsigned short* srcpix;
	unsigned short* dstpix;
//E [Bug 249]Using dynamic memory allocation for Slate screen capture test.

	if( buf[0] != SCR_CMD )
		return 0;

	cmd_code = buf[0];
	if( len == 1 ) {
		x = 0;
		y = 0;
		w = 480;
		h = 800;
		ver = 0;
		goto exit;
	}

	if( len != 11 ) {
		printk("length error\n");
		return 0;
	}

	if( (buf[1] != 1) || (buf[2] != 0 ) ) {
		printk("para error\n");
		return 0;
	}

	x = *((unsigned short*)(buf+3));
	y = *((unsigned short*)(buf+5));
	w = *((unsigned short*)(buf+7));
	h = *((unsigned short*)(buf+9));

#if 0
	printk("x = %d\n",x);
	printk("y = %d\n",y);
	printk("w = %d\n",w);
	printk("h = %d\n",h);
#endif

	if( (x > 480) || (y > 800) ) {
		printk("x,y para error\n");
		return 0;
	}

	if( ((x + w) > 480) || ((y + h) > 800) ) {
		printk("w,h para error\n");
		return 0;
	}
	if( (x==0) && (y==0) && (w==0) && (h==0) ) {
		w = 480;
		h = 800;
	}
	ver = 1;
exit:
	cmd_flag = 0;
	if(cap_flag == 0)
	{
		total_size = info->screen_size;
		if (total_size == 0)
			total_size = info->fix.smem_len;
		if (count >= total_size)
			count = total_size;
		if (count + p > total_size)
			count = total_size - p;
//B [Bug 249]Using dynamic memory allocation for Slate screen capture test.
		if (NULL == IMGBUF) {
			if ((IMGBUF = (char *)vmalloc(480*800*2)) == NULL) {
				printk("fail to allocate IMGBUF\r\n");
				return 0;
			}
		}
		if (NULL == IMGBUFBMP) {
			if ((IMGBUFBMP = (char *)vmalloc(480*800*2+54)) == NULL) {
				printk("fail to allocate IMGBUFBMP\r\n");
				return 0;
			}
		}
//E [Bug 249]Using dynamic memory allocation for Slate screen capture test.
		down(&msm_fb_ioctl_ppp_sem);
		src = (u32 __iomem *) (info->screen_base + p);
		imgbuf = (unsigned char *)src;
		memcpy(IMGBUF,imgbuf,count);
		up(&msm_fb_ioctl_ppp_sem);
		bitmap = (char*)(IMGBUFBMP+54);
		memcpy(bitmap, IMGBUF, count);

//B [Bug 249]Using dynamic memory allocation for Slate screen capture test.
		srcpix = (unsigned short*)(IMGBUF);
		dstpix = (unsigned short*)(IMGBUFBMP+54);

		bhdr[0] = 0x42;
		bhdr[1] = 0x4D;
		bmphdr = (struct bmpformat *)(bhdr+2);
		bmphdr->fsize = 54+(w)*(h)*2;
		bmphdr->reverse = 0x00;
		bmphdr->bitmapoff = 0x36;
		bmphdr->bitmapinfo = 0x28;
		bmphdr->width = (w);
		bmphdr->height = (h);//((~h)+1);
		bmphdr->planes = 0x01;
		bmphdr->bpp = 0x10;
		bmphdr->comp  = 0x00;
		bmphdr->size = (w)*(h)*2;
		bmphdr->hres = 0x00;
		bmphdr->vres = 0x00;
		bmphdr->ucolor = 0x00;
		bmphdr->icolor = 0x00;

		//k = 0;
		memset( IMGBUFBMP, 0, (480*800*2+54) );
		for(i = y ; i < (y+h) ; i++) {
			for(j = x ; j < (x+w) ; j++) {
				short r = ((*( srcpix+(j+(info->var.xres)*i) ))&0xF800)>>12;
				short g = ((*( srcpix+(j+(info->var.xres)*i) ))&0x07E0)>>6;
				short b = ((*( srcpix+(j+(info->var.xres)*i) ))&0x001F);
				*(dstpix+(i-y)*w+(j-x)) = (b)|(g<<6)|(r<<12);
//				*(dstpix+(i-y)*w+(x+w-1-j)) = (b)|(g<<6)|(r<<12);
//				*(dstpix+(y+h-1-i)*w+(j-x)) = (b)|(g<<6)|(r<<12);
//				short r = ((*( srcpix+(j+(info->var.xres)*i) ))&0xF800)>>11;
//				short g = ((*( srcpix+(j+(info->var.xres)*i) ))&0x07E0)>>6;
//				short b = ((*( srcpix+(j+(info->var.xres)*i) ))&0x001F);
//				*(dstpix+(y+h-1-i)*w+(j-x)) = (b)|(g<<5)|(r<<10);
//				k++;
			}
		}
		memcpy( IMGBUFBMP, bhdr, 54 );
		if (NULL != IMGBUF) {
			vfree(IMGBUF);
			IMGBUF = NULL;
		}
	}

	segment = ( w*h*2+54 )/2048;
	last = ( w*h*2+54 )%2048;
	if(last)
		segment++;
	cap_flag = segment-1;
	if(cap_flag < segment) {
		rspbuf = (char*)(&scrcap_rsp);
		memset(rspbuf, 0, sizeof(struct scrcap_handle_rsp));
		scrcap_rsp.cmd_code = cmd_code;
		if( cap_flag != (segment-1) ) {
			if(ver == 1)
			    scrcap_rsp.data_flag = 0x11;
			else
			    scrcap_rsp.data_flag = 0x01;
			memcpy( scrcap_rsp.lcd_data, IMGBUFBMP+cap_flag*2048 , 2048 );
		} else {
			if(ver == 1)
			    scrcap_rsp.data_flag = 0x10;
			else
			    scrcap_rsp.data_flag = 0x00;
			memcpy( scrcap_rsp.lcd_data, IMGBUFBMP+cap_flag*2048 , last );
		}
		cap_flag++;
		
		if(cap_flag == segment) {
			cap_flag = 0;
//			printk("cap_flag = %d  ->  ",cap_flag);
			diag_write_data( rspbuf, sizeof(struct scrcap_handle_rsp) );

			msleep(10);

			if (NULL != IMGBUFBMP) {
				vfree(IMGBUFBMP);
				IMGBUFBMP= NULL;
			}
		} else {
//			printk("cap_flag = %d  ->  ",cap_flag);
			diag_write_data( rspbuf, sizeof(struct scrcap_handle_rsp) );

			msleep(10);

		}
	}
//	printk("%s Done\n",__func__);
//E [Bug 249]Using dynamic memory allocation for Slate screen capture test.
	cmd_flag = 1;
	mod_timer(&diagtimer, jiffies + msecs_to_jiffies(200) );
	return 1;
}

#endif


static int diag_process_apps_pkt(unsigned char *buf, int len)
{
	uint16_t start;
	uint16_t end, subsys_cmd_code;
	int i, cmd_code, subsys_id;
	int packet_type = 1;
	unsigned char *temp = buf;
#ifdef CONFIG_SLATE_TEST
	if( diag_pen_handle(buf, len) != 0 )
	    return 0;

	if( diag_scrcap_handle(buf, len) != 0 )
	    return 0;
#endif

	/* event mask */
	if ((*buf == 0x60) && (*(++buf) == 0x0)) {
		diag_update_event_mask(buf, 0, 0);
		diag_update_userspace_clients(EVENT_MASKS_TYPE);
	}
	/* check for set event mask */
	else if (*buf == 0x82) {
		buf += 4;
		diag_update_event_mask(buf, 1, *(uint16_t *)buf);
		diag_update_userspace_clients(
		EVENT_MASKS_TYPE);
	}
	/* log mask */
	else if (*buf == 0x73) {
		buf += 4;
		if (*(int *)buf == 3) {
			buf += 8;
			diag_update_log_mask(buf+4, *(int *)buf);
			diag_update_userspace_clients(LOG_MASKS_TYPE);
		}
	}
	/* Check for set message mask  */
	else if ((*buf == 0x7d) && (*(++buf) == 0x4)) {
		buf++;
		start = *(uint16_t *)buf;
		buf += 2;
		end = *(uint16_t *)buf;
		buf += 4;
		diag_update_msg_mask((uint32_t)start, (uint32_t)end , buf);
		diag_update_userspace_clients(MSG_MASKS_TYPE);
	}
	/* Set all run-time masks
	if ((*buf == 0x7d) && (*(++buf) == 0x5)) {
		TO DO
	} */

	/* Check for registered clients and forward packet to user-space */
	else{
		cmd_code = (int)(*(char *)buf);
		temp++;
		subsys_id = (int)(*(char *)temp);
		temp++;
		subsys_cmd_code = *(uint16_t *)temp;
		temp += 2;

		for (i = 0; i < diag_max_registration; i++) {
			if (driver->table[i].process_id != 0) {
				if (driver->table[i].cmd_code ==
				     cmd_code && driver->table[i].subsys_id ==
				     subsys_id &&
				    driver->table[i].cmd_code_lo <=
				     subsys_cmd_code &&
					  driver->table[i].cmd_code_hi >=
				     subsys_cmd_code){
					driver->pkt_length = len;
					diag_update_pkt_buffer(buf);
					diag_update_sleeping_process(
						driver->table[i].process_id);
						return 0;
				    } /* end of if */
				else if (driver->table[i].cmd_code == 255
					  && cmd_code == 75) {
					if (driver->table[i].subsys_id ==
					    subsys_id &&
					   driver->table[i].cmd_code_lo <=
					    subsys_cmd_code &&
					     driver->table[i].cmd_code_hi >=
					    subsys_cmd_code){
						driver->pkt_length = len;
						diag_update_pkt_buffer(buf);
						diag_update_sleeping_process(
							driver->table[i].
							process_id);
						return 0;
					}
				} /* end of else-if */
				else if (driver->table[i].cmd_code == 255 &&
					  driver->table[i].subsys_id == 255) {
					if (driver->table[i].cmd_code_lo <=
							 cmd_code &&
						     driver->table[i].
						    cmd_code_hi >= cmd_code){
						driver->pkt_length = len;
						diag_update_pkt_buffer(buf);
						diag_update_sleeping_process
							(driver->table[i].
							 process_id);
						return 0;
					}
				} /* end of else-if */
			} /* if(driver->table[i].process_id != 0) */
		}  /* for (i = 0; i < diag_max_registration; i++) */
	} /* else */
		return packet_type;
}

void diag_process_hdlc(void *data, unsigned len)
{
	struct diag_hdlc_decode_type hdlc;
	int ret, type = 0;
#ifdef DIAG_DEBUG
	int i;
	printk(KERN_INFO "\n HDLC decode function, len of data  %d\n", len);
#endif
	hdlc.dest_ptr = driver->hdlc_buf;
	hdlc.dest_size = USB_MAX_OUT_BUF;
	hdlc.src_ptr = data;
	hdlc.src_size = len;
	hdlc.src_idx = 0;
	hdlc.dest_idx = 0;
	hdlc.escaping = 0;

	ret = diag_hdlc_decode(&hdlc);

	if (ret)
		type = diag_process_apps_pkt(driver->hdlc_buf,
							  hdlc.dest_idx - 3);
	else if (driver->debug_flag) {
		printk(KERN_ERR "Packet dropped due to bad HDLC coding/CRC"
				" errors or partial packet received, packet"
				" length = %d\n", len);
		print_hex_dump(KERN_DEBUG, "Dropped Packet Data: ", 16, 1,
					   DUMP_PREFIX_ADDRESS, data, len, 1);
		driver->debug_flag = 0;
	}
#ifdef DIAG_DEBUG
	printk(KERN_INFO "\n hdlc.dest_idx = %d \n", hdlc.dest_idx);
	for (i = 0; i < hdlc.dest_idx; i++)
		printk(KERN_DEBUG "\t%x", *(((unsigned char *)
							driver->hdlc_buf)+i));
#endif
	/* ignore 2 bytes for CRC, one for 7E and send */
	if ((driver->ch) && (ret) && (type) && (hdlc.dest_idx > 3)) {
		APPEND_DEBUG('g');
		smd_write(driver->ch, driver->hdlc_buf, hdlc.dest_idx - 3);
		APPEND_DEBUG('h');
#ifdef DIAG_DEBUG
		printk(KERN_INFO "writing data to SMD, pkt length %d \n", len);
		print_hex_dump(KERN_DEBUG, "Written Packet Data to SMD: ", 16,
			       1, DUMP_PREFIX_ADDRESS, data, len, 1);
#endif
	}

}

int diagfwd_connect(void)
{
	int err;

	printk(KERN_DEBUG "diag: USB connected\n");
	err = diag_open(driver->poolsize + 3); /* 2 for A9 ; 1 for q6*/
	if (err)
		printk(KERN_ERR "diag: USB port open failed");
	driver->usb_connected = 1;
	driver->in_busy = 0;
	driver->in_busy_qdsp = 0;

	/* Poll SMD channels to check for data*/
	queue_work(driver->diag_wq, &(driver->diag_read_smd_work));
	queue_work(driver->diag_wq, &(driver->diag_read_smd_qdsp_work));

	driver->usb_read_ptr->buf = driver->usb_buf_out;
	driver->usb_read_ptr->length = USB_MAX_OUT_BUF;
	APPEND_DEBUG('a');
	diag_read(driver->usb_read_ptr);
	APPEND_DEBUG('b');
	return 0;
}

int diagfwd_disconnect(void)
{
	printk(KERN_DEBUG "diag: USB disconnected\n");
	driver->usb_connected = 0;
	driver->in_busy = 1;
	driver->in_busy_qdsp = 1;
	driver->debug_flag = 1;
	diag_close();
	/* TBD - notify and flow control SMD */
	return 0;
}

int diagfwd_write_complete(struct diag_request *diag_write_ptr)
{
	unsigned char *buf = diag_write_ptr->buf;
	unsigned long flags = 0;
	/*Determine if the write complete is for data from arm9/apps/q6 */
	/* Need a context variable here instead */
	if (buf == (void *)driver->usb_buf_in) {
		driver->in_busy = 0;
		APPEND_DEBUG('o');
		spin_lock_irqsave(&diagchar_smd_lock, flags);
		__diag_smd_send_req(NON_SMD_CONTEXT);
		spin_unlock_irqrestore(&diagchar_smd_lock, flags);
	} else if (buf == (void *)driver->usb_buf_in_qdsp) {
		driver->in_busy_qdsp = 0;
		APPEND_DEBUG('p');
		spin_lock_irqsave(&diagchar_smd_qdsp_lock, flags);
		__diag_smd_qdsp_send_req(NON_SMD_CONTEXT);
		spin_unlock_irqrestore(&diagchar_smd_qdsp_lock, flags);
	} else {
#ifdef CONFIG_SLATE_TEST
		if( ((int)buf) == ((int)buf_hdl) ) {
			return 0;
		} else
#endif
		diagmem_free(driver, (unsigned char *)buf, POOL_TYPE_HDLC);
		diagmem_free(driver, (unsigned char *)diag_write_ptr,
							 POOL_TYPE_USB_STRUCT);
		APPEND_DEBUG('q');
	}
	return 0;
}

int diagfwd_read_complete(struct diag_request *diag_read_ptr)
{
	int len = diag_read_ptr->actual;

	APPEND_DEBUG('c');
#ifdef DIAG_DEBUG
	printk(KERN_INFO "read data from USB, pkt length %d \n",
		    diag_read_ptr->actual);
	print_hex_dump(KERN_DEBUG, "Read Packet Data from USB: ", 16, 1,
		       DUMP_PREFIX_ADDRESS, diag_read_ptr->buf,
		       diag_read_ptr->actual, 1);
#endif
	driver->read_len = len;
	if (driver->logging_mode == USB_MODE)
		queue_work(driver->diag_wq , &(driver->diag_read_work));
	return 0;
}

static struct diag_operations diagfwdops = {
	.diag_connect = diagfwd_connect,
	.diag_disconnect = diagfwd_disconnect,
	.diag_char_write_complete = diagfwd_write_complete,
	.diag_char_read_complete = diagfwd_read_complete
};

static void diag_smd_notify(void *ctxt, unsigned event)
{
	unsigned long flags = 0;
	spin_lock_irqsave(&diagchar_smd_lock, flags);
	__diag_smd_send_req(SMD_CONTEXT);
	spin_unlock_irqrestore(&diagchar_smd_lock, flags);
}

#if defined(CONFIG_MSM_N_WAY_SMD)
static void diag_smd_qdsp_notify(void *ctxt, unsigned event)
{
	unsigned long flags = 0;
	spin_lock_irqsave(&diagchar_smd_qdsp_lock, flags);
	__diag_smd_qdsp_send_req(SMD_CONTEXT);
	spin_unlock_irqrestore(&diagchar_smd_qdsp_lock, flags);
}
#endif

static int diag_smd_probe(struct platform_device *pdev)
{
	int r = 0;

	if (pdev->id == 0) {
		if (driver->usb_buf_in == NULL &&
			(driver->usb_buf_in =
			kzalloc(USB_MAX_IN_BUF, GFP_KERNEL)) == NULL)

			goto err;
		else

		r = smd_open("DIAG", &driver->ch, driver, diag_smd_notify);
	}
#if defined(CONFIG_MSM_N_WAY_SMD)
	if (pdev->id == 1) {
		if (driver->usb_buf_in_qdsp == NULL &&
			(driver->usb_buf_in_qdsp =
			kzalloc(USB_MAX_IN_BUF, GFP_KERNEL)) == NULL)

			goto err;
		else

		r = smd_named_open_on_edge("DIAG", SMD_APPS_QDSP,
			&driver->chqdsp, driver, diag_smd_qdsp_notify);

	}
#endif
	printk(KERN_INFO "diag opened SMD port ; r = %d\n", r);

err:
	return 0;
}

static struct platform_driver msm_smd_ch1_driver = {

	.probe = diag_smd_probe,
	.driver = {
		   .name = "DIAG",
		   .owner = THIS_MODULE,
		   },
};

void diag_read_work_fn(struct work_struct *work)
{
	APPEND_DEBUG('d');
	diag_process_hdlc(driver->usb_buf_out, driver->read_len);
	driver->usb_read_ptr->buf = driver->usb_buf_out;
	driver->usb_read_ptr->length = USB_MAX_OUT_BUF;
	APPEND_DEBUG('e');
	diag_read(driver->usb_read_ptr);
	APPEND_DEBUG('f');
}

void diagfwd_init(void)
{
	diag_debug_buf_idx = 0;
	spin_lock_init(&diagchar_smd_lock);
	spin_lock_init(&diagchar_smd_qdsp_lock);
	if (driver->usb_buf_out  == NULL &&
	     (driver->usb_buf_out = kzalloc(USB_MAX_OUT_BUF,
					 GFP_KERNEL)) == NULL)
		goto err;
	if (driver->hdlc_buf == NULL
	    && (driver->hdlc_buf = kzalloc(HDLC_MAX, GFP_KERNEL)) == NULL)
		goto err;
	if (driver->msg_masks == NULL
	    && (driver->msg_masks = kzalloc(MSG_MASK_SIZE,
					     GFP_KERNEL)) == NULL)
		goto err;
	if (driver->log_masks == NULL &&
	    (driver->log_masks = kzalloc(LOG_MASK_SIZE, GFP_KERNEL)) == NULL)
		goto err;
	if (driver->event_masks == NULL &&
	    (driver->event_masks = kzalloc(EVENT_MASK_SIZE,
					    GFP_KERNEL)) == NULL)
		goto err;
	if (driver->client_map == NULL &&
	    (driver->client_map = kzalloc
	     ((driver->num_clients) * 4, GFP_KERNEL)) == NULL)
		goto err;
	if (driver->buf_tbl == NULL)
			driver->buf_tbl = kzalloc(buf_tbl_size *
			  sizeof(struct diag_write_device), GFP_KERNEL);
	if (driver->buf_tbl == NULL)
		goto err;
	if (driver->data_ready == NULL &&
	     (driver->data_ready = kzalloc(driver->num_clients * 4,
					    GFP_KERNEL)) == NULL)
		goto err;
	if (driver->table == NULL &&
	     (driver->table = kzalloc(diag_max_registration*
				      sizeof(struct diag_master_table),
				       GFP_KERNEL)) == NULL)
		goto err;
	if (driver->usb_write_ptr == NULL)
			driver->usb_write_ptr = kzalloc(
				sizeof(struct diag_request), GFP_KERNEL);
			if (driver->usb_write_ptr == NULL)
					goto err;
	if (driver->usb_write_ptr_qdsp == NULL)
			driver->usb_write_ptr_qdsp = kzalloc(
				sizeof(struct diag_request), GFP_KERNEL);
			if (driver->usb_write_ptr_qdsp == NULL)
					goto err;
	if (driver->usb_read_ptr == NULL)
			driver->usb_read_ptr = kzalloc(
				sizeof(struct diag_request), GFP_KERNEL);
			if (driver->usb_read_ptr == NULL)
				goto err;
	if (driver->pkt_buf == NULL &&
	     (driver->pkt_buf = kzalloc(PKT_SIZE,
					 GFP_KERNEL)) == NULL)
		goto err;

	driver->diag_wq = create_singlethread_workqueue("diag_wq");
	INIT_WORK(&(driver->diag_read_work), diag_read_work_fn);

	diag_usb_register(&diagfwdops);

	platform_driver_register(&msm_smd_ch1_driver);

#ifdef CONFIG_SLATE_TEST
	init_timer(&diagtimer);
	diagtimer.function = diag_timeout;
#endif
	return;
err:
		printk(KERN_INFO "\n Could not initialize diag buffers\n");
		kfree(driver->usb_buf_out);
		kfree(driver->hdlc_buf);
		kfree(driver->msg_masks);
		kfree(driver->log_masks);
		kfree(driver->event_masks);
		kfree(driver->client_map);
		kfree(driver->buf_tbl);
		kfree(driver->data_ready);
		kfree(driver->table);
		kfree(driver->pkt_buf);
		kfree(driver->usb_write_ptr);
		kfree(driver->usb_write_ptr_qdsp);
		kfree(driver->usb_read_ptr);
}

void diagfwd_exit(void)
{
	smd_close(driver->ch);
	smd_close(driver->chqdsp);
	driver->ch = 0;		/*SMD can make this NULL */
	driver->chqdsp = 0;

	if (driver->usb_connected)
		diag_close();

	platform_driver_unregister(&msm_smd_ch1_driver);

	diag_usb_unregister();

	kfree(driver->usb_buf_in);
	kfree(driver->usb_buf_in_qdsp);
	kfree(driver->usb_buf_out);
	kfree(driver->hdlc_buf);
	kfree(driver->msg_masks);
	kfree(driver->log_masks);
	kfree(driver->event_masks);
	kfree(driver->client_map);
	kfree(driver->buf_tbl);
	kfree(driver->data_ready);
	kfree(driver->table);
	kfree(driver->pkt_buf);
	kfree(driver->usb_write_ptr);
	kfree(driver->usb_write_ptr_qdsp);
	kfree(driver->usb_read_ptr);
#ifdef CONFIG_SLATE_TEST
	del_timer_sync(&diagtimer);
#endif
}
