/*
 * include/linux/nvhost.h
 *
 * Tegra graphics host driver
 *
 * Copyright (c) 2009-2022, NVIDIA Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __LINUX_NVHOST_H
#define __LINUX_NVHOST_H

#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/types.h>
#include <linux/devfreq.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/pm_qos.h>
#include <linux/time.h>
#include <linux/version.h>

#include <uapi/linux/nvdev_fence.h>

#ifdef CONFIG_TEGRA_HOST1X
#include <linux/host1x.h>
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST) && IS_ENABLED(CONFIG_TEGRA_HOST1X)
#error "Unable to enable TEGRA_GRHOST or TEGRA_HOST1X at the same time!"
#endif

struct tegra_bwmgr_client;

struct nvhost_channel;
struct nvhost_master;
struct nvhost_cdma;
struct nvhost_hwctx;
struct nvhost_device_power_attr;
struct nvhost_device_profile;
struct mem_mgr;
struct nvhost_as_moduleops;
struct nvhost_ctrl_sync_fence_info;
struct nvhost_sync_timeline;
struct nvhost_sync_pt;
enum nvdev_fence_kind;
struct nvdev_fence;
struct sync_pt;
struct dma_fence;
struct nvhost_fence;

#define NVHOST_MODULE_MAX_CLOCKS		8
#define NVHOST_MODULE_MAX_SYNCPTS		16
#define NVHOST_MODULE_MAX_WAITBASES		3
#define NVHOST_MODULE_MAX_MODMUTEXES		5
#define NVHOST_MODULE_MAX_IORESOURCE_MEM	5
#define NVHOST_NAME_SIZE			24
#define NVSYNCPT_INVALID			(-1)

#define NVSYNCPT_AVP_0			(10)	/* t20, t30, t114, t148 */
#define NVSYNCPT_3D			(22)	/* t20, t30, t114, t148 */
#define NVSYNCPT_VBLANK0		(26)	/* t20, t30, t114, t148 */
#define NVSYNCPT_VBLANK1		(27)	/* t20, t30, t114, t148 */

#define NVMODMUTEX_ISP_0		(1)	/* t124, t132, t210 */
#define NVMODMUTEX_ISP_1		(2)	/* t124, t132, t210 */
#define NVMODMUTEX_NVJPG		(3)	/* t210 */
#define NVMODMUTEX_NVDEC		(4)	/* t210 */
#define NVMODMUTEX_MSENC		(5)	/* t124, t132, t210 */
#define NVMODMUTEX_TSECA		(6)	/* t124, t132, t210 */
#define NVMODMUTEX_TSECB		(7)	/* t124, t132, t210 */
#define NVMODMUTEX_VI			(8)	/* t124, t132, t210 */
#define NVMODMUTEX_VI_0			(8)	/* t148 */
#define NVMODMUTEX_VIC			(10)	/* t124, t132, t210 */
#define NVMODMUTEX_VI_1			(11)	/* t124, t132, t210 */

enum nvhost_power_sysfs_attributes {
	NVHOST_POWER_SYSFS_ATTRIB_AUTOSUSPEND_DELAY,
	NVHOST_POWER_SYSFS_ATTRIB_FORCE_ON,
	NVHOST_POWER_SYSFS_ATTRIB_MAX
};

struct nvhost_notification {
	struct {			/* 0000- */
		__u32 nanoseconds[2];	/* nanoseconds since Jan. 1, 1970 */
	} time_stamp;			/* -0007 */
	__u32 info32;	/* info returned depends on method 0008-000b */
#define	NVHOST_CHANNEL_FIFO_ERROR_IDLE_TIMEOUT	8
#define	NVHOST_CHANNEL_GR_ERROR_SW_NOTIFY	13
#define	NVHOST_CHANNEL_GR_SEMAPHORE_TIMEOUT	24
#define	NVHOST_CHANNEL_GR_ILLEGAL_NOTIFY	25
#define	NVHOST_CHANNEL_FIFO_ERROR_MMU_ERR_FLT	31
#define	NVHOST_CHANNEL_PBDMA_ERROR		32
#define	NVHOST_CHANNEL_RESETCHANNEL_VERIF_ERROR	43
	__u16 info16;	/* info returned depends on method 000c-000d */
	__u16 status;	/* user sets bit 15, NV sets status 000e-000f */
#define	NVHOST_CHANNEL_SUBMIT_TIMEOUT		1
};

