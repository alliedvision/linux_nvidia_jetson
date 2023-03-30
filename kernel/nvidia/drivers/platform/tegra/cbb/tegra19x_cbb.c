/*
 * Copyright (c) 2017-2022, NVIDIA CORPORATION.  All rights reserved.
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
 * The driver handles Error's from Control Backbone(CBB) generated due to
 * illegal accesses. When an error is reported from a NOC within CBB,
 * the driver checks ErrVld status of all three Error Logger's of that NOC.
 * It then prints debug information about failed transaction using ErrLog
 * registers of error logger which has ErrVld set. Currently, SLV, DEC,
 * TMO, SEC, UNS are the only codes which are supported by CBB.
 */

#include <linux/clk.h>
#include <linux/cvnas.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
#include <asm/traps.h>
#include <soc/tegra/chip-id.h>
#else
#include <asm/cpufeature.h>
#include <soc/tegra/fuse.h>
#endif
#include <linux/platform/tegra/tegra_cbb.h>
#include <linux/platform/tegra/tegra19x_cbb.h>

static LIST_HEAD(cbb_noc_list);
static DEFINE_SPINLOCK(cbb_noc_lock);

#define get_mstr_id(userbits) get_noc_errlog_subfield(userbits, 21, 18)-1;

static void cbbcentralnoc_parse_routeid
		(struct tegra_lookup_noc_aperture *noc_trans_info, u64 routeid)
{
	noc_trans_info->initflow = get_noc_errlog_subfield(routeid, 23, 20);
	noc_trans_info->targflow = get_noc_errlog_subfield(routeid, 19, 16);
	noc_trans_info->targ_subrange =	get_noc_errlog_subfield(routeid, 15, 9);
	noc_trans_info->seqid = get_noc_errlog_subfield(routeid, 8, 0);
}

static void bpmpnoc_parse_routeid
		(struct tegra_lookup_noc_aperture *noc_trans_info, u64 routeid)
{
	noc_trans_info->initflow = get_noc_errlog_subfield(routeid, 20, 18);
	noc_trans_info->targflow = get_noc_errlog_subfield(routeid, 17, 13);
	noc_trans_info->targ_subrange =	get_noc_errlog_subfield(routeid, 12, 9);
	noc_trans_info->seqid = get_noc_errlog_subfield(routeid, 8, 0);
}

static void aonnoc_parse_routeid
		(struct tegra_lookup_noc_aperture *noc_trans_info, u64 routeid)
{
	noc_trans_info->initflow = get_noc_errlog_subfield(routeid, 22, 21);
	noc_trans_info->targflow = get_noc_errlog_subfield(routeid, 20, 15);
	noc_trans_info->targ_subrange = get_noc_errlog_subfield(routeid, 14, 9);
	noc_trans_info->seqid = get_noc_errlog_subfield(routeid, 8, 0);
}

static void scenoc_parse_routeid
	(struct tegra_lookup_noc_aperture *noc_trans_info, u64 routeid)
{
	noc_trans_info->initflow = get_noc_errlog_subfield(routeid, 21, 19);
	noc_trans_info->targflow = get_noc_errlog_subfield(routeid, 18, 14);
	noc_trans_info->targ_subrange = get_noc_errlog_subfield(routeid, 13, 9);
	noc_trans_info->seqid = get_noc_errlog_subfield(routeid, 8, 0);
}

static void cvnoc_parse_routeid
	(struct tegra_lookup_noc_aperture *noc_trans_info, u64 routeid)
{
	noc_trans_info->initflow = get_noc_errlog_subfield(routeid, 18, 16);
	noc_trans_info->targflow = get_noc_errlog_subfield(routeid, 15, 12);
	noc_trans_info->targ_subrange = get_noc_errlog_subfield(routeid, 11, 7);
	noc_trans_info->seqid = get_noc_errlog_subfield(routeid, 6, 0);
}

static void cbbcentralnoc_parse_userbits
	(struct tegra_noc_userbits *noc_trans_usrbits, u64 usrbits)
{
	noc_trans_usrbits->axcache = get_noc_errlog_subfield(usrbits, 3, 0);
	noc_trans_usrbits->non_mod = get_noc_errlog_subfield(usrbits, 4, 4);
	noc_trans_usrbits->axprot = get_noc_errlog_subfield(usrbits, 7, 5);
	noc_trans_usrbits->falconsec = get_noc_errlog_subfield(usrbits, 9, 8);
	noc_trans_usrbits->grpsec = get_noc_errlog_subfield(usrbits, 16, 10);
	noc_trans_usrbits->vqc = get_noc_errlog_subfield(usrbits, 18, 17);
	noc_trans_usrbits->mstr_id = get_noc_errlog_subfield(usrbits, 22, 19)-1;
	noc_trans_usrbits->axi_id = get_noc_errlog_subfield(usrbits, 30, 23);
}

static void clusternoc_parse_userbits
	(struct tegra_noc_userbits *noc_trans_usrbits, u64 usrbits)
{
	noc_trans_usrbits->axcache = get_noc_errlog_subfield(usrbits, 3, 0);
	noc_trans_usrbits->axprot = get_noc_errlog_subfield(usrbits, 6, 4);
	noc_trans_usrbits->falconsec = get_noc_errlog_subfield(usrbits, 8, 7);
	noc_trans_usrbits->grpsec = get_noc_errlog_subfield(usrbits, 15, 9);
	noc_trans_usrbits->vqc = get_noc_errlog_subfield(usrbits, 17, 16);
	noc_trans_usrbits->mstr_id = get_noc_errlog_subfield(usrbits, 21, 18)-1;
}

