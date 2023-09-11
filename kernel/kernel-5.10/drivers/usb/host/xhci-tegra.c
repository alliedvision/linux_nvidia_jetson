// SPDX-License-Identifier: GPL-2.0
/*
 * NVIDIA Tegra xHCI host controller driver
 *
 * Copyright (c) 2014-2022, NVIDIA CORPORATION. All rights reserved.
 * Copyright (C) 2014 Google, Inc.
 */

#include <linux/clk.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <linux/interrupt.h>
#include <linux/iopoll.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/phy/phy.h>
#include <linux/phy/tegra/xusb.h>
#include <linux/platform_device.h>
#include <linux/usb/ch9.h>
#include <linux/pm.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/regulator/consumer.h>
#include <linux/reset.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/tegra-firmwares.h>
#include <linux/tegra-ivc.h>
#include <linux/usb/otg.h>
#include <linux/usb/phy.h>
#include <linux/usb/role.h>
#include <soc/tegra/pmc.h>

#include <linux/platform/tegra/emc_bwmgr.h>
#include <linux/platform/tegra/bwmgr_mc.h>
#include <soc/tegra/fuse.h>

#include "xhci.h"

static bool en_hcd_reinit;
module_param(en_hcd_reinit, bool, 0644);
MODULE_PARM_DESC(en_hcd_reinit, "Enable hcd reinit when hc died");
static void xhci_reinit_work(struct work_struct *work);
static int tegra_xhci_hcd_reinit(struct usb_hcd *hcd);

static bool max_burst_war_enable = true;
module_param(max_burst_war_enable, bool, 0644);
MODULE_PARM_DESC(max_burst_war_enable, "Max burst WAR");

#define TEGRA_XHCI_SS_HIGH_SPEED 120000000
#define TEGRA_XHCI_SS_LOW_SPEED   12000000

/* FPCI CFG registers */
#define XUSB_CFG_1				0x004
#define  XUSB_IO_SPACE_EN			BIT(0)
#define  XUSB_MEM_SPACE_EN			BIT(1)
#define  XUSB_BUS_MASTER_EN			BIT(2)
#define XUSB_CFG_4				0x010
#define  XUSB_BASE_ADDR_SHIFT			15
#define  XUSB_BASE_ADDR_MASK			0x1ffff
#define XUSB_CFG_7				0x01c
#define  XUSB_BASE2_ADDR_SHIFT			16
#define  XUSB_BASE2_ADDR_MASK			0xffff
#define XUSB_CFG_16				0x040
#define XUSB_CFG_24				0x060
#define XUSB_CFG_AXI_CFG			0x0f8
#define XUSB_CFG_ARU_C11PAGESEL			0x404
#define  XUSB_HSP0				BIT(12)
#define XUSB_CFG_ARU_C11_CSBRANGE		0x41c
#define XUSB_CFG_ARU_CONTEXT			0x43c
#define XUSB_CFG_ARU_CONTEXT_HS_PLS		0x478
#define XUSB_CFG_ARU_CONTEXT_FS_PLS		0x47c
#define XUSB_CFG_ARU_CONTEXT_HSFS_SPEED		0x480
#define XUSB_CFG_ARU_CONTEXT_HSFS_PP		0x484
#define XUSB_CFG_ARU_FW_SCRATCH                 0x440
#define XUSB_CFG_HSPX_CORE_CTRL			0x600
#define  XUSB_HSIC_PLLCLK_VLD			BIT(24)
#define XUSB_CFG_CSB_BASE_ADDR			0x800

/* FPCI mailbox registers */
/* XUSB_CFG_ARU_MBOX_CMD */
#define  MBOX_DEST_FALC				BIT(27)
#define  MBOX_DEST_PME				BIT(28)
#define  MBOX_DEST_SMI				BIT(29)
#define  MBOX_DEST_XHCI				BIT(30)
#define  MBOX_INT_EN				BIT(31)
/* XUSB_CFG_ARU_MBOX_DATA_IN and XUSB_CFG_ARU_MBOX_DATA_OUT */
#define  CMD_DATA_SHIFT				0
#define  CMD_DATA_MASK				0xffffff
#define  CMD_TYPE_SHIFT				24
#define  CMD_TYPE_MASK				0xff
/* XUSB_CFG_ARU_MBOX_OWNER */
#define  MBOX_OWNER_NONE			0
#define  MBOX_OWNER_FW				1
#define  MBOX_OWNER_SW				2
#define XUSB_CFG_ARU_SMI_INTR			0x428
#define  MBOX_SMI_INTR_FW_HANG			BIT(1)
#define  MBOX_SMI_INTR_EN			BIT(3)
#define  MBOX_SMI_INTR_HCRST			BIT(4)

/* BAR2 registers */
#define XUSB_BAR2_ARU_MBOX_CMD			0x004
#define XUSB_BAR2_ARU_MBOX_DATA_IN		0x008
#define XUSB_BAR2_ARU_MBOX_DATA_OUT		0x00c
#define XUSB_BAR2_ARU_MBOX_OWNER		0x010
#define XUSB_BAR2_ARU_SMI_INTR			0x014
#define XUSB_BAR2_ARU_SMI_ARU_FW_SCRATCH_DATA0	0x01c
#define XUSB_BAR2_ARU_IFRDMA_CFG0		0x0e0
#define XUSB_BAR2_ARU_IFRDMA_CFG1		0x0e4
#define XUSB_BAR2_ARU_IFRDMA_STREAMID_FIELD	0x0e8
#define XUSB_BAR2_ARU_C11_CSBRANGE		0x9c
#define XUSB_BAR2_ARU_FW_SCRATCH		0x1000
#define XUSB_BAR2_CSB_BASE_ADDR			0x2000

/* IPFS registers */
#define IPFS_XUSB_HOST_MSI_BAR_SZ_0		0x0c0
#define IPFS_XUSB_HOST_MSI_AXI_BAR_ST_0		0x0c4
#define IPFS_XUSB_HOST_MSI_FPCI_BAR_ST_0	0x0c8
#define IPFS_XUSB_HOST_MSI_VEC0_0		0x100
#define IPFS_XUSB_HOST_MSI_EN_VEC0_0		0x140
#define IPFS_XUSB_HOST_CONFIGURATION_0		0x180
#define  IPFS_EN_FPCI				BIT(0)
#define IPFS_XUSB_HOST_FPCI_ERROR_MASKS_0	0x184
#define IPFS_XUSB_HOST_INTR_MASK_0		0x188
#define  IPFS_IP_INT_MASK			BIT(16)
#define IPFS_XUSB_HOST_INTR_ENABLE_0		0x198
#define IPFS_XUSB_HOST_UFPCI_CONFIG_0		0x19c
#define IPFS_XUSB_HOST_CLKGATE_HYSTERESIS_0	0x1bc
#define IPFS_XUSB_HOST_MCCIF_FIFOCTRL_0		0x1dc

#define CSB_PAGE_SELECT_MASK			0x7fffff
#define CSB_PAGE_SELECT_SHIFT			9
#define CSB_PAGE_OFFSET_MASK			0x1ff
#define CSB_PAGE_SELECT(addr)	((addr) >> (CSB_PAGE_SELECT_SHIFT) &	\
				 CSB_PAGE_SELECT_MASK)
#define CSB_PAGE_OFFSET(addr)	((addr) & CSB_PAGE_OFFSET_MASK)

/* Falcon CSB registers */
#define XUSB_FALC_CPUCTL			0x100
#define  CPUCTL_STARTCPU			BIT(1)
#define  CPUCTL_STATE_HALTED			BIT(4)
#define  CPUCTL_STATE_STOPPED			BIT(5)
#define XUSB_FALC_BOOTVEC			0x104
#define XUSB_FALC_DMACTL			0x10c
#define XUSB_FALC_IMFILLRNG1			0x154
#define  IMFILLRNG1_TAG_MASK			0xffff
#define  IMFILLRNG1_TAG_LO_SHIFT		0
#define  IMFILLRNG1_TAG_HI_SHIFT		16
#define XUSB_FALC_IMFILLCTL			0x158

/* CSB ARU  registers */
#define XUSB_CSB_ARU_SCRATCH0			0x100100
#define XUSB_CSB_ARU_SCRATCH1			0x100104

/* MP CSB registers */
#define XUSB_CSB_MP_ILOAD_ATTR			0x101a00
#define XUSB_CSB_MP_ILOAD_BASE_LO		0x101a04
#define XUSB_CSB_MP_ILOAD_BASE_HI		0x101a08
#define XUSB_CSB_MP_L2IMEMOP_SIZE		0x101a10
#define  L2IMEMOP_SIZE_SRC_OFFSET_SHIFT		8
#define  L2IMEMOP_SIZE_SRC_OFFSET_MASK		0x3ff
#define  L2IMEMOP_SIZE_SRC_COUNT_SHIFT		24
#define  L2IMEMOP_SIZE_SRC_COUNT_MASK		0xff
#define XUSB_CSB_MP_L2IMEMOP_TRIG		0x101a14
#define  L2IMEMOP_ACTION_SHIFT			24
#define  L2IMEMOP_INVALIDATE_ALL		(0x40 << L2IMEMOP_ACTION_SHIFT)
#define  L2IMEMOP_LOAD_LOCKED_RESULT		(0x11 << L2IMEMOP_ACTION_SHIFT)
#define XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT	0x101a18
#define  L2IMEMOP_RESULT_VLD			BIT(31)
#define XUSB_CSB_MP_APMAP			0x10181c
#define  APMAP_BOOTPATH				BIT(31)

#define IMEM_BLOCK_SIZE				256

/* Device ID */
#define XHCI_DEVICE_ID_T210 0x0fad

#define XHCI_IS_T210(t) (t->soc ? \
		(t->soc->device_id == XHCI_DEVICE_ID_T210) : false)

#define FW_IOCTL_LOG_BUFFER_LEN         (2)
#define FW_IOCTL_LOG_DEQUEUE_LOW        (4)
#define FW_IOCTL_LOG_DEQUEUE_HIGH       (5)
#define FW_IOCTL_CFGTBL_READ		(17)
#define FW_IOCTL_INIT_LOG_BUF		(31)
#define FW_IOCTL_LOG_DEQUEUE_IDX	(32)
#define FW_IOCTL_DATA_SHIFT             (0)
#define FW_IOCTL_DATA_MASK              (0x00ffffff)
#define FW_IOCTL_TYPE_SHIFT             (24)
#define FW_IOCTL_TYPE_MASK              (0xff000000)
#define FW_LOG_SIZE                     ((int) sizeof(struct log_entry))
#define FW_LOG_COUNT                    (4096)
#define FW_LOG_RING_SIZE                (FW_LOG_SIZE * FW_LOG_COUNT)
#define FW_LOG_PAYLOAD_SIZE             (27)
#define DRIVER                          (0x01)
#define CIRC_BUF_SIZE                   (4 * (1 << 20)) /* 4MB */
#define FW_LOG_THREAD_RELAX             (msecs_to_jiffies(500))

/* tegra_xhci_firmware_log.flags bits */
#define FW_LOG_CONTEXT_VALID            (0)
#define FW_LOG_FILE_OPENED              (1)

#define FW_MAJOR_VERSION(x)             (((x) >> 24) & 0xff)
#define FW_MINOR_VERSION(x)             (((x) >> 16) & 0xff)

#define EMC_RESTORE_DELAY       msecs_to_jiffies(2*1000) /* 2 sec */

enum build_info_log {
	LOG_NONE = 0,
	LOG_MEMORY
};

/* device quirks */
#define QUIRK_FOR_SS_DEVICE				BIT(0)
#define QUIRK_FOR_HS_DEVICE				BIT(1)
#define QUIRK_FOR_FS_DEVICE				BIT(2)
#define QUIRK_FOR_LS_DEVICE				BIT(3)
#define QUIRK_FOR_USB2_DEVICE \
	(QUIRK_FOR_HS_DEVICE | QUIRK_FOR_FS_DEVICE | QUIRK_FOR_LS_DEVICE)

#define USB_DEVICE_USB3(vid, pid) \
	USB_DEVICE(vid, pid), \
	.driver_info = (QUIRK_FOR_USB2_DEVICE | QUIRK_FOR_SS_DEVICE),

#define USB_DEVICE_USB2(vid, pid) \
	USB_DEVICE(vid, pid), \
	.driver_info = QUIRK_FOR_USB2_DEVICE,

#define USB_DEVICE_SS(vid, pid) \
	USB_DEVICE(vid, pid), \
	.driver_info = QUIRK_FOR_SS_DEVICE,

#define USB_DEVICE_HS(vid, pid) \
	USB_DEVICE(vid, pid), \
	.driver_info = QUIRK_FOR_HS_DEVICE,

#define USB_DEVICE_FS(vid, pid) \
	USB_DEVICE(vid, pid), \
	.driver_info = QUIRK_FOR_FS_DEVICE,

#define USB_DEVICE_LS(vid, pid) \
	USB_DEVICE(vid, pid), \
	.driver_info = QUIRK_FOR_LS_DEVICE,

#define PORT_WAKE_BITS (PORT_WKOC_E | PORT_WKDISC_E | PORT_WKCONN_E)

static struct usb_device_id disable_usb_persist_quirk_list[] = {
	/* Sandisk Extreme USB 3.0 pen drive, SuperSpeed */
	{ USB_DEVICE_SS(0x0781, 0x5580) },
	{ }  /* terminating entry must be last */
};

static int usb_match_speed(struct usb_device *udev,
			    const struct usb_device_id *id)
{
	if (!id)
		return 0;

	if ((id->driver_info & QUIRK_FOR_SS_DEVICE) &&
					udev->speed == USB_SPEED_SUPER)
		return 1;

	if ((id->driver_info & QUIRK_FOR_HS_DEVICE) &&
					udev->speed == USB_SPEED_HIGH)
		return 1;

	if ((id->driver_info & QUIRK_FOR_FS_DEVICE) &&
					udev->speed == USB_SPEED_FULL)
		return 1;

	if ((id->driver_info & QUIRK_FOR_LS_DEVICE) &&
					udev->speed == USB_SPEED_LOW)
		return 1;

	return 0;
}

struct tegra_xusb_fw_header {
	__le32 boot_loadaddr_in_imem;
	__le32 boot_codedfi_offset;
	__le32 boot_codetag;
	__le32 boot_codesize;
	__le32 phys_memaddr;
	__le16 reqphys_memsize;
	__le16 alloc_phys_memsize;
	__le32 rodata_img_offset;
	__le32 rodata_section_start;
	__le32 rodata_section_end;
	__le32 main_fnaddr;
	__le32 fwimg_cksum;
	__le32 fwimg_created_time;
	__le32 imem_resident_start;
	__le32 imem_resident_end;
	__le32 idirect_start;
	__le32 idirect_end;
	__le32 l2_imem_start;
	__le32 l2_imem_end;
	__le32 version_id;
	u8 init_ddirect;
	u8 reserved[3];
	__le32 phys_addr_log_buffer;
	__le32 total_log_entries;
	__le32 dequeue_ptr;
	__le32 dummy_var[2];
	__le32 fwimg_len;
	u8 magic[8];
	__le32 ss_low_power_entry_timeout;
	u8 num_hsic_port;
	u8 ss_portmap;
	u8 build_log:4;
	u8 build_type:4;
	u8 padding[137]; /* Pad to 256 bytes */
};

struct tegra_xusb_phy_type {
	const char *name;
	unsigned int num;
};

struct tegra_xusb_mbox_regs {
	u16 cmd;
	u16 data_in;
	u16 data_out;
	u16 owner;
	u16 smi_intr;
};

struct tegra_xusb_context_soc {
	struct {
		const unsigned int *offsets;
		unsigned int num_offsets;
	} ipfs;

	struct {
		const unsigned int *offsets;
		unsigned int num_offsets;
	} fpci;
};

enum tegra_xhci_phy_type {
	USB3_PHY,
	USB2_PHY,
	HSIC_PHY,
	MAX_PHY_TYPES,
};

struct tegra_xusb_soc_ops;
struct tegra_xusb_soc {
	u16 device_id;
	const char *firmware;
	const char * const *supply_names;
	unsigned int num_supplies;
	const struct tegra_xusb_phy_type *phy_types;
	unsigned int num_types;
	unsigned int num_wakes;
	const struct tegra_xusb_context_soc *context;

	struct {
		struct {
			unsigned int offset;
			unsigned int count;
		} usb2, ulpi, hsic, usb3;
	} ports;

	struct tegra_xusb_mbox_regs mbox;
	struct tegra_xusb_soc_ops *ops;

	bool scale_ss_clock;
	bool has_ipfs;
	bool lpm_support;
	bool otg_reset_sspi;
	bool disable_hsic_wake;
	bool disable_u0_ts1_detect;
	bool is_xhci_vf;
	u8 vf_id;

	bool has_bar2;
	bool has_ifr;
	bool load_ifr_rom;
};

struct tegra_xusb_context {
	u32 *ipfs;
	u32 *fpci;
};

struct log_entry {
	u32 sequence_no;
	u8 data[FW_LOG_PAYLOAD_SIZE];
	u8 owner;
};

struct tegra_xhci_firmware_log {
	dma_addr_t phys_addr;           /* dma-able address */
	void *virt_addr;                /* kernel va of the shared log buffer */
	struct log_entry *dequeue;      /* current dequeue pointer (va) */
	struct circ_buf circ;           /* big circular buffer */
	u32 seq;                        /* log sequence number */

	struct task_struct *thread;     /* a thread to consume log */
	struct mutex mutex;
	wait_queue_head_t read_wait;
	wait_queue_head_t write_wait;
	wait_queue_head_t intr_wait;
	struct dentry *log_file;
	unsigned long flags;
};

struct tegra_xusb {
	struct device *dev;
	void __iomem *regs;
	struct usb_hcd *hcd;

	struct mutex lock;

	int xhci_irq;
	int mbox_irq;
	int padctl_irq;
	int *wake_irqs;

	void __iomem *ipfs_base;
	void __iomem *fpci_base;
	resource_size_t fpci_start;
	resource_size_t fpci_len;
	void __iomem *bar2_base;
	resource_size_t bar2_start;
	resource_size_t bar2_len;

	const struct tegra_xusb_soc *soc;

	struct regulator_bulk_data *supplies;

	struct tegra_xusb_padctl *padctl;

	struct clk *host_clk;
	struct clk *falcon_clk;
	struct clk *ss_clk;
	struct clk *ss_src_clk;
	struct clk *hs_src_clk;
	struct clk *fs_src_clk;
	struct clk *pll_u_480m;
	struct clk *clk_m;
	struct clk *pll_e;
	bool clk_enabled;

	struct reset_control *host_rst;
	struct reset_control *ss_rst;

	struct device *genpd_dev_host;
	struct device *genpd_dev_ss;
	bool use_genpd;

	struct phy **phys;
	unsigned int num_phys;

	struct usb_phy **usbphy;
	unsigned int num_usb_phys;
	int otg_usb2_port;
	int otg_usb3_port;
	bool host_mode;
	struct notifier_block id_nb;
	struct work_struct id_work;

	struct notifier_block genpd_nb;

	/* Firmware loading related */
	struct {
		size_t size;
		void *virt;
		dma_addr_t phys;
	} fw;

	u8 build_log;
	time64_t timestamp;
	u32 version_id;

	struct dentry *debugfs_dir;
	struct dentry *dump_ring_file;
	struct tegra_xhci_firmware_log log;

	bool suspended;
	struct tegra_xusb_context context;
	u32 enable_utmi_pad_after_lp0_exit;

	struct tegra_bwmgr_client *bwmgr; /* bandwidth manager handle */
	struct work_struct boost_emcfreq_work;
	struct delayed_work restore_emcfreq_work;
	unsigned int boost_emc_freq;
	unsigned long emcfreq_last_boosted;
	bool emc_boost_enabled;
	bool emc_boosted;
	bool restore_emc_work_scheduled;
	struct device *fwdev;
	struct tegra_hv_ivc_cookie *ivck;
	char ivc_rx[128]; /* Buffer to receive pad ivc message */
	struct work_struct ivc_work;
	bool enable_wake;
};

struct tegra_xusb_soc_ops {
	u32 (*mbox_reg_readl)(struct tegra_xusb *tegra,
			unsigned int offset);
	void (*mbox_reg_writel)(struct tegra_xusb *tegra,
			u32 value, unsigned int offset);
	u32 (*csb_reg_readl)(struct tegra_xusb *tegra,
			unsigned int offset);
	void (*csb_reg_writel)(struct tegra_xusb *tegra,
			u32 value, unsigned int offset);
};

static struct hc_driver __read_mostly tegra_xhci_hc_driver;

