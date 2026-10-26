#include "kxr_aphost.h"

/**  Begin for spi dfu feature **/

static uint32_t crc32_compute(uint8_t const *p_data, uint32_t size,
			      uint32_t const *p_crc);

static int spi_response_polling(kxr_aphost_t *aphost, uint8_t length, uint32_t timeout_10_ms)
{
	ss_spi_dfu_t *dfu = &aphost->dfu;
	uint32_t i = 0;
	int err = 0;
	for (i = 0; i < timeout_10_ms; i++) {
		mdelay(10);
		err = kxr_spi_xfer_sync_dfu(&aphost->xfer, NULL,
				  dfu->rxbuffer, length);
		if (err == 0 && dfu->rxbuffer[0] == 0x60)
			return 0;;
	}

	return -ETIMEDOUT;
}

static int send_select_command(kxr_aphost_t *aphost, uint8_t type)
{
	ss_spi_dfu_t *dfu = &aphost->dfu;
	int err, i;

	//Select Command
	dfu->payload[0] = 0x06;
	dfu->payload[1] = type;
	err = kxr_spi_xfer_sync_dfu(&aphost->xfer, dfu->payload, NULL, 2);
	err = spi_response_polling(aphost, 10, 100);
	pr_err("err = %d, Select command response = %x\n", err,
	       dfu->rxbuffer[0]);
	if (err < 0) {
		// return err;
	}

	for (i = 0; i < 10; i++) {
		pr_err("Select command rxbuffer[%d] = %x\n", i,
		       dfu->rxbuffer[i]);
	}

	return 0;
}

static int send_execute_command(kxr_aphost_t *aphost)
{
	//To-DO: check CRC
	//Execute command
	ss_spi_dfu_t *dfu = &aphost->dfu;
	int err;
	dfu->payload[0] = 0x04;
	err = kxr_spi_xfer_sync_dfu(&aphost->xfer, dfu->payload, NULL, 1);
	err = spi_response_polling(aphost, 3, 100);
	pr_err("err = %d, Execute Command response = %x\n", err,
	       dfu->rxbuffer[0]);
	if (err < 0) {
		// return err;
	}

	if (dfu->rxbuffer[0] != 0x60) {
		pr_err("execute error !!!\n");
	}

	return 0;
}

static uint32_t spi_convertTo32(uint8_t la, uint8_t lb, uint8_t lc, uint8_t ld)
{
	return (la << 24) + (lb << 16) + (lc << 8) + ld;
}

static int spi_send_object(kxr_aphost_t *aphost, uint8_t type, uint8_t *ddata, uint32_t dlength)
{
	ss_spi_dfu_t *dfu = &aphost->dfu;
	int err = 0;
	int i = 0, delay_cnt = 50;
	uint8_t *data = ddata;
	uint32_t length = dlength;
	uint32_t crc = crc32_compute(data, length, &dfu->firmware_crc_last);
	pr_err("data object crc = %x\n", crc);

	do {
		data = ddata;
		length = dlength;

		//Create Object
		dfu->payload[0] = 0x01;
		dfu->payload[1] = type; //Command or Data Object
		dfu->payload[2] = length;
		dfu->payload[3] = length >> 8;
		dfu->payload[4] = length >> 16;
		dfu->payload[5] = length >> 24;
		pr_err("create object type = %d\n", type);
		err = kxr_spi_xfer_sync_dfu(&aphost->xfer, dfu->payload, NULL, 6);
		err = spi_response_polling(aphost, 3, 100);
		pr_err("err = %d, create object response = %x\n", err,
		       dfu->rxbuffer[0]);
		if (err < 0) {
			// return err;
		}

		for (i = 0; i < 10; i++) {
			pr_err("Create Object rxbuffer[%d] = %x\n", i,
			       dfu->rxbuffer[i]);
		}
		while (length > 0) {
			dfu->payload[0] = 0x08; //Write object opcode
			delay_cnt = 100;
			while (delay_cnt--) {
				udelay(dfu->delay_us);
			}
			if (length >= MAX_ACTUAL_PAYLOAD	) {
				memcpy(&dfu->payload[1], data, MAX_ACTUAL_PAYLOAD	);
				err = kxr_spi_xfer_sync_dfu(&aphost->xfer,
						  dfu->payload, NULL,
						  MAX_ACTUAL_PAYLOAD	 + 1);
				pr_err("err = %d, write image response = %x\n",
				       err, dfu->rxbuffer[0]);
				length = length - MAX_ACTUAL_PAYLOAD	;
				data = data + MAX_ACTUAL_PAYLOAD	;
			} else {
				memcpy(&dfu->payload[1], data, length);
				err = kxr_spi_xfer_sync_dfu(&aphost->xfer,
						  dfu->payload, NULL, length + 1);
				pr_err("err = %d, write image response = %x\n",
				       err, dfu->rxbuffer[0]);
				length = 0;
			}
		}

		//Ask for CRC
		dfu->payload[0] = 0x03;
		err = kxr_spi_xfer_sync_dfu(&aphost->xfer, dfu->payload, NULL, 1);
		err = spi_response_polling(aphost, 11, 100);
		if (err < 0) {
			// return err;
		}

		for (i = 0; i < 11; i++) {
			pr_err("err = %d, ask for CRC response rxbuffer[%d] = %x\n",
			       err, i, dfu->rxbuffer[i]);
		}

		if (crc != spi_convertTo32(dfu->rxbuffer[10],
					   dfu->rxbuffer[9],
					   dfu->rxbuffer[8],
					   dfu->rxbuffer[7])) {
			pr_err("crc error !!!\n");
			// return -EINVAL;
		} else {
			if (type == 0x02) { //data mode
                          dfu->firmware_crc_last = crc;
			}
			pr_err("crc is matched!!!");
		}
		udelay(1000);
	} while (0);

	err = send_execute_command(aphost);
	if (err < 0) {
		pr_err("Failed to send_execute_command: %d\n", err);
		// sreturn err;
	}

	mdelay(100);

	return 0;
}