struct nvhost_gating_register {
	u64 addr;
	u32 prod;
	u32 disable;
};

struct nvhost_actmon_register {
	u32 addr;
	u32 val;
};

enum tegra_emc_request_type {
	TEGRA_SET_EMC_FLOOR,		/* lower bound */
	TEGRA_SET_EMC_CAP,		/* upper bound */
	TEGRA_SET_EMC_ISO_CAP,		/* upper bound that affects ISO Bw */
	TEGRA_SET_EMC_SHARED_BW,	/* shared bw request */
	TEGRA_SET_EMC_SHARED_BW_ISO,	/* for use by ISO Mgr only */
	TEGRA_SET_EMC_REQ_COUNT		/* Should always be last */
};

struct nvhost_clock {
	char *name;
	unsigned long default_rate;
	u32 moduleid;
	enum tegra_emc_request_type request_type;
	bool disable_scaling;
	unsigned long devfreq_rate;
};

struct nvhost_vm_hwid {
	u64 addr;
	bool dynamic;
	u32 shift;
};

/*
 * Defines HW and SW class identifiers.
 *
 * This is module ID mapping between userspace and kernelspace.
 * The values of enum entries' are referred from NvRmModuleID enum defined
 * in below userspace file:
 * $TOP/vendor/nvidia/tegra/core/include/nvrm_module.h
 * Please make sure each entry below has same value as set in above file.
 */
enum nvhost_module_identifier {

	/* Specifies external memory (DDR RAM, etc) */
	NVHOST_MODULE_ID_EXTERNAL_MEMORY_CONTROLLER = 75,

	/* Specifies CBUS floor client module */
	NVHOST_MODULE_ID_CBUS_FLOOR = 119,

	/* Specifies shared EMC client module */
	NVHOST_MODULE_ID_EMC_SHARED,
	NVHOST_MODULE_ID_MAX
};

enum nvhost_resource_policy {
	RESOURCE_PER_DEVICE = 0,
	RESOURCE_PER_CHANNEL_INSTANCE,
};

struct nvhost_device_data {
	int		version;	/* ip version number of device */
	int		id;		/* Separates clients of same hw */
	void __iomem	*aperture[NVHOST_MODULE_MAX_IORESOURCE_MEM];
	struct device_dma_parameters dma_parms;

	u32		modulemutexes[NVHOST_MODULE_MAX_MODMUTEXES];
	u32		moduleid;	/* Module id for user space API */

	/* interrupt ISR routine for falcon based engines */
	int (*flcn_isr)(struct platform_device *dev);
	int irq;
	int module_irq;	/* IRQ bit from general intr reg for module intr */
	spinlock_t mirq_lock;	/* spin lock for module irq */
	bool self_config_flcn_isr; /* skip setting up falcon interrupts */

	/* Should we toggle the engine SLCG when we turn on the domain? */
	bool		poweron_toggle_slcg;

	/* Flag to set SLCG notifier (for the modules other than VIC) */
	bool slcg_notifier_enable;

	/* Used to serialize channel when map-at-submit is used w/o mlocks */
	u32		last_submit_syncpt_id;
	u32		last_submit_syncpt_value;

	bool		power_on;	/* If module is powered on */

	u32		class;		/* Device class */
	bool		exclusive;	/* True if only one user at a time */
	bool		keepalive;	/* Do not power gate when opened */
	bool		serialize;	/* Serialize submits in the channel */
	bool		push_work_done;	/* Push_op done into push buffer */
	bool		poweron_reset;	/* Reset the engine before powerup */
	bool		virtual_dev;	/* True if virtualized device */
	char		*devfs_name;	/* Name in devfs */
	char		*devfs_name_family; /* Core of devfs name */

	/* Support aborting the channel with close(channel_fd) */
	bool		support_abort_on_close;

	char		*firmware_name;	/* Name of firmware */
	bool		firmware_not_in_subdir; /* Firmware is not located in
                                                   chip subdirectory */

