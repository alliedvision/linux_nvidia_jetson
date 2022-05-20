/*
 * Copyright (c) 2018-2021, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef NVGPU_DEVICE_H
#define NVGPU_DEVICE_H

/**
 * @file
 *
 * Declare device info specific struct and defines.
 */

#include <nvgpu/types.h>
#include <nvgpu/list.h>
#include <nvgpu/pbdma.h>

struct gk20a;

/**
 * @defgroup NVGPU_DEVTYPE_DEFINES
 *
 * List of engine enumeration values supported for device_info parsing.
 */

/**
 * @ingroup NVGPU_TOP_DEVICE_INFO_DEFINES
 *
 * Device type for all graphics engine instances.
 */
#define NVGPU_DEVTYPE_GRAPHICS		0U

/**
 * @ingroup NVGPU_TOP_DEVICE_INFO_DEFINES
 *
 * Copy Engine 0; obsolete on pascal+. For Pascal+ use the LCE type and relevant
 * instance ID.
 *
 * This describes the 0th copy engine.
 */
#define NVGPU_DEVTYPE_COPY0		1U

/**
 * @ingroup NVGPU_TOP_DEVICE_INFO_DEFINES
 *
 * See #NVGPU_DEVTYPE_COPY0.
 */
#define NVGPU_DEVTYPE_COPY1		2U

/**
 * @ingroup NVGPU_TOP_DEVICE_INFO_DEFINES
 *
 * See #NVGPU_DEVTYPE_COPY0.
 */
#define NVGPU_DEVTYPE_COPY2		3U

/**
 * @ingroup NVGPU_TOP_DEVICE_INFO_DEFINES
 *
 * NVLINK IOCTRL device - used by NVLINK on dGPUs.
 */
#define NVGPU_DEVTYPE_IOCTRL		18U

/**
 * @ingroup NVGPU_TOP_DEVICE_INFO_DEFINES
 *
 * Logical Copy Engine devices.
 */
#define NVGPU_DEVTYPE_LCE		19U

#define NVGPU_MAX_DEVTYPE		24U

#define NVGPU_DEVICE_TOKEN_INIT		0U

/**
 * Structure definition for storing information for the devices and the engines
 * available on the chip.
 */
struct nvgpu_device {
	struct nvgpu_list_node dev_list_node;

	/**
	 * Engine type for this device.
	 */
	u32 type;

	/**
	 * Specifies instance of a device, allowing SW to distinguish between
	 * multiple copies of a device present on the chip.
	 */
	u32 inst_id;

	/**
	 * PRI base register offset for the 0th device instance of this type.
	 */
	u32 pri_base;

	/**
	 * MMU fault ID for this device or the invalid fault ID: U32_MAX.
	 */
	u32 fault_id;

	/**
	 * The unique per-device ID that host uses to identify any given engine.
	 */
	u32 engine_id;

	/**
	 * The ID of the runlist that serves this engine.
	 */
	u32 runlist_id;

	/**
	 * Interrupt ID for determining if this device has a pending interrupt.
	 */
	u32 intr_id;

	/**
	 * Reset ID for resetting the device in MC.
	 */
	u32 reset_id;

	/**
	 * PBDMA ID for this device. Technically not part of the dev_top array,
	 * but it's computable from various registers when the other device info
	 * is read.
	 *
	 * This also makes the vGPU support a little easier as this field gets
	 * passed to the vGPU client in the same data structure as the rest of the
	 * device info.
	 */
	u32 pbdma_id;

	/** @cond DOXYGEN_SHOULD_SKIP_THIS */
	/* Ampere+ device info additions */

	/**
	 * True if the device is an method engine behind host.
	 */
	bool engine;

	/**
	 * Runlist Engine ID; only valid if #engine is true.
	 */
	u32 rleng_id;

	/**
	 * Runlist PRI base - byte aligned based address. CHRAM offset can
	 * be computed from this.
	 */
	u32 rl_pri_base;

	/**
	 * PBDMA info for this device. It may contain multiple PBDMAs as
	 * there can now be multiple PBDMAs per runlist.
	 *
	 * This is in some ways awkward; devices seem to be more directly
	 * linked to runlists; runlists in turn have PBDMAs. Granted that
	 * means there's a computable relation between devices and PBDMAs
	 * it may make sense to not have this link.
	 */
	struct nvgpu_pbdma_info pbdma_info;
	/** @endcond DOXYGEN_SHOULD_SKIP_THIS */
};

struct nvgpu_device_list {
	/**
	 * Array of lists of devices; each list corresponds to one type of
	 * device. By having this as an array it's trivial to go from device
	 * enum type in the HW to the relevant devlist.
	 */
	struct nvgpu_list_node devlist_heads[NVGPU_MAX_DEVTYPE];

	/**
	 * Keep track of how many devices of each type exist.
	 */
	u32 dev_counts[NVGPU_MAX_DEVTYPE];
};