static uint32_t crc32_compute(uint8_t const *p_data, uint32_t size,
			      uint32_t const *p_crc)
{
	uint32_t crc, i, j;

	crc = (p_crc == NULL) ? 0xFFFFFFFF : ~(*p_crc);
	for (i = 0; i < size; i++) {
		crc = crc ^ p_data[i];
		for (j = 8; j > 0; j--) {
			crc = (crc >> 1) ^
			      (0xEDB88320U & ((crc & 1) ? 0xFFFFFFFF : 0));
		}
	}
	return ~crc;
}

static int dfu_probe(kxr_aphost_t *aphost)
{
	ss_spi_dfu_t *dfu = &aphost->dfu;
	int err = 0, i = 0;
	memset(dfu->txbuffer, 0, sizeof(dfu->txbuffer));
	memset(dfu->rxbuffer, 0, sizeof(dfu->rxbuffer));

	// PING
	dfu->txbuffer[0] = 0x09;
	dfu->txbuffer[1] = 0x01;
	dfu->txbuffer[2] = 0xc0;

	err = kxr_spi_xfer_sync_dfu(&aphost->xfer, dfu->txbuffer, NULL,
			  10);
	err = spi_response_polling(aphost, 10, 5);
	if (err < 0) {
		return err;
	}

	for (i = 0; i < 10; i++) {
		pr_err("ping rxbuffer[%d] = %x\n", i, dfu->rxbuffer[i]);
	}

	// PRN
	dfu->txbuffer[0] = 0x02;
	dfu->txbuffer[1] = 0x00;
	dfu->rxbuffer[2] = 0x00;

	err = kxr_spi_xfer_sync_dfu(&aphost->xfer, dfu->txbuffer, NULL,
			  10);
	err = spi_response_polling(aphost, 10, 5);
	if (err < 0) {
		return err;
	}

	pr_err("err = %d\n", err);
	for (i = 0; i < 10; i++) {
		pr_err("PRN rxbuffer[%d] = %x\n", i, dfu->rxbuffer[i]);
	}

	// MTU
	memset(dfu->txbuffer, 0, sizeof(dfu->txbuffer));
	memset(dfu->rxbuffer, 0, sizeof(dfu->rxbuffer));
	dfu->txbuffer[0] = 0x07;
	dfu->txbuffer[1] = 0xc0;

	err = kxr_spi_xfer_sync_dfu(&aphost->xfer, dfu->txbuffer, NULL,
			  10);
	err = spi_response_polling(aphost, 10, 5);
	if (err < 0) {
		return err;
	}

	pr_err("err = %d\n", err);
	for (i = 0; i < 10; i++) {
		pr_err("MTU rxbuffer[%d] = %x\n", i, dfu->rxbuffer[i]);
	}
	return 0;
}

