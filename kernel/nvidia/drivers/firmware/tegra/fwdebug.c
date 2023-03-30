/*
 * Copyright (c) 2015-2020, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/debugfs.h>
#include <linux/firmware.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <soc/tegra/bpmp_abi.h>
#include <linux/version.h>
#if KERNEL_VERSION(4, 15, 0) > LINUX_VERSION_CODE
#include <soc/tegra/chip-id.h>
#else
#include <soc/tegra/fuse.h>
#endif
#include <soc/tegra/tegra_bpmp.h>
#include "bpmp.h"

#define BPMP_MODULE_MAGIC	0x646f6d

struct seqbuf {
	char *buf;
	size_t pos;
	size_t size;
};

static struct device *device;
static DEFINE_MUTEX(lock);
static DEFINE_MUTEX(bpmp_debug_lock);

extern const struct of_device_id bpmp_of_matches[];

static void seqbuf_init(struct seqbuf *seqbuf, void *buf, size_t size)
{
	seqbuf->buf = buf;
	seqbuf->size = size;
	seqbuf->pos = 0;
}

static size_t seqbuf_avail(struct seqbuf *seqbuf)
{
	return seqbuf->pos < seqbuf->size ? seqbuf->size - seqbuf->pos : 0;
}

static size_t seqbuf_status(struct seqbuf *seqbuf)
{
	return seqbuf->pos <= seqbuf->size ? 0 : -EOVERFLOW;
}

static int seqbuf_eof(struct seqbuf *seqbuf)
{
	return seqbuf->pos >= seqbuf->size;
}

static int seqbuf_read(struct seqbuf *seqbuf, void *buf, size_t nbyte)
{
	nbyte = min(nbyte, seqbuf_avail(seqbuf));
	memcpy(buf, seqbuf->buf + seqbuf->pos,
			min(nbyte, seqbuf_avail(seqbuf)));
	seqbuf->pos += nbyte;
	return seqbuf_status(seqbuf);
}

static int seqbuf_read_u32(struct seqbuf *seqbuf, u32 *v)
{
	int err;
	err = seqbuf_read(seqbuf, v, 4);
	*v = le32_to_cpu(*v);
	return err;
}

static const char *seqbuf_strget(struct seqbuf *seqbuf)
{
	const char *ptr = seqbuf->buf + seqbuf->pos;
	seqbuf->pos += strnlen(seqbuf->buf + seqbuf->pos, seqbuf_avail(seqbuf));
	seqbuf->pos++;

	if (seqbuf_status(seqbuf))
		return NULL;
	else
		return ptr;
}

static int seqbuf_seek(struct seqbuf *seqbuf, ssize_t offset)
{
	seqbuf->pos += offset;
	return seqbuf_status(seqbuf);
}

static const char *root_path;

static const char *get_filename(const struct file *file, char *buf, int size)
{
	const char *filename;
	size_t root_len;

	if (!root_path)
		return NULL;

	root_len = strlen(root_path);

	filename = dentry_path(file->f_path.dentry, buf, size);
	if (IS_ERR_OR_NULL(filename))
		return NULL;

	if (strlen(filename) < root_len ||
			strncmp(filename, root_path, root_len))
		return NULL;

	filename += root_len;

	return filename;
}

static int bpmp_debug_open(const char *name, uint32_t *fd,
				uint32_t *len, bool write)
{
	int r = 0;
	char *dest;
	ssize_t sz_name;
	struct mrq_debug_request dbg_rq;
	struct mrq_debug_response dbg_re;

	/* File open */
	dbg_rq.cmd = (uint32_t)cpu_to_le32(write ? CMD_DEBUG_OPEN_WO :
						CMD_DEBUG_OPEN_RO);
	dest = dbg_rq.fop.name;
	sz_name = strscpy(dest, name, sizeof(dbg_rq.fop.name));
	if (sz_name < 0) {
		pr_err("File name too large: %s\n", name);
		r = -EINVAL;
		goto err;
	}
	r = tegra_bpmp_send_receive(MRQ_DEBUG, &dbg_rq, sizeof(dbg_rq),
					&dbg_re, sizeof(dbg_re));
	if (!r) {
		*len = dbg_re.fop.datalen;
		*fd = dbg_re.fop.fd;
	}
err:
	return r;
}

static int bpmp_debug_close(uint32_t fd)
{
	int r = 0;
	struct mrq_debug_request dbg_rq;
	struct mrq_debug_response dbg_re;

	dbg_rq.cmd = (uint32_t)cpu_to_le32(CMD_DEBUG_CLOSE);
	dbg_rq.frd.fd = fd;
	r = tegra_bpmp_send_receive(MRQ_DEBUG, &dbg_rq, sizeof(dbg_rq),
			&dbg_re, sizeof(dbg_re));
	return r;
}

