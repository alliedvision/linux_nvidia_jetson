/*
 * Copyright (c) 2017-2022 NVIDIA Corporation.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

/**
 * @file drivers/media/platform/tegra/camera/fusa-capture/capture-common.c
 *
 * @brief VI/ISP channel common operations for the T186/T194 Camera RTCPU
 * platform.
 */

#include <linux/dma-buf.h>
#include <linux/dma-mapping.h>
#include <linux/nospec.h>
#include <linux/nvhost.h>
#include <linux/slab.h>
#include <linux/hashtable.h>
#include <linux/atomic.h>
#include <media/mc_common.h>

#include <media/fusa-capture/capture-common.h>


/**
 * @brief Capture buffer management table.
 */
struct capture_buffer_table {
	struct device *dev; /**< Originating device (VI or ISP) */
	struct kmem_cache *cache; /**< SLAB allocator cache */
	rwlock_t hlock; /**< Reader/writer lock on table contents */
	DECLARE_HASHTABLE(hhead, 4U); /**< Buffer hashtable head */
};

/**
 * @brief Capture surface NvRm and IOVA addresses handle.
 */
union capture_surface {
	uint64_t raw; /**< Pinned VI or ISP IOVA address */
	struct {
		uint32_t offset; /**< NvRm handle (upper 32 bits) */
		uint32_t hmem;
			/**<
			 * Offset of surface or pushbuffer address in descriptor
			 * (lower 32 bits) [byte]
			 */
	};
};

/**
 * @brief Capture buffer mapping (pinned).
 */
struct capture_mapping {
	struct hlist_node hnode; /**< Hash table node struct */
	atomic_t refcnt; /**< Capture mapping reference count */
	struct dma_buf *buf; /** Capture mapping dma_buf */
	struct dma_buf_attachment *atch;
		/**< dma_buf attachment (VI or ISP device) */
	struct sg_table *sgt; /**< Scatterlist to dma_buf attachment */
	unsigned int flag; /**< Bitmask access flag */
};

/**
 * @brief Determine whether all the bits of @a other are set in @a self.
 *
 * @param[in]	self	Bitmask flag to be compared
 * @param[in]	other	Bitmask value(s) to compare
 *
 * @retval	true	compatible
 * @retval	false	not compatible
 */
static inline bool flag_compatible(
	unsigned int self,
	unsigned int other)
{
	return (self & other) == other;
}

/**
 * @brief Determine whether BUFFER_RDWR is set in @a flag.
 *
 * @param[in]	flag	Bitmask flag to be compared
 *
 * @retval	true	BUFFER_RDWR set
 * @retval	false	BUFFER_RDWR not set
 */
static inline unsigned int flag_access_mode(
	unsigned int flag)
{
	return flag & BUFFER_RDWR;
}

/**
 * @brief Map capture common buffer access flag to a Linux dma_data_direction.
 *
 * @param[in]	flag	Bitmask access flag of capture common buffer
 *
 * @returns	@ref dma_data_direction mapping
 */
static inline enum dma_data_direction flag_dma_direction(
	unsigned int flag)
{
	static const enum dma_data_direction dir[4U] = {
		[0U] = DMA_BIDIRECTIONAL,
		[BUFFER_READ] = DMA_TO_DEVICE,
		[BUFFER_WRITE] = DMA_FROM_DEVICE,
		[BUFFER_RDWR] = DMA_BIDIRECTIONAL,
	};

	return dir[flag_access_mode(flag)];
}

/**
 * @brief Retrieve the scatterlist IOVA address of the capture surface mapping.
 *
 * @param[in]	pin	The capture_mapping of the buffer
 *
 * @returns	Physical address of scatterlist mapping
 */
static inline dma_addr_t mapping_iova(
	const struct capture_mapping *pin)
{
	dma_addr_t addr = sg_dma_address(pin->sgt->sgl);

	return (addr != 0) ? addr : sg_phys(pin->sgt->sgl);
}

/**
 * @brief Retrieve the dma_buf pointer of a capture surface mapping.
 *
 * @param[in]	pin	The capture_mapping of the buffer
 *
 * @returns	Pointer to the capture_mapping @ref dma_buf
 */
static inline struct dma_buf *mapping_buf(
	const struct capture_mapping *pin)
{
	return pin->buf;
}

/**
 * @brief Determine whether BUFFER_ADD is set in the capture surface mapping's
 * access flag.
 *
 * @param[in]	pin	The capture_mapping of the buffer
 *
 * @retval	true	BUFFER_ADD set
 * @retval	false	BUFFER_ADD not set
 */
static inline bool mapping_preserved(
	const struct capture_mapping *pin)
{
	return (bool)(pin->flag & BUFFER_ADD);
}

