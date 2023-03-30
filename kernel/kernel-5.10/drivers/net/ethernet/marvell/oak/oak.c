/*
 *
 * If you received this File from Marvell, you may opt to use, redistribute and/or
 * modify this File in accordance with the terms and conditions of the General
 * Public License Version 2, June 1991 (the "GPL License"), a copy of which is
 * available along with the File in the license.txt file or by writing to the Free
 * Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 or
 * on the worldwide web at http://www.gnu.org/licenses/gpl.txt.
 * THE FILE IS DISTRIBUTED AS-IS, WITHOUT WARRANTY OF ANY KIND, AND THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE ARE EXPRESSLY
 * DISCLAIMED. The GPL License provides additional details about this warranty
 * disclaimer.
 *
 */
#include "oak.h"

static const char oak_driver_name[] = OAK_DRIVER_NAME;
static const char oak_driver_string[] = OAK_DRIVER_STRING;
static const char oak_driver_version[] = OAK_DRIVER_VERSION;
static const char oak_driver_copyright[] = OAK_DRIVER_COPYRIGHT;
static struct pci_device_id oak_pci_tbl[] = {
    {PCI_DEVICE(PCI_VENDOR_ID_SYSKONNECT, 0x1000)},
    {PCI_DEVICE(0x11AB, 0x0000)}, /* FPGA board */
    {PCI_DEVICE(0x11AB, 0xABCD)}, /* FPGA board */
    {PCI_DEVICE(0x11AB, 0x0f13)},
    {PCI_DEVICE(0x11AB, 0x0a72)}, /* Oak */
    {0,} /* Terminate the table */
};
#if CONFIG_PM_SLEEP
/* Device Power Management (DPM) support */
static const struct dev_pm_ops oak_dpm_ops = {
    .suspend = oak_dpm_suspend,
    .resume = oak_dpm_resume,
};
#endif
/* PCIe - interface structure */
static struct pci_driver oak_driver = {
    .name = oak_driver_name,
    .id_table = oak_pci_tbl,
    .probe = oak_probe,
    .remove = oak_remove,
#if CONFIG_PM_SLEEP
    .driver.pm = &oak_dpm_ops,
#endif
};
static struct ethtool_ops oak_ethtool_ops = {
    .get_ethtool_stats = oak_ethtool_get_stats,
    .get_strings = oak_ethtool_get_strings,
    .get_sset_count = oak_ethtool_get_sscnt,
    .get_link = ethtool_op_get_link,
    .get_msglevel = oak_dbg_get_level,
    .set_msglevel = oak_dbg_set_level,
    .get_link_ksettings = oak_ethtool_get_link_ksettings,
};
static struct net_device_ops oak_netdev_ops = {
    .ndo_open = oak_net_open,
    .ndo_stop = oak_net_close,
    .ndo_start_xmit = oak_net_xmit_frame,
    .ndo_do_ioctl = oak_net_ioctl,
    .ndo_set_mac_address = oak_net_set_mac_addr,
    .ndo_select_queue = oak_net_select_queue,
    .ndo_change_mtu = oak_net_esu_set_mtu,
};
int debug = 0;
int txs = 2048;
int rxs = 2048;
int chan = MAX_NUM_OF_CHANNELS;
int rto = 100;
int mhdr = 0;
int port_speed = 10;

/* software level defination */
#define SOFTWARE_INIT 10
#define HARDWARE_INIT 20
#define HARDWARE_STARTED 30
#define SOFTWARE_STARTED 40

/* private function prototypes */
static int oak_init_module(void);
static void oak_exit_module(void);
static int oak_probe(struct pci_dev* pdev, const struct pci_device_id* dev_id);
static void oak_remove(struct pci_dev *pdev);

/* Name      : init_module
 * Returns   : int
 * Parameters: 
 *  */