static int bpmp_debug_read(const char *name, char *data,
				size_t sz_data, uint32_t *nbytes)
{
	int r = 0;
	uint32_t fd, len;
	struct mrq_debug_request dbg_rq;
	struct mrq_debug_response dbg_re;
	int remaining;
	char *dataptr = data;
	int cr;

	mutex_lock(&bpmp_debug_lock);
	/* File open */
	r = bpmp_debug_open(name, &fd, &len, 0);
	if (r)
		goto end;

	/* File read */
	if (len > sz_data) {
		r = -EFBIG;
		goto close_end;
	}
	dbg_rq.cmd = (uint32_t)cpu_to_le32(CMD_DEBUG_READ);
	dbg_rq.frd.fd = fd;
	remaining = len;
	while (remaining > 0) {
		r = tegra_bpmp_send_receive(MRQ_DEBUG, &dbg_rq, sizeof(dbg_rq),
			&dbg_re, sizeof(dbg_re));
		if (r)
			goto close_end;

		if (dbg_re.frd.readlen > remaining) {
			pr_err("%s: read data length invalid\n", __func__);
			r = -EINVAL;
			goto close_end;
		}
		memcpy(dataptr, dbg_re.frd.data, dbg_re.frd.readlen);
		dataptr += dbg_re.frd.readlen;
		remaining -= dbg_re.frd.readlen;
	}
	*nbytes = len;

close_end:
	/* File close */
	cr = bpmp_debug_close(fd);
	if (!r)
		r = cr;
end:
	mutex_unlock(&bpmp_debug_lock);
	return r;
}

static int bpmp_debug_write(const char *name, uint8_t *data,
				size_t sz_data)
{
	int r = 0;
	uint32_t fd, len;
	struct mrq_debug_request dbg_rq;
	struct mrq_debug_response dbg_re;
	char *dataptr;
	int remaining;
	int cr;

	mutex_lock(&bpmp_debug_lock);
	/* File open */
	r = bpmp_debug_open(name, &fd, &len, 1);
	if (r)
		goto err;

	/* File write */
	if (sz_data > len) {
		r = -ENOMEM;
		goto close_ret;
	}
	dbg_rq.fwr.datalen = sz_data;
	dbg_rq.cmd = (uint32_t)cpu_to_le32(CMD_DEBUG_WRITE);
	dbg_rq.fwr.fd = fd;
	remaining = sz_data;
	dataptr = data;
	while (remaining > 0) {
		len = min(remaining, (int)sizeof(dbg_rq.fwr.data));
		memcpy(dbg_rq.fwr.data, dataptr, len);
		dbg_rq.fwr.datalen = len;
		r = tegra_bpmp_send_receive(MRQ_DEBUG, &dbg_rq, sizeof(dbg_rq),
			&dbg_re, sizeof(dbg_re));
		if (r)
			goto close_ret;

		dataptr += dbg_rq.fwr.datalen;
		remaining -= dbg_rq.fwr.datalen;
	}

close_ret:
	/* File close */
	cr = bpmp_debug_close(fd);
	if (!r)
		r = cr;
err:
	mutex_unlock(&bpmp_debug_lock);
	return r;
}

static int bpmp_debug_show(struct seq_file *m, void *p)
{
	struct file *file = m->private;
	char *databuf = NULL;
	char namebuf[SZ_256];
	const char *filename;
	uint32_t nbytes = 0;
	size_t len;
	int ret;

	len = seq_get_buf(m, &databuf);
	if (!databuf)
		return -ENOMEM;

	filename = get_filename(file, namebuf, sizeof(namebuf));
	ret = bpmp_debug_read(filename, databuf, len, &nbytes);
	if (!ret)
		seq_commit(m, nbytes);

	return ret;
}

static ssize_t bpmp_debugfops_store(struct file *file, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	char *databuf = NULL;
	char namebuf[SZ_256];
	const char *filename;
	ssize_t ret;

	filename = get_filename(file, namebuf, sizeof(namebuf));
	if (!filename)
		return -EFAULT;

	if (!buf)
		return -ENOMEM;
	databuf = kmalloc(count, GFP_KERNEL);

	if (!databuf)
		return -ENOMEM;

	if (copy_from_user(databuf, buf, count)) {
		ret = -EFAULT;
		goto free_ret;
	}
	ret = bpmp_debug_write(filename, databuf, count);

free_ret:
	kfree(databuf);

	return ret ?: count;
}

static int bpmp_debugfops_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, bpmp_debug_show, file, SZ_256K);
}

static const struct file_operations bpmp_debug_fops = {
	.open		= bpmp_debugfops_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.write		= bpmp_debugfops_store,
	.release	= single_release,
};

static const char *get_full_path(struct dentry *parent, char *buf, uint32_t sz)
{
	const char *path;
	size_t root_len;

	root_len = strlen(root_path);
	path = dentry_path(parent, buf, sz - 1);
	if (IS_ERR_OR_NULL(path))
		return NULL;

	if (strlen(path) < root_len || strncmp(path, root_path, root_len))
		return NULL;
	if (strlen(path) == root_len)
		strlcat((char *)path, "/", sz - (path - buf));
	path += root_len;

	return path;
}