	bool		engine_can_cg;	/* True if CG is enabled */
	bool		can_powergate;	/* True if module can be power gated */
	int		autosuspend_delay;/* Delay before power gated */
	struct nvhost_clock clocks[NVHOST_MODULE_MAX_CLOCKS];/* Clock names */

	/* Clock gating registers */
	struct nvhost_gating_register *engine_cg_regs;

	int		num_clks;	/* Number of clocks opened for dev */
#if IS_ENABLED(CONFIG_TEGRA_GRHOST)
	struct clk	*clk[NVHOST_MODULE_MAX_CLOCKS];
#else
	struct clk_bulk_data *clks;
#endif
	struct mutex	lock;		/* Power management lock */
	struct list_head client_list;	/* List of clients and rate requests */

	int		num_channels;	/* Max num of channel supported */
	int		num_mapped_chs;	/* Num of channel mapped to device */
	int		num_ppc;	/* Number of pixels per clock cycle */

	/* device node for channel operations */
	dev_t cdev_region;
	struct device *node;
	struct cdev cdev;

	/* Address space device node */
	struct device *as_node;
	struct cdev as_cdev;

	/* device node for ctrl block */
	struct class *nvhost_class;
	struct device *ctrl_node;
	struct cdev ctrl_cdev;
	const struct file_operations *ctrl_ops;    /* ctrl ops for the module */

	/* address space operations */
	const struct nvhost_as_moduleops *as_ops;

	struct kobject *power_kobj;	/* kobject to hold power sysfs entries */
	struct nvhost_device_power_attr *power_attrib;	/* sysfs attributes */
	/* kobject to hold clk_cap sysfs entries */
	struct kobject clk_cap_kobj;
	struct kobj_attribute *clk_cap_attrs;
	struct dentry *debugfs;		/* debugfs directory */

	u32 nvhost_timeout_default;

	/* Data for devfreq usage */
	struct devfreq			*power_manager;
	/* Private device profile data */
	struct nvhost_device_profile	*power_profile;
	/* Should we read load estimate from hardware? */
	bool				actmon_enabled;
	/* Should we do linear emc scaling? */
	bool				linear_emc;
	/* Offset to actmon registers */
	u32				actmon_regs;
	/* WEIGHT_COUNT of actmon */
	u32				actmon_weight_count;
	struct nvhost_actmon_register	*actmon_setting_regs;
	/* Devfreq governor name */
	const char			*devfreq_governor;
	unsigned long *freq_table;

	/* Marks if the device is booted when pm runtime is disabled */
	bool				booted;

	/* Should be marked as true if nvhost shouldn't create device nodes */
	bool				kernel_only;

	void *private_data;		/* private platform data */
	void *falcon_data;		/* store the falcon info */
	struct platform_device *pdev;	/* owner platform_device */
	void *virt_priv;		/* private data for virtualized dev */
#if IS_ENABLED(CONFIG_TEGRA_HOST1X)
	struct host1x *host1x;		/* host1x device */
#endif

	struct mutex no_poweroff_req_mutex;
	struct dev_pm_qos_request no_poweroff_req;
	int no_poweroff_req_count;

	struct notifier_block		toggle_slcg_notifier;

	struct rw_semaphore busy_lock;
	bool forced_idle;

	/* Finalize power on. Can be used for context restore. */
	int (*finalize_poweron)(struct platform_device *dev);

	/* Called each time we enter the class */
	int (*init_class_context)(struct platform_device *dev,
				  struct nvhost_cdma *cdma);

	/*
	 * Reset the unit. Used for timeout recovery, resetting the unit on
	 * probe and when un-powergating.
	 */
	void (*reset)(struct platform_device *dev);

	/* Device is busy. */
	void (*busy)(struct platform_device *);

	/* Device is idle. */
	void (*idle)(struct platform_device *);

	/* Scaling init is run on device registration */
	void (*scaling_init)(struct platform_device *dev);

	/* Scaling deinit is called on device unregistration */
	void (*scaling_deinit)(struct platform_device *dev);

	/* Postscale callback is called after frequency change */
	void (*scaling_post_cb)(struct nvhost_device_profile *profile,
				unsigned long freq);

	/* Preparing for power off. Used for context save. */
	int (*prepare_poweroff)(struct platform_device *dev);