static int oak_init_module(void)
{
    /* automatically added for object flow handling */
    int32_t return_3 = 0; /* start of activity code */
    /* UserCode{15A51487-0028-49e6-916F-ED565CFC9C74}:SdSnX58zaJ */
    pr_info("%s - (%s) version %s\n", oak_driver_string, oak_driver_name, oak_driver_version);
    pr_info("%s\n", oak_driver_copyright);

    /* UserCode{15A51487-0028-49e6-916F-ED565CFC9C74} */

    return_3 = pci_register_driver(&oak_driver);
    return return_3;
}

/* Name      : exit_module
 * Returns   : void
 * Parameters: 
 *  */
static void oak_exit_module(void)
{
    /* start of activity code */
    pci_unregister_driver(&oak_driver);
    return;
}

/* Name      : probe
 * Returns   : int
 * Parameters:  struct pci_dev * pdev,  const struct pci_device_id * dev_id
 *  */
static int oak_probe(struct pci_dev* pdev, const struct pci_device_id* dev_id)
{
    /* automatically added for object flow handling */
    int return_10; /* start of activity code */
    int err = 0;
    /* UserCode{D4E52465-5E93-4791-A8E0-487EDF3C0F48}:1OdSPm6Zyw */
    oak_t* adapter = NULL;
    /* UserCode{D4E52465-5E93-4791-A8E0-487EDF3C0F48} */

#if CONFIG_PM
    /* Set PCI device power state to D0 */
    err = pci_set_power_state(pdev, PCI_D0);
    if (err == 0)
        pr_info("oak: Device power state D%d\n", pdev->current_state);
    else
        pr_err("oak: Failed to set the device power state err: %d\n", err);
#endif

    err = oak_init_software(pdev);

    if (err == 0)
    {
        /* UserCode{B9F90CBD-AD96-487f-9DC8-ADBE43BFC45B}:1EfdPrOPjH */
        struct net_device* netdev = pci_get_drvdata(pdev);
        adapter = netdev_priv(netdev);
        adapter->level = 10;
        /* UserCode{B9F90CBD-AD96-487f-9DC8-ADBE43BFC45B} */

        err = oak_init_hardware(pdev);

        if (err == 0)
        {
            /* UserCode{B6E06B18-56B2-4fbd-92F4-809A0D84D7CB}:PfOr5t1lbV */
            adapter->level = 20;
            /* UserCode{B6E06B18-56B2-4fbd-92F4-809A0D84D7CB} */

            err = oak_start_hardware();

            if (err == 0)
            {
                /* UserCode{86B109DD-5FE2-4f22-9ABD-7AB17864A0D7}:7O7k1PHroI */
                adapter->level = 30;
                /* UserCode{86B109DD-5FE2-4f22-9ABD-7AB17864A0D7} */

                err = oak_start_software(pdev);

                if (err == 0)
                {
                    /* UserCode{CBBFD955-F8A6-4dd6-AFD9-23D7CA52B0DE}:ZWwjfSm1zy */
                    adapter->level = 40;
                    /* UserCode{CBBFD955-F8A6-4dd6-AFD9-23D7CA52B0DE} */
                }
                else
                {
                    return_10 = err;
                }
            }
            else
            {
                return_10 = err;
            }
        }
        else
        {
            return_10 = err;
        }
    }
    else
    {
        return_10 = err;
    }

    if (err == 0)
    {
        return_10 = err;

        if (adapter->sw_base != NULL)
        {
            /* UserCode{B042BBF4-2A74-4399-9B05-784F95B6EA88}:w7hxjq1dvQ */
            pr_info("%s[%d] - ESU register access is supported", oak_driver_name, pdev->devfn);
            /* UserCode{B042BBF4-2A74-4399-9B05-784F95B6EA88} */
        }
        else
        {
        }
    }
    else
    {
        oak_remove(pdev);
    }
    /* UserCode{68984A9B-81AC-45c2-A14C-F4DDF25E36F2}:mlR93Mg2NF */
    oakdbg(debug, PROBE, "pdev=%p ndev=%p err=%d", pdev, pci_get_drvdata(pdev), err);

    /* UserCode{68984A9B-81AC-45c2-A14C-F4DDF25E36F2} */

    return return_10;
}