/**
 * @brief Set or unset the BUFFER_ADD bit in the capture surface mapping's
 * access flag, and correspondingly increment or decrement the mapping's refcnt.
 *
 * @param[in]	pin	The capture_mapping of the buffer
 * @param[in]	val	The capture_mapping of the buffer
 *
 * @retval	true	BUFFER_ADD set
 * @retval	false	BUFFER_ADD not set
 */
static inline void set_mapping_preservation(
	struct capture_mapping *pin,
	bool val)
{
	if (val) {
		pin->flag |= BUFFER_ADD;
		atomic_inc(&pin->refcnt);
	} else {
		pin->flag &= (~BUFFER_ADD);
		atomic_dec(&pin->refcnt);
	}
}

/**
 * @brief Iteratively search a capture buffer management table to find the entry
 * with @a buf, and @a flag bits set in the capture mapping.
 *
 * On success, the capture mapping is incremented by one if it is non-zero.
 *
 * @param[in]	tab	The capture buffer management table
 * @param[in]	buf	The mapping dma_buf pointer to match
 * @param[in]	flag	The mapping bitmask access flag to compare
 *
 * @returns	@ref capture_mapping pointer (success), NULL (failure)
 */
static struct capture_mapping *find_mapping(
	struct capture_buffer_table *tab,
	struct dma_buf *buf,
	unsigned int flag)
{
	struct capture_mapping *pin;
	bool success;

	read_lock(&tab->hlock);

	hash_for_each_possible(tab->hhead, pin, hnode, (unsigned long)buf) {
		if (
			(pin->buf == buf) &&
			flag_compatible(pin->flag, flag)
		) {
			success =  atomic_inc_not_zero(&pin->refcnt);
			if (success) {
				read_unlock(&tab->hlock);
				return pin;
			}
		}
	}

	read_unlock(&tab->hlock);

	return NULL;
}

/**
 * @brief Add an NvRm buffer to the buffer management table and initialize its
 * refcnt to 1.
 *
 * @param[in]	tab	The capture buffer management table
 * @param[in]	fd	The NvRm handle
 * @param[in]	flag	The mapping bitmask access flag to set
 *
 * @returns	@ref capture_mapping pointer (success), PTR_ERR (failure)
 */