#define MAX_LS_SIZE	SZ_16K
#define MAX_FILE_PATH	SZ_256
static int bpmp_debug_create_dir(struct dentry *dir)
{
	struct seqbuf seq;
	char *buf;
	const char *str;
	int r = 0;
	uint32_t real_size;
	uint32_t attrs = 0;
	const char *path;
	struct dentry *dentry;
	umode_t mode;
	char *full_path;

	buf = kmalloc(MAX_LS_SIZE, GFP_KERNEL);
	if (buf == NULL)
		return -ENOMEM;

	full_path = kmalloc(MAX_FILE_PATH, GFP_KERNEL);
	if (!full_path) {
		kfree(buf);
		return -ENOMEM;
	}
	path = get_full_path(dir, full_path, MAX_FILE_PATH);
	if (!path) {
		r = -EINVAL;
		goto free_end;
	}
	r = bpmp_debug_read(path, buf, MAX_LS_SIZE, &real_size);
	if (r)
		goto free_end;

	seqbuf_init(&seq, buf, real_size);
	while (seqbuf_avail(&seq) > sizeof(u32)) {
		r = seqbuf_read_u32(&seq, &attrs);
		if (r)
			goto free_end;
		str = seqbuf_strget(&seq);
		if (str == NULL) {
			r = -EBADFD;
			goto free_end;
		}
		if (attrs & DEBUGFS_S_ISDIR) {
			dentry = debugfs_create_dir(str, dir);
			if (IS_ERR_OR_NULL(dentry)) {
				r = dentry ? PTR_ERR(dentry) : -ENOMEM;
				goto free_end;
			}
			bpmp_debug_create_dir(dentry);
		} else {
			mode = attrs & DEBUGFS_S_IRUSR ? S_IRUSR : 0;
			mode |= attrs & DEBUGFS_S_IWUSR ? S_IWUSR : 0;
			dentry = debugfs_create_file(str, mode,
					dir, NULL, &bpmp_debug_fops);
		}
	}
free_end:
	kfree(full_path);
	kfree(buf);

	return r;
}

static int bpmp_debugfs_read(uint32_t name, uint32_t sz_name,
		dma_addr_t data, size_t sz_data, uint32_t *nbytes)
{
	struct mrq_debugfs_request rq;
	struct mrq_debugfs_response re;
	int r;

	rq.cmd = (uint32_t)cpu_to_le32(CMD_DEBUGFS_READ);
	rq.fop.fnameaddr = (uint32_t)cpu_to_le32(name);
	rq.fop.fnamelen = (uint32_t)cpu_to_le32(sz_name);
	rq.fop.dataaddr = (uint32_t)cpu_to_le32(data);
	rq.fop.datalen = (uint32_t)cpu_to_le32(sz_data);

	r = tegra_bpmp_send_receive(MRQ_DEBUGFS, &rq, sizeof(rq),
			&re, sizeof(re));
	if (r)
		return r;

	*nbytes = re.fop.nbytes;

	return 0;
}

static int debugfs_show(struct seq_file *m, void *p)
{
	struct file *file = m->private;
	const size_t namesize = SZ_256;
	char *databuf = NULL;
	char *namebuf = NULL;
	dma_addr_t dataphys;
	dma_addr_t namephys;
	const char *filename;
	size_t off;
	uint32_t nbytes;
	int len;
	int ret;

	namebuf = tegra_bpmp_alloc_coherent(namesize, &namephys, GFP_KERNEL);
	if (!namebuf)
		return -ENOMEM;

	filename = get_filename(file, namebuf, namesize);
	if (!filename) {
		ret = -ENOENT;
		goto out;
	}

	databuf = tegra_bpmp_alloc_coherent(m->size, &dataphys, GFP_KERNEL);
	if (!databuf) {
		ret = -ENOMEM;
		goto out;
	}

	off = filename - namebuf;
	len = strlen(filename);

	ret = bpmp_debugfs_read(namephys + off, len, dataphys,
			m->size, &nbytes);

	if (!ret)
		seq_write(m, databuf, nbytes);

out:
	tegra_bpmp_free_coherent(namesize, namebuf, namephys);

	if (databuf)
		tegra_bpmp_free_coherent(m->size, databuf, dataphys);

	return ret;
}

static int debugfs_open(struct inode *inode, struct file *file)
{
	return single_open_size(file, debugfs_show, file, SZ_256K);
}

static int bpmp_debugfs_write(uint32_t name, size_t sz_name,
		uint32_t data, size_t sz_data)
{
	struct mrq_debugfs_request rq;

	rq.cmd = (uint32_t)cpu_to_le32(CMD_DEBUGFS_WRITE);
	rq.fop.fnameaddr = (uint32_t)cpu_to_le32(name);
	rq.fop.fnamelen = (uint32_t)cpu_to_le32(sz_name);
	rq.fop.dataaddr = (uint32_t)cpu_to_le32(data);
	rq.fop.datalen = (uint32_t)cpu_to_le32(sz_data);

	return tegra_bpmp_send_receive(MRQ_DEBUGFS, &rq, sizeof(rq),
			NULL, 0);
}