static void tegra194_cbb_errlogger_faulten(void __iomem *addr)
{
	writel(1, addr+OFF_ERRLOGGER_0_FAULTEN_0);
	writel(1, addr+OFF_ERRLOGGER_1_FAULTEN_0);
	writel(1, addr+OFF_ERRLOGGER_2_FAULTEN_0);
}

static void tegra194_cbb_errlogger_stallen(void __iomem *addr)
{
	writel(1, addr+OFF_ERRLOGGER_0_STALLEN_0);
	writel(1, addr+OFF_ERRLOGGER_1_STALLEN_0);
	writel(1, addr+OFF_ERRLOGGER_2_STALLEN_0);
}

static void tegra194_cbb_errlogger_errclr(void __iomem *addr)
{
	writel(1, addr+OFF_ERRLOGGER_0_ERRCLR_0);
	writel(1, addr+OFF_ERRLOGGER_1_ERRCLR_0);
	writel(1, addr+OFF_ERRLOGGER_2_ERRCLR_0);
	dsb(sy);
}

static unsigned int tegra194_cbb_errlogger_errvld(void __iomem *addr)
{
	unsigned int errvld_status = 0;

	errvld_status = readl(addr+OFF_ERRLOGGER_0_ERRVLD_0);
	errvld_status |= (readl(addr+OFF_ERRLOGGER_1_ERRVLD_0) << 1);
	errvld_status |= (readl(addr+OFF_ERRLOGGER_2_ERRVLD_0) << 2);

	dsb(sy);
	return errvld_status;
}

static unsigned int tegra194_axi2apb_errstatus(void __iomem *addr)
{
	unsigned int error_status;

	error_status = readl(addr + DMAAPB_X_RAW_INTERRUPT_STATUS);
	writel(0xFFFFFFFF, addr + DMAAPB_X_RAW_INTERRUPT_STATUS);
	return error_status;
}

static bool tegra194_axi2apb_err(struct seq_file *file, int bridge,
				 u32 bus_status)
{
	int max_axi2apb_err = ARRAY_SIZE(tegra194_axi2apb_errors);
	bool is_fatal = true;
	int j = 0;

	for (j = 0; j < max_axi2apb_err; j++) {
		if (bus_status & (1 << j)) {
			print_cbb_err(file, "\t  AXI2APB_%d bridge error: %s\n",
					bridge, tegra194_axi2apb_errors[j]);

			if (strstr(tegra194_axi2apb_errors[j], "Firewall"))
				is_fatal = false;
		}
	}

	return is_fatal;
}

/*
 * Fetch InitlocalAddress from NOC Aperture lookup table
 * using Targflow, Targsubrange
 */
static int get_init_localaddress(
		struct tegra_lookup_noc_aperture *noc_trans_info,
		struct tegra_lookup_noc_aperture *lookup_noc_aperture,
		int max_cnt)
{
	int targ_f = 0, targ_sr = 0;
	unsigned long long init_localaddress = 0;
	int targflow = noc_trans_info->targflow;
	int targ_subrange = noc_trans_info->targ_subrange;

	for (targ_f = 0; targ_f < max_cnt; targ_f++) {
		if (lookup_noc_aperture[targ_f].targflow == targflow) {
			targ_sr = targ_f;
			do {
				if (lookup_noc_aperture[targ_sr].targ_subrange
							== targ_subrange) {
					init_localaddress
			= lookup_noc_aperture[targ_sr].init_localaddress;
					return init_localaddress;
				}
				if (targ_sr >= max_cnt)
					return 0;
				targ_sr++;
			} while (lookup_noc_aperture[targ_sr].targflow
				== lookup_noc_aperture[targ_sr-1].targflow);
			targ_f = targ_sr;
		}
	}

	return init_localaddress;
}


static void print_errlog5(struct seq_file *file,
		struct tegra_cbb_errlog_record *errlog)
{
	struct tegra_noc_userbits userbits;
	u32 errlog5 = errlog->errlog5;

	errlog->tegra_noc_parse_userbits(&userbits, errlog5);
	if (!strcmp(errlog->name, "CBB-NOC")) {
		print_cbb_err(file, "\t  Non-Modify\t\t: 0x%x\n",
				userbits.non_mod);
		print_cbb_err(file, "\t  AXI ID\t\t: 0x%x\n",
				userbits.axi_id);
	}

	print_cbb_err(file, "\t  Master ID\t\t: %s\n",
			errlog->tegra_cbb_master_id[userbits.mstr_id]);
	print_cbb_err(file, "\t  Security Group(GRPSEC): 0x%x\n",
			userbits.grpsec);
	print_cache(file, userbits.axcache);
	print_prot(file, userbits.axprot);
	print_cbb_err(file, "\t  FALCONSEC\t\t: 0x%x\n", userbits.falconsec);
	print_cbb_err(file, "\t  Virtual Queuing Channel(VQC): 0x%x\n",
			userbits.vqc);
}


/*
 *  Fetch Base Address/InitlocalAddress from NOC aperture lookup table
 *  using TargFlow & Targ_subRange extracted from RouteId.
 *  Perform address reconstruction as below:
 *		Address = Base Address + (ErrLog3+ErrLog4)
 */
static void print_errlog3_4(struct seq_file *file, u32 errlog3, u32 errlog4,
		struct tegra_lookup_noc_aperture *noc_trans_info,
		struct tegra_lookup_noc_aperture *noc_aperture,
		int max_noc_aperture)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	struct resource *res = NULL;