	/* paring for power off. Used for context save. */
	int (*aggregate_constraints)(struct platform_device *dev,
				     int clk_index,
				     unsigned long floor_rate,
				     unsigned long pixel_rate,
				     unsigned long bw_rate);

	/* Called after successful client device init. This can
	 * be used in cases where the hardware specifics differ
	 * between hardware revisions */
	int (*hw_init)(struct platform_device *dev);

	/* Used to add platform specific masks on reloc address */
	dma_addr_t (*get_reloc_phys_addr)(dma_addr_t phys_addr, u32 reloc_type);

	/* Allocates a context handler for the device */
	struct nvhost_hwctx_handler *(*alloc_hwctx_handler)(u32 syncpt,
			struct nvhost_channel *ch);

	/* engine specific init functions */
	int (*pre_virt_init)(struct platform_device *pdev);
	int (*post_virt_init)(struct platform_device *pdev);

	/* engine specific functions */
	int (*memory_init)(struct platform_device *pdev);

	phys_addr_t carveout_addr;
	phys_addr_t carveout_size;

	/* Information related to engine-side synchronization */
	void *syncpt_unit_interface;

	u64 transcfg_addr;
	u32 transcfg_val;
	u64 mamask_addr;
	u32 mamask_val;
	u64 borps_addr;
	u32 borps_val;
	struct nvhost_vm_hwid vm_regs[13];

	/* Actmon IRQ from hintstatus_r */
	unsigned int actmon_irq;

	/* Is the device already forced on? */
	bool forced_on;

	/* Should we map channel at submit time? */
	bool resource_policy;

	/* Should we enable context isolation for this device? */
	bool isolate_contexts;

	/* channel user context list */
	struct mutex userctx_list_lock;
	struct list_head userctx_list;

	/* reset control for this device */
	struct reset_control *reset_control;

	/* For loadable nvgpu module, we dynamically assign function
	 * pointer of gk20a_debug_dump_device once the module loads */
	void *debug_dump_data;
	void (*debug_dump_device)(void *dev);

	/* icc client id for emc requests */
	int icc_id;

	/* icc_path handle handle */
	struct icc_path *icc_path_handle;

	/* bandwidth manager client id for emc requests */
	int bwmgr_client_id;

	/* bandwidth manager handle */
	struct tegra_bwmgr_client *bwmgr_handle;

	/* number of frames mlock can be locked for */
	u32 mlock_timeout_factor;

	/* eventlib id for the device */
	int eventlib_id;

	/* deliver task timestamps for falcon */
	void (*enable_timestamps)(struct platform_device *pdev,
			struct nvhost_cdma *cdma, dma_addr_t timestamp_addr);

	/* enable risc-v boot */
	bool enable_riscv_boot;

	/* store the risc-v info */
	void *riscv_data;

	/* name of riscv descriptor binary */
	char *riscv_desc_bin;

	/* name of riscv image binary */
	char *riscv_image_bin;

};


static inline
struct nvhost_device_data *nvhost_get_devdata(struct platform_device *pdev)
{
	return (struct nvhost_device_data *)platform_get_drvdata(pdev);
}

static inline bool nvhost_dev_is_virtual(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);

	return pdata->virtual_dev;
}

struct nvhost_device_power_attr {
	struct platform_device *ndev;
	struct kobj_attribute power_attr[NVHOST_POWER_SYSFS_ATTRIB_MAX];
};

int flcn_intr_init(struct platform_device *pdev);
int flcn_reload_fw(struct platform_device *pdev);
int nvhost_flcn_prepare_poweroff(struct platform_device *pdev);
int nvhost_flcn_finalize_poweron(struct platform_device *dev);

/* public api to return platform_device ptr to the default host1x instance */
struct platform_device *nvhost_get_default_device(void);

/* common runtime pm and power domain APIs */
int nvhost_module_init(struct platform_device *ndev);
void nvhost_module_deinit(struct platform_device *dev);
void nvhost_module_reset(struct platform_device *dev, bool reboot);
void nvhost_module_idle(struct platform_device *dev);
void nvhost_module_idle_mult(struct platform_device *pdev, int refs);
int nvhost_module_busy(struct platform_device *dev);
extern const struct dev_pm_ops nvhost_module_pm_ops;

void host1x_writel(struct platform_device *dev, u32 r, u32 v);
u32 host1x_readl(struct platform_device *dev, u32 r);