static int fpga_clock_hacks(struct platform_device *pdev)
{
#define CLK_RST_CONTROLLER_RST_DEV_XUSB_0	(0x470000)
#define   SWR_XUSB_HOST_RST			(1 << 0)
#define   SWR_XUSB_DEV_RST			(1 << 1)
#define   SWR_XUSB_PADCTL_RST			(1 << 2)
#define   SWR_XUSB_SS_RST			(1 << 3)
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_XUSB_0	(0x471000)
#define   CLK_ENB_XUSB				(1 << 0)
#define   CLK_ENB_XUSB_DEV			(1 << 1)
#define   CLK_ENB_XUSB_HOST			(1 << 2)
#define   CLK_ENB_XUSB_SS			(1 << 3)
#define CLK_RST_CONTROLLER_CLK_OUT_ENB_XUSB_SET_0	(0x471004)
#define   SET_CLK_ENB_XUSB			(1 << 0)
#define   SET_CLK_ENB_XUSB_DEV			(1 << 1)
#define   SET_CLK_ENB_XUSB_HOST			(1 << 2)
#define   SET_CLK_ENB_XUSB_SS			(1 << 3)

	static void __iomem *car_base;
	u32 val;

	car_base = devm_ioremap(&pdev->dev, 0x20000000, 0x1000000);
	if (IS_ERR(car_base)) {
		dev_err(&pdev->dev, "failed to map CAR mmio\n");
		return PTR_ERR(car_base);
	}

	val = CLK_ENB_XUSB | CLK_ENB_XUSB_DEV | CLK_ENB_XUSB_HOST |
		CLK_ENB_XUSB_SS;
	iowrite32(val, car_base + CLK_RST_CONTROLLER_CLK_OUT_ENB_XUSB_0);

	val = ioread32(car_base + CLK_RST_CONTROLLER_RST_DEV_XUSB_0);
	val &= ~(SWR_XUSB_HOST_RST | SWR_XUSB_DEV_RST |
			SWR_XUSB_PADCTL_RST | SWR_XUSB_SS_RST);
	iowrite32(val, car_base + CLK_RST_CONTROLLER_RST_DEV_XUSB_0);

	val = SET_CLK_ENB_XUSB | SET_CLK_ENB_XUSB_DEV | SET_CLK_ENB_XUSB_HOST |
		SET_CLK_ENB_XUSB_SS;
	iowrite32(val, car_base + CLK_RST_CONTROLLER_CLK_OUT_ENB_XUSB_SET_0);

	devm_iounmap(&pdev->dev, car_base);

	return 0;
}

static inline struct tegra_xusb *hcd_to_tegra_xusb(struct usb_hcd *hcd)
{
	return (struct tegra_xusb *) dev_get_drvdata(hcd->self.controller);
}

static void tegra_xusb_parse_dt(struct platform_device *pdev,
				struct tegra_xusb *tegra)
{
	struct device_node *node = pdev->dev.of_node;

	of_property_read_u32(node, "nvidia,boost_emc_freq",
					&tegra->boost_emc_freq);
}

static void tegra_xusb_boost_emc_freq_fn(struct work_struct *work)
{
	struct tegra_xusb *tegra = container_of(work, struct tegra_xusb,
				boost_emcfreq_work);
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	unsigned long flags;
	int err = 0;

	spin_lock_irqsave(&xhci->lock, flags);
	if (tegra->bwmgr && !tegra->emc_boosted) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		dev_dbg(tegra->dev, "boost EMC freq %d MHz\n",
				tegra->boost_emc_freq);
		err = tegra_bwmgr_set_emc(tegra->bwmgr,
				tegra->boost_emc_freq * 1000000,
				TEGRA_BWMGR_SET_EMC_SHARED_BW);
		if (err)
			dev_warn(tegra->dev, "failed to boost EMC freq %d MHz, err=%d\n",
				tegra->boost_emc_freq, err);
		spin_lock_irqsave(&xhci->lock, flags);
		tegra->emc_boosted = true;
	}

	if (!tegra->restore_emc_work_scheduled) {
		schedule_delayed_work(&tegra->restore_emcfreq_work,
			EMC_RESTORE_DELAY);
		tegra->restore_emc_work_scheduled = true;
	}

	tegra->emcfreq_last_boosted = jiffies;
	spin_unlock_irqrestore(&xhci->lock, flags);
}

static void tegra_xusb_restore_emc_freq_fn(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct tegra_xusb *tegra = container_of(dwork, struct tegra_xusb,
				restore_emcfreq_work);
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	unsigned long flags;

	if (time_is_after_jiffies(
			tegra->emcfreq_last_boosted + EMC_RESTORE_DELAY)) {
		dev_dbg(tegra->dev, "schedule restore EMC work\n");
		schedule_delayed_work(&tegra->restore_emcfreq_work,
			EMC_RESTORE_DELAY);
		return;
	}

	spin_lock_irqsave(&xhci->lock, flags);
	if (tegra->bwmgr && tegra->emc_boosted) {
		spin_unlock_irqrestore(&xhci->lock, flags);
		tegra_bwmgr_set_emc(tegra->bwmgr, 0,
			TEGRA_BWMGR_SET_EMC_SHARED_BW);
		dev_dbg(tegra->dev, "restore EMC freq\n");
		spin_lock_irqsave(&xhci->lock, flags);
		tegra->emc_boosted = false;
		tegra->restore_emc_work_scheduled = false;
	}

	spin_unlock_irqrestore(&xhci->lock, flags);
}

static void tegra_xusb_boost_emc_init(struct tegra_xusb *tegra)
{
	int err = 0;

	tegra->bwmgr = tegra_bwmgr_register(TEGRA_BWMGR_CLIENT_XHCI);
	if (IS_ERR_OR_NULL(tegra->bwmgr)) {
		err = IS_ERR(tegra->bwmgr) ?
				PTR_ERR(tegra->bwmgr) : -ENODEV;
		dev_err(tegra->dev, "can't register EMC bwmgr (%d)\n", err);
		tegra->emc_boost_enabled = false;
		return;
	}

	tegra->emc_boosted = false;
	tegra->restore_emc_work_scheduled = false;

	INIT_WORK(&tegra->boost_emcfreq_work, tegra_xusb_boost_emc_freq_fn);
	INIT_DELAYED_WORK(&tegra->restore_emcfreq_work,
		tegra_xusb_restore_emc_freq_fn);
}

static void tegra_xusb_boost_emc_deinit(struct tegra_xusb *tegra)
{
	if (IS_ERR_OR_NULL(tegra->bwmgr))
		return;

	tegra_bwmgr_set_emc(tegra->bwmgr, 0,
		TEGRA_BWMGR_SET_EMC_SHARED_BW);
	tegra_bwmgr_unregister(tegra->bwmgr);

	cancel_work_sync(&tegra->boost_emcfreq_work);
	cancel_delayed_work_sync(&tegra->restore_emcfreq_work);
}

static bool xhci_err_init;
static ssize_t show_xhci_stats(struct device *dev,
		struct device_attribute *attr, char *buf) {
	struct tegra_xusb *tegra = NULL;
	struct xhci_hcd *xhci = NULL;
	ssize_t ret;

	if (dev != NULL)
		tegra = dev_get_drvdata(dev);

	if (tegra != NULL) {
		xhci = hcd_to_xhci(tegra->hcd);
		ret =  snprintf(buf, PAGE_SIZE, "comp_tx_err:%u\nversion:%u\n",
			xhci->xhci_ereport.comp_tx_err,
			xhci->xhci_ereport.version);
	} else
		ret = snprintf(buf, PAGE_SIZE, "comp_tx_err:0\nversion:0\n");

	return ret;
}

static DEVICE_ATTR(xhci_stats, 0444, show_xhci_stats, NULL);

static struct attribute *tegra_sysfs_entries_errs[] = {
	&dev_attr_xhci_stats.attr,
	NULL,
};

static struct attribute_group tegra_sysfs_group_errors = {
	.name = "xhci-stats",
	.attrs = tegra_sysfs_entries_errs,
};

static inline u32 fpci_readl(struct tegra_xusb *tegra, unsigned int offset)
{
	return readl(tegra->fpci_base + offset);
}

static inline void fpci_writel(struct tegra_xusb *tegra, u32 value,
			       unsigned int offset)
{
	writel(value, tegra->fpci_base + offset);
}

static inline u32 ipfs_readl(struct tegra_xusb *tegra, unsigned int offset)
{
	return readl(tegra->ipfs_base + offset);
}

static inline void ipfs_writel(struct tegra_xusb *tegra, u32 value,
			       unsigned int offset)
{
	writel(value, tegra->ipfs_base + offset);
}

static inline u32 bar2_readl(struct tegra_xusb *tegra, unsigned int offset)
{
	return readl(tegra->bar2_base + offset);
}

static inline void bar2_writel(struct tegra_xusb *tegra, u32 value,
			       unsigned int offset)
{
	writel(value, tegra->bar2_base + offset);
}

static u32 csb_readl(struct tegra_xusb *tegra, unsigned int offset)
{
	struct tegra_xusb_soc_ops *ops = tegra->soc->ops;

	return ops->csb_reg_readl(tegra, offset);
}

static void csb_writel(struct tegra_xusb *tegra, u32 value,
		       unsigned int offset)
{
	struct tegra_xusb_soc_ops *ops = tegra->soc->ops;

	ops->csb_reg_writel(tegra, value, offset);
}

static u32 fpci_csb_readl(struct tegra_xusb *tegra, unsigned int offset)
{
	u32 page = CSB_PAGE_SELECT(offset);
	u32 ofs = CSB_PAGE_OFFSET(offset);

	fpci_writel(tegra, page, XUSB_CFG_ARU_C11_CSBRANGE);

	return fpci_readl(tegra, XUSB_CFG_CSB_BASE_ADDR + ofs);
}

static void fpci_csb_writel(struct tegra_xusb *tegra, u32 value,
		       unsigned int offset)
{
	u32 page = CSB_PAGE_SELECT(offset);
	u32 ofs = CSB_PAGE_OFFSET(offset);

	fpci_writel(tegra, page, XUSB_CFG_ARU_C11_CSBRANGE);
	fpci_writel(tegra, value, XUSB_CFG_CSB_BASE_ADDR + ofs);
}

static u32 bar2_csb_readl(struct tegra_xusb *tegra, unsigned int offset)
{
	u32 page = CSB_PAGE_SELECT(offset);
	u32 ofs = CSB_PAGE_OFFSET(offset);

	bar2_writel(tegra, page, XUSB_BAR2_ARU_C11_CSBRANGE);

	return bar2_readl(tegra, XUSB_BAR2_CSB_BASE_ADDR + ofs);
}

static void bar2_csb_writel(struct tegra_xusb *tegra, u32 value,
		       unsigned int offset)
{
	u32 page = CSB_PAGE_SELECT(offset);
	u32 ofs = CSB_PAGE_OFFSET(offset);

	bar2_writel(tegra, page, XUSB_BAR2_ARU_C11_CSBRANGE);
	bar2_writel(tegra, value, XUSB_BAR2_CSB_BASE_ADDR + ofs);
}

/**
 * fw_log_next - find next log entry in a tegra_xhci_firmware_log context.
 *      This function takes care of wrapping. That means when current log entry
 *      is the last one, it returns with the first one.
 *
 * @param log   The tegra_xhci_firmware_log context.
 * @param this  The current log entry.
 * @return      The log entry which is next to the current one.
 */
static inline struct log_entry *fw_log_next(
		struct tegra_xhci_firmware_log *log, struct log_entry *this)
{
	struct log_entry *first = (struct log_entry *) log->virt_addr;
	struct log_entry *last = first + FW_LOG_COUNT - 1;

	WARN((this < first) || (this > last), "%s: invalid input\n", __func__);

	return (this == last) ? first : (this + 1);
}

/**
 * fw_log_update_dequeue_pointer - update dequeue pointer to both firmware and
 *      tegra_xhci_firmware_log.dequeue.
 *
 * @param log   The tegra_xhci_firmware_log context.
 * @param n     Counts of log entries to fast-forward.
 */
static inline void fw_log_update_deq_pointer(
		struct tegra_xhci_firmware_log *log, int n)
{
	struct tegra_xusb *tegra =
		container_of(log, struct tegra_xusb, log);
	struct device *dev = tegra->dev;
	struct log_entry *deq = tegra->log.dequeue;
	dma_addr_t physical_addr;
	u16 log_index;
	u32 reg;

	dev_vdbg(dev, "curr 0x%p fast-forward %d entries\n", deq, n);
	while (n-- > 0)
		deq = fw_log_next(log, deq);

	tegra->log.dequeue = deq;
	physical_addr = tegra->log.phys_addr +
		((u8 *)deq - (u8 *)tegra->log.virt_addr);
	log_index = (u16)((u8 *)deq - (u8 *)tegra->log.virt_addr) /
			   sizeof(struct log_entry);

	if (tegra->soc->has_ifr) {
		/* update 16 bit log index to firmware */
		reg = (FW_IOCTL_LOG_DEQUEUE_IDX << FW_IOCTL_TYPE_SHIFT);
		reg |= (log_index & 0xffff);
		if (tegra->soc->has_bar2)
			bar2_writel(tegra, reg, XUSB_BAR2_ARU_FW_SCRATCH);
		else
			fpci_writel(tegra, reg, XUSB_CFG_ARU_FW_SCRATCH);

		dev_vdbg(dev, "new 0x%p log_index 0x%x\n", deq, (u32)log_index);
	} else {
		/* update dequeue pointer to firmware */
		reg = (FW_IOCTL_LOG_DEQUEUE_LOW << FW_IOCTL_TYPE_SHIFT);
		reg |= (physical_addr & 0xffff); /* lower 16-bits */
		if (tegra->soc->has_bar2)
			bar2_writel(tegra, reg, XUSB_BAR2_ARU_FW_SCRATCH);
		else
			fpci_writel(tegra, reg, XUSB_CFG_ARU_FW_SCRATCH);

		reg = (FW_IOCTL_LOG_DEQUEUE_HIGH << FW_IOCTL_TYPE_SHIFT);
		reg |= ((physical_addr >> 16) & 0xffff); /* higher 16-bits */
		if (tegra->soc->has_bar2)
			bar2_writel(tegra, reg, XUSB_BAR2_ARU_FW_SCRATCH);
		else
			fpci_writel(tegra, reg, XUSB_CFG_ARU_FW_SCRATCH);

		dev_vdbg(dev, "new 0x%p physical addr 0x%x\n", deq, (u32)physical_addr);
	}

}

static inline bool circ_buffer_full(struct circ_buf *circ)
{
	int space = CIRC_SPACE(circ->head, circ->tail, CIRC_BUF_SIZE);

	return (space <= FW_LOG_SIZE);
}

static inline bool fw_log_available(struct tegra_xusb *tegra)
{
	return (tegra->log.dequeue->owner == DRIVER);
}

/**
 * fw_log_wait_empty_timeout - wait firmware log thread to clean up shared
 *      log buffer.
 * @param tegra:        tegra_xusb context
 * @param msec:         timeout value in millisecond
 * @return true:        shared log buffer is empty,
 *         false:       shared log buffer isn't empty.
 */
static inline bool fw_log_wait_empty_timeout(struct tegra_xusb *tegra,
		unsigned int timeout)
{
	unsigned long target = jiffies + msecs_to_jiffies(timeout);
	struct circ_buf *circ = &tegra->log.circ;
	bool ret;

	mutex_lock(&tegra->log.mutex);

	while (fw_log_available(tegra) && time_is_after_jiffies(target)) {
		if (circ_buffer_full(circ) &&
			!test_bit(FW_LOG_FILE_OPENED, &tegra->log.flags))
			break; /* buffer is full but nobody is reading log */

		mutex_unlock(&tegra->log.mutex);
		usleep_range(1000, 2000);
		mutex_lock(&tegra->log.mutex);
	}

	ret = fw_log_available(tegra);
	mutex_unlock(&tegra->log.mutex);

	return ret;
}

/**
 * fw_log_copy - copy firmware log from device's buffer to driver's circular
 *      buffer.
 * @param tegra tegra_xusb context
 * @return true,        We still have firmware log in device's buffer to copy.
 *                      This function returned due the driver's circular buffer
 *                      is full. Caller should invoke this function again as
 *                      soon as there is space in driver's circular buffer.
 *         false,       Device's buffer is empty.
 */
static inline bool fw_log_copy(struct tegra_xusb *tegra)
{
	struct device *dev = tegra->dev;
	struct circ_buf *circ = &tegra->log.circ;
	int head, tail;
	int buffer_len, copy_len;
	struct log_entry *entry;
	struct log_entry *first = tegra->log.virt_addr;

	while (fw_log_available(tegra)) {

		/* calculate maximum contiguous driver buffer length */
		head = circ->head;
		tail = READ_ONCE(circ->tail);
		buffer_len = CIRC_SPACE_TO_END(head, tail, CIRC_BUF_SIZE);
		/* round down to FW_LOG_SIZE */
		buffer_len -= (buffer_len % FW_LOG_SIZE);
		if (!buffer_len)
			return true; /* log available but no space left */

		/* calculate maximum contiguous log copy length */
		entry = tegra->log.dequeue;
		copy_len = 0;
		do {
			if (tegra->log.seq != entry->sequence_no) {
				dev_warn(dev,
					"%s: discontinuous seq no, expect %u get %u\n",
					__func__, tegra->log.seq, entry->sequence_no);
			}
			tegra->log.seq = entry->sequence_no + 1;

			copy_len += FW_LOG_SIZE;
			buffer_len -= FW_LOG_SIZE;
			if (!buffer_len)
				break; /* no space left */
			entry = fw_log_next(&tegra->log, entry);
		} while ((entry->owner == DRIVER) && (entry != first));

		memcpy(&circ->buf[head], tegra->log.dequeue, copy_len);
		memset(tegra->log.dequeue, 0, copy_len);
		circ->head = (circ->head + copy_len) & (CIRC_BUF_SIZE - 1);

		mb(); /* make sure controller sees it */

		fw_log_update_deq_pointer(&tegra->log, copy_len/FW_LOG_SIZE);

		dev_vdbg(dev, "copied %d entries, new dequeue 0x%p\n",
				copy_len/FW_LOG_SIZE, tegra->log.dequeue);
		wake_up_interruptible(&tegra->log.read_wait);
	}

	return false;
}

static int fw_log_thread(void *data)
{
	struct tegra_xusb *tegra = data;
	struct device *dev = tegra->dev;
	struct circ_buf *circ = &tegra->log.circ;
	bool logs_left;

	dev_dbg(dev, "start firmware log thread\n");

	do {
		mutex_lock(&tegra->log.mutex);
		if (circ_buffer_full(circ)) {
			mutex_unlock(&tegra->log.mutex);
			dev_info(dev, "%s: circ buffer full\n", __func__);
			wait_event_interruptible(tegra->log.write_wait,
				kthread_should_stop() ||
				!circ_buffer_full(circ));
			mutex_lock(&tegra->log.mutex);
		}

		logs_left = fw_log_copy(tegra);
		mutex_unlock(&tegra->log.mutex);

		/* relax if no logs left  */
		if (!logs_left)
			wait_event_interruptible_timeout(tegra->log.intr_wait,
				fw_log_available(tegra), FW_LOG_THREAD_RELAX);
	} while (!kthread_should_stop());

	dev_dbg(dev, "stop firmware log thread\n");
	return 0;
}

static inline bool circ_buffer_empty(struct circ_buf *circ)
{
	return (CIRC_CNT(circ->head, circ->tail, CIRC_BUF_SIZE) == 0);
}

static ssize_t fw_log_file_read(struct file *file, char __user *buf,
		size_t count, loff_t *offp)
{
	struct tegra_xusb *tegra = file->private_data;
	struct device *dev = tegra->dev;
	struct circ_buf *circ = &tegra->log.circ;
	int head, tail;
	size_t n = 0;
	int s;

	mutex_lock(&tegra->log.mutex);

	while (circ_buffer_empty(circ)) {
		mutex_unlock(&tegra->log.mutex);
		if (file->f_flags & O_NONBLOCK)
			return -EAGAIN; /* non-blocking read */

		dev_dbg(dev, "%s: nothing to read\n", __func__);

		if (wait_event_interruptible(tegra->log.read_wait,
				!circ_buffer_empty(circ)))
			return -ERESTARTSYS;

		if (mutex_lock_interruptible(&tegra->log.mutex))
			return -ERESTARTSYS;
	}

	while (count > 0) {
		head = READ_ONCE(circ->head);
		tail = circ->tail;
		s = min_t(int, count,
				CIRC_CNT_TO_END(head, tail, CIRC_BUF_SIZE));

		if (s > 0) {
			if (copy_to_user(&buf[n], &circ->buf[tail], s)) {
				dev_warn(dev, "copy_to_user failed\n");
				mutex_unlock(&tegra->log.mutex);
				return -EFAULT;
			}
			circ->tail = (circ->tail + s) & (CIRC_BUF_SIZE - 1);

			count -= s;
			n += s;
		} else
			break;
	}

	mutex_unlock(&tegra->log.mutex);

	wake_up_interruptible(&tegra->log.write_wait);

	dev_dbg(dev, "%s: %zu bytes\n", __func__, n);

	return n;
}

static int fw_log_file_open(struct inode *inode, struct file *file)
{
	struct tegra_xusb *tegra;

	file->private_data = inode->i_private;
	tegra = file->private_data;

	if (test_and_set_bit(FW_LOG_FILE_OPENED, &tegra->log.flags)) {
		dev_info(tegra->dev, "%s: already opened\n", __func__);
		return -EBUSY;
	}

	return 0;
}

static int fw_log_file_close(struct inode *inode, struct file *file)
{
	struct tegra_xusb *tegra = file->private_data;

	clear_bit(FW_LOG_FILE_OPENED, &tegra->log.flags);

	return 0;
}

static const struct file_operations firmware_log_fops = {
	.open           = fw_log_file_open,
	.release        = fw_log_file_close,
	.read           = fw_log_file_read,
	.owner          = THIS_MODULE,
};

#ifdef CONFIG_USB_XHCI_HCD_DEBUGGING
static ssize_t dump_ring_file_write(struct file *file, const char __user *buf,
	size_t count, loff_t *offp)
{
	struct tegra_xusb *tegra = file->private_data;
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);

	/* trigger xhci_event_ring_work() for debug */
	del_timer_sync(&xhci->event_ring_timer);
	xhci->event_ring_timer.expires = jiffies;
	add_timer(&xhci->event_ring_timer);

	return count;
}

static const struct file_operations dump_ring_fops = {
	.write          = dump_ring_file_write,
	.owner          = THIS_MODULE,
};
#endif