#endif
	u64 addr = 0;

	addr = errlog4;
	addr = (addr << 32) | errlog3;

	/*
	 * if errlog4[7]="1", then it's a joker entry.
	 * joker entry is a rare phenomenon and address is not reliable.
	 * debug should be done using the routeid information alone.
	 */
	if (errlog4 & 0x80)
		print_cbb_err(file, "\t  debug using routeid alone as below"
				" address is a joker entry and not-reliable.");

	addr += get_init_localaddress(noc_trans_info, noc_aperture,
			max_noc_aperture);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	res = locate_resource(&iomem_resource, addr);
	if (res == NULL)
		print_cbb_err(file, "\t  Address\t\t: 0x%llx"
				" (unknown device)\n", addr);
	else
		print_cbb_err(file, "\t  Address\t\t: "
				"0x%llx -- %s + 0x%llx\n", addr, res->name,
				addr - res->start);
#else
	print_cbb_err(file, "\t  Address accessed\t: 0x%llx\n", addr);
#endif
}

/*
 *  Get RouteId from ErrLog1+ErrLog2 registers and fetch values of
 *  InitFlow, TargFlow, Targ_subRange and SeqId values from RouteId
 */
static void print_errlog1_2(struct seq_file *file,
		struct tegra_cbb_errlog_record *errlog,
		struct tegra_lookup_noc_aperture *noc_trans_info)
{
	u64	routeid = 0;
	u32	seqid = 0;

	routeid = errlog->errlog2;
	routeid = (routeid<<32)|errlog->errlog1;
	print_cbb_err(file, "\t  RouteId\t\t: 0x%lx\n", routeid);
	errlog->tegra_noc_parse_routeid(noc_trans_info, routeid);

	print_cbb_err(file, "\t  InitFlow\t\t: %s\n",
		errlog->tegra_noc_routeid_initflow[noc_trans_info->initflow]);
	print_cbb_err(file, "\t  Targflow\t\t: %s\n",
		errlog->tegra_noc_routeid_targflow[noc_trans_info->targflow]);
	print_cbb_err(file, "\t  TargSubRange\t\t: %d\n",
		noc_trans_info->targ_subrange);
	print_cbb_err(file, "\t  SeqId\t\t\t: %d\n", seqid);
}

/*
 * Print transcation type, error code and description from ErrLog0 for all
 * errors. For NOC slave errors, all relevant error info is printed using
 * ErrLog0 only. But additional information is printed for errors from
 * APB slaves because for them:
 *  - All errors are logged as SLV(slave) errors in errlog0 due to APB having
 *    only single bit pslverr to report all errors.
 *  - Exact cause is printed by reading DMAAPB_X_RAW_INTERRUPT_STATUS register.
 *  - The driver prints information showing AXI2APB bridge and exact error
 *    only if there is error in any AXI2APB slave.
 *  - There is still no way to disambiguate a DEC error from SLV error type.
 */
static bool print_errlog0(struct seq_file *file,
			  struct tegra_cbb_errlog_record *errlog)
{
	struct tegra_noc_packet_header hdr;
	bool is_fatal = true;

	hdr.lock    = errlog->errlog0 & 0x1;
	hdr.opc     = get_noc_errlog_subfield(errlog->errlog0, 4, 1);
	hdr.errcode = get_noc_errlog_subfield(errlog->errlog0, 10, 8);
	hdr.len1    = get_noc_errlog_subfield(errlog->errlog0, 27, 16);
	hdr.format  = (errlog->errlog0>>31);

	print_cbb_err(file, "\t  Transaction Type\t: %s\n",
			tegra194_noc_opc_trantype[hdr.opc]);
	print_cbb_err(file, "\t  Error Code\t\t: %s\n",
			tegra194_noc_errors[hdr.errcode].errcode);
	print_cbb_err(file, "\t  Error Source\t\t: %s\n",
			tegra194_noc_errors[hdr.errcode].src);
	print_cbb_err(file, "\t  Error Description\t: %s\n",
			tegra194_noc_errors[hdr.errcode].type);

	if (!strcmp(tegra194_noc_errors[hdr.errcode].errcode, "SEC") ||
	    !strcmp(tegra194_noc_errors[hdr.errcode].errcode, "DEC") ||
	    !strcmp(tegra194_noc_errors[hdr.errcode].errcode, "UNS") ||
	    !strcmp(tegra194_noc_errors[hdr.errcode].errcode, "DISC")
	)
		is_fatal = false;
	else if (!strcmp(tegra194_noc_errors[hdr.errcode].errcode, "SLV")
			&& (errlog->is_ax2apb_bridge_connected)) {
		int i = 0;
		u32 bus_status = 0;

		/* For all SLV errors, read DMAAPB_X_RAW_INTERRUPT_STATUS
		 * register to get error status for all AXI2APB bridges and
		 * print only if a bit set for any bridge due to error in
		 * a APB slave. For other NOC slaves, no bit will be set.
		 * So, below line won't get printed.
		 */

		for (i = 0; i < errlog->apb_bridge_cnt; i++) {
			bus_status = tegra194_axi2apb_errstatus
					(errlog->axi2abp_bases[i]);

			if (bus_status)
				is_fatal =
				tegra194_axi2apb_err(file, i, bus_status);
		}
	}
	print_cbb_err(file, "\t  Packet header Lock\t: %d\n", hdr.lock);
	print_cbb_err(file, "\t  Packet header Len1\t: %d\n", hdr.len1);
	if (hdr.format)
		print_cbb_err(file, "\t  NOC protocol version\t: %s\n",
				"version >= 2.7");
	else
		print_cbb_err(file, "\t  NOC protocol version\t: %s\n",
				"version < 2.7");
	return is_fatal;
}