/* common device management APIs */
int nvhost_client_device_get_resources(struct platform_device *dev);
int nvhost_client_device_release(struct platform_device *dev);
int nvhost_client_device_init(struct platform_device *dev);

/* public host1x sync-point management APIs */
u32 nvhost_get_syncpt_host_managed(struct platform_device *pdev,
				   u32 param, const char *syncpt_name);
u32 nvhost_get_syncpt_client_managed(struct platform_device *pdev,
				     const char *syncpt_name);
void nvhost_syncpt_put_ref_ext(struct platform_device *pdev, u32 id);
bool nvhost_syncpt_is_valid_pt_ext(struct platform_device *dev, u32 id);
void nvhost_syncpt_set_minval(struct platform_device *dev, u32 id, u32 val);
void nvhost_syncpt_set_min_update(struct platform_device *pdev, u32 id, u32 val);
int nvhost_syncpt_read_ext_check(struct platform_device *dev, u32 id, u32 *val);
u32 nvhost_syncpt_read_maxval(struct platform_device *dev, u32 id);
u32 nvhost_syncpt_incr_max_ext(struct platform_device *dev, u32 id, u32 incrs);
int nvhost_syncpt_is_expired_ext(struct platform_device *dev, u32 id,
				 u32 thresh);
dma_addr_t nvhost_syncpt_address(struct platform_device *engine_pdev, u32 id);
int nvhost_syncpt_unit_interface_init(struct platform_device *pdev);
void nvhost_syncpt_unit_interface_deinit(struct platform_device *pdev);

/* public host1x interrupt management APIs */
int nvhost_intr_register_notifier(struct platform_device *pdev,
				  u32 id, u32 thresh,
				  void (*callback)(void *, int),
				  void *private_data);

/* public host1x sync-point management APIs */
#ifdef CONFIG_TEGRA_HOST1X

struct host1x *nvhost_get_host1x(struct platform_device *pdev);

static inline struct flcn *get_flcn(struct platform_device *pdev)
{
	struct nvhost_device_data *pdata = platform_get_drvdata(pdev);

	return pdata ? pdata->falcon_data : NULL;
}

static inline int nvhost_module_set_rate(struct platform_device *dev, void *priv,
			   unsigned long constraint, int index,
			   unsigned long attr)
{
	return 0;
}

static inline int nvhost_module_add_client(struct platform_device *dev, void *priv)
{
	return 0;
}

static inline void nvhost_module_remove_client(struct platform_device *dev, void *priv) { }

static inline int nvhost_syncpt_get_cv_dev_address_table(struct platform_device *engine_pdev,
							 int *count, dma_addr_t **table)
{
	return -ENODEV;
}

static inline const struct firmware *
nvhost_client_request_firmware(struct platform_device *dev,
	const char *fw_name, bool warn)
{
	return NULL;
}

static inline void nvhost_debug_dump_device(struct platform_device *pdev)
{
}

static inline int nvhost_fence_create_fd(
		struct platform_device *pdev,
		struct nvhost_ctrl_sync_fence_info *pts,
		u32 num_pts,
		const char *name,
		s32 *fence_fd)
{
	return -EOPNOTSUPP;
}

static inline int nvhost_fence_foreach_pt(
	struct nvhost_fence *fence,
	int (*iter)(struct nvhost_ctrl_sync_fence_info, void *),
	void *data)
{
	return -EOPNOTSUPP;
}

static inline struct nvhost_job *nvhost_job_alloc(struct nvhost_channel *ch,
		int num_cmdbufs, int num_relocs, int num_waitchks,
		int num_syncpts)
{
	return NULL;
}

static inline void nvhost_job_put(struct nvhost_job *job) {}

static inline int nvhost_job_add_client_gather_address(struct nvhost_job *job,
		u32 num_words, u32 class_id, dma_addr_t gather_address)
{
	return -EOPNOTSUPP;
}

static inline int nvhost_channel_map(struct nvhost_device_data *pdata,
			struct nvhost_channel **ch,
			void *identifier)
{
	return -EOPNOTSUPP;
}

static inline int nvhost_channel_submit(struct nvhost_job *job)
{
	return -EOPNOTSUPP;
}