static int fw_log_init(struct tegra_xusb *tegra)
{
	struct device *dev = tegra->dev;
	int rc = 0;

	if (!tegra->debugfs_dir)
		return -ENODEV; /* no debugfs support */

	if (test_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags))
		return 0; /* already done */

	/* allocate buffer to be shared between driver and firmware */
	tegra->log.virt_addr = dma_alloc_coherent(dev,
			FW_LOG_RING_SIZE, &tegra->log.phys_addr, GFP_KERNEL);

	if (!tegra->log.virt_addr) {
		dev_err(dev, "dma_alloc_coherent() size %d failed\n",
				FW_LOG_RING_SIZE);
		return -ENOMEM;
	}

	dev_info(dev, "%d bytes log buffer physical 0x%llx virtual 0x%p\n",
			FW_LOG_RING_SIZE, tegra->log.phys_addr,
			tegra->log.virt_addr);

	memset(tegra->log.virt_addr, 0, FW_LOG_RING_SIZE);
	tegra->log.dequeue = tegra->log.virt_addr;

	tegra->log.circ.buf = vmalloc(CIRC_BUF_SIZE);
	if (!tegra->log.circ.buf) {
		rc = -ENOMEM;
		goto error_free_dma;
	}

	tegra->log.circ.head = 0;
	tegra->log.circ.tail = 0;

	init_waitqueue_head(&tegra->log.read_wait);
	init_waitqueue_head(&tegra->log.write_wait);
	init_waitqueue_head(&tegra->log.intr_wait);

	mutex_init(&tegra->log.mutex);

	tegra->log.log_file = debugfs_create_file("firmware_log", 0444,
			tegra->debugfs_dir, tegra, &firmware_log_fops);
	if ((!tegra->log.log_file) ||
			(tegra->log.log_file == ERR_PTR(-ENODEV))) {
		dev_warn(dev, "debugfs_create_file() failed\n");
		rc = -ENOMEM;
		goto error_free_mem;
	}

	tegra->log.thread = kthread_run(fw_log_thread, tegra, "xusb-fw-log");
	if (IS_ERR(tegra->log.thread)) {
		dev_warn(dev, "kthread_run() failed\n");
		rc = -ENOMEM;
		goto error_remove_debugfs_file;
	}

	set_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags);
	return rc;

error_remove_debugfs_file:
	debugfs_remove(tegra->log.log_file);
error_free_mem:
	vfree(tegra->log.circ.buf);
error_free_dma:
	dma_free_coherent(dev, FW_LOG_RING_SIZE,
			tegra->log.virt_addr, tegra->log.phys_addr);
	memset(&tegra->log, 0, sizeof(tegra->log));
	return rc;
}

static void fw_log_deinit(struct tegra_xusb *tegra)
{
	struct device *dev = tegra->dev;

	if (test_and_clear_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags)) {

		debugfs_remove(tegra->log.log_file);

		wake_up_interruptible(&tegra->log.read_wait);
		wake_up_interruptible(&tegra->log.write_wait);
		kthread_stop(tegra->log.thread);

		mutex_lock(&tegra->log.mutex);
		dma_free_coherent(dev, FW_LOG_RING_SIZE,
				tegra->log.virt_addr, tegra->log.phys_addr);
		vfree(tegra->log.circ.buf);
		tegra->log.circ.head = tegra->log.circ.tail = 0;
		mutex_unlock(&tegra->log.mutex);

		mutex_destroy(&tegra->log.mutex);
	}
}

static void tegra_xusb_debugfs_init(struct tegra_xusb *tegra)
{
	struct device *dev = tegra->dev;
	char xhcivf[16];

	if (tegra->soc->is_xhci_vf) {
		snprintf(xhcivf, sizeof(xhcivf), "tegra_xhci_vf%d",
			tegra->soc->vf_id);
		tegra->debugfs_dir = debugfs_create_dir(xhcivf, NULL);
	} else {
		tegra->debugfs_dir = debugfs_create_dir("tegra_xhci", NULL);
	}

	if (IS_ERR_OR_NULL(tegra->debugfs_dir)) {
		tegra->debugfs_dir = NULL;
		dev_warn(dev, "debugfs_create_dir() for tegra_xhci failed\n");
		return;
	}

#ifdef CONFIG_USB_XHCI_HCD_DEBUGGING
	tegra->dump_ring_file = debugfs_create_file("dump_ring",
			0220, tegra->debugfs_dir, tegra, &dump_ring_fops);
	if (IS_ERR_OR_NULL(tegra->dump_ring_file)) {
		tegra->dump_ring_file = NULL;
		dev_warn(dev, "debugfs_create_file() for dump_ring failed\n");
		return;
	}
#endif

}

static void tegra_xusb_debugfs_deinit(struct tegra_xusb *tegra)
{
#ifdef CONFIG_USB_XHCI_HCD_DEBUGGING
	debugfs_remove(tegra->dump_ring_file);
	tegra->dump_ring_file = NULL;
#endif

	debugfs_remove(tegra->debugfs_dir);
	tegra->debugfs_dir = NULL;
}

static void tegra_xusb_disable_hsic_wake(struct tegra_xusb *tegra)
{
	u32 reg;

	reg = fpci_readl(tegra, XUSB_CFG_ARU_C11PAGESEL);
	reg |= XUSB_HSP0;
	fpci_writel(tegra, reg, XUSB_CFG_ARU_C11PAGESEL);

	reg = fpci_readl(tegra, XUSB_CFG_HSPX_CORE_CTRL);
	reg &= ~XUSB_HSIC_PLLCLK_VLD;
	fpci_writel(tegra, reg, XUSB_CFG_HSPX_CORE_CTRL);

	reg = fpci_readl(tegra, XUSB_CFG_ARU_C11PAGESEL);
	reg &= ~XUSB_HSP0;
	fpci_writel(tegra, reg, XUSB_CFG_ARU_C11PAGESEL);
}

static int tegra_xusb_set_ss_clk(struct tegra_xusb *tegra,
				 unsigned long rate)
{
	unsigned long new_parent_rate, old_parent_rate;
	struct clk *clk = tegra->ss_src_clk;
	unsigned int div;
	int err;

	if (clk_get_rate(clk) == rate)
		return 0;

	switch (rate) {
	case TEGRA_XHCI_SS_HIGH_SPEED:
		/*
		 * Reparent to PLLU_480M. Set divider first to avoid
		 * overclocking.
		 */
		if (!tegra->pll_u_480m) {
			dev_err(tegra->dev, "tegra->pll_u_480m is NULL\n");
			return -EINVAL;
		}

		old_parent_rate = clk_get_rate(clk_get_parent(clk));
		new_parent_rate = clk_get_rate(tegra->pll_u_480m);
		if (new_parent_rate == 0) {
			dev_err(tegra->dev, "new_parent_rate is zero\n");
			return -EINVAL;
		}
		div = new_parent_rate / rate;

		err = clk_set_rate(clk, old_parent_rate / div);
		if (err)
			return err;

		err = clk_set_parent(clk, tegra->pll_u_480m);
		if (err)
			return err;

		/*
		 * The rate should already be correct, but set it again just
		 * to be sure.
		 */
		err = clk_set_rate(clk, rate);
		if (err)
			return err;

		break;

	case TEGRA_XHCI_SS_LOW_SPEED:
		/* Reparent to CLK_M */
		err = clk_set_parent(clk, tegra->clk_m);
		if (err)
			return err;

		err = clk_set_rate(clk, rate);
		if (err)
			return err;

		break;

	default:
		dev_err(tegra->dev, "Invalid SS rate: %lu Hz\n", rate);
		return -EINVAL;
	}

	if (clk_get_rate(clk) != rate) {
		dev_err(tegra->dev, "SS clock doesn't match requested rate\n");
		return -EINVAL;
	}

	return 0;
}

static unsigned long extract_field(u32 value, unsigned int start,
				   unsigned int count)
{
	return (value >> start) & ((1 << count) - 1);
}

/* Command requests from the firmware */
enum tegra_xusb_mbox_cmd {
	MBOX_CMD_MSG_ENABLED = 1,
	MBOX_CMD_INC_FALC_CLOCK,
	MBOX_CMD_DEC_FALC_CLOCK,
	MBOX_CMD_INC_SSPI_CLOCK,
	MBOX_CMD_DEC_SSPI_CLOCK,
	MBOX_CMD_SET_BW, /* no ACK/NAK required */
	MBOX_CMD_SET_SS_PWR_GATING,
	MBOX_CMD_SET_SS_PWR_UNGATING,
	MBOX_CMD_SAVE_DFE_CTLE_CTX,
	MBOX_CMD_AIRPLANE_MODE_ENABLED, /* unused */
	MBOX_CMD_AIRPLANE_MODE_DISABLED, /* unused */
	MBOX_CMD_START_HSIC_IDLE,
	MBOX_CMD_STOP_HSIC_IDLE,
	MBOX_CMD_DBC_WAKE_STACK, /* unused */
	MBOX_CMD_HSIC_PRETEND_CONNECT,
	MBOX_CMD_RESET_SSPI,
	MBOX_CMD_DISABLE_SS_LFPS_DETECTION,
	MBOX_CMD_ENABLE_SS_LFPS_DETECTION,

	MBOX_CMD_MAX,

	/* Response message to above commands */
	MBOX_CMD_ACK = 128,
	MBOX_CMD_NAK
};

struct tegra_xusb_mbox_msg {
	u32 cmd;
	u32 data;
};

static inline u32 tegra_xusb_mbox_pack(const struct tegra_xusb_mbox_msg *msg)
{
	return (msg->cmd & CMD_TYPE_MASK) << CMD_TYPE_SHIFT |
	       (msg->data & CMD_DATA_MASK) << CMD_DATA_SHIFT;
}
static inline void tegra_xusb_mbox_unpack(struct tegra_xusb_mbox_msg *msg,
					  u32 value)
{
	msg->cmd = (value >> CMD_TYPE_SHIFT) & CMD_TYPE_MASK;
	msg->data = (value >> CMD_DATA_SHIFT) & CMD_DATA_MASK;
}

static bool tegra_xusb_mbox_cmd_requires_ack(enum tegra_xusb_mbox_cmd cmd)
{
	switch (cmd) {
	case MBOX_CMD_SET_BW:
	case MBOX_CMD_ACK:
	case MBOX_CMD_NAK:
		return false;

	default:
		return true;
	}
}

static int tegra_xusb_mbox_send(struct tegra_xusb *tegra,
				const struct tegra_xusb_mbox_msg *msg)
{
	struct tegra_xusb_soc_ops *ops = tegra->soc->ops;
	bool wait_for_idle = false;
	u32 value;

	/*
	 * Acquire the mailbox. The firmware still owns the mailbox for
	 * ACK/NAK messages.
	 */
	if (!(msg->cmd == MBOX_CMD_ACK || msg->cmd == MBOX_CMD_NAK)) {
		value = ops->mbox_reg_readl(tegra, tegra->soc->mbox.owner);
		if (value != MBOX_OWNER_NONE) {
			dev_err(tegra->dev, "mailbox is busy\n");
			return -EBUSY;
		}

		ops->mbox_reg_writel(tegra, MBOX_OWNER_SW, tegra->soc->mbox.owner);

		value = ops->mbox_reg_readl(tegra, tegra->soc->mbox.owner);
		if (value != MBOX_OWNER_SW) {
			dev_err(tegra->dev, "failed to acquire mailbox\n");
			return -EBUSY;
		}

		wait_for_idle = true;
	}

	value = tegra_xusb_mbox_pack(msg);
	ops->mbox_reg_writel(tegra, value, tegra->soc->mbox.data_in);

	value = ops->mbox_reg_readl(tegra, tegra->soc->mbox.cmd);
	value |= MBOX_INT_EN | MBOX_DEST_FALC;
	ops->mbox_reg_writel(tegra, value, tegra->soc->mbox.cmd);

	if (wait_for_idle) {
		unsigned long timeout = jiffies + msecs_to_jiffies(250);

		while (time_before(jiffies, timeout)) {
			value = ops->mbox_reg_readl(tegra, tegra->soc->mbox.owner);
			if (value == MBOX_OWNER_NONE)
				break;

			usleep_range(10, 20);
		}

		if (time_after(jiffies, timeout))
			value = ops->mbox_reg_readl(tegra, tegra->soc->mbox.owner);

		if (value != MBOX_OWNER_NONE)
			return -ETIMEDOUT;
	}

	return 0;
}

static irqreturn_t tegra_xusb_mbox_irq(int irq, void *data)
{
	struct tegra_xusb *tegra = data;
	struct tegra_xusb_soc_ops *ops = tegra->soc->ops;
	u32 value, value2;

	/* clear mailbox interrupts */
	value = ops->mbox_reg_readl(tegra, tegra->soc->mbox.smi_intr);
	ops->mbox_reg_writel(tegra, value, tegra->soc->mbox.smi_intr);

	/* read again to avoid spurious ARU SMI interrupt */
	value2 = ops->mbox_reg_readl(tegra, tegra->soc->mbox.smi_intr);

	if (value & MBOX_SMI_INTR_FW_HANG) {
		dev_err(tegra->dev, "controller error detected\n");
		tegra_xhci_hcd_reinit(tegra->hcd);
		return IRQ_HANDLED;
	}

	if (value & MBOX_SMI_INTR_EN)
		return IRQ_WAKE_THREAD;

	dev_warn(tegra->dev, "unhandled mbox irq: %08x %08x\n", value, value2);

	return value ? IRQ_HANDLED : IRQ_NONE;
}

static void tegra_xusb_mbox_handle(struct tegra_xusb *tegra,
				   const struct tegra_xusb_mbox_msg *msg)
{
	struct tegra_xusb_padctl *padctl = tegra->padctl;
	const struct tegra_xusb_soc *soc = tegra->soc;
	struct device *dev = tegra->dev;
	struct tegra_xusb_mbox_msg rsp;
	unsigned long mask;
	unsigned int port;
	bool idle, enable;
	int err = 0;

	memset(&rsp, 0, sizeof(rsp));

	switch (msg->cmd) {
	case MBOX_CMD_INC_FALC_CLOCK:
	case MBOX_CMD_DEC_FALC_CLOCK:
		rsp.data = clk_get_rate(tegra->falcon_clk) / 1000;
		if (rsp.data != msg->data)
			rsp.cmd = MBOX_CMD_NAK;
		else
			rsp.cmd = MBOX_CMD_ACK;

		break;

	case MBOX_CMD_INC_SSPI_CLOCK:
	case MBOX_CMD_DEC_SSPI_CLOCK:
		if (tegra->soc->scale_ss_clock) {
			err = tegra_xusb_set_ss_clk(tegra, msg->data * 1000);
			if (err < 0)
				rsp.cmd = MBOX_CMD_NAK;
			else
				rsp.cmd = MBOX_CMD_ACK;

			rsp.data = clk_get_rate(tegra->ss_src_clk) / 1000;
		} else {
			rsp.cmd = MBOX_CMD_ACK;
			rsp.data = msg->data;
		}

		break;

	case MBOX_CMD_SET_BW:
		/*
		 * TODO: Request bandwidth once EMC scaling is supported.
		 * Ignore for now since ACK/NAK is not required for SET_BW
		 * messages.
		 */
		break;

	case MBOX_CMD_SAVE_DFE_CTLE_CTX:
		err = tegra_xusb_padctl_usb3_save_context(padctl, msg->data);
		if (err < 0) {
			dev_err(dev, "failed to save context for USB3#%u: %d\n",
				msg->data, err);
			rsp.cmd = MBOX_CMD_NAK;
		} else {
			rsp.cmd = MBOX_CMD_ACK;
		}

		rsp.data = msg->data;
		break;

	case MBOX_CMD_START_HSIC_IDLE:
	case MBOX_CMD_STOP_HSIC_IDLE:
		if (msg->cmd == MBOX_CMD_STOP_HSIC_IDLE)
			idle = false;
		else
			idle = true;

		mask = extract_field(msg->data, 1 + soc->ports.hsic.offset,
				     soc->ports.hsic.count);

		for_each_set_bit(port, &mask, 32) {
			err = tegra_xusb_padctl_hsic_set_idle(padctl, port,
							      idle);
			if (err < 0)
				break;
		}

		if (err < 0) {
			dev_err(dev, "failed to set HSIC#%u %s: %d\n", port,
				idle ? "idle" : "busy", err);
			rsp.cmd = MBOX_CMD_NAK;
		} else {
			rsp.cmd = MBOX_CMD_ACK;
		}

		rsp.data = msg->data;
		break;

	case MBOX_CMD_DISABLE_SS_LFPS_DETECTION:
	case MBOX_CMD_ENABLE_SS_LFPS_DETECTION:
		if (msg->cmd == MBOX_CMD_DISABLE_SS_LFPS_DETECTION)
			enable = false;
		else
			enable = true;

		mask = extract_field(msg->data, 1 + soc->ports.usb3.offset,
				     soc->ports.usb3.count);

		for_each_set_bit(port, &mask, soc->ports.usb3.count) {
			err = tegra_xusb_padctl_usb3_set_lfps_detect(padctl,
								     port,
								     enable);
			if (err < 0)
				break;

			/*
			 * wait 500us for LFPS detector to be disabled before
			 * sending ACK
			 */
			if (!enable)
				usleep_range(500, 1000);
		}

		if (err < 0) {
			dev_err(dev,
				"failed to %s LFPS detection on USB3#%u: %d\n",
				enable ? "enable" : "disable", port, err);
			rsp.cmd = MBOX_CMD_NAK;
		} else {
			rsp.cmd = MBOX_CMD_ACK;
		}

		for_each_set_bit(port, &mask, soc->ports.usb3.count) {
			if (enable && tegra->soc->disable_u0_ts1_detect)
				tegra_xusb_padctl_enable_receiver_detector(
					padctl,
					tegra->phys[port]);
		}

		rsp.data = msg->data;
		break;

	default:
		dev_warn(dev, "unknown message: %#x\n", msg->cmd);
		break;
	}

	if (rsp.cmd) {
		const char *cmd = (rsp.cmd == MBOX_CMD_ACK) ? "ACK" : "NAK";

		err = tegra_xusb_mbox_send(tegra, &rsp);
		if (err < 0)
			dev_err(dev, "failed to send %s: %d\n", cmd, err);
	}
}

static irqreturn_t tegra_xusb_mbox_thread(int irq, void *data)
{
	struct tegra_xusb *tegra = data;
	struct tegra_xusb_soc_ops *ops = tegra->soc->ops;
	struct tegra_xusb_mbox_msg msg;
	u32 value;

	mutex_lock(&tegra->lock);

	if (pm_runtime_suspended(tegra->dev) || tegra->suspended)
		goto out;

	value = ops->mbox_reg_readl(tegra, tegra->soc->mbox.data_out);
	tegra_xusb_mbox_unpack(&msg, value);

	value = ops->mbox_reg_readl(tegra, tegra->soc->mbox.cmd);
	value &= ~MBOX_DEST_SMI;
	ops->mbox_reg_writel(tegra, value, tegra->soc->mbox.cmd);

	/* clear mailbox owner if no ACK/NAK is required */
	if (!tegra_xusb_mbox_cmd_requires_ack(msg.cmd))
		ops->mbox_reg_writel(tegra, MBOX_OWNER_NONE, tegra->soc->mbox.owner);

	tegra_xusb_mbox_handle(tegra, &msg);

out:
	mutex_unlock(&tegra->lock);
	return IRQ_HANDLED;
}

static void tegra_xusb_config(struct tegra_xusb *tegra)
{
	static void __iomem *pad_base;
	u32 regs = tegra->hcd->rsrc_start;
	u32 value;

	if (tegra->soc->has_ipfs) {
		value = ipfs_readl(tegra, IPFS_XUSB_HOST_CONFIGURATION_0);
		value |= IPFS_EN_FPCI;
		ipfs_writel(tegra, value, IPFS_XUSB_HOST_CONFIGURATION_0);

		usleep_range(10, 20);
	}

	/* Program BAR0 space */
	value = fpci_readl(tegra, XUSB_CFG_4);
	value &= ~(XUSB_BASE_ADDR_MASK << XUSB_BASE_ADDR_SHIFT);
	value |= regs & (XUSB_BASE_ADDR_MASK << XUSB_BASE_ADDR_SHIFT);
	fpci_writel(tegra, value, XUSB_CFG_4);

	/* Program BAR2 space */
	if (tegra->soc->has_bar2) {
		value = fpci_readl(tegra, XUSB_CFG_7);
		value &= ~(XUSB_BASE2_ADDR_MASK << XUSB_BASE2_ADDR_SHIFT);
		value |= tegra->bar2_start &
			(XUSB_BASE2_ADDR_MASK << XUSB_BASE2_ADDR_SHIFT);
		fpci_writel(tegra, value, XUSB_CFG_7);
	}

	usleep_range(100, 200);

	/* Enable bus master */
	value = fpci_readl(tegra, XUSB_CFG_1);
	value |= XUSB_IO_SPACE_EN | XUSB_MEM_SPACE_EN | XUSB_BUS_MASTER_EN;
	fpci_writel(tegra, value, XUSB_CFG_1);

	if (tegra->soc->has_ipfs) {
		/* Enable interrupt assertion */
		value = ipfs_readl(tegra, IPFS_XUSB_HOST_INTR_MASK_0);
		value |= IPFS_IP_INT_MASK;
		ipfs_writel(tegra, value, IPFS_XUSB_HOST_INTR_MASK_0);

		/* Set hysteresis */
		ipfs_writel(tegra, 0x80, IPFS_XUSB_HOST_CLKGATE_HYSTERESIS_0);
	}

	if (tegra->soc->has_ifr) {
		pad_base = devm_ioremap(tegra->dev, 0x3520000, 0x20000);
		if (IS_ERR(pad_base)) {
			dev_err(tegra->dev, "failed to map pad mmio\n");
			return;
		}
		iowrite32(0xE, pad_base + 0x10000);
	}
}