/*
 * Print debug information about failed transaction using
 * ErrLog registers of error loggger having ErrVld set
 */
static bool print_errloggerX_info(
		struct seq_file *file,
		struct tegra_cbb_errlog_record *errlog, int errloggerX)
{
	struct tegra_lookup_noc_aperture noc_trans_info = {0,};
	bool is_fatal = true;

	print_cbb_err(file, "\tError Logger\t\t: %d\n", errloggerX);
	if (errloggerX == 0) {
		errlog->errlog0 = readl(errlog->vaddr+OFF_ERRLOGGER_0_ERRLOG0_0);
		errlog->errlog1 = readl(errlog->vaddr+OFF_ERRLOGGER_0_ERRLOG1_0);
		errlog->errlog2 = readl(errlog->vaddr+OFF_ERRLOGGER_0_RESERVED_00_0);
		errlog->errlog3 = readl(errlog->vaddr+OFF_ERRLOGGER_0_ERRLOG3_0);
		errlog->errlog4 = readl(errlog->vaddr+OFF_ERRLOGGER_0_ERRLOG4_0);
		errlog->errlog5 = readl(errlog->vaddr+OFF_ERRLOGGER_0_ERRLOG5_0);
	} else if (errloggerX == 1) {
		errlog->errlog0 = readl(errlog->vaddr+OFF_ERRLOGGER_1_ERRLOG0_0);
		errlog->errlog1 = readl(errlog->vaddr+OFF_ERRLOGGER_1_ERRLOG1_0);
		errlog->errlog2 = readl(errlog->vaddr+OFF_ERRLOGGER_1_RESERVED_00_0);
		errlog->errlog3 = readl(errlog->vaddr+OFF_ERRLOGGER_1_ERRLOG3_0);
		errlog->errlog4 = readl(errlog->vaddr+OFF_ERRLOGGER_1_ERRLOG4_0);
		errlog->errlog5 = readl(errlog->vaddr+OFF_ERRLOGGER_1_ERRLOG5_0);
	} else if (errloggerX == 2) {
		errlog->errlog0 = readl(errlog->vaddr+OFF_ERRLOGGER_2_ERRLOG0_0);
		errlog->errlog1 = readl(errlog->vaddr+OFF_ERRLOGGER_2_ERRLOG1_0);
		errlog->errlog2 = readl(errlog->vaddr+OFF_ERRLOGGER_2_RESERVED_00_0);
		errlog->errlog3 = readl(errlog->vaddr+OFF_ERRLOGGER_2_ERRLOG3_0);
		errlog->errlog4 = readl(errlog->vaddr+OFF_ERRLOGGER_2_ERRLOG4_0);
		errlog->errlog5 = readl(errlog->vaddr+OFF_ERRLOGGER_2_ERRLOG5_0);
	}

	print_cbb_err(file, "\tErrLog0\t\t\t: 0x%x\n", errlog->errlog0);
	is_fatal = print_errlog0(file, errlog);

	print_cbb_err(file, "\tErrLog1\t\t\t: 0x%x\n", errlog->errlog1);
	print_cbb_err(file, "\tErrLog2\t\t\t: 0x%x\n", errlog->errlog2);
	print_errlog1_2(file, errlog, &noc_trans_info);

	print_cbb_err(file, "\tErrLog3\t\t\t: 0x%x\n", errlog->errlog3);
	print_cbb_err(file, "\tErrLog4\t\t\t: 0x%x\n", errlog->errlog4);
	print_errlog3_4(file, errlog->errlog3, errlog->errlog4,
			&noc_trans_info, errlog->noc_aperture,
			errlog->max_noc_aperture);

	print_cbb_err(file, "\tErrLog5\t\t\t: 0x%x\n", errlog->errlog5);
	if(errlog->errlog5)
		print_errlog5(file, errlog);

	return is_fatal;
}