static struct capture_mapping *get_mapping(
	struct capture_buffer_table *tab,
	uint32_t fd,
	unsigned int flag)
{
	struct capture_mapping *pin;
	struct dma_buf *buf;
	void *err;

	if (unlikely(tab == NULL)) {
		pr_err("%s: invalid buffer table\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	buf = dma_buf_get((int)fd);
	if (IS_ERR(buf)) {
		dev_err(tab->dev, "%s:%d: invalid memfd %u; errno %ld \n",
			 __func__, __LINE__, fd, PTR_ERR(buf));
		return ERR_CAST(buf);
	}

	pin = find_mapping(tab, buf, flag);
	if (pin != NULL) {
		dma_buf_put(buf);
		return pin;
	}

	pin = kmem_cache_alloc(tab->cache, GFP_KERNEL);
	if (unlikely(pin == NULL)) {
		err = ERR_PTR(-ENOMEM);
		goto err0;
	}

	pin->atch = dma_buf_attach(buf, tab->dev);
	if (unlikely(IS_ERR(pin->atch))) {
		err = pin->atch;
		goto err1;
	}

	pin->sgt = dma_buf_map_attachment(pin->atch, flag_dma_direction(flag));
	if (unlikely(IS_ERR(pin->sgt))) {
		err = pin->sgt;
		goto err2;
	}

	pin->flag = flag;
	pin->buf = buf;
	atomic_set(&pin->refcnt, 1U);
	INIT_HLIST_NODE(&pin->hnode);

	write_lock(&tab->hlock);
	hash_add(tab->hhead, &pin->hnode, (unsigned long)pin->buf);
	write_unlock(&tab->hlock);

	return pin;
err2:
	dma_buf_detach(buf, pin->atch);
err1:
	kmem_cache_free(tab->cache, pin);
err0:
	dma_buf_put(buf);
	dev_err(tab->dev, "%s:%d: memfd %u, flag %u; errno %ld \n",
		__func__, __LINE__,fd, flag, PTR_ERR(buf));
	return err;
}

struct capture_buffer_table *create_buffer_table(
	struct device *dev)
{
	struct capture_buffer_table *tab;

	tab = kmalloc(sizeof(*tab), GFP_KERNEL);

	if (likely(tab != NULL)) {
		tab->cache = KMEM_CACHE(capture_mapping, 0U);

		if (likely(tab->cache != NULL)) {
			tab->dev = dev;
			hash_init(tab->hhead);
			rwlock_init(&tab->hlock);
		} else {
			kfree(tab);
			tab = NULL;
		}
	}

	return tab;
}

void destroy_buffer_table(
	struct capture_buffer_table *tab)
{
	size_t bkt;
	struct hlist_node *next;
	struct capture_mapping *pin;

	if (unlikely(tab == NULL))
		return;

	write_lock(&tab->hlock);

	hash_for_each_safe(tab->hhead, bkt, next, pin, hnode) {
		hash_del(&pin->hnode);
		dma_buf_unmap_attachment(
			pin->atch, pin->sgt, flag_dma_direction(pin->flag));
		dma_buf_detach(pin->buf, pin->atch);
		dma_buf_put(pin->buf);
		kmem_cache_free(tab->cache, pin);
	}

	write_unlock(&tab->hlock);

	kmem_cache_destroy(tab->cache);
	kfree(tab);
}

static DEFINE_MUTEX(req_lock);

int capture_buffer_request(
	struct capture_buffer_table *tab,
	uint32_t memfd,
	uint32_t flag)
{
	struct capture_mapping *pin;
	struct dma_buf *buf;
	bool add = (bool)(flag & BUFFER_ADD);
	int err = 0;

	if (unlikely(tab == NULL)) {
		pr_err("%s: invalid buffer table\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&req_lock);

	if (add) {
		pin = get_mapping(tab, memfd, flag_access_mode(flag));
		if (IS_ERR(pin)) {
			err = PTR_ERR_OR_ZERO(pin);
			dev_err(tab->dev, "%s:%d: memfd %u, flag %u; errno %d",
				__func__, __LINE__, memfd, flag, err);
			goto end;
		}

		if (mapping_preserved(pin)) {
			err = -EEXIST;
			dev_err(tab->dev, "%s:%d: memfd %u exists; errno %d",
				__func__, __LINE__, memfd, err);
			put_mapping(tab, pin);
			goto end;
		}
	} else {
		buf = dma_buf_get((int)memfd);
		if (IS_ERR(buf)) {
			err = PTR_ERR_OR_ZERO(buf);
			dev_err(tab->dev, "%s:%d: invalid memfd %u; errno %d",
				__func__, __LINE__, memfd, err);
			goto end;
		}

		pin = find_mapping(tab, buf, BUFFER_ADD);
		if (pin == NULL) {
			err = -ENOENT;
			dev_err(tab->dev, "%s:%d: memfd %u not exists; errno %d",
				__func__, __LINE__, memfd, err);
			dma_buf_put(buf);
			goto end;
		}
		dma_buf_put(buf);
	}

	set_mapping_preservation(pin, add);
	put_mapping(tab, pin);

end:
	mutex_unlock(&req_lock);
	return err;
}

int capture_buffer_add(
	struct capture_buffer_table *t,
	uint32_t fd)
{
	return capture_buffer_request(t, fd, BUFFER_ADD | BUFFER_RDWR);
}

void put_mapping(
	struct capture_buffer_table *t,
	struct capture_mapping *pin)
{
	bool zero;

	zero = atomic_dec_and_test(&pin->refcnt);
	if (zero) {
		if (unlikely(mapping_preserved(pin))) {
			dev_err(t->dev, "%s:%d: unexpected put for a preserved mapping",
				__func__, __LINE__);
			atomic_inc(&pin->refcnt);
			return;
		}

		write_lock(&t->hlock);
		hash_del(&pin->hnode);
		write_unlock(&t->hlock);

		dma_buf_unmap_attachment(
			pin->atch, pin->sgt, flag_dma_direction(pin->flag));
		dma_buf_detach(pin->buf, pin->atch);
		dma_buf_put(pin->buf);
		kmem_cache_free(t->cache, pin);
	}
}

int capture_common_pin_and_get_iova(struct capture_buffer_table *buf_ctx,
		uint32_t mem_handle, uint64_t mem_offset,
		uint64_t *meminfo_base_address, uint64_t *meminfo_size,
		struct capture_common_unpins *unpins)
{
	struct capture_mapping *map;
	struct dma_buf *buf;
	uint64_t size;
	uint64_t iova;

	/* NULL is a valid unput indicating unused data field */
	if (!mem_handle) {
		return 0;
	}

	if (unpins->num_unpins >= MAX_PIN_BUFFER_PER_REQUEST) {
		pr_err("%s: too many buffers per request\n", __func__);
			return -ENOMEM;
	}

	map = get_mapping(buf_ctx, mem_handle, BUFFER_RDWR);

	if (IS_ERR(map)) {
		pr_err("%s: cannot get mapping\n", __func__);
		return -EINVAL;
	}

	buf = mapping_buf(map);
	size = buf->size;
	iova = mapping_iova(map);

	if (mem_offset >= size) {
		pr_err("%s: offset is out of bounds\n", __func__);
		return -EINVAL;
	}

	*meminfo_base_address = iova + mem_offset;
	*meminfo_size = size - mem_offset;

	unpins->data[unpins->num_unpins] = map;
	unpins->num_unpins++;
	return 0;
}

int capture_common_setup_progress_status_notifier(
	struct capture_common_status_notifier *status_notifier,
	uint32_t mem,
	uint32_t buffer_size,
	uint32_t mem_offset)
{
	struct dma_buf *dmabuf;
	void *va;

	/* take reference for the userctx */
	dmabuf = dma_buf_get(mem);
	if (IS_ERR(dmabuf))
		return PTR_ERR(dmabuf);

	if (buffer_size > U32_MAX - mem_offset) {
		pr_err("%s: buffer_size or mem_offset too large\n", __func__);
		return -EINVAL;
	}

	if ((buffer_size + mem_offset) > dmabuf->size) {
		dma_buf_put(dmabuf);
		pr_err("%s: invalid offset\n", __func__);
		return -EINVAL;
	}

	/* map handle and clear error notifier struct */
	va = dma_buf_vmap(dmabuf);
	if (!va) {
		dma_buf_put(dmabuf);
		pr_err("%s: Cannot map notifier handle\n", __func__);
		return -ENOMEM;
	}

	memset(va, 0, buffer_size);

	status_notifier->buf = dmabuf;
	status_notifier->va = va;
	status_notifier->offset = mem_offset;
	return 0;
}

int capture_common_release_progress_status_notifier(
	struct capture_common_status_notifier *progress_status_notifier)
{
	struct dma_buf *dmabuf = progress_status_notifier->buf;
	void *va = progress_status_notifier->va;

	if (dmabuf != NULL) {
		if (va != NULL)
			dma_buf_vunmap(dmabuf, va);

		dma_buf_put(dmabuf);
	}

	progress_status_notifier->buf = NULL;
	progress_status_notifier->va = NULL;
	progress_status_notifier->offset = 0;

	return 0;
}

int capture_common_set_progress_status(
	struct capture_common_status_notifier *progress_status_notifier,
	uint32_t buffer_slot,
	uint32_t buffer_depth,
	uint8_t new_val)
{
	uint32_t *status_notifier = (uint32_t *) (progress_status_notifier->va +
			progress_status_notifier->offset);

	if (buffer_slot >= buffer_depth) {
		pr_err("%s: Invalid offset!", __func__);
		return -EINVAL;
	}
	buffer_slot = array_index_nospec(buffer_slot, buffer_depth);

	/*
	 * Since UMD and KMD can both write to the shared progress status
	 * notifier buffer, insert memory barrier here to ensure that any
	 * other store operations to the buffer would be done before the
	 * write below.
	 */
	wmb();

	status_notifier[buffer_slot] = new_val;

	return 0;
}

int capture_common_pin_memory(
	struct device *dev,
	uint32_t mem,
	struct capture_common_buf *unpin_data)
{
	struct dma_buf *buf;
	struct dma_buf_attachment *attach;
	struct sg_table *sgt;
	int err = 0;

	buf = dma_buf_get(mem);
	if (IS_ERR(buf)) {
		err = PTR_ERR(buf);
		goto fail;
	}

	attach = dma_buf_attach(buf, dev);
	if (IS_ERR(attach)) {
		err = PTR_ERR(attach);
		goto fail;
	}

	sgt = dma_buf_map_attachment(attach, DMA_BIDIRECTIONAL);
	if (IS_ERR(sgt)) {
		err = PTR_ERR(sgt);
		goto fail;
	}

	if (sg_dma_address(sgt->sgl) == 0)
		sg_dma_address(sgt->sgl) = sg_phys(sgt->sgl);

	unpin_data->va = dma_buf_vmap(buf);

	if (unpin_data->va == NULL) {
		pr_err("%s: failed to map pinned memory\n", __func__);
		goto fail;
	}

	unpin_data->iova = sg_dma_address(sgt->sgl);
	unpin_data->buf = buf;
	unpin_data->attach = attach;
	unpin_data->sgt = sgt;

	return 0;

fail:
	capture_common_unpin_memory(unpin_data);
	return err;
}

void capture_common_unpin_memory(
	struct capture_common_buf *unpin_data)
{
	if (unpin_data->va)
		dma_buf_vunmap(unpin_data->buf, unpin_data->va);

	if (unpin_data->sgt != NULL)
		dma_buf_unmap_attachment(unpin_data->attach, unpin_data->sgt,
				DMA_BIDIRECTIONAL);
	if (unpin_data->attach != NULL)
		dma_buf_detach(unpin_data->buf, unpin_data->attach);
	if (unpin_data->buf != NULL)
		dma_buf_put(unpin_data->buf);

	unpin_data->sgt = NULL;
	unpin_data->attach = NULL;
	unpin_data->buf = NULL;
	unpin_data->iova = 0;
	unpin_data->va = NULL;
}