static ssize_t debugfs_store(struct file *file, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	const size_t namesize = SZ_256;
	char *databuf = NULL;
	char *namebuf = NULL;
	const char *filename;
	ssize_t ret;
	dma_addr_t phys_data;
	dma_addr_t phys_name;
	size_t off;
	int len;

	databuf = tegra_bpmp_alloc_coherent(count, &phys_data, GFP_KERNEL);
	if (!databuf)
		return -ENOMEM;

	namebuf = tegra_bpmp_alloc_coherent(namesize, &phys_name, GFP_KERNEL);
	if (!namebuf) {
		ret = -ENOMEM;
		goto out;
	}

	if (copy_from_user(databuf, buf, count)) {
		ret = -EFAULT;
		goto out;
	}

	filename = get_filename(file, namebuf, namesize);
	if (!filename) {
		ret = -EFAULT;
		goto out;
	}

	off = filename - namebuf;
	len = strlen(filename);

	ret = bpmp_debugfs_write(phys_name + off, len, phys_data, count);

out:
	tegra_bpmp_free_coherent(namesize, namebuf, phys_name);

	if (databuf)
		tegra_bpmp_free_coherent(count, databuf, phys_data);

	return ret ?: count;
}

static const struct file_operations debugfs_fops = {
	.open		= debugfs_open,
	.read		= seq_read,
	.llseek		= seq_lseek,
	.write		= debugfs_store,
	.release	= single_release,
};

static int bpmp_populate_dir(struct seqbuf *seqbuf, struct dentry *parent,
		u32 depth)
{
	int err;

	while (!seqbuf_eof(seqbuf)) {
		u32 d, t;
		const char *name;
		struct dentry *dentry;

		seqbuf_read_u32(seqbuf, &d);
		if (d < depth) {
			seqbuf_seek(seqbuf, -4);
			/* go up a level */
			return 0;
		}
		seqbuf_read_u32(seqbuf, &t);
		name = seqbuf_strget(seqbuf);

		if (seqbuf_status(seqbuf))
			return seqbuf_status(seqbuf);

		if (d != depth) {
			/* malformed data received from BPMP */
			return -EIO;
		}

		if (t & DEBUGFS_S_ISDIR) {
			dentry = debugfs_create_dir(name, parent);
			if (IS_ERR_OR_NULL(dentry))
				return dentry ? PTR_ERR(dentry) : -ENOMEM;
			err = bpmp_populate_dir(seqbuf, dentry, depth+1);
			if (err)
				return err;
		} else {
			umode_t mode;
			mode = t & DEBUGFS_S_IRUSR ? S_IRUSR : 0;
			mode |= t & DEBUGFS_S_IWUSR ? S_IWUSR : 0;
			dentry = debugfs_create_file(name, mode,
					parent, NULL,
					&debugfs_fops);
			if (IS_ERR_OR_NULL(dentry))
				return -ENOMEM;
		}
	}

	return 0;
}

static struct dentry *bpmp_debugfs_root;
static char root_path_buf[256];

static int bpmp_debug_create_root(struct dentry *root)
{
	int err = 0;

	bpmp_debugfs_root = debugfs_create_dir("debug", root);
	if (IS_ERR_OR_NULL(bpmp_debugfs_root)) {
		pr_err("failed to create bpmp debugfs directory\n");
		bpmp_debugfs_root = NULL;
		return -ENOMEM;
	}

	root_path = dentry_path_raw(bpmp_debugfs_root, root_path_buf,
			sizeof(root_path_buf));
	if (IS_ERR_OR_NULL(root_path)) {
		/* if this happens, then to recover bpmp debugfs needs
		 * to be unmounted from userspace */
		pr_err("failed to figure out bpmp root path\n");
		err = root_path ? PTR_ERR(root_path) : -ENOENT;
		root_path = NULL;
	}

	return err;
}

static int bpmp_debugfs_dumpdir(uint32_t addr, size_t size, uint32_t *nbytes)
{
	struct mrq_debugfs_request rq;
	struct mrq_debugfs_response re;
	int r;

	rq.cmd = (uint32_t)cpu_to_le32(CMD_DEBUGFS_DUMPDIR);
	rq.dumpdir.dataaddr = (uint32_t)cpu_to_le32(addr);
	rq.dumpdir.datalen = (uint32_t)cpu_to_le32(size);

	r = tegra_bpmp_send_receive(MRQ_DEBUGFS, &rq, sizeof(rq),
			&re, sizeof(re));
	if (r)
		return r;

	*nbytes = re.dumpdir.nbytes;

	return 0;
}

static void do_debugfs_unmount(struct work_struct *work)
{
	debugfs_remove_recursive(bpmp_debugfs_root);
	bpmp_debugfs_root = NULL;
}
static DECLARE_WORK(debugfs_unmount_work, do_debugfs_unmount);