static bool print_errlog(struct seq_file *file,
		struct tegra_cbb_errlog_record *errlog,
		int errvld_status)
{
	bool is_fatal = true;
	pr_crit("**************************************\n");
	pr_crit("* For more Internal Decode Help\n");
	pr_crit("*     http://nv/cbberr\n");
	pr_crit("* NVIDIA userID is required to access\n");
	pr_crit("**************************************\n");
	pr_crit("CPU:%d, Error:%s\n", smp_processor_id(), errlog->name);

	if (errvld_status & 0x1)
		is_fatal = print_errloggerX_info(file, errlog, 0);
	else if (errvld_status & 0x2)
		is_fatal = print_errloggerX_info(file, errlog, 1);
	else if (errvld_status & 0x4)
		is_fatal = print_errloggerX_info(file, errlog, 2);

	tegra_cbb_errclr(errlog->vaddr);
	print_cbb_err(file, "\t**************************************\n");

	return is_fatal;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
static int tegra194_cbb_serr_callback(struct pt_regs *regs, int reason,
		unsigned int esr, void *priv)
{
	unsigned int errvld_status = 0;
	struct tegra_cbb_errlog_record *errlog = priv;
	int retval = 1;

	if ((!errlog->is_clk_rst) ||
			(errlog->is_clk_rst && errlog->is_clk_enabled())) {
		errvld_status = tegra_cbb_errvld(errlog->vaddr);
		if (errvld_status) {
			print_errlog(NULL, errlog, errvld_status);
			retval = 0;
		}
	}
	return retval;
}
#endif

#ifdef CONFIG_DEBUG_FS
static DEFINE_MUTEX(cbb_err_mutex);

static int tegra194_cbb_err_show(struct seq_file *file, void *data)
{
	struct tegra_cbb_errlog_record *errlog;
	unsigned int errvld_status = 0;

	mutex_lock(&cbb_err_mutex);

	list_for_each_entry(errlog, &cbb_noc_list, node) {
		if ((!errlog->is_clk_rst) ||
			(errlog->is_clk_rst && errlog->is_clk_enabled())) {

			errvld_status = tegra_cbb_errvld(errlog->vaddr);
			if (errvld_status) {
				print_errlog(file, errlog,
						errvld_status);
			}
		}
	}

	mutex_unlock(&cbb_err_mutex);
	return 0;
}
#endif

/*
 * Handler for CBB errors from masters other than CCPLEX
 */
static irqreturn_t tegra194_cbb_error_isr(int irq, void *dev_id)
{
	bool is_inband_err = false, is_fatal = false;
	struct tegra_cbb_errlog_record *errlog;
	unsigned int errvld_status = 0;
	unsigned long flags;
	u8 mstr_id = 0;

	spin_lock_irqsave(&cbb_noc_lock, flags);

	list_for_each_entry(errlog, &cbb_noc_list, node) {
		if ((!errlog->is_clk_rst) ||
			(errlog->is_clk_rst && errlog->is_clk_enabled())) {
			errvld_status = tegra_cbb_errvld(errlog->vaddr);

			if (errvld_status && ((irq == errlog->noc_secure_irq)
				|| (irq == errlog->noc_nonsecure_irq))) {
				print_cbb_err(NULL, "CPU:%d, Error:%s@0x%llx,"
				"irq=%d\n", smp_processor_id(), errlog->name,
				errlog->start, irq);

				is_fatal = print_errlog(NULL, errlog,
							errvld_status);

				mstr_id = get_mstr_id(errlog->errlog5);
				/* If illegal request is from CCPLEX(id:0x1)
				 * master then call BUG() to crash system.
				 */
				if ((mstr_id == 0x1) &&
						(errlog->erd_mask_inband_err))
					is_inband_err = 1;
			}
		}
	}
	spin_unlock_irqrestore(&cbb_noc_lock, flags);

	if (is_inband_err) {
		if (is_fatal)
			BUG();
		else
			WARN(true, "Warning due to CBB Error\n");
	}

	return IRQ_HANDLED;
}

/*
 * Register handler for CBB_NONSECURE & CBB_SECURE interrupts due to
 * CBB errors from masters other than CCPLEX
 */
static int tegra194_cbb_enable_interrupt(struct platform_device *pdev,
		int noc_secure_irq, int noc_nonsecure_irq)
{
	int err = 0;

	if (noc_secure_irq) {
		if (request_irq(noc_secure_irq, tegra194_cbb_error_isr, 0,
					dev_name(&pdev->dev), pdev)) {
			dev_err(&pdev->dev,
				"%s: Unable to register (%d) interrupt\n",
				__func__, noc_secure_irq);
			goto isr_err_free_sec_irq;
		}
	}
	if (noc_nonsecure_irq) {
		if (request_irq(noc_nonsecure_irq, tegra194_cbb_error_isr, 0,
					dev_name(&pdev->dev), pdev)) {
			dev_err(&pdev->dev,
				"%s: Unable to register (%d) interrupt\n",
				__func__, noc_nonsecure_irq);
			goto isr_err_free_nonsec_irq;
		}
	}

	return 0;

isr_err_free_nonsec_irq:
	if (noc_nonsecure_irq)
		free_irq(noc_nonsecure_irq, pdev);
isr_err_free_sec_irq:
	if (noc_secure_irq)
		free_irq(noc_secure_irq, pdev);

	return err;
}

static void tegra194_cbb_error_enable(void __iomem *vaddr)
{

	/* set “StallEn=1” to enable queuing of error packets till
	 * first is served & cleared
	 */
	tegra_cbb_stallen(vaddr);

	/* set “FaultEn=1” to enable error reporting signal “Fault” */
	tegra_cbb_faulten(vaddr);
}


static int tegra194_cbb_remove(struct platform_device *pdev)
{
	struct resource *res_base;
	struct tegra_cbb_errlog_record *errlog;
	unsigned long flags;

	res_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_base)
		return -EINVAL;

	spin_lock_irqsave(&cbb_noc_lock, flags);
	list_for_each_entry(errlog, &cbb_noc_list, node) {
		if (errlog->start == res_base->start) {
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
			unregister_serr_hook(errlog->callback);
#endif
			list_del(&errlog->node);
			break;
		}
	}
	spin_unlock_irqrestore(&cbb_noc_lock, flags);

	return 0;
}

static struct tegra_cbberr_ops tegra194_cbb_errlogger_ops = {
	.errvld	 = tegra194_cbb_errlogger_errvld,
	.errclr	 = tegra194_cbb_errlogger_errclr,
	.faulten = tegra194_cbb_errlogger_faulten,
	.stallen = tegra194_cbb_errlogger_stallen,
	.cbb_error_enable = tegra194_cbb_error_enable,
	.cbb_enable_interrupt = tegra194_cbb_enable_interrupt,
#ifdef CONFIG_DEBUG_FS
	.cbb_err_debugfs_show = tegra194_cbb_err_show,
#endif
};

