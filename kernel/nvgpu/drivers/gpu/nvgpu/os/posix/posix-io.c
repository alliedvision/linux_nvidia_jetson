/*
 * Copyright (c) 2018-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <nvgpu/io.h>
#include <nvgpu/io_usermode.h>
#include <nvgpu/bug.h>

#include <nvgpu/posix/io.h>
#include <nvgpu/posix/posix-fault-injection.h>
#include <nvgpu/posix/probe.h>

#include "os_posix.h"


#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
struct nvgpu_posix_fault_inj *nvgpu_readl_get_fault_injection(void)
{
	struct nvgpu_posix_fault_inj_container *c =
			nvgpu_posix_fault_injection_get_container();

	return &c->nvgpu_readl_fi;
}
#endif

/*
 * This function sets the IO callbacks to the passed set of callbacks. It
 * returns the value of the old IO callback struct pointer. This function
 * cannot fail.
 *
 * This is expected to be called from modules to set up their IO interaction.
 */
struct nvgpu_posix_io_callbacks *nvgpu_posix_register_io(
	struct gk20a *g,
	struct nvgpu_posix_io_callbacks *io_callbacks)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	struct nvgpu_posix_io_callbacks *old_io = p->callbacks;

	p->callbacks = io_callbacks;

	return old_io;
}

static void nvgpu_posix_writel(struct gk20a *g, u32 r, u32 v)
{
	struct nvgpu_posix_io_callbacks *callbacks =
		nvgpu_os_posix_from_gk20a(g)->callbacks;

	struct nvgpu_reg_access access = {
		.addr = r,
		.value = v
	};

	if (callbacks == NULL || callbacks->writel == NULL) {
		BUG();
	}

	callbacks->writel(g, &access);
}

static u32 nvgpu_posix_readl(struct gk20a *g, u32 r)
{
	struct nvgpu_posix_io_callbacks *callbacks =
		nvgpu_os_posix_from_gk20a(g)->callbacks;

	struct nvgpu_reg_access access = {
		.addr = r,
		.value = 0L
	};

#ifdef NVGPU_UNITTEST_FAULT_INJECTION_ENABLEMENT
	if (nvgpu_posix_fault_injection_handle_call(
			nvgpu_readl_get_fault_injection()) == true) {
		return 0;
	}
#endif

	if (callbacks == NULL || callbacks->readl == NULL) {
		BUG();
	}

	callbacks->readl(g, &access);

	return access.value;
}

static void nvgpu_posix_bar1_writel(struct gk20a *g, u32 b, u32 v)
{
	struct nvgpu_posix_io_callbacks *callbacks =
		nvgpu_os_posix_from_gk20a(g)->callbacks;

	struct nvgpu_reg_access access = {
		.addr = b,
		.value = v
	};

	if (callbacks == NULL || callbacks->bar1_writel == NULL) {
		BUG();
	}

	callbacks->bar1_writel(g, &access);
}

static u32 nvgpu_posix_bar1_readl(struct gk20a *g, u32 b)
{
	struct nvgpu_posix_io_callbacks *callbacks =
		nvgpu_os_posix_from_gk20a(g)->callbacks;

	struct nvgpu_reg_access access = {
		.addr = b,
		.value = 0L
	};

	if (callbacks == NULL || callbacks->bar1_readl == NULL) {
		BUG();
	}

	callbacks->bar1_readl(g, &access);

	return access.value;
}

void nvgpu_usermode_writel(struct gk20a *g, u32 r, u32 v)
{
	struct nvgpu_posix_io_callbacks *callbacks =
		nvgpu_os_posix_from_gk20a(g)->callbacks;

	struct nvgpu_reg_access access = {
		.addr = r,
		.value = v
	};

	if (callbacks == NULL || callbacks->usermode_writel == NULL) {
		BUG();
	}

	callbacks->usermode_writel(g, &access);
}