static int bpmp_mrq_is_supported(int mrq)
{
	struct mrq_query_abi_request rq;
	struct mrq_query_abi_response re;
	int r;

	rq.mrq = mrq;

	r = tegra_bpmp_send_receive(MRQ_QUERY_ABI, &rq, sizeof(rq),
			&re, sizeof(re));

	/* something went wrong; assume not supported */
	if (WARN_ON(r))
		return 0;

	return re.status ? 0 : 1;
}

static int bpmp_debugfs_create_tree(struct dentry *root, bool inband_mrq)
{
	int ret;
	dma_addr_t phys;
	void *virt;
	const size_t sz = SZ_256K;
	uint32_t nbytes;
	struct seqbuf seqbuf;

	if (inband_mrq) {
		ret = bpmp_debug_create_root(root);
		if (!ret)
			ret = bpmp_debug_create_dir(bpmp_debugfs_root);
	} else {
		virt = tegra_bpmp_alloc_coherent(sz, &phys, GFP_KERNEL);
		if (!virt) {
			pr_err("%s: memory allocation failed\n", __func__);
			return -ENOMEM;
		}
		ret = bpmp_debugfs_dumpdir(phys, sz, &nbytes);
		if (!ret) {
			ret = bpmp_debug_create_root(root);
			if (!ret) {
				seqbuf_init(&seqbuf, virt, nbytes);
				ret = bpmp_populate_dir(&seqbuf,
							bpmp_debugfs_root, 0);
			}
		}
		tegra_bpmp_free_coherent(sz, virt, phys);
	}

	return ret;
}

static int bpmp_fwdebug_init(struct dentry *root)
{
	int ret;
	int mrq_debug_sup = 0;

	if (!root)
		return -EINVAL;

	mrq_debug_sup = bpmp_mrq_is_supported(MRQ_DEBUG);
	if (!(mrq_debug_sup || bpmp_mrq_is_supported(MRQ_DEBUGFS)))
		return 0;

	mutex_lock(&lock);

	if (bpmp_debugfs_root) {
		mutex_unlock(&lock);
		return -EINVAL;
	}

	ret = bpmp_debugfs_create_tree(root, mrq_debug_sup);
	if (ret) {
		pr_err("creation of BPMP-FW debugfs failed (%d)\n", ret);
	} else {
		cancel_work_sync(&debugfs_unmount_work);
		pr_info("bpmp: mounted debugfs mirror\n");
	}

	mutex_unlock(&lock);

	return ret;
}

static int bpmp_fwdebug_uninit(struct dentry *root)
{
	mutex_lock(&lock);

	if (!bpmp_debugfs_root) {
		mutex_unlock(&lock);
		return -EINVAL;
	}

	schedule_work(&debugfs_unmount_work);

	mutex_unlock(&lock);

	return 0;
}

static struct dentry *bpmp_root;
static struct dentry *module_root;
static LIST_HEAD(modules);
static DEFINE_MUTEX(bpmp_lock);

struct bpmp_module {
	struct list_head entry;
	struct work_struct unload_work;
	char name[MODULE_NAME_LEN];
	struct dentry *root;
	u32 handle;
	u32 size;
};

struct module_hdr {
	u32 magic;
	u32 size;
	u32 reloc_size;
	u32 bss_size;
	u32 init_offset;
	u32 cleanup_offset;
	u8 reserved[72];
	u8 parent_tag[32];
};

int bpmp_create_attrs(const struct fops_entry *fent,
		struct dentry *parent, void *data)
{
	struct dentry *d;

	while (fent->name) {
		d = debugfs_create_file(fent->name, fent->mode, parent, data,
				fent->fops);
		if (IS_ERR_OR_NULL(d))
			return -EFAULT;
		fent++;
	}

	return 0;
}

static struct bpmp_module *bpmp_find_module(const char *name)
{
	struct bpmp_module *m;

	list_for_each_entry(m, &modules, entry) {
		if (!strncmp(m->name, name, MODULE_NAME_LEN))
			return m;
	}

	return NULL;
}

static int bpmp_module_load(struct device *dev, const void *base, u32 size,
		u32 *handle)
{
	void *virt;
	dma_addr_t phys;
	struct { u32 phys; u32 size; } __packed msg;
	int r;

	virt = tegra_bpmp_alloc_coherent(size, &phys, GFP_KERNEL);
	if (virt == NULL)
		return -ENOMEM;

	memcpy(virt, base, size);

	msg.phys = phys;
	msg.size = size;

	r = tegra_bpmp_send_receive(MRQ_MODULE_LOAD, &msg, sizeof(msg),
			handle, sizeof(*handle));

	tegra_bpmp_free_coherent(size, virt, phys);
	return r;
}

static int bpmp_module_unload(struct device *dev, u32 handle)
{
	return tegra_bpmp_send_receive(MRQ_MODULE_UNLOAD,
			&handle, sizeof(handle), NULL, 0);
}