static struct tegra_cbb_noc_data tegra194_cbb_central_noc_data = {
	.name   = "CBB-NOC",
	.is_ax2apb_bridge_connected = 1,
	.is_clk_rst = false,
	.erd_mask_inband_err = true,
	.off_mask_erd = 0x120c,
	.tegra_cbb_noc_set_erd = tegra_miscreg_set_erd
};

static struct tegra_cbb_noc_data tegra194_aon_noc_data = {
	.name   = "AON-NOC",
	.is_ax2apb_bridge_connected = 0,
	.is_clk_rst = false,
	.erd_mask_inband_err = false
};

static struct tegra_cbb_noc_data tegra194_bpmp_noc_data = {
	.name   = "BPMP-NOC",
	.is_ax2apb_bridge_connected = 1,
	.is_clk_rst = false,
	.erd_mask_inband_err = false
};

static struct tegra_cbb_noc_data tegra194_rce_noc_data = {
	.name   = "RCE-NOC",
	.is_ax2apb_bridge_connected = 1,
	.is_clk_rst = false,
	.erd_mask_inband_err = false
};

static struct tegra_cbb_noc_data tegra194_sce_noc_data = {
	.name   = "SCE-NOC",
	.is_ax2apb_bridge_connected = 1,
	.is_clk_rst = false,
	.erd_mask_inband_err = false
};

static struct tegra_cbb_noc_data tegra194_cv_noc_data = {
	.name   = "CV-NOC",
	.is_ax2apb_bridge_connected = 1,
	.is_clk_rst = true,
	.erd_mask_inband_err = false,
	.is_cluster_probed = is_nvcvnas_probed,
	.is_clk_enabled = is_nvcvnas_clk_enabled,
	.tegra_noc_en_clk_rpm = nvcvnas_busy,
	.tegra_noc_dis_clk_rpm = nvcvnas_idle,
	.tegra_noc_en_clk_no_rpm = nvcvnas_busy_no_rpm,
	.tegra_noc_dis_clk_no_rpm = nvcvnas_idle_no_rpm
};

static const struct of_device_id tegra194_cbb_match[] = {
	{.compatible    = "nvidia,tegra194-CBB-NOC",
		.data = &tegra194_cbb_central_noc_data},
	{.compatible    = "nvidia,tegra194-AON-NOC",
		.data = &tegra194_aon_noc_data},
	{.compatible    = "nvidia,tegra194-BPMP-NOC",
		.data = &tegra194_bpmp_noc_data},
	{.compatible    = "nvidia,tegra194-RCE-NOC",
		.data = &tegra194_rce_noc_data},
	{.compatible    = "nvidia,tegra194-SCE-NOC",
		.data = &tegra194_sce_noc_data},
	{.compatible    = "nvidia,tegra194-CV-NOC",
		.data = &tegra194_cv_noc_data},
	{},
};
MODULE_DEVICE_TABLE(of, tegra194_cbb_match);

static int tegra194_cbb_noc_set_data(struct tegra_cbb_errlog_record *errlog)
{
	int err = 0;
	if (!strcmp(errlog->name, "CBB-NOC")) {
		errlog->tegra_cbb_master_id = t194_master_id;
		errlog->noc_aperture = t194_cbbcentralnoc_aperture_lookup;
		errlog->max_noc_aperture =
			ARRAY_SIZE(t194_cbbcentralnoc_aperture_lookup);
		errlog->tegra_noc_routeid_initflow =
			t194_cbbcentralnoc_routeid_initflow;
		errlog->tegra_noc_routeid_targflow =
			t194_cbbcentralnoc_routeid_targflow;
		errlog->tegra_noc_parse_routeid = cbbcentralnoc_parse_routeid;
		errlog->tegra_noc_parse_userbits = cbbcentralnoc_parse_userbits;
	} else if (!strcmp(errlog->name, "AON-NOC")) {
		errlog->tegra_cbb_master_id = t194_master_id;
		errlog->noc_aperture = t194_aonnoc_aperture_lookup;
		errlog->max_noc_aperture =
			ARRAY_SIZE(t194_aonnoc_aperture_lookup);
		errlog->tegra_noc_routeid_initflow =
			t194_aonnoc_routeid_initflow;
		errlog->tegra_noc_routeid_targflow =
			t194_aonnoc_routeid_targflow;
		errlog->tegra_noc_parse_routeid = aonnoc_parse_routeid;
		errlog->tegra_noc_parse_userbits = clusternoc_parse_userbits;
	} else if (!strcmp(errlog->name, "BPMP-NOC")) {
		errlog->tegra_cbb_master_id = t194_master_id;
		errlog->noc_aperture = t194_bpmpnoc_aperture_lookup;
		errlog->max_noc_aperture =
			ARRAY_SIZE(t194_bpmpnoc_aperture_lookup);
		errlog->tegra_noc_routeid_initflow =
			t194_bpmpnoc_routeid_initflow;
		errlog->tegra_noc_routeid_targflow =
			t194_bpmpnoc_routeid_targflow;
		errlog->tegra_noc_parse_routeid = bpmpnoc_parse_routeid;
		errlog->tegra_noc_parse_userbits = clusternoc_parse_userbits;
	} else if (!strcmp(errlog->name, "RCE-NOC")) {
		errlog->tegra_cbb_master_id = t194_master_id;
		errlog->noc_aperture = t194_scenoc_aperture_lookup;
		errlog->max_noc_aperture =
			ARRAY_SIZE(t194_scenoc_aperture_lookup);
		errlog->tegra_noc_routeid_initflow =
			t194_scenoc_routeid_initflow;
		errlog->tegra_noc_routeid_targflow =
			t194_scenoc_routeid_targflow;
		errlog->tegra_noc_parse_routeid = scenoc_parse_routeid;
		errlog->tegra_noc_parse_userbits = clusternoc_parse_userbits;
	} else if (!strcmp(errlog->name, "SCE-NOC")) {
		errlog->tegra_cbb_master_id = t194_master_id;
		errlog->noc_aperture = t194_scenoc_aperture_lookup;
		errlog->max_noc_aperture =
			ARRAY_SIZE(t194_scenoc_aperture_lookup);
		errlog->tegra_noc_routeid_initflow =
			t194_scenoc_routeid_initflow;
		errlog->tegra_noc_routeid_targflow =
			t194_scenoc_routeid_targflow;
		errlog->tegra_noc_parse_routeid = scenoc_parse_routeid;
		errlog->tegra_noc_parse_userbits = clusternoc_parse_userbits;
	} else if (!strcmp(errlog->name, "CV-NOC")) {
		errlog->tegra_cbb_master_id = t194_master_id;
		errlog->noc_aperture = t194_cvnoc_aperture_lookup;
		errlog->max_noc_aperture =
			ARRAY_SIZE(t194_cvnoc_aperture_lookup);
		errlog->tegra_noc_routeid_initflow =
			t194_cvnoc_routeid_initflow;
		errlog->tegra_noc_routeid_targflow =
			t194_cvnoc_routeid_targflow;
		errlog->tegra_noc_parse_routeid = cvnoc_parse_routeid;
		errlog->tegra_noc_parse_userbits = clusternoc_parse_userbits;
	} else
		return -EINVAL;

	return err;
}