/* Name        : oak_remove
 * Returns     : void
 * Parameters  : struct pci_dev * pdev
 * Description : This function remove the device from kernel
 */
static void oak_remove(struct pci_dev *pdev)
{
	struct net_device *netdev = pci_get_drvdata(pdev);
	oak_t *adapter = NULL;

	if (netdev != NULL)
		adapter = netdev_priv(netdev);
	if (adapter != NULL) {
		if (adapter->level >= SOFTWARE_STARTED)
			oak_stop_software(pdev);
		if (adapter->level >= HARDWARE_STARTED)
			oak_stop_hardware();
		if (adapter->level >= HARDWARE_INIT)
			oak_release_hardware(pdev);
		if (adapter->level >= SOFTWARE_INIT)
			oak_release_software(pdev);
	}
	oakdbg(debug, PROBE, "pdev=%p ndev=%p", pdev, pci_get_drvdata(pdev));

#ifndef OAK_MSIX_LEGACY
	pci_free_irq_vectors(pdev);
#endif
}

/* Name        : oak_get_msix_resources
 * Returns     : int
 * Parameters  : struct pci_dev * pdev
 * Description : Allocate and get the MSIX resources
 */
int oak_get_msix_resources(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	oak_t *adapter = netdev_priv(dev);
	uint32_t num_irqs;
	uint32_t num_cpus = num_online_cpus();
	int err = 0;
	int i;
	int cnt;
	int retval;
#ifndef OAK_MSIX_LEGACY
	int vec;
#endif

	num_irqs = sizeof(adapter->gicu.msi_vec) / sizeof(struct msix_entry);
	cnt = pci_msix_vec_count(pdev);

	if (cnt <= 0)
		retval = -EFAULT;
	else {
		if (cnt <= num_irqs)
			num_irqs = cnt;
		if (num_irqs > num_cpus)
			num_irqs = num_cpus;
		for (i = 0; i < num_irqs; i++) {
			adapter->gicu.msi_vec[i].vector = 0;
			adapter->gicu.msi_vec[i].entry = i;
		}
#ifdef OAK_MSIX_LEGACY
		err = pci_enable_msix_range(pdev, adapter->gicu.msi_vec,
					    num_irqs, num_irqs);
#else
		vec = pci_alloc_irq_vectors(pdev, num_irqs, num_irqs,
					    PCI_IRQ_ALL_TYPES);
		if (vec) {
			pr_info("int vec count %d\n", vec);
			num_irqs = vec;
		}
#endif
		adapter->gicu.num_ldg = num_irqs;
		if (err < 0)
			retval = -EFAULT;
		else
			retval = 0;
		oakdbg(debug, PROBE, "pdev=%p ndev=%p num_irqs=%d/%d err=%d",
				pdev, dev, num_irqs, cnt, err);
	}
	return retval;
}

/* Name      : release_hardware
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_release_hardware(struct pci_dev* pdev)
{
    struct net_device* dev = pci_get_drvdata(pdev);
    oak_t* adapter = netdev_priv(dev);
    int err = 0; /* start of activity code */
    /* UserCode{0E3580D1-501C-4414-9D92-443D07286EDB}:ZzNzE489RP */
    if (adapter->gicu.num_ldg > 0)
    {
        pci_disable_msix(pdev);
    }
    /* UserCode{0E3580D1-501C-4414-9D92-443D07286EDB} */

    /* UserCode{EFF7A608-71E6-469f-8AC3-546A28EDC525}:nM1DlFpdXU */
    pci_release_regions(pdev);
    /* UserCode{EFF7A608-71E6-469f-8AC3-546A28EDC525} */

    /* UserCode{00442176-44C4-422d-9582-6B2EBA4E7CD5}:v0iOY3v2UI */
    pci_disable_device(pdev);
    /* UserCode{00442176-44C4-422d-9582-6B2EBA4E7CD5} */

    /* UserCode{D8F4F7E6-D311-40e6-9180-AF666AD901F1}:mlR93Mg2NF */
    oakdbg(debug, PROBE, "pdev=%p ndev=%p err=%d", pdev, pci_get_drvdata(pdev), err);
    /* UserCode{D8F4F7E6-D311-40e6-9180-AF666AD901F1} */

    return;
}

