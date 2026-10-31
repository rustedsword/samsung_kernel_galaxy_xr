// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/elf.h>
#include <linux/sched.h>
#include <linux/qtee_shmbridge.h>
#include <soc/qcom/smci_object.h>
#include <soc/qcom/smci_clientenv.h>
#include "smci_qseecomcompat.h"
#include "smci_qseecomcompatapploader.h"

#define QSEE_MAX		(512 * 1024)
#define QSEE_NAME_MAX		64
#define QSEE_DIST_NAME_MAX	256
#define APP_LOADER_UID		122

struct qsee_session {
	struct mutex lock;
	struct smci_object env;
	struct smci_object loader;
	struct smci_object app;
	u32 is64;
	unsigned char *in;
	unsigned char *out;
	size_t out_len;
	size_t rpos;
};

static void qsee_release_objects(struct qsee_session *s)
{
	if (!SMCI_OBJECT_IS_NULL(s->app))
		smci_object_release(s->app);
	if (!SMCI_OBJECT_IS_NULL(s->loader))
		smci_object_release(s->loader);
	if (!SMCI_OBJECT_IS_NULL(s->env))
		smci_object_release(s->env);
	s->app = s->loader = s->env = SMCI_OBJECT_NULL;
}

static int qsee_open(struct inode *ino, struct file *f)
{
	struct qsee_session *s = kzalloc(sizeof(*s), GFP_KERNEL);

	if (!s)
		return -ENOMEM;
	s->in = kvmalloc(QSEE_MAX + 8, GFP_KERNEL);
	s->out = kvmalloc(QSEE_MAX + 4, GFP_KERNEL);
	if (!s->in || !s->out) {
		kvfree(s->in);
		kvfree(s->out);
		kfree(s);
		return -ENOMEM;
	}
	s->env = s->loader = s->app = SMCI_OBJECT_NULL;
	mutex_init(&s->lock);
	f->private_data = s;
	return 0;
}

static int qsee_close(struct inode *ino, struct file *f)
{
	struct qsee_session *s = f->private_data;

	if (current->files)
		qsee_release_objects(s);
	kvfree(s->in);
	kvfree(s->out);
	kfree(s);
	return 0;
}

static int qsee_start(struct qsee_session *s, size_t len, size_t *reply_len)
{
	struct qtee_shm image_shm = { 0 };
	char name[QSEE_NAME_MAX];
	size_t name_len = len - 4;
	size_t image_size = 0;
	size_t dist_len = 0;
	char *image;
	int ret;

	if (!SMCI_OBJECT_IS_NULL(s->env) || len < 5 || name_len >= QSEE_NAME_MAX)
		return -EINVAL;
	memcpy(&s->is64, s->in, 4);
	memcpy(name, s->in + 4, name_len);
	name[name_len] = 0;

	ret = get_client_env_object(&s->env);
	if (!ret)
		ret = smci_clientenv_open(s->env, APP_LOADER_UID, &s->loader);
	if (!ret && smci_qseecomcompatapploader_lookupta(s->loader, name, name_len, &s->app) != 0) {
		image = firmware_request_from_smcinvoke(name, &image_size, &image_shm);
		if (!image)
			ret = -ENOENT;
		else {
			s->is64 = image[EI_CLASS] == ELFCLASS64;
			ret = smci_qseecomcompatapploader_loadfrombuffer(s->loader, image, image_size, name,
									 name_len, s->out + 4,
									 QSEE_DIST_NAME_MAX, &dist_len,
									 &s->app);
			qtee_shmbridge_free_shm(&image_shm);
		}
	}
	*reply_len = ret ? 0 : min_t(size_t, dist_len, QSEE_DIST_NAME_MAX);
	if (ret)
		qsee_release_objects(s);
	return ret;
}

static int qsee_command(struct qsee_session *s, size_t len, size_t *reply_len)
{
	size_t req_out = 0;
	size_t rsp_out = 0;
	u32 want;
	int ret;

	if (SMCI_OBJECT_IS_NULL(s->app) || len < 5)
		return -EINVAL;
	memcpy(&want, s->in, 4);
	if (want == 0 || want > QSEE_MAX)
		return -EINVAL;
	memset(s->out + 4, 0, want);
	ret = smci_qseecomcompat_sendrequest(s->app, s->in + 4, len - 4, s->out + 4, want,
					     s->in + 4, len - 4, &req_out, s->out + 4, want, &rsp_out,
					     NULL, 0, s->is64, SMCI_OBJECT_NULL, SMCI_OBJECT_NULL,
					     SMCI_OBJECT_NULL, SMCI_OBJECT_NULL);
	*reply_len = want;
	return ret;
}

static ssize_t qsee_write(struct file *f, const char __user *ubuf, size_t len, loff_t *off)
{
	struct qsee_session *s = f->private_data;
	size_t reply_len = 0;
	char op;
	int ret;

	if (len < 2 || len > QSEE_MAX + 8)
		return -EINVAL;
	if (get_user(op, ubuf))
		return -EFAULT;
	mutex_lock(&s->lock);
	if (copy_from_user(s->in, ubuf + 1, len - 1)) {
		mutex_unlock(&s->lock);
		return -EFAULT;
	}
	if (op == 'S')
		ret = qsee_start(s, len - 1, &reply_len);
	else if (op == 'C')
		ret = qsee_command(s, len - 1, &reply_len);
	else if (op == 'X')
		ret = (qsee_release_objects(s), 0);
	else
		ret = -EINVAL;
	memcpy(s->out, &ret, 4);
	s->out_len = 4 + reply_len;
	s->rpos = 0;
	mutex_unlock(&s->lock);
	return len;
}

static ssize_t qsee_read(struct file *f, char __user *ubuf, size_t len, loff_t *off)
{
	struct qsee_session *s = f->private_data;
	size_t n;

	mutex_lock(&s->lock);
	n = min(len, s->out_len - s->rpos);
	if (n && copy_to_user(ubuf, s->out + s->rpos, n)) {
		mutex_unlock(&s->lock);
		return -EFAULT;
	}
	s->rpos += n;
	mutex_unlock(&s->lock);
	return n;
}

static const struct file_operations qsee_fops = {
	.owner = THIS_MODULE,
	.open = qsee_open,
	.release = qsee_close,
	.read = qsee_read,
	.write = qsee_write,
	.llseek = noop_llseek,
};

static struct miscdevice qsee_dev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "qsee",
	.fops = &qsee_fops,
	.mode = 0600,
};

static int __init qsee_init(void)
{
	return misc_register(&qsee_dev);
}

static void __exit qsee_exit(void)
{
	misc_deregister(&qsee_dev);
}

module_init(qsee_init);
module_exit(qsee_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("/dev/qsee: load a QSEECom compat TA (long distinguished names too) and send it requests");