static int tegra_xusb_clk_enable(struct tegra_xusb *tegra)
{
	int err;

	if (tegra->clk_enabled)
		return 0;

	err = clk_prepare_enable(tegra->pll_e);
	if (err < 0)
		return err;

	err = clk_prepare_enable(tegra->host_clk);
	if (err < 0)
		goto disable_plle;

	err = clk_prepare_enable(tegra->ss_clk);
	if (err < 0)
		goto disable_host;

	err = clk_prepare_enable(tegra->falcon_clk);
	if (err < 0)
		goto disable_ss;

	err = clk_prepare_enable(tegra->fs_src_clk);
	if (err < 0)
		goto disable_falc;

	err = clk_prepare_enable(tegra->hs_src_clk);
	if (err < 0)
		goto disable_fs_src;

	if (tegra->soc->scale_ss_clock) {
		err = tegra_xusb_set_ss_clk(tegra, TEGRA_XHCI_SS_HIGH_SPEED);
		if (err < 0)
			goto disable_hs_src;
	}
	tegra->clk_enabled = true;

	return 0;

disable_hs_src:
	clk_disable_unprepare(tegra->hs_src_clk);
disable_fs_src:
	clk_disable_unprepare(tegra->fs_src_clk);
disable_falc:
	clk_disable_unprepare(tegra->falcon_clk);
disable_ss:
	clk_disable_unprepare(tegra->ss_clk);
disable_host:
	clk_disable_unprepare(tegra->host_clk);
disable_plle:
	clk_disable_unprepare(tegra->pll_e);
	return err;
}

static void tegra_xusb_clk_disable(struct tegra_xusb *tegra)
{
	if (tegra->clk_enabled) {
		clk_disable_unprepare(tegra->pll_e);
		clk_disable_unprepare(tegra->host_clk);
		clk_disable_unprepare(tegra->ss_clk);
		clk_disable_unprepare(tegra->falcon_clk);
		clk_disable_unprepare(tegra->fs_src_clk);
		clk_disable_unprepare(tegra->hs_src_clk);
		tegra->clk_enabled = false;
	}
}

static int tegra_xusb_phy_enable(struct tegra_xusb *tegra)
{
	unsigned int i;
	int err;

	for (i = 0; i < tegra->num_phys; i++) {
		err = phy_init(tegra->phys[i]);
		if (err)
			goto disable_phy;

		err = phy_power_on(tegra->phys[i]);
		if (err) {
			phy_exit(tegra->phys[i]);
			goto disable_phy;
		}
	}

	return 0;

disable_phy:
	while (i--) {
		phy_power_off(tegra->phys[i]);
		phy_exit(tegra->phys[i]);
	}

	return err;
}

static void tegra_xusb_phy_disable(struct tegra_xusb *tegra)
{
	unsigned int i;

	for (i = 0; i < tegra->num_phys; i++) {
		phy_power_off(tegra->phys[i]);
		phy_exit(tegra->phys[i]);
	}
}

#ifdef CONFIG_PM_SLEEP
static int tegra_xusb_init_context(struct tegra_xusb *tegra)
{
	const struct tegra_xusb_context_soc *soc = tegra->soc->context;

	tegra->context.ipfs = devm_kcalloc(tegra->dev, soc->ipfs.num_offsets,
					   sizeof(u32), GFP_KERNEL);
	if (!tegra->context.ipfs)
		return -ENOMEM;

	if (!tegra->soc->is_xhci_vf) {
		tegra->context.fpci = devm_kcalloc(tegra->dev,
						   soc->fpci.num_offsets,
						   sizeof(u32), GFP_KERNEL);
		if (!tegra->context.fpci)
			return -ENOMEM;
	}

	return 0;
}
#else
static inline int tegra_xusb_init_context(struct tegra_xusb *tegra)
{
	return 0;
}
#endif

static int tegra_xusb_request_firmware(struct tegra_xusb *tegra)
{
	struct tegra_xusb_fw_header *header;
	const struct firmware *fw;
	int err;

	err = request_firmware(&fw, tegra->soc->firmware, tegra->dev);
	if (err < 0) {
		dev_err(tegra->dev, "failed to request firmware: %d\n", err);
		return err;
	}

	/* Load Falcon controller with its firmware. */
	header = (struct tegra_xusb_fw_header *)fw->data;
	tegra->fw.size = le32_to_cpu(header->fwimg_len);

	tegra->fw.virt = dma_alloc_coherent(tegra->dev, tegra->fw.size,
					    &tegra->fw.phys, GFP_KERNEL);
	if (!tegra->fw.virt) {
		dev_err(tegra->dev, "failed to allocate memory for firmware\n");
		release_firmware(fw);
		return -ENOMEM;
	}

	header = (struct tegra_xusb_fw_header *)tegra->fw.virt;
	memcpy(tegra->fw.virt, fw->data, tegra->fw.size);
	release_firmware(fw);

	return 0;
}

static int tegra_xusb_check_controller(struct tegra_xusb *tegra)
{
	struct xhci_cap_regs __iomem *cap_regs;
	struct xhci_op_regs __iomem *op_regs;
	unsigned long timeout;
	u32 val;

	cap_regs = tegra->regs;
	op_regs = tegra->regs + HC_LENGTH(readl(&cap_regs->hc_capbase));

	timeout = jiffies + msecs_to_jiffies(600);
	do {
		val = readl(&op_regs->status);
		if (!(val & STS_CNR))
			break;
		usleep_range(1000, 2000);
	} while (time_is_after_jiffies(timeout));

	val = readl(&op_regs->status);
	if (val & STS_CNR) {
		dev_err(tegra->dev, "XHCI Controller not ready. Falcon state: 0x%x\n",
			csb_readl(tegra, XUSB_FALC_CPUCTL));
		return -EIO;
	}

	return 0;
}

static int tegra_xusb_load_firmware(struct tegra_xusb *tegra)
{
	unsigned int code_tag_blocks, code_size_blocks, code_blocks;
	struct tegra_xusb_fw_header *header;
	struct device *dev = tegra->dev;
	struct tm time;
	u64 address;
	u32 value;
	int err;

	header = (struct tegra_xusb_fw_header *)tegra->fw.virt;

	if (csb_readl(tegra, XUSB_CSB_MP_ILOAD_BASE_LO) != 0) {
		dev_info(dev, "Firmware already loaded, Falcon state %#x\n",
			 csb_readl(tegra, XUSB_FALC_CPUCTL));
		return 0;
	}

	if (header->build_log == LOG_MEMORY)
		fw_log_init(tegra);

	/* update the phys_log_buffer and total_entries here */
	if (test_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags)) {
		header->phys_addr_log_buffer =
					cpu_to_le32(tegra->log.phys_addr);
		header->total_log_entries = cpu_to_le32(FW_LOG_COUNT);
	}

	/* Program the size of DFI into ILOAD_ATTR. */
	csb_writel(tegra, tegra->fw.size, XUSB_CSB_MP_ILOAD_ATTR);

	/*
	 * Boot code of the firmware reads the ILOAD_BASE registers
	 * to get to the start of the DFI in system memory.
	 */
	address = tegra->fw.phys + sizeof(*header);
	csb_writel(tegra, address >> 32, XUSB_CSB_MP_ILOAD_BASE_HI);
	csb_writel(tegra, address, XUSB_CSB_MP_ILOAD_BASE_LO);

	/* Set BOOTPATH to 1 in APMAP. */
	csb_writel(tegra, APMAP_BOOTPATH, XUSB_CSB_MP_APMAP);

	/* Invalidate L2IMEM. */
	csb_writel(tegra, L2IMEMOP_INVALIDATE_ALL, XUSB_CSB_MP_L2IMEMOP_TRIG);

	/*
	 * Initiate fetch of bootcode from system memory into L2IMEM.
	 * Program bootcode location and size in system memory.
	 */
	code_tag_blocks = DIV_ROUND_UP(le32_to_cpu(header->boot_codetag),
				       IMEM_BLOCK_SIZE);
	code_size_blocks = DIV_ROUND_UP(le32_to_cpu(header->boot_codesize),
					IMEM_BLOCK_SIZE);
	code_blocks = code_tag_blocks + code_size_blocks;

	value = ((code_tag_blocks & L2IMEMOP_SIZE_SRC_OFFSET_MASK) <<
			L2IMEMOP_SIZE_SRC_OFFSET_SHIFT) |
		((code_size_blocks & L2IMEMOP_SIZE_SRC_COUNT_MASK) <<
			L2IMEMOP_SIZE_SRC_COUNT_SHIFT);
	csb_writel(tegra, value, XUSB_CSB_MP_L2IMEMOP_SIZE);

	/* Trigger L2IMEM load operation. */
	csb_writel(tegra, L2IMEMOP_LOAD_LOCKED_RESULT,
		   XUSB_CSB_MP_L2IMEMOP_TRIG);

	/* Setup Falcon auto-fill. */
	csb_writel(tegra, code_size_blocks, XUSB_FALC_IMFILLCTL);

	value = ((code_tag_blocks & IMFILLRNG1_TAG_MASK) <<
			IMFILLRNG1_TAG_LO_SHIFT) |
		((code_blocks & IMFILLRNG1_TAG_MASK) <<
			IMFILLRNG1_TAG_HI_SHIFT);
	csb_writel(tegra, value, XUSB_FALC_IMFILLRNG1);

	csb_writel(tegra, 0, XUSB_FALC_DMACTL);

	/* wait for RESULT_VLD to get set */
#define tegra_csb_readl(offset) csb_readl(tegra, offset)
	err = readx_poll_timeout(tegra_csb_readl,
				 XUSB_CSB_MEMPOOL_L2IMEMOP_RESULT, value,
				 value & L2IMEMOP_RESULT_VLD, 100, 10000);
	if (err < 0) {
		dev_err(dev, "DMA controller not ready %#010x\n", value);
		return err;
	}
#undef tegra_csb_readl

	csb_writel(tegra, le32_to_cpu(header->boot_codetag),
		   XUSB_FALC_BOOTVEC);

	/* Boot Falcon CPU and wait for USBSTS_CNR to get cleared. */
	csb_writel(tegra, CPUCTL_STARTCPU, XUSB_FALC_CPUCTL);

	if (tegra_xusb_check_controller(tegra))
		return -EIO;

	tegra->build_log = header->build_log;
	tegra->version_id = header->version_id;
	tegra->timestamp = le32_to_cpu(header->fwimg_created_time);
	time64_to_tm(tegra->timestamp, 0, &time);

	dev_info(dev, "Firmware timestamp: %ld-%02d-%02d %02d:%02d:%02d UTC, Version: %2x.%02x %s\n",
		time.tm_year + 1900, time.tm_mon + 1, time.tm_mday,
		time.tm_hour, time.tm_min, time.tm_sec,
		FW_MAJOR_VERSION(tegra->version_id),
		FW_MINOR_VERSION(tegra->version_id),
		(tegra->build_log == LOG_MEMORY) ? "debug" : "release");

	return 0;
}

static u32 tegra_xusb_read_firmware_header(struct tegra_xusb *tegra, int i)
{
	if (i >= (sizeof(struct tegra_xusb_fw_header) >> 2))
		return 0;

	bar2_writel(tegra, (FW_IOCTL_CFGTBL_READ << FW_IOCTL_TYPE_SHIFT) | (i << 2),
			XUSB_BAR2_ARU_FW_SCRATCH);
	return bar2_readl(tegra, XUSB_BAR2_ARU_SMI_ARU_FW_SCRATCH_DATA0);
}

static int tegra_xusb_init_ifr_firmware(struct tegra_xusb *tegra)
{
	u32 val;
	struct tm time;

	if (tegra->soc->load_ifr_rom) {
		dev_info(tegra->dev, "load ifr firmware: %llx %ld\n",
			tegra->fw.phys, tegra->fw.size);

		if (tegra_platform_is_fpga()) {
			/* set IFRDMA address */
			bar2_writel(tegra, cpu_to_le32(tegra->fw.phys),
					XUSB_BAR2_ARU_IFRDMA_CFG0);
			bar2_writel(tegra,
				(cpu_to_le64(tegra->fw.phys) >> 32) & 0xff,
				XUSB_BAR2_ARU_IFRDMA_CFG1);

			/* set streamid */
			val = bar2_readl(tegra,
					 XUSB_BAR2_ARU_IFRDMA_STREAMID_FIELD);
			val &= ~((u32) 0xff);
			val |= 0x7F;
			bar2_writel(tegra, val,
				    XUSB_BAR2_ARU_IFRDMA_STREAMID_FIELD);
		} else {
			static void __iomem *ao_base;

			ao_base = devm_ioremap(tegra->dev, 0x3540000, 0x10000);
			if (IS_ERR(ao_base)) {
				dev_err(tegra->dev, "failed to map AO mmio\n");
				return PTR_ERR(ao_base);
			}

			/* set IFRDMA address */
			iowrite32(cpu_to_le32(tegra->fw.phys), ao_base + 0x1bc);
			iowrite32((cpu_to_le64(tegra->fw.phys) >> 32) & 0xffffffff,
					ao_base + 0x1c0);

			/* set streamid */
			val = ioread32(ao_base + 0x1c4);
			val &= ~((u32) 0xff);
			val |= 0xE;
			iowrite32(val, ao_base + 0x1c4);

			devm_iounmap(tegra->dev, ao_base);
		}
	}

	if (tegra_xusb_check_controller(tegra))
		return -EIO;

	/* init fw log buffer if needed */
#define offsetof_32(X, Y) ((u8)(offsetof(X, Y) / sizeof(__le32)))
	tegra->build_log = (tegra_xusb_read_firmware_header(tegra,
				offsetof_32(struct tegra_xusb_fw_header,
					num_hsic_port)) >> 16) & 0xf;

	if (tegra->build_log == LOG_MEMORY) {

		fw_log_init(tegra);

		/* set up fw log buffer address */
		csb_writel(tegra, (u32) tegra->log.phys_addr,
				XUSB_CSB_ARU_SCRATCH0);
		if (tegra->soc->has_ifr)
			csb_writel(tegra, (u32) (tegra->log.phys_addr >> 32),
				   XUSB_CSB_ARU_SCRATCH1);

		/* set up fw log buffer size */
		val = (FW_IOCTL_INIT_LOG_BUF << FW_IOCTL_TYPE_SHIFT);
		val |= FW_LOG_COUNT;
		bar2_writel(tegra, val, XUSB_BAR2_ARU_FW_SCRATCH);
	}

	tegra->timestamp = tegra_xusb_read_firmware_header(tegra,
			offsetof_32(struct tegra_xusb_fw_header,
				fwimg_created_time));
	time64_to_tm(tegra->timestamp, 0, &time);

	tegra->version_id = tegra_xusb_read_firmware_header(tegra,
			offsetof_32(struct tegra_xusb_fw_header,
				version_id));
#undef offsetof_32

	dev_info(tegra->dev, "Firmware timestamp: %ld-%02d-%02d %02d:%02d:%02d UTC, Version: %2x.%02x %s\n",
		 time.tm_year + 1900, time.tm_mon + 1, time.tm_mday,
		 time.tm_hour, time.tm_min, time.tm_sec,
		 FW_MAJOR_VERSION(tegra->version_id),
		 FW_MINOR_VERSION(tegra->version_id),
		 (tegra->build_log == LOG_MEMORY) ? "debug" : "release");

	return 0;
}

static void tegra_genpd_down_postwork(struct tegra_xusb *tegra)
{
	unsigned int i;
	bool wakeup = device_may_wakeup(tegra->dev);

	if (tegra->suspended) {
		for (i = 0; i < tegra->num_phys; i++) {
			if (!tegra->phys[i])
				continue;

			phy_power_off(tegra->phys[i]);
			if (!wakeup)
				phy_exit(tegra->phys[i]);
		}
	}
}

static int tegra_xhci_genpd_notify(struct notifier_block *nb,
					unsigned long action, void *data)
{
	struct tegra_xusb *tegra = container_of(nb, struct tegra_xusb,
						    genpd_nb);

	if (action == GENPD_NOTIFY_OFF)
		tegra_genpd_down_postwork(tegra);

	return 0;
}

static void tegra_xusb_powerdomain_remove(struct device *dev,
					  struct tegra_xusb *tegra)
{
	if (!tegra->use_genpd)
		return;

	dev_pm_genpd_remove_notifier(tegra->genpd_dev_host);

	if (!IS_ERR_OR_NULL(tegra->genpd_dev_ss))
		dev_pm_domain_detach(tegra->genpd_dev_ss, true);
	if (!IS_ERR_OR_NULL(tegra->genpd_dev_host))
		dev_pm_domain_detach(tegra->genpd_dev_host, true);
}

static int tegra_xusb_powerdomain_init(struct device *dev,
				       struct tegra_xusb *tegra)
{
	int err;

	tegra->genpd_dev_host = dev_pm_domain_attach_by_name(dev, "xusb_host");
	if (IS_ERR(tegra->genpd_dev_host)) {
		err = PTR_ERR(tegra->genpd_dev_host);
		dev_err(dev, "failed to get host pm-domain: %d\n", err);
		return err;
	}

	tegra->genpd_dev_ss = dev_pm_domain_attach_by_name(dev, "xusb_ss");
	if (IS_ERR(tegra->genpd_dev_ss)) {
		err = PTR_ERR(tegra->genpd_dev_ss);
		dev_pm_domain_detach(tegra->genpd_dev_host, true);
		tegra->genpd_dev_host = NULL;
		dev_err(dev, "failed to get superspeed pm-domain: %d\n", err);
		return err;
	}

	tegra->genpd_nb.notifier_call = tegra_xhci_genpd_notify;
	dev_pm_genpd_add_notifier(tegra->genpd_dev_host, &tegra->genpd_nb);

	tegra->use_genpd = true;

	return 0;
}

static int tegra_xusb_unpowergate_partitions(struct tegra_xusb *tegra)
{
	struct device *dev = tegra->dev;
	int rc;

	if (tegra->use_genpd) {
		rc = pm_runtime_get_sync(tegra->genpd_dev_ss);
		if (rc < 0) {
			dev_err(dev, "failed to enable XUSB SS partition\n");
			return rc;
		}

		rc = pm_runtime_get_sync(tegra->genpd_dev_host);
		if (rc < 0) {
			dev_err(dev, "failed to enable XUSB Host partition\n");
			pm_runtime_put_sync(tegra->genpd_dev_ss);
			return rc;
		}
	} else {
		rc = tegra_powergate_sequence_power_up(TEGRA_POWERGATE_XUSBA,
							tegra->ss_clk,
							tegra->ss_rst);
		if (rc < 0) {
			dev_err(dev, "failed to enable XUSB SS partition\n");
			return rc;
		}

		rc = tegra_powergate_sequence_power_up(TEGRA_POWERGATE_XUSBC,
							tegra->host_clk,
							tegra->host_rst);
		if (rc < 0) {
			dev_err(dev, "failed to enable XUSB Host partition\n");
			tegra_powergate_power_off(TEGRA_POWERGATE_XUSBA);
			return rc;
		}
	}

	return 0;
}

static int tegra_xusb_powergate_partitions(struct tegra_xusb *tegra)
{
	struct device *dev = tegra->dev;
	int rc;

	if (tegra->use_genpd) {
		rc = pm_runtime_put_sync(tegra->genpd_dev_host);
		if (rc < 0) {
			dev_err(dev, "failed to disable XUSB Host partition\n");
			return rc;
		}

		rc = pm_runtime_put_sync(tegra->genpd_dev_ss);
		if (rc < 0) {
			dev_err(dev, "failed to disable XUSB SS partition\n");
			pm_runtime_get_sync(tegra->genpd_dev_host);
			return rc;
		}
	} else {
		rc = tegra_powergate_power_off(TEGRA_POWERGATE_XUSBC);
		if (rc < 0) {
			dev_err(dev, "failed to disable XUSB Host partition\n");
			return rc;
		}

		rc = tegra_powergate_power_off(TEGRA_POWERGATE_XUSBA);
		if (rc < 0) {
			dev_err(dev, "failed to disable XUSB SS partition\n");
			tegra_powergate_sequence_power_up(TEGRA_POWERGATE_XUSBC,
							  tegra->host_clk,
							  tegra->host_rst);
			return rc;
		}
	}

	return 0;
}

static int __tegra_xusb_enable_firmware_messages(struct tegra_xusb *tegra)
{
	struct tegra_xusb_mbox_msg msg;
	int err;

	/* Enable firmware messages from controller. */
	msg.cmd = MBOX_CMD_MSG_ENABLED;
	msg.data = 0;

	err = tegra_xusb_mbox_send(tegra, &msg);
	if (err < 0)
		dev_err(tegra->dev, "failed to enable messages: %d\n", err);

	return err;
}

static irqreturn_t tegra_xusb_padctl_irq(int irq, void *data)
{
	struct tegra_xusb *tegra = data;

	mutex_lock(&tegra->lock);

	if (tegra->suspended) {
		mutex_unlock(&tegra->lock);
		return IRQ_HANDLED;
	}

	mutex_unlock(&tegra->lock);

	pm_runtime_resume(tegra->dev);

	return IRQ_HANDLED;
}

static int tegra_xusb_enable_firmware_messages(struct tegra_xusb *tegra)
{
	int err;

	mutex_lock(&tegra->lock);
	err = __tegra_xusb_enable_firmware_messages(tegra);
	mutex_unlock(&tegra->lock);

	return err;
}

static int tegra_xhci_update_device(struct usb_hcd *hcd,
				    struct usb_device *udev)
{
	const struct usb_device_id *id;

	for (id = disable_usb_persist_quirk_list; id->match_flags; id++) {
		if (usb_match_device(udev, id) && usb_match_speed(udev, id)) {
			udev->persist_enabled = 0;
			break;
		}
	}

	return xhci_update_device(hcd, udev);
}