/* Name        : oak_init_map_config
 * Returns     : int
 * Parameters  : struct pci_dev *pdev
 * Description : This function create a virtual mapping cookie for a PCI BAR
 */
int oak_init_map_config(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	oak_t *adapter = netdev_priv(dev);
	int mem_flags;
	int retval = 0;

	/* This function returns the flags associated with the PCI resource.
	 * Resource flags are used to define some features of the individual
	 * resource. For PCI resources associated with PCI I/O regions, the
	 * information is extracted from the base address registers, but can
	 * come from elsewhere for resources not associated with PCI devices.
	 */
	mem_flags = pci_resource_flags(pdev, 2);

	if ((mem_flags & IORESOURCE_MEM) == 0)
		retval = -EINVAL;
	else
		adapter->sw_base = pci_iomap(pdev, 2, 0);

	oakdbg(debug, PROBE,
		"Device found: dom=%d bus=%d dev=%d fun=%d reg-addr=%p",
		pci_domain_nr(pdev->bus), pdev->bus->number,
		PCI_SLOT(pdev->devfn), pdev->devfn, adapter->um_base);

	return retval;
}

/* Name        : oak_init_read_write_config
 * Returns     : int
 * Parameters  : struct pci_dev *pdev
 * Description : This function read and write into config space.
 */
int oak_init_read_write_config(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	oak_t *adapter = netdev_priv(dev);
	uint32_t v0, v1;
	uint16_t devctl;
	int retval = 0;

	/* Create virtual mapping the PCI BAR configuration space before doing
	 * read or write into configuration space
	 */
	retval = oak_init_map_config(pdev);

	if (retval != 0)
		pr_err("PCI config space mapping failed %d\n", retval);

	/* After the driver has detected the device, it usually needs to read
	 * from or write to the three address spaces: memory, port, and
	 * configuration. In particular, accessing the configuration space is
	 * vital to the driver, because it is the only way it can find out
	 * where the device is mapped in memory and in the I/O space.
	 */
	pci_read_config_dword(pdev, 0x10, &v0);
	pci_read_config_dword(pdev, 0x14, &v1);
	v0 &= 0xfffffff0;
	v0 |= 1;
	pci_write_config_dword(pdev, 0x944, v1);
	pci_write_config_dword(pdev, 0x940, v0);

	pcie_capability_read_word(pdev, PCI_EXP_DEVCTL,	&devctl);
	/* Calculate and store TX max burst size */
	adapter->rrs = 1 << (((devctl &	PCI_EXP_DEVCTL_READRQ) >> 12) + 6);

	if (retval == 0)
		retval = oak_get_msix_resources(pdev);

	return retval;
}

/* Name        : oak_init_pci_config
 * Returns     : int
 * Parameters  : struct pci_dev *pdev
 * Description : This function initialize oak pci config space
 */
int oak_init_pci_config(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	oak_t *adapter = netdev_priv(dev);
	int err = 0;

	err = pci_request_regions(pdev, OAK_DRIVER_NAME);

	if (err == 0) {
		/* Enables bus-mastering for device */
		pci_set_master(pdev);
		/* Save the PCI configuration space */
		pci_save_state(pdev);

		if (err == 0) {
			/* create a virtual mapping cookie for a PCI BAR. Using
			 * this function you will get a __iomem address to your
			 * device BAR. You can access it using ioread*() and
			 * iowrite*(). These functions hide the details if this
			 * is a MMIO or PIO address space and will just do what
			 * you expect from them in the correct way.
			 * void __iomem * pci_iomap(struct pci_dev * dev,
			 * int bar, unsigned long, maxlen);
			 * maxlen specifies the maximum length to map. If you
			 * want to get access to the complete BAR without
			 * checking for its length first, pass 0 here.
			 */
			adapter->um_base = pci_iomap(pdev, 0, 0);

			if (adapter->um_base == NULL)
				err = -ENOMEM;
			else
				err = oak_init_read_write_config(pdev);
		}
	}
	return err;
}

