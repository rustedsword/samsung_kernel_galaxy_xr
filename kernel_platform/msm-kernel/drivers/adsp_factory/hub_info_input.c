/*
 * character driver for gathering hub information 
 *  Copyright (C) 2025, Samsung Electronics Co. Ltd. All Rights Reserved.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/kdev_t.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/slab.h>                 //kmalloc()
#include <linux/uaccess.h>              //copy_to/from_user()
#include <linux/kthread.h>
#include <linux/wait.h>                 //Required for the wait queues
#include <linux/poll.h>
#include <linux/sysfs.h> 
#include <linux/kobject.h>
#include <linux/err.h>

#define TIMESTAMP_HEAD_LEN  20 // ex)[20250416 01:24:49]  
#define HEADER_SSR_REASON_LEN  7 // 1,1,5,1,(size 2byte),7
#define SSR_REASON_LEN  128
#define REPORT_SSR_REASON_CMD  0x11
#define DEVICE_NAME "ssp_hub_info"
//Waitqueue
DECLARE_WAIT_QUEUE_HEAD(wait_queue_hub_info_data);
bool ready_to_read;

dev_t dev = 0;
static int hub_info_probe_done = 0;
static struct class *dev_class;
static struct cdev hub_info_cdev;
struct kobject *hub_info_kobj_ref;

static unsigned char report_buff[HEADER_SSR_REASON_LEN + SSR_REASON_LEN];

/*
** Function Prototypes
*/
static int      __init hub_info_driver_init(void);
static void     __exit hub_info_driver_exit(void);
 
/*************** Driver Fuctions **********************/
static int      hub_info_open(struct inode *inode, struct file *file);
static int      hub_info_release(struct inode *inode, struct file *file);
static ssize_t  hub_info_read(struct file *filp, char __user *buf, size_t len,loff_t * off);
static unsigned int hub_info_poll(struct file *filp, struct poll_table_struct *wait);

void adding_pkt_header(char* ssr_history, short size)
{
    ssr_history[0] = 1;
    ssr_history[1] = 1;
    ssr_history[2] = 5;
    ssr_history[3] = 1;
    memcpy(ssr_history + 4, &size, sizeof(short));
    ssr_history[6] = REPORT_SSR_REASON_CMD;
}

void report_hub_info(char* ssr_history)
{
    int history_len = strlen(ssr_history) - TIMESTAMP_HEAD_LEN;
    memset(report_buff, 0, sizeof(report_buff));
    strncpy(report_buff + HEADER_SSR_REASON_LEN, ssr_history + TIMESTAMP_HEAD_LEN, history_len);

    adding_pkt_header(report_buff, history_len + 1);
    pr_info("[report ssr_history] 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, %s\n", 
      report_buff[0], report_buff[1],report_buff[2],report_buff[3],report_buff[4],report_buff[5],report_buff[6],
      &report_buff[7]);
    if (hub_info_probe_done) {
      ready_to_read = true;
      wake_up_interruptible(&wait_queue_hub_info_data);
      pr_info("wake up [report ssr_history]");
    } else {
      pr_info("device probe yet[report ssr_history]");
    }
}

/*
** File operation sturcture
*/
static struct file_operations fops =
{
  .owner          = THIS_MODULE,
  .read           = hub_info_read,
  .open           = hub_info_open,
  .release        = hub_info_release,
  .poll           = hub_info_poll
};

/*
** This function will be called when we open the Device file
*/
static int hub_info_open(struct inode *inode, struct file *file)
{
  return 0;
}

/*
** This function will be called when we close the Device file
*/
static int hub_info_release(struct inode *inode, struct file *file)
{
  return 0;
}

/*
** This fuction will be called when we read the Device file
*/
static ssize_t hub_info_read(struct file *filp, char __user *buf, size_t len, loff_t *off)
{
  len = strlen(report_buff + HEADER_SSR_REASON_LEN) + HEADER_SSR_REASON_LEN;

   pr_info("[read ssr_history] 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, %s(%d)\n", 
  report_buff[0], report_buff[1],report_buff[2],report_buff[3],report_buff[4],report_buff[5],report_buff[6],
  &report_buff[7], len);
 
  if( copy_to_user(buf, report_buff, len) > 0)
  {
    pr_err("ERROR: Not all the bytes have been copied to user\n");
  }
  
  return len;
}

/*
** This fuction will be called when app calls the poll function
*/
//static unsigned int hub_info_poll(struct file *filp, struct poll_table_struct *wait)
static __poll_t hub_info_poll(struct file *filp, struct poll_table_struct *wait)
{
  __poll_t mask = 0;
  
  wait_event_interruptible(wait_queue_hub_info_data, ready_to_read); 
  pr_info("Poll function\n");
  
  mask |= ( POLLIN | POLLRDNORM );
  ready_to_read = false;
  return mask;
}
 
/*
** Module Init function
*/
static int __init hub_info_driver_init(void)
{
  /*Allocating Major number*/
  if((alloc_chrdev_region(&dev, 0, 1, DEVICE_NAME)) <0)
  {
    pr_err("Cannot allocate major number\n");
    return -1;
  }
  
  /*Creating cdev structure*/
  cdev_init(&hub_info_cdev,&fops);
  hub_info_cdev.owner = THIS_MODULE;
  hub_info_cdev.ops = &fops;
  
  /*Adding character device to the system*/
  if((cdev_add(&hub_info_cdev,dev,1)) < 0)
  {
    pr_err("Cannot add the device to the system\n");
    goto r_class;
  }
 
  /*Creating struct class*/
  if(IS_ERR(dev_class = class_create(THIS_MODULE,"hub_info_class")))
  {
    pr_err("Cannot create the struct class\n");
    goto r_class;
  }
        
  /*Creating device*/
  if(IS_ERR(device_create(dev_class,NULL,dev,NULL,DEVICE_NAME)))
  {
    pr_err("Cannot create the hub_info Driver\n");
    goto r_device;
  }
  
  pr_info("hub_info Driver Insert...Done!!!\n");
  hub_info_probe_done = 1;
  return 0;

r_device:
  class_destroy(dev_class);
r_class:
  unregister_chrdev_region(dev,1);
  return -1;
}

/*
** Module exit function
*/ 
static void __exit hub_info_driver_exit(void)
{
  kobject_put(hub_info_kobj_ref); 
  device_destroy(dev_class,dev);
  class_destroy(dev_class);
  cdev_del(&hub_info_cdev);
  unregister_chrdev_region(dev, 1);
  pr_info("hub_info Driver Remove...Done!!!\n");
}
 
module_init(hub_info_driver_init);
module_exit(hub_info_driver_exit);