static int start_dfu(kxr_aphost_t *aphost)
{
	ss_spi_dfu_t *dfu = &aphost->dfu;
	int err = 0;
	struct file *fp;
	loff_t pos;
	int len = 0;
	uint8_t *frame = NULL;
	const char *app_img_path = dfu->bin_path;
	const char *init_packet_path = dfu->dat_path;

	dfu->firmware_crc_last = 0;
	dfu->dfu_state = 0;

	pr_err("start_dfu app_img_path = %s, init_packet_path = %s\n", app_img_path,
	       init_packet_path);

	// mutex_lock(&aphost->js_mutex);

	// Read init packet file
	fp = filp_open(init_packet_path, O_RDONLY, 0644);
	if (IS_ERR(fp)) {
		pr_err("open init_packet file failed\n");
		// mutex_unlock(&aphost->js_mutex);
		err = -2;
		goto out_kxr_aphost_set_power;
	}

	kxr_spi_xfer_mode_set(&aphost->xfer, KXR_SPI_WORK_MODE_USER);
	kxr_aphost_power_mode_set(aphost, KXR_SPI_POWER_MODE_DFU);
	mdelay(200);

	err = dfu_probe(aphost);
	if (err < 0) {
		pr_err("Failed to dfu_probe: %d\n", err);
		goto out_filp_close;
	}

	pos = 0;
	len = kernel_read(fp, dfu->txbuffer, sizeof(dfu->txbuffer),
		       &pos);
	pr_err("read  init packet file len = %d, pos = %lld\n", len, pos);
	if (len <= 0) {
		// mutex_unlock(&aphost->js_mutex);
		err = -1;
		goto out_filp_close;
	}

	filp_close(fp, NULL);
	err = send_select_command(aphost, COMMAND_OBJECT);
	if (err < 0) {
		pr_err("Failed to send_select_command: %d\n", err);
		// goto out_kxr_aphost_set_power;
	}

	err = spi_send_object(aphost, COMMAND_OBJECT, dfu->txbuffer, len);
	if (err < 0) {
		pr_err("Failed to spi_send_object: %d\n", err);
		// goto out_kxr_aphost_set_power;
	}

	// Read firmware file
	fp = filp_open(app_img_path, O_RDONLY, 0644);
	if (IS_ERR(fp)) {
		pr_err("open firmware failed\n");
		// mutex_unlock(&aphost->js_mutex);
		err = -1;
		goto out_kxr_aphost_set_power;
	}

	frame = kzalloc(MAX_FRAME_SIZE, GFP_KERNEL);
	if (frame == NULL) {
		pr_err("Failed to kzalloc");
		err = -ENOMEM;
		goto out_filp_close;
	}

	pos = 0;

	err = send_select_command(aphost, DATA_OBJECT);
	if (err < 0) {
		pr_err("Failed to spi_send_object: %d\n", err);
		// goto out_kfree;
	}

	while (1) {
		memset(frame, 0, MAX_FRAME_SIZE);
		len = kernel_read(fp, frame, MAX_FRAME_SIZE, &pos);
		pr_err("read len = %d, pos = %lld\n", len, pos);
		if (len <= 0)
			break;

		err = spi_send_object(aphost, DATA_OBJECT, frame, len);
		if (err < 0) {
			pr_err("Failed to spi_send_object: %d\n", err);
			// goto out_kfree;
		}
	}

// out_kfree:
	kfree(frame);
out_filp_close:
	filp_close(fp, NULL);
out_kxr_aphost_set_power:
	kxr_aphost_power_mode_set(aphost, KXR_SPI_POWER_MODE_ON);
	kxr_spi_xfer_mode_set(&aphost->xfer, KXR_SPI_WORK_MODE_UART);

	return err;
}

static ssize_t jsdfu_show(struct device *dev, struct device_attribute *attr,
			  char *buf)
{
	kxr_aphost_t *aphost = kxr_aphost_get_drv_data(dev);
	return sprintf(buf, "%d\n", aphost->dfu.dfu_state);
}

static ssize_t jsdfu_store(struct device *dev, struct device_attribute *attr,
			   const char *buf, size_t count)
{
	kxr_aphost_t *aphost = kxr_aphost_get_drv_data(dev);
	ss_spi_dfu_t *dfu = &aphost->dfu;
	int err;

	err = sscanf(buf, "%s %s", dfu->bin_path, dfu->dat_path);
	if (err != 2) {
		pr_err("invalid param");
		return -EINVAL;
	}

	err = start_dfu(aphost);
	if (err < 0) {
		return err;
	}

	return count;
}

static ssize_t jsparam_show(struct device *dev, struct device_attribute *attr,
			    char *buf)
{
	kxr_aphost_t *aphost = kxr_aphost_get_drv_data(dev);
	return sprintf(buf, "%ld\n", aphost->dfu.delay_us);
}

static ssize_t jsparam_store(struct device *dev, struct device_attribute *attr,
			     const char *buf, size_t size)
{
	kxr_aphost_t *aphost = kxr_aphost_get_drv_data(dev);
	unsigned long delay_us = 0;

	if (sscanf(buf, "%ld", &delay_us) == 1) {
		printk("[%s]set delay_us: %ld\n", __func__, delay_us);
		aphost->dfu.delay_us = delay_us;
	}

	return size;
}

static DEVICE_ATTR(jsdfu, S_IRUGO | S_IWUSR | S_IWGRP, jsdfu_show, jsdfu_store);
static DEVICE_ATTR(jsparam, S_IRUGO | S_IWUSR | S_IWGRP, jsparam_show,
		   jsparam_store);

/** End for spi dfu feature **/