u32 nvgpu_os_readl(uintptr_t addr)
{
	struct gk20a *g = nvgpu_posix_current_device();
	u32 r, type;

	r = (u32)(addr & ~(NVGPU_POSIX_REG_MASK << NVGPU_POSIX_REG_SHIFT));
	type = (u32)(addr >> NVGPU_POSIX_REG_SHIFT);
	switch (type) {
	case NVGPU_POSIX_REG_BAR0:
		return nvgpu_posix_readl(g, r);
	case NVGPU_POSIX_REG_BAR1:
		return nvgpu_posix_bar1_readl(g, r);
	default:
		BUG();
	}
}

void nvgpu_os_writel(u32 v, uintptr_t addr)
{
	struct gk20a *g = nvgpu_posix_current_device();
	u32 r;
	u64 type;

	r = (u32)(addr & ~(NVGPU_POSIX_REG_MASK << NVGPU_POSIX_REG_SHIFT));
	type = addr >> NVGPU_POSIX_REG_SHIFT;
	switch (type) {
	case NVGPU_POSIX_REG_BAR0:
		return nvgpu_posix_writel(g, r, v);
	case NVGPU_POSIX_REG_BAR1:
		return nvgpu_posix_bar1_writel(g, r, v);
	default:
		BUG();
	}
}

void nvgpu_os_writel_relaxed(u32 v, uintptr_t addr)
{
	nvgpu_os_writel(v, addr);
}

void nvgpu_posix_io_init_reg_space(struct gk20a *g)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	p->recording = false;
	p->error_code = 0;
	nvgpu_init_list_node(&p->reg_space_head);
	nvgpu_init_list_node(&p->recorder_head);
}

int nvgpu_posix_io_get_error_code(struct gk20a *g)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	return p->error_code;
}

void nvgpu_posix_io_reset_error_code(struct gk20a *g)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	p->error_code = 0;
}

/*
 * Register a pre-initialized register space to the list of spaces.
 * This allows registering a space with statically initialized data.
 */
int nvgpu_posix_io_register_reg_space(struct gk20a *g,
		struct nvgpu_posix_io_reg_space *reg_space)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	if (reg_space == NULL || reg_space->data == NULL) {
		return -ENOMEM;
	}

	/*
	 * Add new register spaces to the front of the list. This lets unit
	 * tests define their own smaller register spaces that take precedence
	 * over the default reg lists.
	 */
	nvgpu_list_add(&reg_space->link, &p->reg_space_head);
	return 0;
}

void nvgpu_posix_io_unregister_reg_space(struct gk20a *g,
		struct nvgpu_posix_io_reg_space *reg_space)
{
	(void)g;
	nvgpu_list_del(&reg_space->link);
}

/*
 * Allocate and register a new register space to the list of spaces,
 * defined by a base address and a size.
 */
int nvgpu_posix_io_add_reg_space(struct gk20a *g, u32 base, u32 size)
{
	struct nvgpu_posix_io_reg_space *new_reg_space =
		nvgpu_kzalloc(g, sizeof(struct nvgpu_posix_io_reg_space));

	if (new_reg_space == NULL) {
		return -ENOMEM;
	}

	new_reg_space->base = base;
	new_reg_space->size = size;

	new_reg_space->data = nvgpu_vzalloc(g, size);
	if (new_reg_space->data == NULL) {
		nvgpu_kfree(g, new_reg_space);
		return -ENOMEM;
	}

	return nvgpu_posix_io_register_reg_space(g, new_reg_space);
}

void nvgpu_posix_io_delete_reg_space(struct gk20a *g, u32 base)
{
	struct nvgpu_posix_io_reg_space *reg_space =
		nvgpu_posix_io_get_reg_space(g, base);
	if (reg_space == NULL) {
		/* Invalid space, or already de-allocated */
		return;
	}

	nvgpu_posix_io_unregister_reg_space(g, reg_space);
	nvgpu_vfree(g, reg_space->data);
	nvgpu_kfree(g, reg_space);
}