static void tegra_xhci_set_port_power(struct tegra_xusb *tegra, bool main,
						 bool set)
{
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	struct usb_hcd *hcd = main ?  xhci->main_hcd : xhci->shared_hcd;
	unsigned int wait = (!main && !set) ? 1000 : 10;
	u16 typeReq = set ? SetPortFeature : ClearPortFeature;
	u16 wIndex = main ? tegra->otg_usb2_port + 1 : tegra->otg_usb3_port + 1;
	u32 status;
	u32 stat_power = main ? USB_PORT_STAT_POWER : USB_SS_PORT_STAT_POWER;
	u32 status_val = set ? stat_power : 0;

	dev_dbg(tegra->dev, "%s():%s %s port power\n", __func__,
		set ? "set" : "clear", main ? "HS" : "SS");

	hcd->driver->hub_control(hcd, typeReq, USB_PORT_FEAT_POWER, wIndex,
				 NULL, 0);

	do {
		tegra_xhci_hc_driver.hub_control(hcd, GetPortStatus, 0, wIndex,
					(char *) &status, sizeof(status));
		if (status_val == (status & stat_power))
			break;

		if (!main && !set)
			usleep_range(600, 700);
		else
			usleep_range(10, 20);
	} while (--wait > 0);

	if (status_val != (status & stat_power))
		dev_info(tegra->dev, "failed to %s %s PP %d\n",
						set ? "set" : "clear",
						main ? "HS" : "SS", status);
}

static struct phy *tegra_xusb_get_phy(struct tegra_xusb *tegra, char *name,
								int port)
{
	unsigned int i, phy_count = 0;

	for (i = 0; i < tegra->soc->num_types; i++) {
		if (!strncmp(tegra->soc->phy_types[i].name, name,
							    strlen(name)))
			return tegra->phys[phy_count+port];

		phy_count += tegra->soc->phy_types[i].num;
	}

	return NULL;
}

static void tegra_xhci_id_work(struct work_struct *work)
{
	struct tegra_xusb *tegra = container_of(work, struct tegra_xusb,
						id_work);
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	struct tegra_xusb_mbox_msg msg;
	struct phy *phy = tegra_xusb_get_phy(tegra, "usb2",
						    tegra->otg_usb2_port);
	u32 status;
	int ret;

	if (xhci->recovery_in_progress)
		return;

	dev_dbg(tegra->dev, "host mode %s\n", tegra->host_mode ? "on" : "off");

	mutex_lock(&tegra->lock);

	if (tegra->host_mode)
		phy_set_mode_ext(phy, PHY_MODE_USB_OTG, USB_ROLE_HOST);
	else
		phy_set_mode_ext(phy, PHY_MODE_USB_OTG, USB_ROLE_NONE);

	mutex_unlock(&tegra->lock);

	pm_runtime_get_sync(tegra->dev);
	if (tegra->host_mode) {
		/* switch to host mode */
		if (tegra->otg_usb3_port >= 0) {
			if (tegra->soc->otg_reset_sspi) {
				/* set PP=0 */
				tegra_xhci_hc_driver.hub_control(
					xhci->shared_hcd, GetPortStatus,
					0, tegra->otg_usb3_port+1,
					(char *) &status, sizeof(status));
				if (status & USB_SS_PORT_STAT_POWER)
					tegra_xhci_set_port_power(tegra, false,
								  false);

				/* reset OTG port SSPI */
				msg.cmd = MBOX_CMD_RESET_SSPI;
				msg.data = tegra->otg_usb3_port+1;

				ret = tegra_xusb_mbox_send(tegra, &msg);
				if (ret < 0) {
					dev_info(tegra->dev,
						"failed to RESET_SSPI %d\n",
						ret);
				}
			}

			tegra_xhci_set_port_power(tegra, false, true);
		}

		tegra_xhci_set_port_power(tegra, true, true);
		pm_runtime_mark_last_busy(tegra->dev);
	} else {
		if (tegra->otg_usb3_port >= 0)
			tegra_xhci_set_port_power(tegra, false, false);

		tegra_xhci_set_port_power(tegra, true, false);
	}
	pm_runtime_put_autosuspend(tegra->dev);
}

static bool is_usb2_otg_phy(struct tegra_xusb *tegra, unsigned int index)
{
	return (tegra->usbphy[index] != NULL);
}

static bool is_usb3_otg_phy(struct tegra_xusb *tegra, unsigned int index)
{
	struct tegra_xusb_padctl *padctl = tegra->padctl;
	unsigned int i;
	int port;

	for (i = 0; i < tegra->num_usb_phys; i++) {
		if (is_usb2_otg_phy(tegra, i)) {
			port = tegra_xusb_padctl_get_usb3_companion(padctl, i);
			if ((port >= 0) && (index == (unsigned int)port))
				return true;
		}
	}

	return false;
}

static bool is_host_mode_phy(struct tegra_xusb *tegra, unsigned int phy_type, unsigned int index)
{
	if (strcmp(tegra->soc->phy_types[phy_type].name, "hsic") == 0)
		return true;

	if (strcmp(tegra->soc->phy_types[phy_type].name, "usb2") == 0) {
		if (is_usb2_otg_phy(tegra, index))
			return ((index == tegra->otg_usb2_port) && tegra->host_mode);
		else
			return true;
	}

	if (strcmp(tegra->soc->phy_types[phy_type].name, "usb3") == 0) {
		if (is_usb3_otg_phy(tegra, index))
			return ((index == tegra->otg_usb3_port) && tegra->host_mode);
		else
			return true;
	}

	return false;
}

static int tegra_xusb_get_usb2_port(struct tegra_xusb *tegra,
					      struct usb_phy *usbphy)
{
	unsigned int i;

	for (i = 0; i < tegra->num_usb_phys; i++) {
		if (tegra->usbphy[i] && usbphy == tegra->usbphy[i])
			return i;
	}

	return -1;
}

static int tegra_xhci_id_notify(struct notifier_block *nb,
					 unsigned long action, void *data)
{
	struct tegra_xusb *tegra = container_of(nb, struct tegra_xusb,
						    id_nb);
	struct usb_phy *usbphy = (struct usb_phy *)data;

	dev_dbg(tegra->dev, "%s(): action is %d", __func__, usbphy->last_event);

	if ((tegra->host_mode && usbphy->last_event == USB_EVENT_ID) ||
		(!tegra->host_mode && usbphy->last_event != USB_EVENT_ID)) {
		dev_dbg(tegra->dev, "Same role(%d) received. Ignore",
			tegra->host_mode);
		return NOTIFY_OK;
	}

	tegra->otg_usb2_port = tegra_xusb_get_usb2_port(tegra, usbphy);
	tegra->otg_usb3_port = tegra_xusb_padctl_get_usb3_companion(
							tegra->padctl,
							tegra->otg_usb2_port);

	tegra->host_mode = (usbphy->last_event == USB_EVENT_ID) ? true : false;

	schedule_work(&tegra->id_work);

	return NOTIFY_OK;
}

static int tegra_xusb_init_usb_phy(struct tegra_xusb *tegra)
{
	unsigned int i;

	tegra->usbphy = devm_kcalloc(tegra->dev, tegra->num_usb_phys,
				   sizeof(*tegra->usbphy), GFP_KERNEL);
	if (!tegra->usbphy)
		return -ENOMEM;

	INIT_WORK(&tegra->id_work, tegra_xhci_id_work);
	tegra->id_nb.notifier_call = tegra_xhci_id_notify;
	tegra->otg_usb2_port = -EINVAL;
	tegra->otg_usb3_port = -EINVAL;

	for (i = 0; i < tegra->num_usb_phys; i++) {
		struct phy *phy = tegra_xusb_get_phy(tegra, "usb2", i);

		if (!phy)
			continue;

		tegra->usbphy[i] = devm_usb_get_phy_by_node(tegra->dev,
							phy->dev.of_node,
							&tegra->id_nb);
		if (!IS_ERR(tegra->usbphy[i])) {
			dev_dbg(tegra->dev, "usbphy-%d registered", i);
			otg_set_host(tegra->usbphy[i]->otg, &tegra->hcd->self);
		} else {
			/*
			 * usb-phy is optional, continue if its not available.
			 */
			tegra->usbphy[i] = NULL;
		}
	}

	return 0;
}

static void tegra_xusb_deinit_usb_phy(struct tegra_xusb *tegra)
{
	unsigned int i;

	cancel_work_sync(&tegra->id_work);

	for (i = 0; i < tegra->num_usb_phys; i++)
		if (tegra->usbphy[i])
			otg_set_host(tegra->usbphy[i]->otg, NULL);
}

static void tegra_xusb_enable_eu3s(struct tegra_xusb *tegra)
{
	struct xhci_hcd *xhci;
	u32 value;

	xhci = hcd_to_xhci(tegra->hcd);

	/* Enable EU3S bit of USBCMD */
	value = readl(&xhci->op_regs->command);
	value |= CMD_PM_INDEX;
	writel(value, &xhci->op_regs->command);
}

static int tegra_sysfs_register(struct platform_device *pdev)
{
	int ret = 0;
	struct device *dev = NULL;

	if (pdev != NULL)
		dev = &pdev->dev;

	if (!xhci_err_init && dev != NULL) {
		ret = sysfs_create_group(&dev->kobj, &tegra_sysfs_group_errors);
		xhci_err_init = true;
	}

	if (ret) {
		pr_err("%s: failed to create tegra sysfs group %s\n",
			__func__, tegra_sysfs_group_errors.name);
	}

	return ret;
}

static irqreturn_t tegra_xhci_pad_ivc_irq(int irq, void *data)
{
	struct tegra_xusb *tegra = data;
	int ret;

	if (tegra_hv_ivc_channel_notified(tegra->ivck) != 0) {
		dev_info(tegra->dev, "ivc channel not usable\n");
		return IRQ_HANDLED;
	}

	if (tegra_hv_ivc_can_read(tegra->ivck)) {
		/* Read the current message for the xhci_server to be
		 * able to send further messages on next pad interrupt
		 */
		ret = tegra_hv_ivc_read(tegra->ivck, tegra->ivc_rx, 128);
		if (ret < 0) {
			dev_err(tegra->dev,
				"IVC Read of PAD Interrupt Failed: %d\n", ret);
		} else {
			/* Schedule work to execute the PADCTL IRQ Function
			 * which takes the appropriate action.
			 */
			schedule_work(&tegra->ivc_work);
		}
	} else {
		dev_info(tegra->dev, "Can not read ivc channel: %d\n",
							tegra->ivck->irq);
	}
	return IRQ_HANDLED;
}

static void tegra_xhci_ivc_work(struct work_struct *work)
{
	struct tegra_xusb *tegra = container_of(work, struct tegra_xusb,
						ivc_work);

	tegra_xusb_padctl_irq(tegra->ivck->irq, tegra);
}

int init_ivc_communication(struct platform_device *pdev)
{
	uint32_t id;
	int ret;
	struct device *dev = &pdev->dev;
	struct tegra_xusb *tegra = platform_get_drvdata(pdev);
	struct device_node *np, *hv_np;

	np = dev->of_node;
	if (!np) {
		dev_err(dev, "init_ivc: couldnt get of_node handle\n");
		return -EINVAL;
	}

	hv_np = of_parse_phandle(np, "ivc", 0);
	if (!hv_np) {
		dev_err(dev, "ivc_init: couldnt find ivc DT node\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(np, "ivc", 1, &id);
	if (ret) {
		dev_err(dev, "ivc_init: Error in reading IVC DT\n");
		of_node_put(hv_np);
		return -EINVAL;
	}

	tegra->ivck = tegra_hv_ivc_reserve(hv_np, id, NULL);
	of_node_put(hv_np);
	if (IS_ERR_OR_NULL(tegra->ivck)) {
		dev_err(dev, "Failed to reserve ivc channel:%d\n", id);
		ret = PTR_ERR(tegra->ivck);
		tegra->ivck = NULL;
		return ret;
	}

	dev_info(dev, "Reserved IVC channel #%d - frame_size=%d irq %d\n",
			id, tegra->ivck->frame_size, tegra->ivck->irq);

	tegra_hv_ivc_channel_reset(tegra->ivck);

	INIT_WORK(&tegra->ivc_work, tegra_xhci_ivc_work);

	ret = devm_request_irq(dev, tegra->ivck->irq, tegra_xhci_pad_ivc_irq,
						0, dev_name(dev), tegra);
	if (ret) {
		dev_err(dev, "Unable to request irq(%d)\n", tegra->ivck->irq);
		tegra_hv_ivc_unreserve(tegra->ivck);
		return ret;
	}

	return 0;
}

static ssize_t store_reload_hcd(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct tegra_xusb *tegra = platform_get_drvdata(pdev);
	struct usb_hcd	*hcd = tegra->hcd;
	int ret, reload;

	ret = kstrtoint(buf, 0, &reload);
	if (ret != 0 || reload < 0 || reload > 1)
		return -EINVAL;

	if (reload)
		tegra_xhci_hcd_reinit(hcd);

	return count;
}
static DEVICE_ATTR(reload_hcd, 0200, NULL, store_reload_hcd);

/* sysfs node for fw check*/
static ssize_t fw_version_show(struct device *dev, char *buf, size_t size)
{
	struct tegra_xusb *tegra = dev_get_drvdata(dev);
	struct tm time;

	if (!tegra)
		return scnprintf(buf, size, "device is not available\n");

	time64_to_tm(tegra->timestamp, 0, &time);

	return scnprintf(buf, size,
		"Firmware timestamp: %ld-%02d-%02d %02d:%02d:%02d UTC, Version: %2x.%02x %s\n",
		time.tm_year + 1900, time.tm_mon + 1, time.tm_mday,
		time.tm_hour, time.tm_min, time.tm_sec,
		FW_MAJOR_VERSION(tegra->version_id),
		FW_MINOR_VERSION(tegra->version_id),
		(tegra->build_log == LOG_MEMORY) ? "debug" : "release");
}

static int tegra_xusb_probe(struct platform_device *pdev)
{
	struct tegra_xusb *tegra;
	struct device_node *np;
	struct resource *res, *regs;
	struct xhci_hcd *xhci;
	unsigned int i, j, k;
	struct phy *phy;
	int err;

	BUILD_BUG_ON(sizeof(struct tegra_xusb_fw_header) != 256);

	tegra = devm_kzalloc(&pdev->dev, sizeof(*tegra), GFP_KERNEL);
	if (!tegra)
		return -ENOMEM;

	tegra->soc = of_device_get_match_data(&pdev->dev);
	mutex_init(&tegra->lock);
	tegra->dev = &pdev->dev;

	tegra_xusb_parse_dt(pdev, tegra);

	if (tegra->boost_emc_freq > 0) {
		dev_dbg(&pdev->dev, "BWMGR EMC freq boost enabled\n");
		tegra->emc_boost_enabled = true;
	}

	err = tegra_xusb_init_context(tegra);
	if (err < 0)
		return err;

	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	tegra->regs = devm_ioremap_resource(&pdev->dev, regs);
	if (IS_ERR(tegra->regs))
		return PTR_ERR(tegra->regs);

	if (!tegra->soc->is_xhci_vf) {
		tegra->fpci_base = devm_platform_ioremap_resource(pdev, 1);
		if (IS_ERR(tegra->fpci_base))
			return PTR_ERR(tegra->fpci_base);
		res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		tegra->fpci_start = res->start;
		tegra->fpci_len = resource_size(res);
	}

	if (tegra->soc->has_ipfs) {
		tegra->ipfs_base = devm_platform_ioremap_resource(pdev, 2);
		if (IS_ERR(tegra->ipfs_base))
			return PTR_ERR(tegra->ipfs_base);
	} else if (tegra->soc->has_bar2) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
		tegra->bar2_base = devm_ioremap_resource(&pdev->dev, res);
		if (IS_ERR(tegra->bar2_base))
			return PTR_ERR(tegra->bar2_base);

		tegra->bar2_start = res->start;
		tegra->bar2_len = resource_size(res);
	}

	tegra->xhci_irq = platform_get_irq(pdev, 0);
	if (tegra->xhci_irq < 0)
		return tegra->xhci_irq;

	if (!tegra->soc->is_xhci_vf) {
		tegra->mbox_irq = platform_get_irq(pdev, 1);
		if (tegra->mbox_irq < 0)
			return tegra->mbox_irq;
	}

	if ((tegra->soc->num_wakes > 0) &&
		!device_property_read_bool(tegra->dev, "disable-wake"))
		tegra->enable_wake = true;

	if (!tegra->soc->is_xhci_vf && tegra->enable_wake) {
		tegra->wake_irqs = devm_kcalloc(&pdev->dev,
					tegra->soc->num_wakes,
					sizeof(*tegra->wake_irqs), GFP_KERNEL);
		if (!tegra->wake_irqs)
			return -ENOMEM;

		for (i = 0; i < tegra->soc->num_wakes; i++) {
			char irq_name[] = "wakeX";
			struct irq_desc *desc;

			snprintf(irq_name, sizeof(irq_name), "wake%d", i);
			tegra->wake_irqs[i] = platform_get_irq_byname(pdev, irq_name);
			if (tegra->wake_irqs[i] < 0)
				return tegra->wake_irqs[i];
			desc = irq_to_desc(tegra->wake_irqs[i]);
			if (!desc)
				return -EINVAL;
			irq_set_irq_type(tegra->wake_irqs[i],
					irqd_get_trigger_type(&desc->irq_data));
		}
	}

	tegra->padctl = tegra_xusb_padctl_get(&pdev->dev);
	if (IS_ERR(tegra->padctl))
		return PTR_ERR(tegra->padctl);

	np = of_parse_phandle(pdev->dev.of_node, "nvidia,xusb-padctl", 0);
	if (!np)
		return -ENODEV;

	if (tegra->soc->is_xhci_vf)
		goto skip_clock_and_reg;

	tegra->padctl_irq = of_irq_get(np, 0);
	if (tegra->padctl_irq <= 0)
		return (tegra->padctl_irq == 0) ? -ENODEV : tegra->padctl_irq;

	tegra->host_clk = devm_clk_get(&pdev->dev, "xusb_host");
	if (IS_ERR(tegra->host_clk)) {
		err = PTR_ERR(tegra->host_clk);
		dev_err(&pdev->dev, "failed to get xusb_host: %d\n", err);
		goto put_padctl;
	}

	tegra->falcon_clk = devm_clk_get(&pdev->dev, "xusb_falcon_src");
	if (IS_ERR(tegra->falcon_clk)) {
		err = PTR_ERR(tegra->falcon_clk);
		dev_err(&pdev->dev, "failed to get xusb_falcon_src: %d\n", err);
		goto put_padctl;
	}

	tegra->ss_clk = devm_clk_get(&pdev->dev, "xusb_ss");
	if (IS_ERR(tegra->ss_clk)) {
		err = PTR_ERR(tegra->ss_clk);
		dev_err(&pdev->dev, "failed to get xusb_ss: %d\n", err);
		goto put_padctl;
	}

	tegra->ss_src_clk = devm_clk_get(&pdev->dev, "xusb_ss_src");
	if (IS_ERR(tegra->ss_src_clk)) {
		err = PTR_ERR(tegra->ss_src_clk);
		dev_err(&pdev->dev, "failed to get xusb_ss_src: %d\n", err);
		goto put_padctl;
	}

	tegra->hs_src_clk = devm_clk_get(&pdev->dev, "xusb_hs_src");
	if (IS_ERR(tegra->hs_src_clk)) {
		err = PTR_ERR(tegra->hs_src_clk);
		dev_err(&pdev->dev, "failed to get xusb_hs_src: %d\n", err);
		goto put_padctl;
	}

	tegra->fs_src_clk = devm_clk_get(&pdev->dev, "xusb_fs_src");
	if (IS_ERR(tegra->fs_src_clk)) {
		err = PTR_ERR(tegra->fs_src_clk);
		dev_err(&pdev->dev, "failed to get xusb_fs_src: %d\n", err);
		goto put_padctl;
	}

	tegra->pll_u_480m = devm_clk_get(&pdev->dev, "pll_u_480m");
	if (IS_ERR(tegra->pll_u_480m)) {
		err = PTR_ERR(tegra->pll_u_480m);
		dev_err(&pdev->dev, "failed to get pll_u_480m: %d\n", err);
		goto put_padctl;
	}

	tegra->clk_m = devm_clk_get(&pdev->dev, "clk_m");
	if (IS_ERR(tegra->clk_m)) {
		err = PTR_ERR(tegra->clk_m);
		dev_err(&pdev->dev, "failed to get clk_m: %d\n", err);
		goto put_padctl;
	}

	tegra->pll_e = devm_clk_get(&pdev->dev, "pll_e");
	if (IS_ERR(tegra->pll_e)) {
		err = PTR_ERR(tegra->pll_e);
		dev_err(&pdev->dev, "failed to get pll_e: %d\n", err);
		goto put_padctl;
	}

	if (!of_property_read_bool(pdev->dev.of_node, "power-domains")) {
		tegra->host_rst = devm_reset_control_get(&pdev->dev,
							 "xusb_host");
		if (IS_ERR(tegra->host_rst)) {
			err = PTR_ERR(tegra->host_rst);
			dev_err(&pdev->dev,
				"failed to get xusb_host reset: %d\n", err);
			goto put_padctl;
		}

		tegra->ss_rst = devm_reset_control_get(&pdev->dev, "xusb_ss");
		if (IS_ERR(tegra->ss_rst)) {
			err = PTR_ERR(tegra->ss_rst);
			dev_err(&pdev->dev, "failed to get xusb_ss reset: %d\n",
				err);
			goto put_padctl;
		}
	} else {
		err = tegra_xusb_powerdomain_init(&pdev->dev, tegra);
		if (err)
			goto put_powerdomains;
	}

	tegra->supplies = devm_kcalloc(&pdev->dev, tegra->soc->num_supplies,
				       sizeof(*tegra->supplies), GFP_KERNEL);
	if (!tegra->supplies) {
		err = -ENOMEM;
		goto put_powerdomains;
	}

	regulator_bulk_set_supply_names(tegra->supplies,
					tegra->soc->supply_names,
					tegra->soc->num_supplies);

	err = devm_regulator_bulk_get(&pdev->dev, tegra->soc->num_supplies,
				      tegra->supplies);
	if (err) {
		dev_err(&pdev->dev, "failed to get regulators: %d\n", err);
		goto put_powerdomains;
	}

	if (tegra_platform_is_fpga()) {
		err = fpga_clock_hacks(pdev);
		if (err)
			goto put_powerdomains;
	}

skip_clock_and_reg:
	for (i = 0; i < tegra->soc->num_types; i++) {
		if (!strncmp(tegra->soc->phy_types[i].name, "usb2", 4))
			tegra->num_usb_phys = tegra->soc->phy_types[i].num;
		tegra->num_phys += tegra->soc->phy_types[i].num;
	}

	tegra->phys = devm_kcalloc(&pdev->dev, tegra->num_phys,
				   sizeof(*tegra->phys), GFP_KERNEL);
	if (!tegra->phys) {
		err = -ENOMEM;
		goto put_powerdomains;
	}

	for (i = 0, k = 0; i < tegra->soc->num_types; i++) {
		char prop[16];

		for (j = 0; j < tegra->soc->phy_types[i].num; j++) {
			if (tegra->soc->is_xhci_vf) {
				snprintf(prop, sizeof(prop), "vf%d-%s-%d",
					tegra->soc->vf_id,
					 tegra->soc->phy_types[i].name, j);
			} else {
				snprintf(prop, sizeof(prop), "%s-%d",
					 tegra->soc->phy_types[i].name, j);
			}

			phy = devm_phy_optional_get(&pdev->dev, prop);
			if (IS_ERR(phy)) {
				dev_err(&pdev->dev,
					"failed to get PHY %s: %ld\n", prop,
					PTR_ERR(phy));
				err = PTR_ERR(phy);
				goto put_powerdomains;
			}

			if (phy || !tegra->soc->is_xhci_vf)
				tegra->phys[k++] = phy;
		}
		if (tegra->soc->is_xhci_vf)
			k = tegra->soc->phy_types[i].num;
	}

	tegra->hcd = usb_create_hcd(&tegra_xhci_hc_driver, &pdev->dev,
				    dev_name(&pdev->dev));
	if (!tegra->hcd) {
		err = -ENOMEM;
		goto put_powerdomains;
	}

	tegra->hcd->skip_phy_initialization = 1;
	tegra->hcd->regs = tegra->regs;
	tegra->hcd->rsrc_start = regs->start;
	tegra->hcd->rsrc_len = resource_size(regs);

	/*
	 * This must happen after usb_create_hcd(), because usb_create_hcd()
	 * will overwrite the drvdata of the device with the hcd it creates.
	 */
	platform_set_drvdata(pdev, tegra);

	if (tegra->soc->is_xhci_vf)
		goto skip_clock_and_reg_en;

	err = tegra_xusb_clk_enable(tegra);
	if (err) {
		dev_err(tegra->dev, "failed to enable clocks: %d\n", err);
		goto put_hcd;
	}

	err = regulator_bulk_enable(tegra->soc->num_supplies, tegra->supplies);
	if (err) {
		dev_err(tegra->dev, "failed to enable regulators: %d\n", err);
		goto disable_clk;
	}

skip_clock_and_reg_en:

	err = tegra_xusb_phy_enable(tegra);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to enable PHYs: %d\n", err);
		goto disable_regulator;
	}

	/*
	 * The XUSB Falcon microcontroller can only address 40 bits, so set
	 * the DMA mask accordingly.
	 */
	err = dma_set_mask_and_coherent(tegra->dev, DMA_BIT_MASK(40));
	if (err < 0) {
		dev_err(&pdev->dev, "failed to set DMA mask: %d\n", err);
		goto disable_phy;
	}

	tegra_xusb_debugfs_init(tegra);
	tegra_sysfs_register(pdev);

	if (tegra->soc->is_xhci_vf)
		goto skip_firmware_load;

	if (!tegra->soc->has_ifr || tegra->soc->load_ifr_rom) {
		err = tegra_xusb_request_firmware(tegra);
		if (err < 0) {
			dev_err(&pdev->dev,
				"failed to request firmware: %d\n", err);
			goto disable_phy;
		}
	}

	err = tegra_xusb_unpowergate_partitions(tegra);
	if (err)
		goto free_firmware;

	tegra_xusb_config(tegra);

	if (tegra->soc->disable_hsic_wake)
		tegra_xusb_disable_hsic_wake(tegra);

	if (tegra->soc->has_ifr)
		err = tegra_xusb_init_ifr_firmware(tegra);
	else
		err = tegra_xusb_load_firmware(tegra);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to load firmware: %d\n", err);
		goto powergate;
	}