int js_open(struct inode *inode, struct file *filp)
{
	kxr_aphost_t *aphost = container_of(inode->i_cdev, kxr_aphost_t, dfu.js_cdev);
	filp->private_data = aphost;
	pr_err("enter js_open!\n");
	return 0;
}

ssize_t js_read(struct file *filp, char __user *buf, size_t count,
		loff_t *offset)
{
	kxr_aphost_t *aphost = (kxr_aphost_t *) filp->private_data;
	char response[100];
	int length;
	int remain;

	if (*offset > 0) {
		return 0;
	}

	pr_err("enter js_read to get version\n");

	length = kxr_spi_xchg_show_command(aphost, 0x81000000, response, sizeof(response));
	if (length < 0) {
		return length;
	}

	pr_err("fw version: %s\n", response);

	remain = copy_to_user(buf, response, length);
	if (remain > 0) {
		pr_err("copy_to_user: %d\n", remain);
	}

	length -= remain;
	*offset += length;

	return length;
}

ssize_t js_write(struct file *filp, const char __user *buf, size_t count,
		 loff_t *offset)
{
	kxr_aphost_t *aphost = (kxr_aphost_t *) filp->private_data;
	ss_spi_dfu_t *dfu = &aphost->dfu;

	memset(&dfu->info, 0, sizeof(struct js_spi_info));
	if (copy_from_user(&dfu->info, buf, sizeof(struct js_spi_info))) {
		pr_err("copy_from_user failed.\n");
		return -1;
	}

	pr_err("enter js_write! app_image_path: %s, init_packet_path: %s \n",
	       dfu->info.app_image_path, dfu->info.init_packet_path);

	strcpy(dfu->bin_path, dfu->info.app_image_path);
	strcpy(dfu->dat_path, dfu->info.init_packet_path);

	return 0;
}

long js_ioctl(struct file *filp, unsigned int cmd, unsigned long data)
{
	kxr_aphost_t *aphost = (kxr_aphost_t *) filp->private_data;
	return start_dfu(aphost);
}

int js_release(struct inode *inode, struct file *filp)
{
	pr_err("enter js_release!\n");
	return 0;
}

struct file_operations js_fops = {
	.owner = THIS_MODULE,
	.open = js_open,
	.read = js_read,
	.write = js_write,
	.unlocked_ioctl = js_ioctl,
	.release = js_release,
};

static int js_init_cdev(ss_spi_dfu_t *dfu)
{
	int ret = 0;
	do {
		ret = alloc_chrdev_region(&dfu->js_devno, 0, 1,
					  "js_spi_device");
		if (ret < 0) {
			pr_err("alloc_chrdev_region failed");
			break;
		}

		cdev_init(&dfu->js_cdev, &js_fops);
		ret = cdev_add(&dfu->js_cdev, dfu->js_devno, 1);

		if (ret < 0) {
			pr_err("cdev_add failed");
			break;
		}

		/*Creating struct class*/
		if ((dfu->js_class = class_create(
			     THIS_MODULE, "js_spi_class")) == NULL) {
			pr_err("Cannot create the struct class\n");
			break;
		}
		/*Creating device*/
		if ((device_create(dfu->js_class, NULL,
				   dfu->js_devno, NULL,
				   "js_spi_device")) == NULL) {
			pr_err("Cannot create the Device 1\n");
			class_destroy(dfu->js_class);
			break;
		}
		pr_err("Device Driver Insert...Done!!!\n");

	} while (0);

	if (ret < 0) {
		unregister_chrdev_region(dfu->js_devno, 1);
	}
	return ret;
}

static void js_release_cdev(ss_spi_dfu_t *dfu)
{
	cdev_del(&dfu->js_cdev);
	device_destroy(dfu->js_class, dfu->js_devno);
	class_destroy(dfu->js_class);
	unregister_chrdev_region(dfu->js_devno, 1);
}

int ss_spi_dfu_probe(kxr_aphost_t *aphost)
{
	struct device *dev = &aphost->xfer.spi->dev;
	ss_spi_dfu_t *dfu = &aphost->dfu;
	int ret;

	dfu->delay_us = 50;

	ret = js_init_cdev(dfu);
	if (ret < 0) {
		dev_err(dev, "Failed to js_init_cdev: %d\n", ret);
		return ret;
	}

	device_create_file(dev, &dev_attr_jsparam);
	device_create_file(dev, &dev_attr_jsdfu);

	return 0;
}

void ss_spi_dfu_remove(kxr_aphost_t *aphost)
{
	struct device *dev = &aphost->xfer.spi->dev;

	device_remove_file(dev, &dev_attr_jsparam);
	device_remove_file(dev, &dev_attr_jsdfu);
	js_release_cdev(&aphost->dfu);
}

#ifndef CONFIG_KXR_NO_VFS_IMPORT
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
#endif