static void tegra194_cbb_noc_set_clk_en_ops(
		struct tegra_cbb_errlog_record *errlog,
		const struct tegra_cbb_noc_data *bdata)
{
	if (bdata->is_clk_rst) {
		errlog->is_clk_rst = bdata->is_clk_rst;
		errlog->is_cluster_probed = bdata->is_cluster_probed;
		errlog->is_clk_enabled = bdata->is_clk_enabled;
		errlog->tegra_noc_en_clk_rpm = bdata->tegra_noc_en_clk_rpm;
		errlog->tegra_noc_dis_clk_rpm = bdata->tegra_noc_dis_clk_rpm;
		errlog->tegra_noc_en_clk_no_rpm =
					bdata->tegra_noc_en_clk_no_rpm;
		errlog->tegra_noc_dis_clk_no_rpm =
					bdata->tegra_noc_dis_clk_no_rpm;
	}
}

static const struct of_device_id axi2apb_match[] = {
	{ .compatible = "nvidia,tegra194-AXI2APB-bridge", },
	{},
};

static int tegra194_cbb_get_axi2apb_data(struct platform_device *pdev,
		int *apb_bridge_cnt,
		void __iomem ***bases)
{
	static void __iomem **axi2apb_bases;
	struct device_node *np;
	int ret = 0, i = 0;

	if (!axi2apb_bases) {
		np = of_find_matching_node(NULL, axi2apb_match);
		if (!np) {
			dev_info(&pdev->dev, "No match found for axi2apb\n");
			return -ENOENT;
		}
		*apb_bridge_cnt = (of_property_count_elems_of_size
				(np, "reg", sizeof(u32))) / 4;

		axi2apb_bases = devm_kzalloc(&pdev->dev,
				sizeof(void *) * (*apb_bridge_cnt),
				GFP_KERNEL);
		if (!axi2apb_bases)
			return -ENOMEM;

		for (i = 0; i < *apb_bridge_cnt; i++) {
			void __iomem *base = of_iomap(np, i);

			if (!base) {
				dev_err(&pdev->dev,
					"failed to map axi2apb range\n");
				return -ENOENT;
			}
			axi2apb_bases[i] = base;
		}
	}
	*bases = axi2apb_bases;

	return ret;
}

static int tegra194_cbb_errlogger_init(struct platform_device *pdev,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
		struct serr_hook *callback,
#endif
		const struct tegra_cbb_noc_data *bdata,
		struct tegra_cbb_init_data *cbb_init_data)
{
	struct tegra_cbb_errlog_record *errlog;
	unsigned long flags;
	struct resource *res_base = cbb_init_data->res_base;
	int err = 0;

	errlog = devm_kzalloc(&pdev->dev, sizeof(*errlog), GFP_KERNEL);
	if (!errlog)
		return -ENOMEM;

	errlog->start = res_base->start;
	errlog->vaddr = devm_ioremap_resource(&pdev->dev, res_base);
	if (IS_ERR(errlog->vaddr))
		return -EPERM;

	errlog->name      = bdata->name;
	errlog->tegra_cbb_master_id = bdata->tegra_cbb_master_id;
	errlog->is_ax2apb_bridge_connected = bdata->is_ax2apb_bridge_connected;
	errlog->erd_mask_inband_err = bdata->erd_mask_inband_err;

	tegra_cbberr_set_ops(&tegra194_cbb_errlogger_ops);
	tegra194_cbb_noc_set_clk_en_ops(errlog, bdata);
	err = tegra194_cbb_noc_set_data(errlog);
	if (err) {
		dev_err(&pdev->dev, "Err logger name mismatch\n");
		return -EINVAL;
	}