/*
 * Lookup a register space from a given address. If no register space is found
 * this is a bug similar to a translation fault.
 */
struct nvgpu_posix_io_reg_space *nvgpu_posix_io_get_reg_space(struct gk20a *g,
		u32 addr)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	struct nvgpu_posix_io_reg_space *reg_space;

	nvgpu_list_for_each_entry(reg_space, &p->reg_space_head,
			nvgpu_posix_io_reg_space, link) {
		u32 offset = addr - reg_space->base;

		if ((addr >= reg_space->base) && (offset < reg_space->size)) {
			return reg_space;
		}
	}
	p->error_code = -EFAULT;
	nvgpu_err(g, "ABORT for address 0x%x", addr);
	return NULL;
}

void nvgpu_posix_io_writel_reg_space(struct gk20a *g, u32 addr, u32 data)
{
	struct nvgpu_posix_io_reg_space *space =
			nvgpu_posix_io_get_reg_space(g, addr);

	if (space != NULL) {
		u32 offset = (addr - space->base) / ((u32) sizeof(u32));

		*(space->data + offset) = data;
	}
}

u32 nvgpu_posix_io_readl_reg_space(struct gk20a *g, u32 addr)
{
	struct nvgpu_posix_io_reg_space *space =
			nvgpu_posix_io_get_reg_space(g, addr);

	if (space != NULL) {
		u32 offset = (addr - space->base) / ((u32) sizeof(u32));

		return *(space->data + offset);
	} else {
		return 0;
	}
}

/*
 * Start recording register writes. If this function is called again,
 * it will free all previously recorded events.
 */
void nvgpu_posix_io_start_recorder(struct gk20a *g)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	struct nvgpu_posix_io_reg_access *ptr;

	/* If list already has events, delete them all */
	if (p->recording == true) {
		while (!nvgpu_list_empty(&p->recorder_head)) {
			ptr = nvgpu_list_first_entry(&p->recorder_head,
				nvgpu_posix_io_reg_access, link);
			nvgpu_list_del(&ptr->link);
			nvgpu_kfree(g, ptr);
		}
	}
	p->recording = true;
}

void nvgpu_posix_io_record_access(struct gk20a *g,
		struct nvgpu_reg_access *access)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);

	if (p->recording == true) {
		struct nvgpu_posix_io_reg_access *new_event = nvgpu_kzalloc(g,
			sizeof(struct nvgpu_posix_io_reg_access));
		(void) memcpy(&(new_event->access), access,
			sizeof(struct nvgpu_reg_access));
		nvgpu_list_add_tail(&new_event->link, &p->recorder_head);
	}
}

/*
 * Take an array of accesses and compare to the recorded sequence. Returns true
 * if the array matches the recorded sequence.
 * If strict mode is false, this function allows extra accesses to be present
 * in the recording.
 */
bool nvgpu_posix_io_check_sequence(struct gk20a *g,
		struct nvgpu_reg_access *sequence, u32 size, bool strict)
{
	struct nvgpu_os_posix *p = nvgpu_os_posix_from_gk20a(g);
	struct nvgpu_posix_io_reg_access *ptr;
	u32 i = 0;

	if (p->recording == false) {
		return false;
	}

	nvgpu_list_for_each_entry(ptr, &p->recorder_head,
			nvgpu_posix_io_reg_access, link) {
		if ((sequence[i].addr == ptr->access.addr) &&
			(sequence[i].value == ptr->access.value)) {
			i++;
		} else {
			if (strict == true) {
				return false;
			}
		}
	}

	if (i != size) {
		/* Either missing or too many accesses */
		return false;
	}

	if (&ptr->link == &p->recorder_head) {
		/* Identical match */
		return true;
	}

	/* Not an identical match */
	if (strict) {
		return false;
	} else {
		return true;
	}
}
