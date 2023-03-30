/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * PCIe DMA EPF Library for Tegra PCIe
 *
 * Copyright (C) 2021 NVIDIA Corporation. All rights reserved.
 */

#ifndef TEGRA_PCIE_EDMA_H
#define TEGRA_PCIE_EDMA_H

#define DMA_RD_CHNL_NUM			2
#define DMA_WR_CHNL_NUM			4

#define EDMA_DESC_SZ			32
/**
 * Application can use this macro as default number of descriptors.
 * Number of descriptors should always be power of 2.
 */
#define NUM_EDMA_DESC			4096

/**
 * @brief typedef to define various values for xfer status passed for edma_complete_t or
 * tegra_pcie_edma_submit_xfer()
 */
typedef enum {
	EDMA_XFER_SUCCESS = 0,
	EDMA_XFER_FAIL_INVAL_INPUTS,
	EDMA_XFER_FAIL_NOMEM,
	EDMA_XFER_FAIL_TIMEOUT,
	EDMA_XFER_ABORT,
	EDMA_XFER_DEINIT,
} edma_xfer_status_t;

/** @brief typedef to define various values for xfer type passed tegra_pcie_edma_submit_xfer() */
typedef enum {
	EDMA_XFER_WRITE = 0,
	EDMA_XFER_READ,
} edma_xfer_type_t;

/**
 * @brief typedef to define various values for channel xfer configuration done
 * during tegra_pcie_edma_submit_init()
 */
typedef enum {
	EDMA_CHAN_XFER_SYNC = 0,
	EDMA_CHAN_XFER_ASYNC,
} edma_chan_type_t;

/** Forward declaration */
struct tegra_pcie_edma_desc;

/** @brief Tx Async callback function pointer */
typedef void (edma_complete_t)(void *priv, edma_xfer_status_t status,
			       struct tegra_pcie_edma_desc *desc);


/** @brief Remote EDMA controller details.
 *  @note: this is initial revision and expected to be modified.
 */
struct pcie_tegra_edma_remote_info {
	/** MSI IRQ number */
	uint32_t msi_irq;
	/** MSI data that needs to be configured on EP DMA registers */
	uint16_t msi_data;
	/** MSI address that needs to be configured on EP DMA registers */
	uint64_t msi_addr;
	/** EP's DMA PHY base address, which same as BAR4 base address */
	phys_addr_t dma_phy_base;
	/** EP's DMA register spec size, which same as BAR4 size */
	uint32_t dma_size;
	/** &pci_dev.dev poniter used for devm_* and logging */
	struct device *dev;
};

/** @brief details of EDMA Tx channel configuration */
struct tegra_pcie_edma_chans_info {
	/** Variable to specify if corresponding channel should run in Sync/Async mode. */
	edma_chan_type_t ch_type;
	/** Number of descriptors that needs to be configured for this channel.
	 *  @note
	 *   - If 0 is passed, this channel will be treated un-used.
	 *   - else it must be power of 2.
	 */
	uint32_t num_descriptors;
	/* Below parameter are used, only if edma_remote is present in #tegra_pcie_edma_init_info */
	/** Descriptor PHY base allocated by client which is part of BAR0. Memory allocated for this
	 *  should be 1 more than number of descriptors.
	 */
	phys_addr_t desc_phy_base;
	/** Abosolute IOVA address of desc of desc_phy_base. */
	dma_addr_t desc_iova;
};

/** @brief init data structure to be used for tegra_pcie_edma_init() API */
struct tegra_pcie_edma_init_info {
	/** configuration details for edma Tx channels */
	struct tegra_pcie_edma_chans_info tx[DMA_WR_CHNL_NUM];
	/** configuration details for edma Tx channels */
	struct tegra_pcie_edma_chans_info rx[DMA_RD_CHNL_NUM];
	/** device node for corresponding edma to read information from DT */
	struct device_node *np;
	/**
	 * Remote edma pointer: if not NULL, library uses remote EDMA engine for transfers
	 * else uses local controller EDMA engine.
	 */
	struct pcie_tegra_edma_remote_info *edma_remote;
};

/** @brief edma descriptor for data transfer operations */
struct tegra_pcie_edma_desc {
	/** source address of data buffer */
	dma_addr_t src;
	/** destination address where data buffer needs to be transferred */
	dma_addr_t dst;
	/** Size of data buffer */
	uint32_t sz;
};

/** @brief data strcuture needs to be passed for Tx operations */
struct tegra_pcie_edma_xfer_info {
	/** Read or write operation. 0 -> write, 1->read */
	edma_xfer_type_t type;
	/** Channel on which operation needs to be performed.
	 *  Range 0 to (DMA_RD_CHNL_NUM-1)/(DMA_WR_CHNL_NUM-1)
	 */
	uint32_t channel_num;
	/** EDMA descriptor structure with source, destination DMA addr along with its size. */
	struct tegra_pcie_edma_desc *desc;
	/** Number of desc entries. */
	uint32_t nents;
	/** complete callback to be called */
	edma_complete_t *complete;
	/** Caller'z private data pointer which will be passed as part of edma_complete_t */
	void *priv;
};

/**
 * @brief: API to perform EDMA library initialization.
 * @param[in] info: EDMA init data structure. Refer struct tegra_pcie_edma_init_info for details.
 * @retVal: NULL on failure and valid pointer if success and this should
 *          be passed for all subsequent calls of this library.
 */
void *tegra_pcie_edma_initialize(struct tegra_pcie_edma_init_info *info);

/**
 * @brief: API to perform transfer operation.
 * @param[in] tx_info: EDMA Tx data structure. Refer struct tegra_pcie_edma_xfer_info for details.
 * @param[in] coockie : cookie data returned in tegra_pcie_edma_initialize() call.
 * @retVal: Refer edma_xfer_status_t.
 */
edma_xfer_status_t tegra_pcie_edma_submit_xfer(void *cookie,
						struct tegra_pcie_edma_xfer_info *tx_info);

/**
 * @brief: API to perform de-init of EDMA library.
 * @param[in] cookie : cookie data returned in tegra_pcie_edma_initialize() call.
 */
void tegra_pcie_edma_deinit(void *cookie);

#endif //TEGRA_PCIE_EDMA_H