static inline void nvhost_putchannel(struct nvhost_channel *ch, int cnt) {}

static inline struct nvhost_fence *nvhost_fence_get(int fd)
{
	return NULL;
}

static inline void nvhost_fence_put(struct nvhost_fence *fence) {}

static inline int nvhost_fence_num_pts(struct nvhost_fence *fence)
{
	return 0;
}

static inline dma_addr_t nvhost_t194_get_reloc_phys_addr(dma_addr_t phys_addr,
							 u32 reloc_type)
{
	return 0;
}

static inline dma_addr_t nvhost_t23x_get_reloc_phys_addr(dma_addr_t phys_addr,
							 u32 reloc_type)
{
	return 0;
}

static inline void nvhost_eventlib_log_task(struct platform_device *pdev,
					    u32 syncpt_id,
					    u32 syncpt_thres,
					    u64 timestamp_start,
					    u64 timestamp_end)
{
}

static inline void nvhost_eventlib_log_submit(struct platform_device *pdev,
					      u32 syncpt_id,
					      u32 syncpt_thresh,
					      u64 timestamp)
{
}

static inline void nvhost_eventlib_log_fences(struct platform_device *pdev,
                                              u32 task_syncpt_id,
                                              u32 task_syncpt_thresh,
                                              struct nvdev_fence *fences,
                                              u8 num_fences,
                                              enum nvdev_fence_kind kind,
                                              u64 timestamp)
{
}
#else

#ifdef CONFIG_DEBUG_FS
void nvhost_register_dump_device(
		struct platform_device *dev,
		void (*nvgpu_debug_dump_device)(void *),
		void *data);
void nvhost_unregister_dump_device(struct platform_device *dev);
#else
static inline void nvhost_register_dump_device(
		struct platform_device *dev,
		void (*nvgpu_debug_dump_device)(void *),
		void *data)
{
}

static inline void nvhost_unregister_dump_device(struct platform_device *dev)
{
}
#endif

void host1x_channel_writel(struct nvhost_channel *ch, u32 r, u32 v);
u32 host1x_channel_readl(struct nvhost_channel *ch, u32 r);

void host1x_sync_writel(struct nvhost_master *dev, u32 r, u32 v);
u32 host1x_sync_readl(struct nvhost_master *dev, u32 r);

/* public host1x power management APIs */
bool nvhost_module_powered_ext(struct platform_device *dev);
/* This power ON only host1x and doesn't power ON module */
int nvhost_module_busy_ext(struct platform_device *dev);
/* This power OFF only host1x and doesn't power OFF module */
void nvhost_module_idle_ext(struct platform_device *dev);

 /* Public PM nvhost APIs. */
/* This power ON both host1x and module */
int nvhost_module_busy(struct platform_device *dev);
/* This power OFF both host1x and module */
void nvhost_module_idle(struct platform_device *dev);

/* public api to register/unregister a subdomain */
void nvhost_register_client_domain(struct generic_pm_domain *domain);
void nvhost_unregister_client_domain(struct generic_pm_domain *domain);

int nvhost_module_add_client(struct platform_device *dev,
		void *priv);
void nvhost_module_remove_client(struct platform_device *dev,
		void *priv);

int nvhost_module_set_rate(struct platform_device *dev, void *priv,
		unsigned long constraint, int index, unsigned long attr);

/* public APIs required to submit in-kernel work */
int nvhost_channel_map(struct nvhost_device_data *pdata,
			struct nvhost_channel **ch,
			void *identifier);
void nvhost_putchannel(struct nvhost_channel *ch, int cnt);
/* Allocate memory for a job. Just enough memory will be allocated to
 * accomodate the submit announced in submit header.
 */
struct nvhost_job *nvhost_job_alloc(struct nvhost_channel *ch,
		int num_cmdbufs, int num_relocs, int num_waitchks,
		int num_syncpts);
/* Decrement reference job, free if goes to zero. */
void nvhost_job_put(struct nvhost_job *job);

/* Add a gather with IOVA address to job */
int nvhost_job_add_client_gather_address(struct nvhost_job *job,
		u32 num_words, u32 class_id, dma_addr_t gather_address);
int nvhost_channel_submit(struct nvhost_job *job);