/* Name        : oak_init_hardware
 * Returns     : int
 * Parameters  : struct pci_dev * pdev
 * Description : Initialize oak hardware
 */
int oak_init_hardware(struct pci_dev *pdev)
{
	struct net_device *dev = pci_get_drvdata(pdev);
	oak_t *adapter = netdev_priv(dev);
	int retval = 0;
	int mem_flags;

	/* Initialize device before it's used by a driver */
	retval = pci_enable_device(pdev);

	if (retval != 0)
		pr_err("PCI enable device failed %d\n", retval);

	/* This function returns the flags associated with this resource.
	 * Resource flags are used to define some features of the individual
	 * resource. For PCI resources associated with PCI I/O regions, the
	 * information is extracted from the base address registers, but can
	 * come from elsewhere for resources not associated with PCI devices
	 */
	mem_flags = pci_resource_flags(pdev, 0);

	if ((mem_flags & IORESOURCE_MEM) == 0)
		retval = -EINVAL;
	else {
		pci_read_config_dword(pdev, PCI_CLASS_REVISION,
				&adapter->pci_class_revision);
		adapter->pci_class_revision &= 0x0000000F;
		if (adapter->pci_class_revision > OAK_REVISION_B0)
			retval = -EINVAL;
		else {
			if (retval != 0)
				retval = dma_set_mask_and_coherent(&pdev->dev,
						DMA_BIT_MASK(32));
			else
				retval = dma_set_mask_and_coherent(&pdev->dev,
						DMA_BIT_MASK(64));
		}
	if (retval == 0)
		retval = oak_init_pci_config(pdev);
	}
	oakdbg(debug, PROBE, "pdev=%p ndev=%p err=%d", pdev,
			pci_get_drvdata(pdev), retval);
	return retval;
}

/* Name      : init_pci
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_init_pci(struct pci_dev* pdev)
{
    /* start of activity code */
    return;
}

/* Name      : oak_set_mtu_config
 * Returns   : void
 * Parameters:  struct net_device *netdev
 * Description: This function sets the min and max MTU size in the linux netdev.
 */
void oak_set_mtu_config(struct net_device *netdev)
{
    netdev->min_mtu = ETH_MIN_MTU;
    netdev->max_mtu = OAK_MAX_JUMBO_FRAME_SIZE - (ETH_HLEN + ETH_FCS_LEN);
}

/* Name      : init_software
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_init_software(struct pci_dev* pdev)
{
    struct net_device* netdev = NULL;
    oak_t* oak;
    int err = 0; /* automatically added for object flow handling */
    int return_2; /* start of activity code */
    /* UserCode{1A7566C2-C70F-443d-960E-FB23E34E1F9E}:14MzP1MQD0 */
    netdev = alloc_etherdev_mq(sizeof(oak_t), chan);

    /* UserCode{1A7566C2-C70F-443d-960E-FB23E34E1F9E} */

    if (netdev != NULL)
    {
        /* UserCode{B8255E97-3D1D-4c73-9FB2-ED43A46ECE92}:1eGJs9Is5c */
        SET_NETDEV_DEV(netdev, &pdev->dev);
        pci_set_drvdata(pdev, netdev);
        oak = netdev_priv(netdev);
        oak->device = &pdev->dev;
        oak->netdev = netdev;
        oak->pdev = pdev;
        /* UserCode{B8255E97-3D1D-4c73-9FB2-ED43A46ECE92} */

#if CONFIG_PM
        /* Create sysfs entry for D0, D1, D2 and D3 states testing */
        oak_dpm_create_sysfs(oak);
#endif

        return_2 = err;

        if (err != 0)
        {
            oak_release_software(pdev);
        }
        else
        {
            /* UserCode{38D79BE3-9956-4d88-8279-6757E485A1FE}:tesoi2C7cs */
            netdev->netdev_ops = &oak_netdev_ops;
            netdev->features = oak_chksum_get_config();
            oak_set_mtu_config(netdev);
            spin_lock_init(&oak->lock);
            /* UserCode{38D79BE3-9956-4d88-8279-6757E485A1FE} */
	    /* Assign random MAC address */
	    eth_hw_addr_random(netdev);
        }
    }
    else
    {
        return_2 = err = -ENOMEM;
    }

    /* UserCode{5AF52ED9-43FE-475f-84BF-964C2C4928A9}:mlR93Mg2NF */
    oakdbg(debug, PROBE, "pdev=%p ndev=%p err=%d", pdev, pci_get_drvdata(pdev), err);
    /* UserCode{5AF52ED9-43FE-475f-84BF-964C2C4928A9} */

    return return_2;
}