skip_firmware_load:
	err = usb_add_hcd(tegra->hcd, tegra->xhci_irq, IRQF_SHARED);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to add USB HCD: %d\n", err);
		goto powergate;
	}

	device_wakeup_enable(tegra->hcd->self.controller);

	xhci = hcd_to_xhci(tegra->hcd);

	xhci->shared_hcd = usb_create_shared_hcd(&tegra_xhci_hc_driver,
						 &pdev->dev,
						 dev_name(&pdev->dev),
						 tegra->hcd);
	if (!xhci->shared_hcd) {
		dev_err(&pdev->dev, "failed to create shared HCD\n");
		err = -ENOMEM;
		goto remove_usb2;
	}

	if (HCC_MAX_PSA(xhci->hcc_params) >= 4)
		xhci->shared_hcd->can_do_streams = 1;

	err = usb_add_hcd(xhci->shared_hcd, tegra->xhci_irq, IRQF_SHARED);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to add shared HCD: %d\n", err);
		goto put_usb3;
	}

	if (tegra->soc->is_xhci_vf)
		goto skip_mbox_and_padctl;

	tegra->fwdev = devm_tegrafw_register(&pdev->dev, NULL,
			TFW_NORMAL, fw_version_show, NULL);
	if (IS_ERR(tegra->fwdev))
		dev_warn(&pdev->dev, "cannot register firmware reader");

	err = devm_request_threaded_irq(&pdev->dev, tegra->mbox_irq,
					tegra_xusb_mbox_irq,
					tegra_xusb_mbox_thread,
					IRQF_ONESHOT,
					dev_name(&pdev->dev), tegra);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to request IRQ: %d\n", err);
		goto remove_usb3;
	}

	err = devm_request_threaded_irq(&pdev->dev, tegra->padctl_irq, NULL, tegra_xusb_padctl_irq,
					IRQF_ONESHOT, dev_name(&pdev->dev), tegra);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to request padctl IRQ: %d\n", err);
		goto remove_mbox_irq;
	}

	err = tegra_xusb_enable_firmware_messages(tegra);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to enable messages: %d\n", err);
		goto remove_padctl_irq;
	}

skip_mbox_and_padctl:
	if (tegra->soc->is_xhci_vf) {
		/* In Virtualization, the PAD interrupt is owned by xhci_server
		 *  and Host driver needs to initiate an IVC channel to receive
		 * the interruptd from the xhci_server.
		 */
		err = init_ivc_communication(pdev);
		if (err < 0) {
			dev_err(&pdev->dev,
			"Failed to init IVC channel with xhci_server\n");
			goto remove_padctl_irq;
		}
	}

	err = tegra_xusb_init_usb_phy(tegra);
	if (err < 0) {
		dev_err(&pdev->dev, "failed to init USB PHY: %d\n", err);
		goto ivc_unreserve;
	}

	tegra_xusb_enable_eu3s(tegra);
	/* Enable Async suspend mode, to resume early */
	device_enable_async_suspend(tegra->dev);

	/* Init BWMGR for EMC boost */
	if (tegra->emc_boost_enabled)
		tegra_xusb_boost_emc_init(tegra);

	/* Enable wake for both USB 2.0 and USB 3.0 roothubs */
	device_init_wakeup(&tegra->hcd->self.root_hub->dev, true);
	device_init_wakeup(&xhci->shared_hcd->self.root_hub->dev, true);
	device_init_wakeup(tegra->dev, true);

	pm_runtime_use_autosuspend(tegra->dev);
	pm_runtime_set_autosuspend_delay(tegra->dev, 2000);
	pm_runtime_mark_last_busy(tegra->dev);
	pm_runtime_set_active(tegra->dev);
	pm_runtime_enable(tegra->dev);

	err = device_create_file(tegra->dev, &dev_attr_reload_hcd);
	if (err) {
		dev_err(tegra->dev,
			"Can't register reload_hcd attribute\n");
		goto ivc_unreserve;
	}

	INIT_WORK(&xhci->tegra_xhci_reinit_work, xhci_reinit_work);
	xhci->recovery_in_progress = false;
	xhci->pdev = pdev;

	return 0;

ivc_unreserve:
	if (tegra->soc->is_xhci_vf)
		tegra_hv_ivc_unreserve(tegra->ivck);
remove_padctl_irq:
	if (!tegra->soc->is_xhci_vf)
		devm_free_irq(tegra->dev, tegra->padctl_irq, tegra);
remove_mbox_irq:
	if (!tegra->soc->is_xhci_vf)
		devm_free_irq(tegra->dev, tegra->mbox_irq, tegra);
remove_usb3:
	usb_remove_hcd(xhci->shared_hcd);
put_usb3:
	usb_put_hcd(xhci->shared_hcd);
remove_usb2:
	usb_remove_hcd(tegra->hcd);
powergate:
	if (!tegra->soc->is_xhci_vf)
		tegra_xusb_powergate_partitions(tegra);
free_firmware:
	dma_free_coherent(&pdev->dev, tegra->fw.size, tegra->fw.virt,
			  tegra->fw.phys);
disable_phy:
	tegra_xusb_debugfs_deinit(tegra);
	tegra_xusb_phy_disable(tegra);
disable_regulator:
	if (!tegra->soc->is_xhci_vf)
		regulator_bulk_disable(tegra->soc->num_supplies,
				       tegra->supplies);
disable_clk:
	if (!tegra->soc->is_xhci_vf)
		tegra_xusb_clk_disable(tegra);
put_hcd:
	usb_put_hcd(tegra->hcd);
put_powerdomains:
	if (!tegra->soc->is_xhci_vf)
		tegra_xusb_powerdomain_remove(&pdev->dev, tegra);
put_padctl:
	tegra_xusb_padctl_put(tegra->padctl);
	return err;
}

static void tegra_xusb_power_down(struct tegra_xusb *tegra)
{
	if (tegra->soc->is_xhci_vf)
		goto skip_powerdomain;

	if (!of_property_read_bool(tegra->dev->of_node, "power-domains")) {
		tegra_powergate_power_off(TEGRA_POWERGATE_XUSBC);
		tegra_powergate_power_off(TEGRA_POWERGATE_XUSBA);
	} else {
		tegra_xusb_powerdomain_remove(tegra->dev, tegra);
	}

skip_powerdomain:
	tegra_xusb_phy_disable(tegra);
}

static void tegra_xusb_shutdown(struct platform_device *pdev)
{
	struct tegra_xusb *tegra = platform_get_drvdata(pdev);

	if (!tegra)
		return;

	pm_runtime_get_sync(tegra->dev);
	disable_irq(tegra->xhci_irq);

	if (tegra->hcd) {
		struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);

		clear_bit(HCD_FLAG_POLL_RH, &tegra->hcd->flags);
		del_timer_sync(&tegra->hcd->rh_timer);
		clear_bit(HCD_FLAG_POLL_RH, &xhci->shared_hcd->flags);
		del_timer_sync(&xhci->shared_hcd->rh_timer);

		xhci_shutdown(tegra->hcd);
	}

	tegra_xusb_power_down(tegra);
}

static int tegra_xusb_remove(struct platform_device *pdev)
{
	struct tegra_xusb *tegra = platform_get_drvdata(pdev);
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	struct device *dev = &pdev->dev;
	unsigned int i;

	if (xhci_err_init && dev != NULL) {
		sysfs_remove_group(&dev->kobj, &tegra_sysfs_group_errors);
		xhci_err_init = false;
	}

	if (tegra->emc_boost_enabled)
		tegra_xusb_boost_emc_deinit(tegra);

	if (tegra->soc->is_xhci_vf){
		cancel_work_sync(&tegra->ivc_work);
		if (tegra->ivck != NULL) {
			tegra_hv_ivc_unreserve(tegra->ivck);
			devm_free_irq(dev, tegra->ivck->irq, tegra);
		}
	}

	tegra_xusb_deinit_usb_phy(tegra);

	pm_runtime_get_sync(&pdev->dev);
	device_remove_file(&pdev->dev, &dev_attr_reload_hcd);
	usb_remove_hcd(xhci->shared_hcd);
	usb_put_hcd(xhci->shared_hcd);
	xhci->shared_hcd = NULL;
	usb_remove_hcd(tegra->hcd);
	disable_irq(tegra->xhci_irq);
	disable_irq(tegra->padctl_irq);
	if (!tegra->soc->is_xhci_vf) {
		disable_irq(tegra->mbox_irq);
		devm_iounmap(&pdev->dev, tegra->fpci_base);
		devm_release_mem_region(&pdev->dev, tegra->fpci_start,
			tegra->fpci_len);
	}

	if (!tegra->soc->is_xhci_vf && tegra->enable_wake) {
		for (i = 0; i < tegra->soc->num_wakes; i++)
			irq_dispose_mapping(tegra->wake_irqs[i]);
	}

	if (tegra->soc->has_bar2) {
		devm_iounmap(&pdev->dev, tegra->bar2_base);
		devm_release_mem_region(&pdev->dev, tegra->bar2_start,
			tegra->bar2_len);
	}

	devm_iounmap(&pdev->dev, tegra->hcd->regs);
	devm_release_mem_region(&pdev->dev, tegra->hcd->rsrc_start,
		tegra->hcd->rsrc_len);
	usb_put_hcd(tegra->hcd);

	if (tegra->soc->is_xhci_vf)
		goto skip_firmware_deinit;

	if (!IS_ERR(tegra->fwdev))
		devm_tegrafw_unregister(&pdev->dev, tegra->fwdev);

	dma_free_coherent(&pdev->dev, tegra->fw.size, tegra->fw.virt,
			  tegra->fw.phys);

	fw_log_deinit(tegra);

skip_firmware_deinit:
	pm_runtime_disable(&pdev->dev);
	pm_runtime_put(&pdev->dev);

	if (!tegra->soc->is_xhci_vf) {
		devm_free_irq(&pdev->dev, tegra->padctl_irq, tegra);
		devm_free_irq(&pdev->dev, tegra->mbox_irq, tegra);
	} else {
		goto skip_clock_and_powergate;
	}

	tegra_xusb_powergate_partitions(tegra);

	tegra_xusb_powerdomain_remove(&pdev->dev, tegra);

	tegra_xusb_phy_disable(tegra);
	tegra_xusb_clk_disable(tegra);
	regulator_bulk_disable(tegra->soc->num_supplies, tegra->supplies);

skip_clock_and_powergate:
	tegra_xusb_padctl_put(tegra->padctl);

	tegra_xusb_debugfs_deinit(tegra);

	return 0;
}

static int tegra_xhci_urb_enqueue(struct usb_hcd *hcd, struct urb *urb,
						gfp_t mem_flags)
{
	int xfertype;
	struct tegra_xusb *tegra = hcd_to_tegra_xusb(hcd);

	xfertype = usb_endpoint_type(&urb->ep->desc);
	switch (xfertype) {
	case USB_ENDPOINT_XFER_ISOC:
	case USB_ENDPOINT_XFER_BULK:
		if (tegra->emc_boost_enabled)
			schedule_work(&tegra->boost_emcfreq_work);
		break;
	case USB_ENDPOINT_XFER_INT:
	case USB_ENDPOINT_XFER_CONTROL:
	default:
		/* Do nothing special here */
		break;
	}
	return xhci_urb_enqueue(hcd, urb, mem_flags);
}

static inline u32 read_portsc(struct tegra_xusb *tegra, unsigned int port)
{
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);

	return readl(&xhci->op_regs->port_status_base + NUM_PORT_REGS * port);
}

static int tegra_xhci_hub_status_data(struct usb_hcd *hcd, char *buf)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct tegra_xusb *tegra = hcd_to_tegra_xusb(hcd);
	unsigned long flags;
	int port;

	if ((hcd->speed != HCD_USB3) ||
		!tegra->soc->disable_u0_ts1_detect)
		goto no_rx_control;

	for (port = 0; port < tegra->soc->phy_types[0].num; port++) {
		u32 portsc = read_portsc(tegra, port);
		struct phy *phy;

		if (portsc == 0xffffffff)
			break;

		spin_lock_irqsave(&xhci->lock, flags);
		phy = tegra->phys[port];
		if (!phy) {
			spin_unlock_irqrestore(&xhci->lock, flags);
			break;
		}
		if ((portsc & PORT_PLS_MASK) == XDEV_U0)
			tegra_xusb_padctl_disable_receiver_detector(
							tegra->padctl, phy);
		else {
			tegra_xusb_padctl_disable_clamp_en_early(
							tegra->padctl, phy);
			tegra_xusb_padctl_enable_receiver_detector(
							tegra->padctl, phy);
		}
		spin_unlock_irqrestore(&xhci->lock, flags);
	}

no_rx_control:
	return xhci_hub_status_data(hcd, buf);
}

static bool tegra_xhci_is_u0_ts1_detect_disabled(struct usb_hcd *hcd)
{
	struct tegra_xusb *tegra = hcd_to_tegra_xusb(hcd);

	return tegra->soc->disable_u0_ts1_detect;
}

static void tegra_xhci_endpoint_soft_retry(struct usb_hcd *hcd,
		struct usb_host_endpoint *ep, bool on)
{
	struct usb_device *udev = (struct usb_device *) ep->hcpriv;
	struct tegra_xusb *tegra = hcd_to_tegra_xusb(hcd);
	struct phy *phy;
	int port = -1;
	int delay = 0;
	u32 portsc;

	if (!udev || udev->speed != USB_SPEED_SUPER ||
					!(ep->desc.bEndpointAddress & USB_DIR_IN) ||
			!tegra->soc->disable_u0_ts1_detect)
		return;

	/* trace back to roothub port */
	while (udev->parent) {
		if (udev->parent == udev->bus->root_hub) {
			port = udev->portnum - 1;
			break;
		}
		udev = udev->parent;
	}

	if (port < 0 || port >= tegra->soc->phy_types[0].num)
		return;

	portsc = read_portsc(tegra, port);
	phy = tegra->phys[port];
	if (!phy)
		return;

	if (on) {
		while ((portsc & PORT_PLS_MASK) != XDEV_U0 && delay++ < 6) {
			udelay(50);
			portsc = read_portsc(tegra, port);
		}

		if ((portsc & PORT_PLS_MASK) != XDEV_U0) {
			dev_info(tegra->dev, "%s port %d doesn't reach U0 in 300us, portsc 0x%x\n",
				__func__, port, portsc);
		}
		tegra_xusb_padctl_disable_receiver_detector(tegra->padctl, phy);
		tegra_xusb_padctl_enable_clamp_en_early(tegra->padctl, phy);
	} else
		tegra_xusb_padctl_disable_clamp_en_early(tegra->padctl, phy);
}

#ifdef CONFIG_PM_SLEEP
static bool xhci_hub_ports_suspended(struct tegra_xusb *tegra, struct xhci_hub *hub)
{
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	struct device *dev = hub->hcd->self.controller;
	bool status = true;
	unsigned int i;
	u32 value;
	unsigned long flags;

	for (i = 0; i < hub->num_ports; i++) {
		value = readl(hub->ports[i]->addr);
		if ((value & PORT_PE) == 0)
			continue;

		if ((value & PORT_PLS_MASK) != XDEV_U3) {
			status = false;
			if (XHCI_IS_T210(tegra) &&
				DEV_SUPERSPEED(value)) {
				unsigned long end = jiffies + msecs_to_jiffies(200);

				while (time_before(jiffies, end)) {
					if ((value & PORT_PLS_MASK) == XDEV_RESUME)
						break;
					spin_unlock_irqrestore(&xhci->lock, flags);
					msleep(20);
					spin_lock_irqsave(&xhci->lock, flags);

					value = readl(hub->ports[i]->addr);
					if ((value & PORT_PLS_MASK) == XDEV_U3) {
						dev_info(dev, "%u-%u is suspended: %#010x\n",
							hub->hcd->self.busnum, i + 1, value);
						status = true;
						break;
					}
				}
			}

			if (!status) {
				dev_info(dev, "%u-%u isn't suspended: %#010x\n",
					 hub->hcd->self.busnum, i + 1, value);
			}
		}
	}

	return status;
}

static int tegra_xusb_check_ports(struct tegra_xusb *tegra)
{
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	struct xhci_hub *rhub =  xhci_get_rhub(xhci->main_hcd);
	struct xhci_bus_state *bus_state = &rhub->bus_state;
	unsigned long flags;
	int err = 0;

	if (bus_state->bus_suspended) {
		/* xusb_hub_suspend() has just directed one or more USB2 port(s)
		 * to U3 state, it takes 3ms to enter U3.
		 */
		usleep_range(3000, 4000);
	}

	spin_lock_irqsave(&xhci->lock, flags);

	if (!xhci_hub_ports_suspended(tegra, &xhci->usb2_rhub) ||
	    !xhci_hub_ports_suspended(tegra, &xhci->usb3_rhub))
		err = -EBUSY;

	spin_unlock_irqrestore(&xhci->lock, flags);

	return err;
}