/* public host1x sync-point management APIs */
u32 nvhost_get_syncpt_client_managed(struct platform_device *pdev,
				const char *syncpt_name);
void nvhost_syncpt_get_ref_ext(struct platform_device *pdev, u32 id);
const char *nvhost_syncpt_get_name(struct platform_device *dev, int id);
void nvhost_syncpt_cpu_incr_ext(struct platform_device *dev, u32 id);
int nvhost_syncpt_read_ext_check(struct platform_device *dev, u32 id, u32 *val);
int nvhost_syncpt_wait_timeout_ext(struct platform_device *dev, u32 id, u32 thresh,
	u32 timeout, u32 *value, struct timespec64 *ts);
int nvhost_syncpt_create_fence_single_ext(struct platform_device *dev,
	u32 id, u32 thresh, const char *name, int *fence_fd);
void nvhost_syncpt_set_min_eq_max_ext(struct platform_device *dev, u32 id);
int nvhost_syncpt_nb_pts_ext(struct platform_device *dev);
bool nvhost_syncpt_is_valid_pt_ext(struct platform_device *dev, u32 id);
u32 nvhost_syncpt_read_minval(struct platform_device *dev, u32 id);
void nvhost_syncpt_set_maxval(struct platform_device *dev, u32 id, u32 val);
int nvhost_syncpt_fd_get_ext(int fd, struct platform_device *pdev, u32 *id);

void nvhost_eventlib_log_task(struct platform_device *pdev,
			      u32 syncpt_id,
			      u32 syncpt_thres,
			      u64 timestamp_start,
			      u64 timestamp_end);

void nvhost_eventlib_log_submit(struct platform_device *pdev,
				u32 syncpt_id,
				u32 syncpt_thresh,
				u64 timestamp);

void nvhost_eventlib_log_fences(struct platform_device *pdev,
				u32 task_syncpt_id,
				u32 task_syncpt_thresh,
				struct nvdev_fence *fences,
				u8 num_fences,
				enum nvdev_fence_kind kind,
				u64 timestamp);

dma_addr_t nvhost_t194_get_reloc_phys_addr(dma_addr_t phys_addr,
					   u32 reloc_type);
dma_addr_t nvhost_t23x_get_reloc_phys_addr(dma_addr_t phys_addr,
					   u32 reloc_type);

/* public host1x interrupt management APIs */
int nvhost_intr_register_fast_notifier(struct platform_device *pdev,
				  u32 id, u32 thresh,
				  void (*callback)(void *, int),
				  void *private_data);

#if IS_ENABLED(CONFIG_TEGRA_GRHOST) && defined(CONFIG_DEBUG_FS)
void nvhost_debug_dump_device(struct platform_device *pdev);
#else
static inline void nvhost_debug_dump_device(struct platform_device *pdev)
{
}
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST)
const struct firmware *
nvhost_client_request_firmware(struct platform_device *dev,
	const char *fw_name, bool warn);
#else
static inline const struct firmware *
nvhost_client_request_firmware(struct platform_device *dev,
	const char *fw_name, bool warn)
{
	return NULL;
}
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_SYNC)

int nvhost_fence_foreach_pt(
	struct nvhost_fence *fence,
	int (*iter)(struct nvhost_ctrl_sync_fence_info, void *),
	void *data);

int nvhost_fence_get_pt(
	struct nvhost_fence *fence, size_t i,
	u32 *id, u32 *threshold);

struct nvhost_fence *nvhost_fence_create(
		struct platform_device *pdev,
		struct nvhost_ctrl_sync_fence_info *pts,
		u32 num_pts,
		const char *name);
int nvhost_fence_create_fd(
		struct platform_device *pdev,
		struct nvhost_ctrl_sync_fence_info *pts,
		u32 num_pts,
		const char *name,
		s32 *fence_fd);

struct nvhost_fence *nvhost_fence_get(int fd);
struct nvhost_fence *nvhost_fence_dup(struct nvhost_fence *fence);
int nvhost_fence_num_pts(struct nvhost_fence *fence);
int nvhost_fence_install(struct nvhost_fence *fence, int fence_fd);
void nvhost_fence_put(struct nvhost_fence *fence);
void nvhost_fence_wait(struct nvhost_fence *fence, u32 timeout_in_ms);

#else