/* Name      : release_software
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_release_software(struct pci_dev* pdev)
{
    int retval;
    struct net_device* netdev = pci_get_drvdata(pdev); /* start of activity code */
    oak_t* oak;
    oak = netdev_priv(netdev);

#if CONFIG_PM
    /* Set the PCI device power state to D3hot */
    retval = pci_set_power_state(pdev, PCI_D3hot);
    if (retval == 0)
        pr_info("oak: Device power state D%d\n", pdev->current_state);
    else
        pr_err("oak: Failed to set the device power state err: %d\n", retval);

    /* Remove sysfs entry */
    oak_dpm_remove_sysfs(oak);
#endif

    /* UserCode{2F73D0CB-CB50-40fe-B322-F30B1A2AB2B1}:199kbfBgmo */
    oakdbg(debug, PROBE, "pdev=%p ndev=%p", pdev, pci_get_drvdata(pdev));
    /* UserCode{2F73D0CB-CB50-40fe-B322-F30B1A2AB2B1} */

    /* UserCode{87B990A3-874F-4ad1-AC84-DD968E06EF5F}:11nvw95fpZ */
    free_netdev(netdev);
    /* UserCode{87B990A3-874F-4ad1-AC84-DD968E06EF5F} */

    return;
}

/* Name      : start_hardware
 * Returns   : int
 * Parameters: 
 *  */
int oak_start_hardware(void)
{
    /* automatically added for object flow handling */
    int return_1; /* start of activity code */
    return_1 = 0;
    return return_1;
}

/* Name      : start_software
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_start_software(struct pci_dev* pdev)
{
    struct net_device* netdev = pci_get_drvdata(pdev);
    int err = 0; /* automatically added for object flow handling */
    int return_2; /* start of activity code */
    /* UserCode{87C65DAE-FE7F-431d-A228-F340FBA3ED6E}:uyU18eMDt5 */
    netdev->ethtool_ops = &oak_ethtool_ops;
    /* UserCode{87C65DAE-FE7F-431d-A228-F340FBA3ED6E} */

    oak_net_add_napi(netdev);
    /* UserCode{B9650390-79FB-4f72-BC9A-1DC5BF52FD2E}:ejE8ZxF32E */
    err = register_netdev(netdev);
    /* UserCode{B9650390-79FB-4f72-BC9A-1DC5BF52FD2E} */

    return_2 = 0;
    return return_2;
}

/* Name      : stop_hardware
 * Returns   : void
 * Parameters: 
 *  */
void oak_stop_hardware(void)
{
    /* start of activity code */
    return;
}

/* Name      : stop_software
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_stop_software(struct pci_dev* pdev)
{
    struct net_device* netdev = pci_get_drvdata(pdev); /* start of activity code */
    /* UserCode{4605CD77-3541-4856-BC73-81E5F109AD07}:XKWNJ00x3g */
    unregister_netdev(netdev);
    /* UserCode{4605CD77-3541-4856-BC73-81E5F109AD07} */

    return;
}