static void do_unload_module(struct work_struct *w)
{
	struct bpmp_module *m;
	int err;

	m = container_of(w, struct bpmp_module, unload_work);

	if (m->handle) {
		err = bpmp_module_unload(device, m->handle);
		if (err) {
			dev_err(device, "%s: failed to unload module (%d)\n",
				m->name, err);
			return;
		}
	}

	debugfs_remove_recursive(m->root);
	kfree(m);
}

static ssize_t bpmp_module_unload_store(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	struct bpmp_module *m;
	char buf[MODULE_NAME_LEN];
	char *name;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (strncpy_from_user(buf, user_buf, count) <= 0)
		return -EFAULT;

	buf[count] = 0;
	name = strim(buf);

	mutex_lock(&bpmp_lock);

	m = bpmp_find_module(name);
	if (!m) {
		mutex_unlock(&bpmp_lock);
		return -ENODEV;
	}

	list_del(&m->entry);
	schedule_work(&m->unload_work);
	mutex_unlock(&bpmp_lock);

	return count;
}

static const struct file_operations bpmp_module_unload_fops = {
	.write = bpmp_module_unload_store
};

static int bpmp_module_ready(const char *name, const struct firmware *fw,
		struct bpmp_module *m)
{
	struct module_hdr *hdr;
	const int sz = sizeof(firmware_tag);
	char fmt[sz + 1];
	int err;

	hdr = (struct module_hdr *)fw->data;

	if (fw->size < sizeof(struct module_hdr) ||
			hdr->magic != BPMP_MODULE_MAGIC ||
			hdr->size + hdr->reloc_size != fw->size) {
		dev_err(device, "%s: invalid module format\n", name);
		return -EINVAL;
	}

	if (memcmp(hdr->parent_tag, firmware_tag, sz)) {
		dev_err(device, "%s: bad module - tag mismatch\n", name);
		memcpy(fmt, firmware_tag, sz);
		fmt[sz] = 0;
		dev_err(device, "firmware: %s\n", fmt);
		memcpy(fmt, hdr->parent_tag, sz);
		fmt[sz] = 0;
		dev_err(device, "%s : %s\n", name, fmt);
		return -EINVAL;
	}

	m->size = hdr->size + hdr->bss_size;

	err = bpmp_module_load(device, fw->data, fw->size, &m->handle);
	if (err) {
		dev_err(device, "failed to load module, code=%d\n", err);
		return err;
	}

	if (!debugfs_create_x32("handle", S_IRUGO, m->root, &m->handle))
		return -ENOMEM;

	if (!debugfs_create_x32("size", S_IRUGO, m->root, &m->size))
		return -ENOMEM;

	list_add_tail(&m->entry, &modules);

	return 0;
}

static ssize_t bpmp_module_load_store(struct file *file,
		const char __user *user_buf, size_t count, loff_t *ppos)
{
	const struct firmware *fw;
	struct bpmp_module *m;
	char buf[MODULE_NAME_LEN];
	int r;

	if (count >= sizeof(buf))
		return -EINVAL;

	if (strncpy_from_user(buf, user_buf, count) <= 0)
		return -EFAULT;

	buf[count] = 0;

	m = kzalloc(sizeof(*m), GFP_KERNEL);
	if (m == NULL)
		return -ENOMEM;

	mutex_lock(&bpmp_lock);

	strncpy(m->name, strim(buf), sizeof(m->name) - 1);
	m->name[sizeof(m->name) - 1] = 0;

	INIT_WORK(&m->unload_work, do_unload_module);

	if (bpmp_find_module(m->name)) {
		dev_err(device, "module %s exist\n", m->name);
		r = -EEXIST;
		goto clean;
	}

	m->root = debugfs_create_dir(m->name, module_root);
	if (!m->root) {
		r = -ENOMEM;
		goto clean;
	}

	r = request_firmware(&fw, m->name, device);
	if (r) {
		dev_err(device, "request_firmware() failed: %d\n", r);
		goto clean;
	}

	if (!fw) {
		r = -EFAULT;
		WARN_ON(0);
		goto clean;
	}

	dev_info(device, "%s: module ready %zu@%p\n",
			m->name, fw->size, fw->data);
	r = bpmp_module_ready(m->name, fw, m);
	release_firmware(fw);

clean:
	mutex_unlock(&bpmp_lock);

	if (r) {
		schedule_work(&m->unload_work);
		return r;
	}

	return count;
}

static const struct file_operations bpmp_module_load_fops = {
	.write = bpmp_module_load_store
};

static int bpmp_init_modules(struct platform_device *pdev,
		struct dentry *parent)
{
	const struct fops_entry mod_attrs[] = {
		{ "load", &bpmp_module_load_fops, S_IWUSR },
		{ "unload", &bpmp_module_unload_fops, S_IWUSR },
		{ NULL, NULL, 0 }
	};

	module_root = debugfs_create_dir("module", parent);
	if (IS_ERR_OR_NULL(module_root))
		goto clean;

	if (bpmp_create_attrs(mod_attrs, module_root, pdev))
		goto clean;

	return 0;

clean:
	WARN_ON(1);
	debugfs_remove_recursive(module_root);
	module_root = NULL;
	return -EFAULT;
}