	if (bdata->is_ax2apb_bridge_connected) {
		err = tegra194_cbb_get_axi2apb_data(pdev,
				&(errlog->apb_bridge_cnt),
				&(errlog->axi2abp_bases));
		if (err) {
			dev_err(&pdev->dev, "axi2apb bridge read failed\n");
			return -EINVAL;
		}
	}

	err = tegra_cbb_err_getirq(pdev,
				&errlog->noc_nonsecure_irq,
				&errlog->noc_secure_irq, &errlog->num_intr);
	if (err)
		return -EINVAL;

	cbb_init_data->secure_irq = errlog->noc_secure_irq;
	cbb_init_data->nonsecure_irq = errlog->noc_nonsecure_irq;
	cbb_init_data->vaddr = errlog->vaddr;
	cbb_init_data->addr_mask_erd = bdata->off_mask_erd;

	platform_set_drvdata(pdev, errlog);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	if (callback) {
		errlog->callback = callback;
		callback->fn = tegra194_cbb_serr_callback;
		callback->priv = errlog;
	}
#endif
	spin_lock_irqsave(&cbb_noc_lock, flags);
	list_add(&errlog->node, &cbb_noc_list);
	spin_unlock_irqrestore(&cbb_noc_lock, flags);

	return err;
};

static int tegra194_cbb_probe(struct platform_device *pdev)
{
	const struct tegra_cbb_noc_data *bdata;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	struct serr_hook *callback;
#endif
	struct resource *res_base;
	struct tegra_cbb_init_data cbb_init_data;
	int err = 0;

	/*
	 * CBB don't exist on the simulator
	 */
	if (tegra_cpu_is_asim()) {
		dev_err(&pdev->dev, "Running on asim\n");
		return -EINVAL;
	}

	bdata = of_device_get_match_data(&pdev->dev);
	if (!bdata) {
		dev_err(&pdev->dev, "No device match found\n");
		return -EINVAL;
	}

	if (bdata->is_clk_rst) {
		if (bdata->is_cluster_probed() && !bdata->is_clk_enabled()) {
			bdata->tegra_noc_en_clk_rpm();
		}
		else {
			dev_info(&pdev->dev, "defer probe as %s not probed yet",
					bdata->name);
			return -EPROBE_DEFER;
		}
	}

	res_base = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res_base) {
		dev_err(&pdev->dev, "Could not find base address");
		return -ENOENT;
	}

	memset(&cbb_init_data, 0, sizeof(cbb_init_data));
	cbb_init_data.res_base = res_base;

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 15, 0)
	callback = devm_kzalloc(&pdev->dev, sizeof(*callback), GFP_KERNEL);
	if (callback == NULL)
		return -ENOMEM;

	err = tegra194_cbb_errlogger_init(pdev, callback, bdata, &cbb_init_data);
	if (err) {
		dev_err(&pdev->dev, "cbberr init for soc failing\n");
		return -EINVAL;
	}

	err = tegra_cbberr_register_hook_en(pdev, bdata, callback, cbb_init_data);
	if(err)
		return err;
#else
	err = tegra194_cbb_errlogger_init(pdev, bdata, &cbb_init_data);
	if (err) {
		dev_err(&pdev->dev, "cbberr init for soc failing\n");
		return -EINVAL;
	}

	err = tegra_cbberr_register_hook_en(pdev, bdata, cbb_init_data);
	if(err)
		return err;
#endif
	if ((bdata->is_clk_rst) && (bdata->is_cluster_probed())
			&& bdata->is_clk_enabled())
		bdata->tegra_noc_dis_clk_rpm();

	return err;
}

#ifdef CONFIG_PM_SLEEP
static int tegra194_cbb_resume_noirq(struct device *dev)
{
	struct tegra_cbb_errlog_record *errlog = dev_get_drvdata(dev);
	int ret = 0;

	if (errlog->is_clk_rst) {
		if (errlog->is_cluster_probed() && !errlog->is_clk_enabled())
			errlog->tegra_noc_en_clk_no_rpm();
		else {
			dev_info(dev, "%s not resumed", errlog->name);
			return -EINVAL;
		}
	}

	tegra194_cbb_error_enable(errlog->vaddr);
	dsb(sy);

	if ((errlog->is_clk_rst) && (errlog->is_cluster_probed())
			&& errlog->is_clk_enabled())
		errlog->tegra_noc_dis_clk_no_rpm();

	dev_info(dev, "%s resumed\n", errlog->name);
	return ret;
}

static int tegra194_cbb_suspend_noirq(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops tegra194_cbb_pm = {
	SET_NOIRQ_SYSTEM_SLEEP_PM_OPS(tegra194_cbb_suspend_noirq,
			tegra194_cbb_resume_noirq)
};
#endif

static struct platform_driver tegra194_cbb_driver = {
	.probe          = tegra194_cbb_probe,
	.remove         = tegra194_cbb_remove,
	.driver = {
		.owner  = THIS_MODULE,
		.name   = "tegra19x-cbb",
		.of_match_table = of_match_ptr(tegra194_cbb_match),
#ifdef CONFIG_PM_SLEEP
		.pm     = &tegra194_cbb_pm,
#endif
	},
};

static int __init tegra194_cbb_init(void)
{
	return platform_driver_register(&tegra194_cbb_driver);
}

static void __exit tegra194_cbb_exit(void)
{
	platform_driver_unregister(&tegra194_cbb_driver);
}

pure_initcall(tegra194_cbb_init);
module_exit(tegra194_cbb_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Control Backbone error handling driver for Tegra194");