/*
 * Internal function: this shouldn't be necessary to directly call.
 * But in any event it converts a list_node pointer to the parent
 * nvgpu_device pointer.
 */
static inline struct nvgpu_device *
nvgpu_device_from_dev_list_node(struct nvgpu_list_node *node)
{
	return (struct nvgpu_device *)
		((uintptr_t)node - offsetof(struct nvgpu_device,
					    dev_list_node));
};

/**
 * @brief Iterate over each device of the specified type.
 *
 * @param g        [in] The GPU.
 * @param dev      [in] Device pointer to visit each device with.
 * @param dev_type [in] Device type to iterate over.
 *
 * Visit each device of the specified type; _do_not_ modify this device
 * list. It is immutable. Although it's' not type checked the dev pointer
 * should be a const struct device *.
 */
#define nvgpu_device_for_each(g, dev, dev_type)				\
	nvgpu_list_for_each_entry(dev,					\
				  &g->devs->devlist_heads[dev_type],	\
				  nvgpu_device,				\
				  dev_list_node)

/**
 * @brief Initialize the SW device list from the HW device list.
 *
 * @param g [in] The GPU.
 *
 * @return 0 on success; a negative error code otherwise.
 */
int nvgpu_device_init(struct gk20a *g);

/**
 * @brief Cleanup the device list on power down.
 *
 * @param g [in] The GPU.
 */
void nvgpu_device_cleanup(struct gk20a *g);

/**
 * @brief Read device info from SW device table.
 *
 * @param g [in]		GPU device struct pointer
 * @param dev_info [out]	Pointer to device information struct
 *				which gets populated with all the
 *				engine related information.
 * @param engine_type [in]	Engine enumeration value
 * @param inst_id [in]		Engine's instance identification number
 *
 * This will copy the contents of the requested device into the passed
 * device pointer. The device copied is chosen based on the \a type and
 * \a inst_id fields provided.
 */
const struct nvgpu_device *nvgpu_device_get(struct gk20a *g,
					    u32 type, u32 inst_id);

/**
 * @brief Return number of devices of type \a type.
 *
 * @param g [in]	The GPU.
 * @param type [i]	The type of device.
 */
u32 nvgpu_device_count(struct gk20a *g, u32 type);

/**
 * @brief Return true if dev is a copy engine device.
 *
 * @param g [in]	The GPU.
 * @param dev [in]	A device.
 *
 * @return true if \a dev matches a copy engine device type. For pre-Pascal
 * chips this is COPY[0, 1, 2], for Pascal and onward this is LCE.
 */
bool nvgpu_device_is_ce(struct gk20a *g, const struct nvgpu_device *dev);

/**
 * @brief Return true if dev is a graphics device.
 *
 * @param g [in]	The GPU.
 * @param dev [in]	A device.
 *
 * @return true if \a dev matches the graphics device type.
 */
bool nvgpu_device_is_graphics(struct gk20a *g, const struct nvgpu_device *dev);

/**
 * @brief Get all the copy engine pointers for this chip.
 *
 * @param g [in]	The GPU.
 * @param ces [out]	List to store CE pointers.
 * @param max [in]	Length of the ces list.
 *
 * Due to the change from Maxwell to Pascal - the adddition of logical copy
 * engines - the copy engine type changed from Maxwell to Pascal. For code that
 * aims to be chip agnostic, often it's useful obtain the set of copy engines,
 * regardless of type. This function does that by returning any COPY[0-2]
 * egnines detected and then any LCEs detected. No chip has both LCEs and
 * COPY[0-2], but by combining them, it's possible to avoid any chip specific
 * checks.
 *
 * @return The number of copy engine pointers written to the \a ces array.
 */
u32 nvgpu_device_get_copies(struct gk20a *g,
			    const struct nvgpu_device **ces,
			    u32 max);

/**
 * @brief Query list of async copy engines in the chip.
 *
 * @param g [in]	The GPU.
 * @param ces [out]	List to store CE pointers.
 * @param max [in]	Length of the ces list.
 *
 * Like nvgpu_device_get_copies() only this returns only asynchronous copy
 * engines. Async copy engines are engines that do not share a runlist with
 * a GR engine.
 *
 * @return The number of copy engine pointers written to the \a ces array.
 */
u32 nvgpu_device_get_async_copies(struct gk20a *g,
				  const struct nvgpu_device **ces,
				  u32 max);

/**
 * @brief Dubug dump for a device. Prints under the gpu_dbg_device log
 *        level.
 *
 * @param g [in]	The GPU.
 * @param dev [in]	A device.
 *
 * Use the nvgpu_info() framework for printing a given given device's
 * state information. This uses the gpu_dbg_device log level to condition
 * the prints.
 */
void nvgpu_device_dump_dev(struct gk20a *g, const struct nvgpu_device *dev);

#endif /* NVGPU_DEVICE_H */