static void tegra_xusb_save_context(struct tegra_xusb *tegra)
{
	const struct tegra_xusb_context_soc *soc = tegra->soc->context;
	struct tegra_xusb_context *ctx = &tegra->context;
	unsigned int i;

	if (soc->ipfs.num_offsets > 0) {
		for (i = 0; i < soc->ipfs.num_offsets; i++)
			ctx->ipfs[i] = ipfs_readl(tegra, soc->ipfs.offsets[i]);
	}

	if (soc->fpci.num_offsets > 0) {
		for (i = 0; i < soc->fpci.num_offsets; i++)
			ctx->fpci[i] = fpci_readl(tegra, soc->fpci.offsets[i]);
	}
}

static void tegra_xusb_restore_context(struct tegra_xusb *tegra)
{
	const struct tegra_xusb_context_soc *soc = tegra->soc->context;
	struct tegra_xusb_context *ctx = &tegra->context;
	unsigned int i;

	if (soc->fpci.num_offsets > 0) {
		for (i = 0; i < soc->fpci.num_offsets; i++)
			fpci_writel(tegra, ctx->fpci[i], soc->fpci.offsets[i]);
	}

	if (soc->ipfs.num_offsets > 0) {
		for (i = 0; i < soc->ipfs.num_offsets; i++)
			ipfs_writel(tegra, ctx->ipfs[i], soc->ipfs.offsets[i]);
	}
}

static enum usb_device_speed tegra_xhci_portsc_to_speed(struct tegra_xusb *tegra, u32 portsc)
{
	if (DEV_LOWSPEED(portsc))
		return USB_SPEED_LOW;

	if (DEV_HIGHSPEED(portsc))
		return USB_SPEED_HIGH;

	if (DEV_FULLSPEED(portsc))
		return USB_SPEED_FULL;

	if (DEV_SUPERSPEED_ANY(portsc))
		return USB_SPEED_SUPER;

	return USB_SPEED_UNKNOWN;
}

static void tegra_xhci_enable_phy_sleepwalk_wake(struct tegra_xusb *tegra)
{
	struct tegra_xusb_padctl *padctl = tegra->padctl;
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	enum usb_device_speed speed;
	struct phy *phy;
	unsigned int index, offset;
	unsigned int i, j, k;
	struct xhci_hub *rhub;
	u32 portsc;

	for (i = 0, k = 0; i < tegra->soc->num_types; i++) {
		if (strcmp(tegra->soc->phy_types[i].name, "usb3") == 0)
			rhub = &xhci->usb3_rhub;
		else
			rhub = &xhci->usb2_rhub;

		if (strcmp(tegra->soc->phy_types[i].name, "hsic") == 0)
			offset = tegra->soc->ports.usb2.count;
		else
			offset = 0;

		for (j = 0; j < tegra->soc->phy_types[i].num; j++) {
			phy = tegra->phys[k++];

			if (!phy)
				continue;

			index = j + offset;

			if (index >= rhub->num_ports)
				continue;

			if (!is_host_mode_phy(tegra, i, j))
				continue;

			portsc = readl(rhub->ports[index]->addr);
			if (!(portsc & PORT_WAKE_BITS))
				continue;
			speed = tegra_xhci_portsc_to_speed(tegra, portsc);
			tegra_xusb_padctl_enable_phy_sleepwalk(padctl, phy, speed);
			tegra_xusb_padctl_enable_phy_wake(padctl, phy);
		}
	}
}

static void tegra_xhci_disable_phy_wake(struct tegra_xusb *tegra)
{
	struct tegra_xusb_padctl *padctl = tegra->padctl;
	unsigned int i, j;
	char phy_name[5];

	for (i = 0; i < tegra->num_phys; i++) {
		if (!tegra->phys[i])
			continue;
		if (tegra_xusb_padctl_remote_wake_detected(padctl, tegra->phys[i])) {
			if (i < (tegra->soc->ports.usb3.offset +
					tegra->soc->ports.usb3.count)) {
				j = i;
				strcpy(phy_name, "usb3");
			} else if (i < (tegra->soc->ports.usb2.offset +
					tegra->soc->ports.usb2.count)) {
				j = i - tegra->soc->ports.usb2.offset;
				strcpy(phy_name, "usb2");

				tegra_phy_xusb_utmi_pad_power_on(tegra->phys[i]);
			} else {
				j = i - (tegra->soc->ports.usb2.offset +
					tegra->soc->ports.usb2.count);
				strcpy(phy_name, "hsic");
			}
			dev_dbg(tegra->dev,
				"%s port %u (0 based) remote wake detected\n", phy_name, j);

		}
		tegra_xusb_padctl_disable_phy_wake(padctl, tegra->phys[i]);
	}
}

static void tegra_xhci_disable_phy_sleepwalk(struct tegra_xusb *tegra)
{
	struct tegra_xusb_padctl *padctl = tegra->padctl;
	unsigned int i;

	for (i = 0; i < tegra->num_phys; i++) {
		if (!tegra->phys[i])
			continue;
		tegra_xusb_padctl_disable_phy_sleepwalk(padctl, tegra->phys[i]);
	}
}

static void tegra_xhci_program_utmi_power_lp0_exit(
	struct tegra_xusb *tegra)
{
	unsigned int i;

	for (i = 0; i < tegra->soc->ports.usb2.count; i++) {
		if (!is_host_mode_phy(tegra, USB2_PHY, i))
			continue;
		if (tegra->enable_utmi_pad_after_lp0_exit & BIT(i))
			tegra_phy_xusb_utmi_pad_power_on(
					tegra->phys[tegra->soc->ports.usb2.offset + i]);
		else
			tegra_phy_xusb_utmi_pad_power_down(
					tegra->phys[tegra->soc->ports.usb2.offset + i]);
	}
}

static int tegra_xusb_enter_elpg(struct tegra_xusb *tegra, bool runtime)
{
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	struct device *dev = tegra->dev;
	bool wakeup = runtime ? true : device_may_wakeup(dev);
	unsigned int i;
	int err;
	u32 usbcmd;
	u32 portsc;

	dev_dbg(dev, "entering ELPG\n");

	usbcmd = readl(&xhci->op_regs->command);
	usbcmd &= ~CMD_EIE;
	writel(usbcmd, &xhci->op_regs->command);

	err = tegra_xusb_check_ports(tegra);
	if (err < 0) {
		dev_err(tegra->dev, "not all ports suspended: %d\n", err);
		goto out;
	}

	for (i = 0; i < tegra->soc->ports.usb2.count; i++) {
		if (!xhci->usb2_rhub.ports[i])
			continue;
		portsc = readl(xhci->usb2_rhub.ports[i]->addr);
		tegra->enable_utmi_pad_after_lp0_exit &= ~BIT(i);
		if (((portsc & PORT_PLS_MASK) == XDEV_U3) ||
			((portsc & DEV_SPEED_MASK) == XDEV_FS))
			tegra->enable_utmi_pad_after_lp0_exit |= BIT(i);
	}

	err = xhci_suspend(xhci, wakeup);
	if (err < 0) {
		dev_err(tegra->dev, "failed to suspend XHCI: %d\n", err);
		goto out;
	}

	if (!tegra->soc->is_xhci_vf)
		tegra_xusb_save_context(tegra);

	if (wakeup)
		tegra_xhci_enable_phy_sleepwalk_wake(tegra);

	if (tegra->soc->is_xhci_vf)
		goto skip_firmware_and_powergate;

	/* In ELPG, firmware log context is gone. Rewind shared log buffer. */
	if (test_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags)) {
		if (!circ_buffer_full(&tegra->log.circ)) {
			if (fw_log_wait_empty_timeout(tegra, 500))
				dev_info(tegra->dev, "%s still has logs\n", __func__);
		}

		tegra->log.dequeue = tegra->log.virt_addr;
		tegra->log.seq = 0;
	}

	tegra_xusb_powergate_partitions(tegra);

	if ((!runtime) && (tegra->use_genpd)) {
		/*
		 * in system suspend path, phy_power_off()/phy_exit() will be done
		 * in genpd notifier.
		 */
		goto out;
	}

skip_firmware_and_powergate:

	for (i = 0; i < tegra->num_phys; i++) {
		if (!tegra->phys[i])
			continue;

		phy_power_off(tegra->phys[i]);
		if (!wakeup)
			phy_exit(tegra->phys[i]);
	}

	if (!tegra->soc->is_xhci_vf)
		tegra_xusb_clk_disable(tegra);

out:
	if (!err)
		dev_info(tegra->dev, "entering ELPG done\n");
	else {
		unsigned long flags;
		u32 usbcmd;

		spin_lock_irqsave(&xhci->lock, flags);

		usbcmd = readl(&xhci->op_regs->command);
		usbcmd |= CMD_EIE;
		writel(usbcmd, &xhci->op_regs->command);

		spin_unlock_irqrestore(&xhci->lock, flags);

		dev_info(tegra->dev, "entering ELPG failed\n");
		pm_runtime_mark_last_busy(tegra->dev);
	}

	return err;
}

static int tegra_xusb_exit_elpg(struct tegra_xusb *tegra, bool runtime)
{
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	struct device *dev = tegra->dev;
	bool wakeup = runtime ? true : device_may_wakeup(dev);
	unsigned int i;
	int err;
	u32 usbcmd;

	dev_dbg(dev, "exiting ELPG\n");
	pm_runtime_mark_last_busy(tegra->dev);

	if (tegra->soc->is_xhci_vf)
		goto skip_clock_and_powergate;

	if ((!runtime) && (tegra->use_genpd)) {
		/*
		 * in system resume path, skip enabling partition clocks because
		 * the clocks are not disabled at system suspend.
		 */
		goto skip_clk_enable;
	}

	err = tegra_xusb_clk_enable(tegra);
	if (err < 0) {
		dev_err(tegra->dev, "failed to enable clocks: %d\n", err);
		goto out;
	}


skip_clk_enable:
	err = tegra_xusb_unpowergate_partitions(tegra);
	if (err)
		goto disable_clks;

skip_clock_and_powergate:

	if (wakeup)
		tegra_xhci_disable_phy_wake(tegra);

	for (i = 0; i < tegra->num_phys; i++) {
		if (!tegra->phys[i])
			continue;

		if (!wakeup)
			phy_init(tegra->phys[i]);

		phy_power_on(tegra->phys[i]);
	}

	if (tegra->suspended)
		tegra_xhci_program_utmi_power_lp0_exit(tegra);

	if (!tegra->soc->is_xhci_vf)
		tegra_xusb_config(tegra);

	if (tegra->soc->disable_hsic_wake)
		tegra_xusb_disable_hsic_wake(tegra);

	if (tegra->soc->is_xhci_vf)
		goto skip_firmware;

	tegra_xusb_restore_context(tegra);

	if (tegra->soc->has_ifr)
		err = tegra_xusb_init_ifr_firmware(tegra);
	else
		err = tegra_xusb_load_firmware(tegra);
	if (err < 0) {
		dev_err(tegra->dev, "failed to load firmware: %d\n", err);
		goto disable_phy;
	}

	err = __tegra_xusb_enable_firmware_messages(tegra);
	if (err < 0) {
		dev_err(tegra->dev, "failed to enable messages: %d\n", err);
		goto disable_phy;
	}

skip_firmware:
	if (wakeup)
		tegra_xhci_disable_phy_sleepwalk(tegra);

	err = xhci_resume(xhci, 0);
	if (err < 0) {
		dev_err(tegra->dev, "failed to resume XHCI: %d\n", err);
		goto disable_phy;
	}

	usbcmd = readl(&xhci->op_regs->command);
	usbcmd |= CMD_EIE;
	writel(usbcmd, &xhci->op_regs->command);

	goto out;

disable_phy:
	for (i = 0; i < tegra->num_phys; i++) {
		if (!tegra->phys[i])
			continue;

		phy_power_off(tegra->phys[i]);
		if (!wakeup)
			phy_exit(tegra->phys[i]);
	}
	if (!tegra->soc->is_xhci_vf)
		tegra_xusb_powergate_partitions(tegra);
disable_clks:
	if (!tegra->soc->is_xhci_vf)
		tegra_xusb_clk_disable(tegra);
out:
	if (!err)
		dev_dbg(dev, "exiting ELPG done\n");
	else
		dev_dbg(dev, "exiting ELPG failed\n");

	return err;
}

static int tegra_xhci_add_endpoint(struct usb_hcd *hcd, struct usb_device *udev,
				struct usb_host_endpoint *ep)
{
	struct usb_endpoint_descriptor *desc = &ep->desc;

	if ((udev->speed >= USB_SPEED_SUPER) &&
		((desc->bEndpointAddress & USB_ENDPOINT_DIR_MASK) ==
			USB_DIR_OUT) && usb_endpoint_xfer_bulk(desc) &&
			max_burst_war_enable) {
		if (ep->ss_ep_comp.bMaxBurst != 15) {
			dev_dbg(&udev->dev, "change ep %02x bMaxBurst (%d) to 15\n",
				ep->ss_ep_comp.bMaxBurst,
				desc->bEndpointAddress);
			ep->ss_ep_comp.bMaxBurst = 15;
		}
	}

	return xhci_add_endpoint(hcd, udev, ep);
}

static int tegra_xusb_suspend(struct device *dev)
{
	struct tegra_xusb *tegra = dev_get_drvdata(dev);
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	int err;
	unsigned int i;

	if (xhci->recovery_in_progress)
		return 0;

	if (!tegra->soc->is_xhci_vf)
		synchronize_irq(tegra->mbox_irq);

	if (tegra->soc->is_xhci_vf)
		flush_work(&tegra->ivc_work);

	mutex_lock(&tegra->lock);

	if (pm_runtime_suspended(dev)) {
		err = tegra_xusb_exit_elpg(tegra, true);
		if (err < 0)
			goto out;
	}

	err = tegra_xusb_enter_elpg(tegra, false);
	if (err < 0) {
		if (pm_runtime_suspended(dev)) {
			pm_runtime_disable(dev);
			pm_runtime_set_active(dev);
			pm_runtime_enable(dev);
		}

		goto out;
	}

out:
	if (!err) {
		tegra->suspended = true;
		pm_runtime_disable(dev);

		if (!tegra->soc->is_xhci_vf && device_may_wakeup(dev)) {
			if (enable_irq_wake(tegra->padctl_irq))
				dev_err(dev, "failed to enable padctl wakes\n");

			if (tegra->enable_wake) {
				for (i = 0; i < tegra->soc->num_wakes; i++)
					enable_irq_wake(tegra->wake_irqs[i]);
			}
		}
	}

	mutex_unlock(&tegra->lock);

	return err;
}

static int tegra_xusb_resume(struct device *dev)
{
	struct tegra_xusb *tegra = dev_get_drvdata(dev);
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	int err;
	unsigned int i;

	if (xhci->recovery_in_progress)
		return 0;

	mutex_lock(&tegra->lock);

	if (!tegra->suspended) {
		mutex_unlock(&tegra->lock);
		return 0;
	}

	err = tegra_xusb_exit_elpg(tegra, false);
	if (err < 0) {
		mutex_unlock(&tegra->lock);
		return err;
	}

	if (!tegra->soc->is_xhci_vf && device_may_wakeup(dev)) {
		if (disable_irq_wake(tegra->padctl_irq))
			dev_err(dev, "failed to disable padctl wakes\n");

		if (tegra->enable_wake) {
			for (i = 0; i < tegra->soc->num_wakes; i++)
				disable_irq_wake(tegra->wake_irqs[i]);
		}
	}

	tegra->suspended = false;
	mutex_unlock(&tegra->lock);

	pm_runtime_set_active(dev);
	pm_runtime_enable(dev);

	return 0;
}
#endif

#ifdef CONFIG_PM
static int tegra_xusb_runtime_suspend(struct device *dev)
{
	struct tegra_xusb *tegra = dev_get_drvdata(dev);
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	int ret;

	if (xhci->recovery_in_progress)
		return 0;

	if (!tegra->soc->is_xhci_vf)
		synchronize_irq(tegra->mbox_irq);

	mutex_lock(&tegra->lock);
	ret = tegra_xusb_enter_elpg(tegra, true);
	mutex_unlock(&tegra->lock);

	return ret;
}

static int tegra_xusb_runtime_resume(struct device *dev)
{
	struct tegra_xusb *tegra = dev_get_drvdata(dev);
	struct xhci_hcd *xhci = hcd_to_xhci(tegra->hcd);
	int err;

	if (xhci->recovery_in_progress)
		return 0;

	mutex_lock(&tegra->lock);
	err = tegra_xusb_exit_elpg(tegra, true);
	mutex_unlock(&tegra->lock);

	return err;
}
#endif

static const struct dev_pm_ops tegra_xusb_pm_ops = {
	SET_RUNTIME_PM_OPS(tegra_xusb_runtime_suspend,
			   tegra_xusb_runtime_resume, NULL)
	SET_SYSTEM_SLEEP_PM_OPS(tegra_xusb_suspend, tegra_xusb_resume)
};

static const char * const tegra124_supply_names[] = {
	"avddio-pex",
	"dvddio-pex",
	"avdd-usb",
	"hvdd-usb-ss",
};

static const struct tegra_xusb_phy_type tegra124_phy_types[] = {
	{ .name = "usb3", .num = 2, },
	{ .name = "usb2", .num = 3, },
	{ .name = "hsic", .num = 2, },
};

static const unsigned int tegra124_xusb_context_ipfs[] = {
	IPFS_XUSB_HOST_MSI_BAR_SZ_0,
	IPFS_XUSB_HOST_MSI_AXI_BAR_ST_0,
	IPFS_XUSB_HOST_MSI_FPCI_BAR_ST_0,
	IPFS_XUSB_HOST_MSI_VEC0_0,
	IPFS_XUSB_HOST_MSI_EN_VEC0_0,
	IPFS_XUSB_HOST_FPCI_ERROR_MASKS_0,
	IPFS_XUSB_HOST_INTR_MASK_0,
	IPFS_XUSB_HOST_INTR_ENABLE_0,
	IPFS_XUSB_HOST_UFPCI_CONFIG_0,
	IPFS_XUSB_HOST_CLKGATE_HYSTERESIS_0,
	IPFS_XUSB_HOST_MCCIF_FIFOCTRL_0,
};

static const unsigned int tegra124_xusb_context_fpci[] = {
	XUSB_CFG_ARU_CONTEXT_HS_PLS,
	XUSB_CFG_ARU_CONTEXT_FS_PLS,
	XUSB_CFG_ARU_CONTEXT_HSFS_SPEED,
	XUSB_CFG_ARU_CONTEXT_HSFS_PP,
	XUSB_CFG_ARU_CONTEXT,
	XUSB_CFG_AXI_CFG,
	XUSB_CFG_24,
	XUSB_CFG_16,
};

static const struct tegra_xusb_context_soc tegra124_xusb_context = {
	.ipfs = {
		.num_offsets = ARRAY_SIZE(tegra124_xusb_context_ipfs),
		.offsets = tegra124_xusb_context_ipfs,
	},
	.fpci = {
		.num_offsets = ARRAY_SIZE(tegra124_xusb_context_fpci),
		.offsets = tegra124_xusb_context_fpci,
	},
};

static struct tegra_xusb_soc_ops tegra124_ops = {
	.mbox_reg_readl = &fpci_readl,
	.mbox_reg_writel = &fpci_writel,
	.csb_reg_readl = &fpci_csb_readl,
	.csb_reg_writel = &fpci_csb_writel,
};

static const struct tegra_xusb_soc tegra124_soc = {
	.firmware = "nvidia/tegra124/xusb.bin",
	.supply_names = tegra124_supply_names,
	.num_supplies = ARRAY_SIZE(tegra124_supply_names),
	.phy_types = tegra124_phy_types,
	.num_types = ARRAY_SIZE(tegra124_phy_types),
	.context = &tegra124_xusb_context,
	.ports = {
		.usb2 = { .offset = 4, .count = 4, },
		.hsic = { .offset = 6, .count = 2, },
		.usb3 = { .offset = 0, .count = 2, },
	},
	.scale_ss_clock = true,
	.has_ipfs = true,
	.otg_reset_sspi = false,
	.disable_hsic_wake = false,
	.ops = &tegra124_ops,
	.mbox = {
		.cmd = 0xe4,
		.data_in = 0xe8,
		.data_out = 0xec,
		.owner = 0xf0,
		.smi_intr = XUSB_CFG_ARU_SMI_INTR,
	},
};
MODULE_FIRMWARE("nvidia/tegra124/xusb.bin");

static const char * const tegra210_supply_names[] = {
	"dvddio-pex",
	"hvddio-pex",
	"avdd-usb",
};

static const struct tegra_xusb_phy_type tegra210_phy_types[] = {
	{ .name = "usb3", .num = 4, },
	{ .name = "usb2", .num = 4, },
	{ .name = "hsic", .num = 1, },
};

static const struct tegra_xusb_soc tegra210_soc = {
	.device_id = XHCI_DEVICE_ID_T210,
	.firmware = "nvidia/tegra210/xusb.bin",
	.supply_names = tegra210_supply_names,
	.num_supplies = ARRAY_SIZE(tegra210_supply_names),
	.phy_types = tegra210_phy_types,
	.num_types = ARRAY_SIZE(tegra210_phy_types),
	.context = &tegra124_xusb_context,
	.ports = {
		.usb2 = { .offset = 4, .count = 4, },
		.hsic = { .offset = 8, .count = 1, },
		.usb3 = { .offset = 0, .count = 4, },
	},
	.scale_ss_clock = false,
	.has_ipfs = true,
	.otg_reset_sspi = true,
	.disable_hsic_wake = false,
	.ops = &tegra124_ops,
	.mbox = {
		.cmd = 0xe4,
		.data_in = 0xe8,
		.data_out = 0xec,
		.owner = 0xf0,
		.smi_intr = XUSB_CFG_ARU_SMI_INTR,
	},
	.disable_u0_ts1_detect = true,
};
MODULE_FIRMWARE("nvidia/tegra210/xusb.bin");

static const char * const tegra210b01_supply_names[] = {
	"hvdd_usb",
	"avdd_pll_utmip",
	"avddio_usb",
	"avddio_pll_uerefe",
};