static int bpmp_ping_show(void *data, u64 *val)
{
	unsigned long flags;
	ktime_t tm;
	int ret;

	local_irq_save(flags);
	tm = ktime_get();
	ret = __bpmp_do_ping();
	tm = ktime_sub(ktime_get(), tm);
	local_irq_restore(flags);

	*val = ret ?: ktime_to_us(tm);
	return 0;
}

static int bpmp_modify_trace_mask(uint32_t clr, uint32_t set)
{
	uint32_t mb[] = { clr, set };
	uint32_t new;

	return tegra_bpmp_send_receive(MRQ_TRACE_MODIFY, mb, sizeof(mb),
			&new, sizeof(new)) ?: new;
}

static int bpmp_trace_enable_show(void *data, u64 *val)
{
	*val = bpmp_modify_trace_mask(0, 0);
	return 0;
}

static int bpmp_trace_enable_store(void *data, u64 val)
{
	int r;

	r = bpmp_modify_trace_mask(0, val);
	if (r < 0)
		return r;

	return 0;
}

static int bpmp_trace_disable_store(void *data, u64 val)
{
	int r;

	r = bpmp_modify_trace_mask(val, 0);
	if (r < 0)
		return r;

	return 0;
}

static int bpmp_mount_show(void *data, u64 *val)
{
	*val = bpmp_fwdebug_init(bpmp_root);
	return 0;
}