static inline int nvhost_fence_foreach_pt(
	struct nvhost_fence *fence,
	int (*iter)(struct nvhost_ctrl_sync_fence_info, void *d),
	void *d)
{
	return -EOPNOTSUPP;
}

static inline struct nvhost_fence *nvhost_create_fence(
		struct platform_device *pdev,
		struct nvhost_ctrl_sync_fence_info *pts,
		u32 num_pts,
		const char *name)
{
	return ERR_PTR(-EINVAL);
}

static inline int nvhost_fence_create_fd(
		struct platform_device *pdev,
		struct nvhost_ctrl_sync_fence_info *pts,
		u32 num_pts,
		const char *name,
		s32 *fence_fd)
{
	return -EINVAL;
}

static inline struct nvhost_fence *nvhost_fence_get(int fd)
{
	return NULL;
}

static inline int nvhost_fence_num_pts(struct nvhost_fence *fence)
{
	return 0;
}

static inline void nvhost_fence_put(struct nvhost_fence *fence)
{
}

static inline void nvhost_fence_wait(struct nvhost_fence *fence, u32 timeout_in_ms)
{
}

#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_SYNC) && !defined(CONFIG_SYNC)
int nvhost_dma_fence_unpack(struct dma_fence *fence, u32 *id, u32 *threshold);
bool nvhost_dma_fence_is_waitable(struct dma_fence *fence);
#else
static inline int nvhost_dma_fence_unpack(struct dma_fence *fence, u32 *id,
					  u32 *threshold)
{
	return -EINVAL;
}
static inline bool nvhost_dma_fence_is_waitable(struct dma_fence *fence)
{
	return false;
}
#endif

#if IS_ENABLED(CONFIG_TEGRA_GRHOST_SYNC) && defined(CONFIG_SYNC)
struct sync_fence *nvhost_sync_fdget(int fd);
int nvhost_sync_num_pts(struct sync_fence *fence);
struct sync_fence *nvhost_sync_create_fence(struct platform_device *pdev,
		struct nvhost_ctrl_sync_fence_info *pts,
				u32 num_pts, const char *name);
int nvhost_sync_create_fence_fd(
		struct platform_device *pdev,
		struct nvhost_ctrl_sync_fence_info *pts,
		u32 num_pts,
		const char *name,
		s32 *fence_fd);
int nvhost_sync_fence_set_name(int fence_fd, const char *name);
u32 nvhost_sync_pt_id(struct sync_pt *__pt);
u32 nvhost_sync_pt_thresh(struct sync_pt *__pt);
struct sync_pt *nvhost_sync_pt_from_fence_index(struct sync_fence *fence,
                u32 sync_pt_index);
#else
static inline struct sync_fence *nvhost_sync_fdget(int fd)
{
	return NULL;
}

static inline int nvhost_sync_num_pts(struct sync_fence *fence)
{
	return 0;
}

static inline struct sync_fence *nvhost_sync_create_fence(struct platform_device *pdev,
		struct nvhost_ctrl_sync_fence_info *pts,
				u32 num_pts, const char *name)
{
	return ERR_PTR(-EINVAL);
}

static inline int nvhost_sync_create_fence_fd(
		struct platform_device *pdev,
		struct nvhost_ctrl_sync_fence_info *pts,
		u32 num_pts,
		const char *name,
		s32 *fence_fd)
{
	return -EINVAL;
}

static inline int nvhost_sync_fence_set_name(int fence_fd, const char *name)
{
	return -EINVAL;
}

static inline u32 nvhost_sync_pt_id(struct sync_pt *__pt)
{
	return 0;
}

static inline u32 nvhost_sync_pt_thresh(struct sync_pt *__pt)
{
	return 0;
}

static inline struct sync_pt *nvhost_sync_pt_from_fence_index(
                struct sync_fence *fence, u32 sync_pt_index)
{
	return NULL;
}
#endif

/* Hacky way to get access to struct nvhost_device_data for VI device. */
extern struct nvhost_device_data t20_vi_info;
extern struct nvhost_device_data t30_vi_info;
extern struct nvhost_device_data t11_vi_info;
extern struct nvhost_device_data t14_vi_info;

int nvdec_do_idle(void);
int nvdec_do_unidle(void);

#endif

#endif