static const struct tegra_xusb_phy_type tegra210b01_phy_types[] = {
	{ .name = "usb3", .num = 4, },
	{ .name = "usb2", .num = 4, },
	{ .name = "hsic", .num = 1, },
};

static const struct tegra_xusb_soc tegra210b01_soc = {
	.device_id = XHCI_DEVICE_ID_T210,
	.firmware = "nvidia/tegra210b01/xusb.bin",
	.supply_names = tegra210b01_supply_names,
	.num_supplies = ARRAY_SIZE(tegra210b01_supply_names),
	.phy_types = tegra210b01_phy_types,
	.num_types = ARRAY_SIZE(tegra210b01_phy_types),
	.context = &tegra124_xusb_context,
	.ports = {
		.usb2 = { .offset = 4, .count = 4, },
		.hsic = { .offset = 8, .count = 1, },
		.usb3 = { .offset = 0, .count = 4, },
	},
	.scale_ss_clock = false,
	.has_ipfs = true,
	.otg_reset_sspi = true,
	.disable_hsic_wake = true,
	.ops = &tegra124_ops,
	.mbox = {
		.cmd = 0xe4,
		.data_in = 0xe8,
		.data_out = 0xec,
		.owner = 0xf0,
		.smi_intr = XUSB_CFG_ARU_SMI_INTR,
	},
};
MODULE_FIRMWARE("nvidia/tegra210b01/xusb.bin");

static const char * const tegra186_supply_names[] = {
};
MODULE_FIRMWARE("nvidia/tegra186/xusb.bin");

static const struct tegra_xusb_phy_type tegra186_phy_types[] = {
	{ .name = "usb3", .num = 3, },
	{ .name = "usb2", .num = 3, },
	{ .name = "hsic", .num = 1, },
};

static const struct tegra_xusb_context_soc tegra186_xusb_context = {
	.fpci = {
		.num_offsets = ARRAY_SIZE(tegra124_xusb_context_fpci),
		.offsets = tegra124_xusb_context_fpci,
	},
};

static const struct tegra_xusb_soc tegra186_soc = {
	.firmware = "nvidia/tegra186/xusb.bin",
	.supply_names = tegra186_supply_names,
	.num_supplies = ARRAY_SIZE(tegra186_supply_names),
	.phy_types = tegra186_phy_types,
	.num_types = ARRAY_SIZE(tegra186_phy_types),
	.context = &tegra186_xusb_context,
	.ports = {
		.usb3 = { .offset = 0, .count = 3, },
		.usb2 = { .offset = 3, .count = 3, },
		.hsic = { .offset = 6, .count = 1, },
	},
	.scale_ss_clock = false,
	.has_ipfs = false,
	.otg_reset_sspi = false,
	.disable_hsic_wake = false,
	.ops = &tegra124_ops,
	.mbox = {
		.cmd = 0xe4,
		.data_in = 0xe8,
		.data_out = 0xec,
		.owner = 0xf0,
		.smi_intr = XUSB_CFG_ARU_SMI_INTR,
	},
	.lpm_support = true,
	.disable_u0_ts1_detect = false,
};

static const char * const tegra194_supply_names[] = {
};

static const struct tegra_xusb_phy_type tegra194_phy_types[] = {
	{ .name = "usb3", .num = 4, },
	{ .name = "usb2", .num = 4, },
};

static const struct tegra_xusb_soc tegra194_soc = {
	.firmware = "nvidia/tegra194/xusb.bin",
	.supply_names = tegra194_supply_names,
	.num_supplies = ARRAY_SIZE(tegra194_supply_names),
	.phy_types = tegra194_phy_types,
	.num_types = ARRAY_SIZE(tegra194_phy_types),
	.num_wakes = 7,
	.context = &tegra186_xusb_context,
	.ports = {
		.usb3 = { .offset = 0, .count = 4, },
		.usb2 = { .offset = 4, .count = 4, },
	},
	.scale_ss_clock = false,
	.has_ipfs = false,
	.otg_reset_sspi = false,
	.disable_hsic_wake = false,
	.ops = &tegra124_ops,
	.mbox = {
		.cmd = 0x68,
		.data_in = 0x6c,
		.data_out = 0x70,
		.owner = 0x74,
		.smi_intr = XUSB_CFG_ARU_SMI_INTR,
	},
	.lpm_support = true,
	.disable_u0_ts1_detect = false,
};
MODULE_FIRMWARE("nvidia/tegra194/xusb.bin");

static const struct tegra_xusb_soc tegra194_vf1_soc = {
	.firmware = "nvidia/tegra194/xusb.bin",
	.is_xhci_vf = true,
	.vf_id = 1,
	.supply_names = tegra194_supply_names,
	.num_supplies = ARRAY_SIZE(tegra194_supply_names),
	.phy_types = tegra194_phy_types,
	.num_types = ARRAY_SIZE(tegra194_phy_types),
	.context = &tegra186_xusb_context,
	.ports = {
		.usb3 = { .offset = 0, .count = 4, },
		.usb2 = { .offset = 4, .count = 4, },
	},
	.scale_ss_clock = false,
	.has_ipfs = false,
	.otg_reset_sspi = false,
	.disable_hsic_wake = false,
	.ops = &tegra124_ops,
	.mbox = {
		.cmd = 0x68,
		.data_in = 0x6c,
		.data_out = 0x70,
		.owner = 0x74,
	},
	.lpm_support = true,
	.disable_u0_ts1_detect = false,
};

static const struct tegra_xusb_soc tegra194_vf2_soc = {
	.firmware = "nvidia/tegra194/xusb.bin",
	.is_xhci_vf = true,
	.vf_id = 2,
	.supply_names = tegra194_supply_names,
	.num_supplies = ARRAY_SIZE(tegra194_supply_names),
	.phy_types = tegra194_phy_types,
	.num_types = ARRAY_SIZE(tegra194_phy_types),
	.context = &tegra186_xusb_context,
	.ports = {
		.usb3 = { .offset = 0, .count = 4, },
		.usb2 = { .offset = 4, .count = 4, },
	},
	.scale_ss_clock = false,
	.has_ipfs = false,
	.otg_reset_sspi = false,
	.disable_hsic_wake = false,
	.ops = &tegra124_ops,
	.mbox = {
		.cmd = 0x68,
		.data_in = 0x6c,
		.data_out = 0x70,
		.owner = 0x74,
	},
	.lpm_support = true,
	.disable_u0_ts1_detect = false,
};

static const struct tegra_xusb_soc tegra194_vf3_soc = {
	.firmware = "nvidia/tegra194/xusb.bin",
	.is_xhci_vf = true,
	.vf_id = 3,
	.supply_names = tegra194_supply_names,
	.num_supplies = ARRAY_SIZE(tegra194_supply_names),
	.phy_types = tegra194_phy_types,
	.num_types = ARRAY_SIZE(tegra194_phy_types),
	.context = &tegra186_xusb_context,
	.ports = {
		.usb3 = { .offset = 0, .count = 4, },
		.usb2 = { .offset = 4, .count = 4, },
	},
	.scale_ss_clock = false,
	.has_ipfs = false,
	.otg_reset_sspi = false,
	.disable_hsic_wake = false,
	.ops = &tegra124_ops,
	.mbox = {
		.cmd = 0x68,
		.data_in = 0x6c,
		.data_out = 0x70,
		.owner = 0x74,
	},
	.lpm_support = true,
	.disable_u0_ts1_detect = false,
};

static const struct tegra_xusb_soc tegra194_vf4_soc = {
	.firmware = "nvidia/tegra194/xusb.bin",
	.is_xhci_vf = true,
	.vf_id = 4,
	.supply_names = tegra194_supply_names,
	.num_supplies = ARRAY_SIZE(tegra194_supply_names),
	.phy_types = tegra194_phy_types,
	.num_types = ARRAY_SIZE(tegra194_phy_types),
	.context = &tegra186_xusb_context,
	.ports = {
		.usb3 = { .offset = 0, .count = 4, },
		.usb2 = { .offset = 4, .count = 4, },
	},
	.scale_ss_clock = false,
	.has_ipfs = false,
	.otg_reset_sspi = false,
	.disable_hsic_wake = false,
	.ops = &tegra124_ops,
	.mbox = {
		.cmd = 0x68,
		.data_in = 0x6c,
		.data_out = 0x70,
		.owner = 0x74,
	},
	.lpm_support = true,
	.disable_u0_ts1_detect = false,
};

static struct tegra_xusb_soc_ops tegra234_ops = {
	.mbox_reg_readl = &bar2_readl,
	.mbox_reg_writel = &bar2_writel,
	.csb_reg_readl = &bar2_csb_readl,
	.csb_reg_writel = &bar2_csb_writel,
};

static const struct tegra_xusb_soc tegra234_soc = {
	.firmware = "nvidia/tegra234/xusb.bin",
	.supply_names = tegra194_supply_names,
	.num_supplies = ARRAY_SIZE(tegra194_supply_names),
	.phy_types = tegra194_phy_types,
	.num_types = ARRAY_SIZE(tegra194_phy_types),
	.num_wakes = 7,
	.context = &tegra186_xusb_context,
	.ports = {
		.usb3 = { .offset = 0, .count = 4, },
		.usb2 = { .offset = 4, .count = 4, },
	},
	.scale_ss_clock = false,
	.has_ipfs = false,
	.otg_reset_sspi = false,
	.disable_hsic_wake = false,
	.ops = &tegra234_ops,
	.mbox = {
		.cmd = XUSB_BAR2_ARU_MBOX_CMD,
		.data_in = XUSB_BAR2_ARU_MBOX_DATA_IN,
		.data_out = XUSB_BAR2_ARU_MBOX_DATA_OUT,
		.owner = XUSB_BAR2_ARU_MBOX_OWNER,
		.smi_intr = XUSB_BAR2_ARU_SMI_INTR,
	},
	.lpm_support = true,
	.disable_u0_ts1_detect = false,

	.has_bar2 = true,
	.has_ifr = true,
	.load_ifr_rom = false,
};
MODULE_FIRMWARE("nvidia/tegra234/xusb.bin");

static const struct of_device_id tegra_xusb_of_match[] = {
	{ .compatible = "nvidia,tegra124-xusb", .data = &tegra124_soc },
	{ .compatible = "nvidia,tegra210-xusb", .data = &tegra210_soc },
	{ .compatible = "nvidia,tegra210b01-xusb", .data = &tegra210b01_soc },
	{ .compatible = "nvidia,tegra186-xusb", .data = &tegra186_soc },
	{ .compatible = "nvidia,tegra194-xusb", .data = &tegra194_soc },
	{ .compatible = "nvidia,tegra194-xusb-vf1", .data = &tegra194_vf1_soc },
	{ .compatible = "nvidia,tegra194-xusb-vf2", .data = &tegra194_vf2_soc },
	{ .compatible = "nvidia,tegra194-xusb-vf3", .data = &tegra194_vf3_soc },
	{ .compatible = "nvidia,tegra194-xusb-vf4", .data = &tegra194_vf4_soc },
	{ .compatible = "nvidia,tegra234-xusb", .data = &tegra234_soc },
	{ },
};
MODULE_DEVICE_TABLE(of, tegra_xusb_of_match);

static struct platform_driver tegra_xusb_driver = {
	.probe = tegra_xusb_probe,
	.remove = tegra_xusb_remove,
	.shutdown = tegra_xusb_shutdown,
	.driver = {
		.name = "tegra-xusb",
		.pm = &tegra_xusb_pm_ops,
		.of_match_table = tegra_xusb_of_match,
	},
};

static void tegra_xhci_quirks(struct device *dev, struct xhci_hcd *xhci)
{
	struct tegra_xusb *tegra = dev_get_drvdata(dev);

	xhci->quirks |= XHCI_PLAT | XHCI_SPURIOUS_WAKEUP;
	if (tegra && tegra->soc->lpm_support)
		xhci->quirks |= XHCI_LPM_SUPPORT;
}

static int tegra_xhci_setup(struct usb_hcd *hcd)
{
	return xhci_gen_setup(hcd, tegra_xhci_quirks);
}

static int tegra_xhci_start(struct usb_hcd *hcd)
{
	int rval;

	rval = xhci_run(hcd);
	if (rval >= 0) {
		struct xhci_hcd *xhci = hcd_to_xhci(hcd);
		u32	command;

		command = readl(&xhci->op_regs->command);
		command |= CMD_HSEIE;
		writel(command, &xhci->op_regs->command);
	}

	return rval;
}

static const struct xhci_driver_overrides tegra_xhci_overrides __initconst = {
	.reset = tegra_xhci_setup,
	.start = tegra_xhci_start,
};

static bool device_has_isoch_ep_and_interval_one(struct usb_device *udev)
{
	struct usb_host_config *config;
	struct usb_host_interface *alt;
	struct usb_endpoint_descriptor *desc;
	int i, j;

	config = udev->actconfig;
	if (!config)
		return false;

	for (i = 0; i < config->desc.bNumInterfaces; i++) {
		alt = config->interface[i]->cur_altsetting;

		if (!alt)
			continue;

		for (j = 0; j < alt->desc.bNumEndpoints; j++) {
			desc = &alt->endpoint[j].desc;
			if (usb_endpoint_xfer_isoc(desc) &&
				desc->bInterval == 1)
				return true;
		}
	}

	return false;
}

static int tegra_xhci_enable_usb3_lpm_timeout(struct usb_hcd *hcd,
			struct usb_device *udev, enum usb3_link_state state)
{
	struct tegra_xusb *tegra = hcd_to_tegra_xusb(hcd);

	if (tegra->soc->disable_u0_ts1_detect)
		return USB3_LPM_DISABLED;

	if ((state == USB3_LPM_U1 || state == USB3_LPM_U2) &&
		device_has_isoch_ep_and_interval_one(udev))
		return USB3_LPM_DISABLED;

	return xhci_enable_usb3_lpm_timeout(hcd, udev, state);
}

static irqreturn_t tegra_xhci_irq(struct usb_hcd *hcd)
{
	struct tegra_xusb *tegra = hcd_to_tegra_xusb(hcd);

	if (test_bit(FW_LOG_CONTEXT_VALID, &tegra->log.flags))
		wake_up_interruptible(&tegra->log.intr_wait);

	return xhci_irq(hcd);
}

static int tegra_xhci_hub_control(struct usb_hcd *hcd, u16 type_req,
		u16 value, u16 index, char *buf, u16 length)

{
	struct tegra_xusb *tegra = hcd_to_tegra_xusb(hcd);
	struct xhci_hub *rhub;
	struct xhci_bus_state *bus_state;
	int port = (index & 0xff) - 1;
	int port_index;
	struct xhci_port **ports;
	u32 portsc;
	int ret;

	rhub = xhci_get_rhub(hcd);
	bus_state = &rhub->bus_state;
	if (bus_state->resuming_ports && hcd->speed == HCD_USB2) {
		ports = rhub->ports;
		port_index = rhub->num_ports;
		while (port_index--) {
			if (!test_bit(port_index, &bus_state->resuming_ports))
				continue;
			portsc = readl(ports[port_index]->addr);
			if ((port_index < tegra->soc->ports.usb2.count)
					&& ((portsc & PORT_PLS_MASK) == XDEV_RESUME))
				tegra_phy_xusb_utmi_pad_power_on(
					tegra->phys[tegra->soc->ports.usb2.offset + port_index]);
		}
	}

	if (hcd->speed == HCD_USB2) {
		if ((type_req == ClearPortFeature) &&
				(value == USB_PORT_FEAT_SUSPEND))
			tegra_phy_xusb_utmi_pad_power_on(
				tegra->phys[tegra->soc->ports.usb2.offset + port]);
		if ((type_req == SetPortFeature) &&
		    (value == USB_PORT_FEAT_RESET)) {
			ports = rhub->ports;
			portsc = readl(ports[port]->addr);
			if (portsc & PORT_CONNECT)
				tegra_phy_xusb_utmi_pad_power_on(
					  tegra->phys[tegra->soc->ports.usb2.offset + port]);
		}
	}

	ret = xhci_hub_control(hcd, type_req, value, index, buf, length);

	if ((hcd->speed == HCD_USB2) && (ret == 0)) {
		if ((type_req == SetPortFeature) &&
			(value == USB_PORT_FEAT_SUSPEND))
			/* We dont suspend the PAD while HNP role swap happens
			 * on the OTG port
			 */
			if (!((hcd->self.otg_port == (port + 1)) &&
					hcd->self.b_hnp_enable))
				tegra_phy_xusb_utmi_pad_power_down(
					tegra->phys[tegra->soc->ports.usb2.offset + port]);

		if ((type_req == ClearPortFeature) &&
				(value == USB_PORT_FEAT_C_CONNECTION)) {
			ports = rhub->ports;
			portsc = readl(ports[port]->addr);
			if (!(portsc & PORT_CONNECT)) {
				/* We dont suspend the PAD while HNP
				 * role swap happens on the OTG port
				 */
				if (!((hcd->self.otg_port == (port + 1))
						&& hcd->self.b_hnp_enable))
					tegra_phy_xusb_utmi_pad_power_down(
						tegra->phys[tegra->soc->ports.usb2.offset + port]);
			}
		}
		if ((type_req == SetPortFeature) &&
		    (value == USB_PORT_FEAT_TEST))
			tegra_phy_xusb_utmi_pad_power_on(
				tegra->phys[tegra->soc->ports.usb2.offset + port]);
	}

	return ret;
}

static void xhci_reinit_work(struct work_struct *work)
{
	unsigned long flags;
	struct xhci_hcd *xhci = container_of(work,
				struct xhci_hcd, tegra_xhci_reinit_work);
	struct platform_device *pdev = xhci->pdev;
	struct tegra_xusb *tegra = platform_get_drvdata(pdev);
	struct device *dev = tegra->dev;
	unsigned long target;
	bool has_active_slots = true;
	int j, ret;

	/* If the controller is in ELPG during reinit, then bring it out of
	 * elpg first before doing reinit. Otherwise we will see inconsistent
	 * behaviour. For example phy->power_count variable will be decremented
	 * twice. Once dring elpg and once during usb_remove. when phy_power_on
	 * is called during probe, the phy wont actually power on.
	 */
	mutex_lock(&tegra->lock);
	if (pm_runtime_suspended(dev)) {
		ret = tegra_xusb_exit_elpg(tegra, true);
		if (ret < 0) {
			mutex_unlock(&tegra->lock);
			dev_err(tegra->dev, "ELPG exit failed during reinit\n");
			return;
		}
	}
	mutex_unlock(&tegra->lock);

	for (j = 0; j < tegra->soc->ports.usb2.count; j++) {
		if (!is_host_mode_phy(tegra, USB2_PHY, j))
			continue;
		/* turn off VBUS to disconnect all devices */
		tegra_xusb_padctl_vbus_power_off(
				tegra->phys[tegra->soc->ports.usb2.offset + j]);
	}

	target = jiffies + msecs_to_jiffies(5000);
	/* wait until all slots have been disabled. Also waiting for 5
	 * seconds to make sure pending STOP_EP commands are completed
	 */
	while (has_active_slots && time_is_after_jiffies(target)) {
		has_active_slots = false;
		for (j = 1; j < MAX_HC_SLOTS; j++) {
			if (xhci->devs[j])
				has_active_slots = true;
		}
		msleep(300);
	}

	spin_lock_irqsave(&xhci->lock, flags);
	/* Abort pending commands, clean up pending URBs */
	xhci_hc_died(xhci);
	spin_unlock_irqrestore(&xhci->lock, flags);
	tegra_xusb_remove(pdev);
	usleep_range(10, 20);

	tegra_xusb_probe(pdev);
	/* probe will set recovery_in_progress to false */
}

static int tegra_xhci_hcd_reinit(struct usb_hcd *hcd)
{
	struct xhci_hcd *xhci = hcd_to_xhci(hcd);
	struct tegra_xusb *tegra = hcd_to_tegra_xusb(hcd);

	if (en_hcd_reinit && !xhci->recovery_in_progress) {
		xhci->recovery_in_progress = true;

		schedule_work(&xhci->tegra_xhci_reinit_work);
	} else {
		dev_info(tegra->dev, "hcd_reinit is disabled or in progress\n");
	}
	return 0;
}

static int __init tegra_xusb_init(void)
{
	xhci_init_driver(&tegra_xhci_hc_driver, &tegra_xhci_overrides);
	tegra_xhci_hc_driver.hcd_reinit = tegra_xhci_hcd_reinit;
	tegra_xhci_hc_driver.hub_control = tegra_xhci_hub_control;
	tegra_xhci_hc_driver.add_endpoint = tegra_xhci_add_endpoint;
	tegra_xhci_hc_driver.enable_usb3_lpm_timeout =
		tegra_xhci_enable_usb3_lpm_timeout;
	tegra_xhci_hc_driver.urb_enqueue = tegra_xhci_urb_enqueue;
	tegra_xhci_hc_driver.irq = tegra_xhci_irq;
	tegra_xhci_hc_driver.hub_status_data = tegra_xhci_hub_status_data;
	tegra_xhci_hc_driver.endpoint_soft_retry =
			tegra_xhci_endpoint_soft_retry;
	tegra_xhci_hc_driver.is_u0_ts1_detect_disabled =
			tegra_xhci_is_u0_ts1_detect_disabled;

	return platform_driver_register(&tegra_xusb_driver);
}
module_init(tegra_xusb_init);

static void __exit tegra_xusb_exit(void)
{
	platform_driver_unregister(&tegra_xusb_driver);
	tegra_xhci_hc_driver.update_device = tegra_xhci_update_device;
}
module_exit(tegra_xusb_exit);

MODULE_AUTHOR("Andrew Bresticker <abrestic@chromium.org>");
MODULE_DESCRIPTION("NVIDIA Tegra XUSB xHCI host-controller driver");
MODULE_LICENSE("GPL v2");