static int bpmp_unmount_show(void *data, u64 *val)
{
	*val = bpmp_fwdebug_uninit(bpmp_root);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(bpmp_ping_fops, bpmp_ping_show, NULL, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(trace_enable_fops, bpmp_trace_enable_show,
		bpmp_trace_enable_store, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(trace_disable_fops, NULL,
		bpmp_trace_disable_store, "0x%llx\n");
DEFINE_SIMPLE_ATTRIBUTE(bpmp_mount_fops, bpmp_mount_show, NULL, "%lld\n");
DEFINE_SIMPLE_ATTRIBUTE(bpmp_unmount_fops, bpmp_unmount_show, NULL, "%lld\n");

#ifdef CONFIG_BPMP_DEBUGFS_MOUNT_ON_BOOT
static __init int bpmp_init_mount(void)
{
	struct device_node *np = NULL;

	/* mirroring takes a while */
	if (!tegra_platform_is_silicon())
		return 0;

	/* continue with the init only if the bpmp node is active in the DTB */
	np = of_find_matching_node(NULL, bpmp_of_matches);
	if (!np || !of_device_is_available(np))
		return 0;

	return bpmp_fwdebug_init(bpmp_root);
}
late_initcall(bpmp_init_mount);
#endif

struct bpmp_trace_iter {
	dma_addr_t phys;
	void *virt;
	int eof;
};

static int bpmp_trace_show(struct seq_file *file, void *v)
{
	struct bpmp_trace_iter *i = file->private;
	uint32_t mb[] = { i->phys, PAGE_SIZE };
	int ret;

	i->eof = 0;
	ret = tegra_bpmp_send_receive(MRQ_WRITE_TRACE, mb, sizeof(mb),
			&i->eof, sizeof(i->eof));
	pr_debug("%s: ret %d eof %d\n", __func__, ret, i->eof);
	if (ret < 0)
		return ret;

	ret = seq_write(file, i->virt, ret);
	if (ret < 0)
		return ret;

	return 0;
}

static void *bpmp_trace_start(struct seq_file *file, loff_t *pos)
{
	struct bpmp_trace_iter *i = file->private;
	uint32_t cmd;
	int first;
	int ret;

	first = (*pos == 0);

	if (first && bpmp_mrq_is_supported(MRQ_TRACE_ITER)) {
		cmd = 0;
		ret = tegra_bpmp_send_receive(MRQ_TRACE_ITER,
				&cmd, sizeof(cmd), NULL, 0);
		if (WARN_ON(ret))
			return NULL;
	}

	pr_debug("%s: first %d eof %d pos %llu\n",
			__func__, first, i->eof, *pos);
	if (!first && i->eof == 1)
		return NULL;

	i->virt = tegra_bpmp_alloc_coherent(PAGE_SIZE, &i->phys, GFP_KERNEL);
	if (!i->virt)
		return NULL;

	return i;
}

static void *bpmp_trace_next(struct seq_file *file, void *v, loff_t *pos)
{
	struct bpmp_trace_iter *i = file->private;

	pr_debug("%s: eof %d pos %llu\n", __func__, i->eof, *pos);
	return NULL;
}

static void bpmp_trace_stop(struct seq_file *file, void *v)
{
	struct bpmp_trace_iter *i = file->private;

	pr_debug("%s: eof %d\n", __func__, i->eof);
	if (i->virt) {
		tegra_bpmp_free_coherent(PAGE_SIZE, i->virt, i->phys);
		i->virt = NULL;
	}
}

static const struct seq_operations bpmp_trace_seq_ops = {
	.start = bpmp_trace_start,
	.show = bpmp_trace_show,
	.next = bpmp_trace_next,
	.stop = bpmp_trace_stop
};

static ssize_t bpmp_trace_store(struct file *file, const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	uint32_t cmd = 1;
	int ret;

	if (!bpmp_mrq_is_supported(MRQ_TRACE_ITER))
		return count;

	ret = tegra_bpmp_send_receive(MRQ_TRACE_ITER,
			&cmd, sizeof(cmd), NULL, 0);
	return ret ?: count;
}

static int bpmp_trace_open(struct inode *inode, struct file *file)
{
	return seq_open_private(file, &bpmp_trace_seq_ops,
				sizeof(struct bpmp_trace_iter));
}

static const struct file_operations trace_fops = {
	.open = bpmp_trace_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = bpmp_trace_store,
	.release = seq_release_private,
};

static int bpmp_tag_show(struct seq_file *file, void *data)
{
	seq_write(file, firmware_tag, sizeof(firmware_tag));
	seq_putc(file, '\n');
	return 0;
}

static int bpmp_tag_open(struct inode *inode, struct file *file)
{
	return single_open(file, bpmp_tag_show, inode->i_private);
}

static const struct file_operations bpmp_tag_fops = {
	.open = bpmp_tag_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release
};

#define MSG_NR_FIELDS	((MSG_DATA_MIN_SZ + 3) / 4)
#define MSG_DATA_COUNT	(MSG_NR_FIELDS + 1)

static uint32_t inbox_data[MSG_DATA_COUNT];

static ssize_t bpmp_mrq_write(struct file *file, const char __user *user_buf,
		size_t count, loff_t *ppos)
{
	/* size in dec, space, new line, terminator */
	char buf[MSG_DATA_COUNT * 11 + 1 + 1];
	uint32_t outbox_data[MSG_DATA_COUNT];
	char *line;
	char *p;
	int i;
	int ret;

	memset(outbox_data, 0, sizeof(outbox_data));
	memset(inbox_data, 0, sizeof(inbox_data));

	count = min(count, sizeof(buf) - 1);
	if (copy_from_user(buf, user_buf, count)) {
		ret = -EFAULT;
		goto complete;
	}

	buf[count] = 0;
	line = strim(buf);


	for (i = 0; i < MSG_DATA_COUNT && line; i++) {
		p = strsep(&line, " ");
		ret = kstrtouint(p, 0, outbox_data + i);
		if (ret)
			break;
	}

	if (!i) {
		ret = -EINVAL;
		goto complete;
	}

	ret = tegra_bpmp_send_receive(outbox_data[0], outbox_data + 1,
			MSG_DATA_MIN_SZ, inbox_data + 1, MSG_DATA_MIN_SZ);

complete:
	inbox_data[0] = ret;
	return ret ?: count;
}

static int bpmp_mrq_show(struct seq_file *file, void *data)
{
	int i;

	for (i = 0; i < MSG_DATA_COUNT; i++) {
		seq_printf(file, "0x%x%s", inbox_data[i],
				i == MSG_DATA_COUNT - 1 ? "\n" : " ");
	}

	return 0;
}

static int bpmp_mrq_open(struct inode *inode, struct file *file)
{
	return single_open(file, bpmp_mrq_show, inode->i_private);
}

static const struct file_operations bpmp_mrq_fops = {
	.open = bpmp_mrq_open,
	.llseek = seq_lseek,
	.read = seq_read,
	.write = bpmp_mrq_write,
	.release = single_release
};

static const struct fops_entry root_attrs[] = {
	{ "ping", &bpmp_ping_fops, S_IRUGO },
	{ "trace_enable", &trace_enable_fops, S_IRUGO | S_IWUSR },
	{ "trace_disable", &trace_disable_fops, S_IWUSR },
	{ "trace", &trace_fops, S_IRUGO | S_IWUSR },
	{ "tag", &bpmp_tag_fops, S_IRUGO },
	{ "mrq", &bpmp_mrq_fops, S_IRUGO | S_IWUSR },
	{ "mount", &bpmp_mount_fops, S_IRUGO },
	{ "unmount", &bpmp_unmount_fops, S_IRUGO },
	{ NULL, NULL, 0 }
};

struct dentry *bpmp_init_debug(struct platform_device *pdev)
{
	struct dentry *root;

	root = debugfs_create_dir("bpmp", NULL);
	if (IS_ERR_OR_NULL(root))
		goto clean;

	if (bpmp_create_attrs(root_attrs, root, pdev))
		goto clean;

	if (bpmp_init_modules(pdev, root))
		goto clean;

	device = &pdev->dev;

	bpmp_root = root;

	return root;

clean:
	WARN_ON(1);

	debugfs_remove_recursive(root);

	return NULL;
}

struct dentry *tegra_bpmp_debugfs_add_file(char *name,
	umode_t mode, void *data, const struct file_operations *fops)
{
	if (!bpmp_root)
		return NULL;

	return debugfs_create_file(name, mode, bpmp_root, data, fops);
}
