/*
 * Copyright (c) 2021-2023, NVIDIA CORPORATION. All rights reserved.
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

#ifdef MACSEC_SUPPORT
#include <osi_macsec.h>
#include "macsec.h"
#include "../osi/common/common.h"
#include "core_local.h"

#if 0 /* Qnx */
#define MACSEC_LOG(...) \
	{ \
		slogf(0, 6, ##__VA_ARGS__); \
	}

#elif 0 /* Linux */
#include <linux/printk.h>
#define MACSEC_LOG(...) \
	{ \
		pr_debug(__VA_ARGS__); \
	}
#else
#define MACSEC_LOG(...)
#endif

#ifdef DEBUG_MACSEC
/**
 * @brief poll_for_dbg_buf_update - Query the status of a debug buffer update.
 *
 * @note
 * Algorithm:
 *  - Waits for reset of MACSEC_DEBUG_BUF_CONFIG_0_UPDATE for max polling count of 1000.
 *   - Sleeps for 1 micro sec for each iteration.
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.Used param macsec_base, osd_ops.udelay.
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t poll_for_dbg_buf_update(struct osi_core_priv_data *const osi_core)
{
	nveu32_t retry = RETRY_COUNT;
	nveu32_t dbg_buf_config;
	nve32_t cond = COND_NOT_MET;
	nve32_t ret = 0;
	nveu32_t count;

	count = 0;
	while (cond == COND_NOT_MET) {
		if (count > retry) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				      "timeout!\n", 0ULL);
			ret = -1;
			goto err;
		}

		dbg_buf_config = osi_readla(osi_core,
			(nveu8_t *)osi_core->macsec_base +
			 MACSEC_DEBUG_BUF_CONFIG_0);
		if ((dbg_buf_config & MACSEC_DEBUG_BUF_CONFIG_0_UPDATE) == OSI_NONE) {
			cond = COND_MET;
		}

		count++;
		/* wait on UPDATE bit to reset */
		osi_core->osd_ops.udelay(RETRY_DELAY);
	}
err:
	return ret;

}

/**
 * @brief write_dbg_buf_data - Commit debug buffer to HW
 *
 * @note
 * Algorithm:
 *  - Writes debug buffer data to MACSEC_DEBUG_BUF_DATA_0 register
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.Used param macsec_base
 * @param[in] dbg_buf: Pointer to debug buffer data to be written
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void write_dbg_buf_data(
				struct osi_core_priv_data *const osi_core,
				nveu32_t const *const dbg_buf)
{

	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;
	nveu32_t i;

	/* Commit the dbg buffer to HW */
	for (i = 0; i < DBG_BUF_LEN; i++) {
		osi_writela(osi_core, dbg_buf[i], base +
			    MACSEC_DEBUG_BUF_DATA_0(i));
	}
}

/**
 * @brief read_dbg_buf_data - Read debug buffer from HW
 *
 * @note
 * Algorithm:
 *  - Reads debug buffer data from MACSEC_DEBUG_BUF_DATA_0 register
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.Used param macsec_base
 * @param[in] dbg_buf: Pointer to debug buffer data to be read
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void read_dbg_buf_data(
				struct osi_core_priv_data *const osi_core,
				nveu32_t *dbg_buf)
{

	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;
	nveu32_t i;

	/* Read debug buffer from HW */
	for (i = 0; i < DBG_BUF_LEN; i++) {
		dbg_buf[i] = osi_readla(osi_core, base +
					MACSEC_DEBUG_BUF_DATA_0(i));
	}
}

/**
 * @brief write_tx_dbg_trigger_evts - Trigger and start capturing the tx dbg events
 *
 * @note
 * Algorithm:
 *  - Enables Tx Debug events for the events passed from dbg_buf_config
 *  - Start capturing the triggered events by enabling the same in MACSEC_TX_DEBUG_CONTROL_0
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.Used param macsec_base
 * @param[in] dbg_buf_config: Pointer to dbg buffer events. Used param flags
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void write_tx_dbg_trigger_evts(
		struct osi_core_priv_data *const osi_core,
		const struct osi_macsec_dbg_buf_config *const dbg_buf_config)
{

	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;
	nveu32_t flags = 0;
	nveu32_t tx_trigger_evts;
	nveu32_t debug_ctrl_reg;

	flags = dbg_buf_config->flags;
	tx_trigger_evts = osi_readla(osi_core,
				     base + MACSEC_TX_DEBUG_TRIGGER_EN_0);
	if ((flags & OSI_TX_DBG_LKUP_MISS_EVT) != OSI_NONE) {
		tx_trigger_evts |= MACSEC_TX_DBG_LKUP_MISS;
	} else {
		tx_trigger_evts &= ~MACSEC_TX_DBG_LKUP_MISS;
	}

	if ((flags & OSI_TX_DBG_AN_NOT_VALID_EVT) != OSI_NONE) {
		tx_trigger_evts |= MACSEC_TX_DBG_AN_NOT_VALID;
	} else {
		tx_trigger_evts &= ~MACSEC_TX_DBG_AN_NOT_VALID;
	}

	if ((flags & OSI_TX_DBG_KEY_NOT_VALID_EVT) != OSI_NONE) {
		tx_trigger_evts |= MACSEC_TX_DBG_KEY_NOT_VALID;
	} else {
		tx_trigger_evts &= ~MACSEC_TX_DBG_KEY_NOT_VALID;
	}

	if ((flags & OSI_TX_DBG_CRC_CORRUPT_EVT) != OSI_NONE) {
		tx_trigger_evts |= MACSEC_TX_DBG_CRC_CORRUPT;
	} else {
		tx_trigger_evts &= ~MACSEC_TX_DBG_CRC_CORRUPT;
	}

	if ((flags & OSI_TX_DBG_ICV_CORRUPT_EVT) != OSI_NONE) {
		tx_trigger_evts |= MACSEC_TX_DBG_ICV_CORRUPT;
	} else {
		tx_trigger_evts &= ~MACSEC_TX_DBG_ICV_CORRUPT;
	}

	if ((flags & OSI_TX_DBG_CAPTURE_EVT) != OSI_NONE) {
		tx_trigger_evts |= MACSEC_TX_DBG_CAPTURE;
	} else {
		tx_trigger_evts &= ~MACSEC_TX_DBG_CAPTURE;
	}

	MACSEC_LOG("%s: 0x%x", __func__, tx_trigger_evts);
	osi_writela(osi_core, tx_trigger_evts,
		    base + MACSEC_TX_DEBUG_TRIGGER_EN_0);
	if (tx_trigger_evts != OSI_NONE) {
		/** Start the tx debug buffer capture */
		debug_ctrl_reg = osi_readla(osi_core,
				    base + MACSEC_TX_DEBUG_CONTROL_0);
		debug_ctrl_reg |= MACSEC_TX_DEBUG_CONTROL_0_START_CAP;
		MACSEC_LOG("%s: debug_ctrl_reg 0x%x", __func__,
		       debug_ctrl_reg);
		osi_writela(osi_core, debug_ctrl_reg,
			    base + MACSEC_TX_DEBUG_CONTROL_0);
	}
}

/**
 * @brief tx_dbg_trigger_evts - Trigger or read the Tx dbg buffer events
 *
 * @note
 * Algorithm:
 *  - Enables Tx Debug events for the events passed from dbg_buf_config or
 *  - Reads the enabled tx dbg buffers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.Used param macsec_base
 * @param[in] dbg_buf_config: Pointer to dbg buffer events. Used param rw, flags
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void tx_dbg_trigger_evts(
		struct osi_core_priv_data *const osi_core,
		struct osi_macsec_dbg_buf_config *const dbg_buf_config)
{

	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;
	nveu32_t flags = 0;
	nveu32_t tx_trigger_evts;

	if (dbg_buf_config->rw == OSI_LUT_WRITE) {
		write_tx_dbg_trigger_evts(osi_core, dbg_buf_config);
	} else {
		tx_trigger_evts = osi_readla(osi_core,
					base + MACSEC_TX_DEBUG_TRIGGER_EN_0);
		MACSEC_LOG("%s: 0x%x", __func__, tx_trigger_evts);
		if ((tx_trigger_evts & MACSEC_TX_DBG_LKUP_MISS) != OSI_NONE) {
			flags |= OSI_TX_DBG_LKUP_MISS_EVT;
		}
		if ((tx_trigger_evts & MACSEC_TX_DBG_AN_NOT_VALID) != OSI_NONE) {
			flags |= OSI_TX_DBG_AN_NOT_VALID_EVT;
		}
		if ((tx_trigger_evts & MACSEC_TX_DBG_KEY_NOT_VALID) != OSI_NONE) {
			flags |= OSI_TX_DBG_KEY_NOT_VALID_EVT;
		}
		if ((tx_trigger_evts & MACSEC_TX_DBG_CRC_CORRUPT) != OSI_NONE) {
			flags |= OSI_TX_DBG_CRC_CORRUPT_EVT;
		}
		if ((tx_trigger_evts & MACSEC_TX_DBG_ICV_CORRUPT) != OSI_NONE) {
			flags |= OSI_TX_DBG_ICV_CORRUPT_EVT;
		}
		if ((tx_trigger_evts & MACSEC_TX_DBG_CAPTURE) != OSI_NONE) {
			flags |= OSI_TX_DBG_CAPTURE_EVT;
		}
		dbg_buf_config->flags = flags;
	}
}

/**
 * @brief write_rx_dbg_trigger_evts - Trigger and start capturing the rx dbg events
 *
 * @note
 * Algorithm:
 *  - Enables Rx Debug events for the events passed from dbg_buf_config
 *  - Start capturing the triggered events by enabling the same in MACSEC_RX_DEBUG_CONTROL_0
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.Used param macsec_base
 * @param[in] dbg_buf_config: Pointer to dbg buffer events. Used param flags
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void write_rx_dbg_trigger_evts(
		struct osi_core_priv_data *const osi_core,
		const struct osi_macsec_dbg_buf_config *const dbg_buf_config)
{

	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;
	nveu32_t flags = 0;
	nveu32_t rx_trigger_evts = 0;
	nveu32_t debug_ctrl_reg;

	flags = dbg_buf_config->flags;
	rx_trigger_evts = osi_readla(osi_core,
				base + MACSEC_RX_DEBUG_TRIGGER_EN_0);
	if ((flags & OSI_RX_DBG_LKUP_MISS_EVT) != OSI_NONE) {
		rx_trigger_evts |= MACSEC_RX_DBG_LKUP_MISS;
	} else {
		rx_trigger_evts &= ~MACSEC_RX_DBG_LKUP_MISS;
	}

	if ((flags & OSI_RX_DBG_KEY_NOT_VALID_EVT) != OSI_NONE) {
		rx_trigger_evts |= MACSEC_RX_DBG_KEY_NOT_VALID;
	} else {
		rx_trigger_evts &= ~MACSEC_RX_DBG_KEY_NOT_VALID;
	}

	if ((flags & OSI_RX_DBG_REPLAY_ERR_EVT) != OSI_NONE) {
		rx_trigger_evts |= MACSEC_RX_DBG_REPLAY_ERR;
	} else {
		rx_trigger_evts &= ~MACSEC_RX_DBG_REPLAY_ERR;
	}

	if ((flags & OSI_RX_DBG_CRC_CORRUPT_EVT) != OSI_NONE) {
		rx_trigger_evts |= MACSEC_RX_DBG_CRC_CORRUPT;
	} else {
		rx_trigger_evts &= ~MACSEC_RX_DBG_CRC_CORRUPT;
	}

	if ((flags & OSI_RX_DBG_ICV_ERROR_EVT) != OSI_NONE) {
		rx_trigger_evts |= MACSEC_RX_DBG_ICV_ERROR;
	} else {
		rx_trigger_evts &= ~MACSEC_RX_DBG_ICV_ERROR;
	}

	if ((flags & OSI_RX_DBG_CAPTURE_EVT) != OSI_NONE) {
		rx_trigger_evts |= MACSEC_RX_DBG_CAPTURE;
	} else {
		rx_trigger_evts &= ~MACSEC_RX_DBG_CAPTURE;
	}
	MACSEC_LOG("%s: 0x%x", __func__, rx_trigger_evts);
	osi_writela(osi_core, rx_trigger_evts,
		    base + MACSEC_RX_DEBUG_TRIGGER_EN_0);
	if (rx_trigger_evts != OSI_NONE) {
		/** Start the tx debug buffer capture */
		debug_ctrl_reg = osi_readla(osi_core,
				    base + MACSEC_RX_DEBUG_CONTROL_0);
		debug_ctrl_reg |= MACSEC_RX_DEBUG_CONTROL_0_START_CAP;
		MACSEC_LOG("%s: debug_ctrl_reg 0x%x", __func__,
		       debug_ctrl_reg);
		osi_writela(osi_core, debug_ctrl_reg,
			    base + MACSEC_RX_DEBUG_CONTROL_0);
	}
}

/**
 * @brief rx_dbg_trigger_evts - Trigger or read the Rx dbg buffer events
 *
 * @note
 * Algorithm:
 *  - Enables Rx Debug events for the events passed from dbg_buf_config or
 *  - Reads the enabled rx dbg buffers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.Used param macsec_base
 * @param[in] dbg_buf_config: Pointer to dbg buffer events. Used param rw, flags
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void rx_dbg_trigger_evts(
		struct osi_core_priv_data *const osi_core,
		struct osi_macsec_dbg_buf_config *const dbg_buf_config)
{

	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;
	nveu32_t flags = 0;
	nveu32_t rx_trigger_evts = 0;

	if (dbg_buf_config->rw == OSI_LUT_WRITE) {
		write_rx_dbg_trigger_evts(osi_core, dbg_buf_config);
	} else {
		rx_trigger_evts = osi_readla(osi_core,
					base + MACSEC_RX_DEBUG_TRIGGER_EN_0);
		MACSEC_LOG("%s: 0x%x", __func__, rx_trigger_evts);
		if ((rx_trigger_evts & MACSEC_RX_DBG_LKUP_MISS) != OSI_NONE) {
			flags |= OSI_RX_DBG_LKUP_MISS_EVT;
		}
		if ((rx_trigger_evts & MACSEC_RX_DBG_KEY_NOT_VALID) != OSI_NONE) {
			flags |= OSI_RX_DBG_KEY_NOT_VALID_EVT;
		}
		if ((rx_trigger_evts & MACSEC_RX_DBG_REPLAY_ERR) != OSI_NONE) {
			flags |= OSI_RX_DBG_REPLAY_ERR_EVT;
		}
		if ((rx_trigger_evts & MACSEC_RX_DBG_CRC_CORRUPT) != OSI_NONE) {
			flags |= OSI_RX_DBG_CRC_CORRUPT_EVT;
		}
		if ((rx_trigger_evts & MACSEC_RX_DBG_ICV_ERROR) != OSI_NONE) {
			flags |= OSI_RX_DBG_ICV_ERROR_EVT;
		}
		if ((rx_trigger_evts & MACSEC_RX_DBG_CAPTURE) != OSI_NONE) {
			flags |= OSI_RX_DBG_CAPTURE_EVT;
		}
		dbg_buf_config->flags = flags;
	}
}

/**
 * @brief validate_inputs_macsec_dbg_buf_conf - validates the dbg buffer configuration
 *
 * @note
 * Algorithm:
 *  - Validates if rw and ctrl_sel is valid else returns -1
 *  - Validates if the index is valid else return -1
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] dbg_buf_config: Pointer to dbg buffer events. Used param rw, index, ctlr_sel
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t validate_inputs_macsec_dbg_buf_conf(
		const struct osi_core_priv_data *const osi_core,
		const struct osi_macsec_dbg_buf_config *const dbg_buf_config)
{
	nve32_t ret = 0;

	(void) osi_core;
	/* Validate inputs */
	if ((dbg_buf_config->rw > OSI_RW_MAX) ||
		(dbg_buf_config->ctlr_sel > OSI_CTLR_SEL_MAX)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			      "Params validation failed\n", 0ULL);
		ret = -1;
		goto err;
	}

	if (((dbg_buf_config->ctlr_sel == OSI_CTLR_SEL_TX) &&
		(dbg_buf_config->index > OSI_TX_DBG_BUF_IDX_MAX)) ||
		((dbg_buf_config->ctlr_sel == OSI_CTLR_SEL_RX) &&
		(dbg_buf_config->index > OSI_RX_DBG_BUF_IDX_MAX))) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			      "Wrong index \n", dbg_buf_config->index);
		ret = -1;
		goto err;
	}
err:
	return ret;
}

/**
 * @brief validate_inputs_macsec_dbg_buf_conf - validates the dbg buffer configuration
 *
 * @note
 * Algorithm:
 *  - Validates if dbg buffer configuration is valid else returns -1
 *  - Reads MACSEC_DEBUG_BUF_CONFIG_0 register
 *  - Reads or writes the dbg buffer configuration depending on the rw field in dbg_buf_config
 *  - poll for read or write happens successfully
 *  - Read the dbg buffer if only read is enabled
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] dbg_buf_config: Pointer to dbg buffer events. Used param rw, index, ctlr_sel
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t macsec_dbg_buf_config(struct osi_core_priv_data *const osi_core,
		struct osi_macsec_dbg_buf_config *const dbg_buf_config)
{

	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;
	nveu32_t dbg_config_reg = 0;
	nve32_t ret = 0;

	if (validate_inputs_macsec_dbg_buf_conf(osi_core, dbg_buf_config) < 0) {
		ret = -1;
		goto err;
	}

	dbg_config_reg = osi_readla(osi_core, base + MACSEC_DEBUG_BUF_CONFIG_0);

	if (dbg_buf_config->ctlr_sel != OSI_NONE) {
		dbg_config_reg |= MACSEC_DEBUG_BUF_CONFIG_0_CTLR_SEL;
	} else {
		dbg_config_reg &= ~MACSEC_DEBUG_BUF_CONFIG_0_CTLR_SEL;
	}

	if (dbg_buf_config->rw != OSI_NONE) {
		dbg_config_reg |= MACSEC_DEBUG_BUF_CONFIG_0_RW;
		/** Write data to debug buffer */
		write_dbg_buf_data(osi_core, dbg_buf_config->dbg_buf);
	} else {
		dbg_config_reg &= ~MACSEC_DEBUG_BUF_CONFIG_0_RW;
	}

	dbg_config_reg &= ~MACSEC_DEBUG_BUF_CONFIG_0_IDX_MASK;
	dbg_config_reg |= dbg_buf_config->index ;
	dbg_config_reg |= MACSEC_DEBUG_BUF_CONFIG_0_UPDATE;
	osi_writela(osi_core, dbg_config_reg, base + MACSEC_DEBUG_BUF_CONFIG_0);
	ret = poll_for_dbg_buf_update(osi_core);
	if (ret < 0) {
		goto err;
	}

	if (dbg_buf_config->rw == OSI_LUT_READ) {
		read_dbg_buf_data(osi_core, dbg_buf_config->dbg_buf);
	}
err:
	return ret;
}

/**
 * @brief macsec_dbg_events_config - Configures dbg events
 *
 * @note
 * Algorithm:
 *  - Validates if dbg buffer configuration is valid else returns -1
 *  - If more than 1 event is requested to be configured return -1
 *  - Configures Tx or Rx dbg trigger events
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] dbg_buf_config: Pointer to dbg buffer events. Used param rw, flags, ctlr_sel
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t macsec_dbg_events_config(
		struct osi_core_priv_data *const osi_core,
		struct osi_macsec_dbg_buf_config *const dbg_buf_config)
{
	nveu64_t events = 0;
	nveu32_t i, flags = dbg_buf_config->flags;
	nve32_t ret = 0;

	/* Validate inputs */
	if ((dbg_buf_config->rw > OSI_RW_MAX) ||
		(dbg_buf_config->ctlr_sel > OSI_CTLR_SEL_MAX)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Params validation failed!\n", 0ULL);
		ret = -1;
		goto err;
	}

	/* Only one event allowed to configure at a time */
	if ((flags != OSI_NONE) && (dbg_buf_config->rw == OSI_LUT_WRITE)) {
		for (i = 0; i < 32U; i++) {
			if ((flags & ((nveu32_t)(1U) << i)) != OSI_NONE) {
				CERT_C__POST_INC__U64(events);
			}
		}
		if (events > 1U) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "Don't allow more than one debug events set\n", flags);
			ret = -1;
			goto err;
		}
	}

	switch (dbg_buf_config->ctlr_sel) {
	case OSI_CTLR_SEL_TX:
		tx_dbg_trigger_evts(osi_core, dbg_buf_config);
		break;
	case OSI_CTLR_SEL_RX:
		rx_dbg_trigger_evts(osi_core, dbg_buf_config);
		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Unknown controller select\n", 0ULL);
		break;
	}
err:
	return ret;
}
#endif /* DEBUG_MACSEC */

/**
 * @brief update_macsec_mmc_val - Reads specific macsec mmc counters
 *
 * @note
 * Algorithm:
 *  - Reads and returns macsec mmc counters
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. Used param macsec_base
 * @param[in] offset: Memory offset where mmc counters are stored
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval value of mmc counters read
 */
static inline nveul64_t update_macsec_mmc_val(
			struct osi_core_priv_data *osi_core,
			nveu64_t offset)
{
	nveul64_t value_lo;
	nveul64_t value_hi;

	value_lo = osi_readla(osi_core,
			      (nveu8_t *)osi_core->macsec_base + offset);
	value_hi = osi_readla(osi_core,
			      (nveu8_t *)osi_core->macsec_base +
			      ((offset & 0xFFFFU) + 4U));
	return ((value_lo) | (value_hi << 31));
}

/**
 * @brief macsec_read_mmc - Reads all macsec mmc counters
 *
 * @note
 * Algorithm:
 *  - Reads and updates the macsec_mmc counters in osi_core
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. Used param macsec_mmc
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void macsec_read_mmc(struct osi_core_priv_data *const osi_core)
{
	struct osi_macsec_mmc_counters *mmc = &osi_core->macsec_mmc;
	nveu16_t i;

	mmc->tx_pkts_untaged =
		update_macsec_mmc_val(osi_core, MACSEC_TX_PKTS_UNTG_LO_0);
	mmc->tx_pkts_too_long =
		update_macsec_mmc_val(osi_core, MACSEC_TX_PKTS_TOO_LONG_LO_0);
	mmc->tx_octets_protected =
		update_macsec_mmc_val(osi_core, MACSEC_TX_OCTETS_PRTCTD_LO_0);
	mmc->rx_pkts_no_tag =
		update_macsec_mmc_val(osi_core, MACSEC_RX_PKTS_NOTG_LO_0);
	mmc->rx_pkts_untagged =
		update_macsec_mmc_val(osi_core, MACSEC_RX_PKTS_UNTG_LO_0);
	mmc->rx_pkts_bad_tag =
		update_macsec_mmc_val(osi_core, MACSEC_RX_PKTS_BADTAG_LO_0);
	mmc->rx_pkts_no_sa_err =
		update_macsec_mmc_val(osi_core, MACSEC_RX_PKTS_NOSAERROR_LO_0);
	mmc->rx_pkts_no_sa =
		update_macsec_mmc_val(osi_core, MACSEC_RX_PKTS_NOSA_LO_0);
	mmc->rx_pkts_overrun =
		update_macsec_mmc_val(osi_core, MACSEC_RX_PKTS_OVRRUN_LO_0);
	mmc->rx_octets_validated =
		update_macsec_mmc_val(osi_core, MACSEC_RX_OCTETS_VLDTD_LO_0);

	for (i = 0; i <= OSI_SC_INDEX_MAX; i++) {
		mmc->tx_pkts_protected[i] =
			update_macsec_mmc_val(osi_core,
					MACSEC_TX_PKTS_PROTECTED_SCx_LO_0(i));
		mmc->rx_pkts_late[i] =
			update_macsec_mmc_val(osi_core,
					      MACSEC_RX_PKTS_LATE_SCx_LO_0(i));
		mmc->rx_pkts_delayed[i] = mmc->rx_pkts_late[i];
		mmc->rx_pkts_not_valid[i] =
			update_macsec_mmc_val(osi_core,
					MACSEC_RX_PKTS_NOTVALID_SCx_LO_0(i));
		mmc->in_pkts_invalid[i] = mmc->rx_pkts_not_valid[i];
		mmc->rx_pkts_unchecked[i] = mmc->rx_pkts_not_valid[i];
		mmc->rx_pkts_ok[i] =
			update_macsec_mmc_val(osi_core,
					      MACSEC_RX_PKTS_OK_SCx_LO_0(i));
	}
}

/**
 * @brief macsec_enable - Enable/Disable macsec Tx/Rx controller
 *
 * @note
 * Algorithm:
 *  - Acquire the macsec_fpe lock
 *  - Return -1 if mac is mgbe and request is to enable macsec when fpe is
 *    already enabled
 *  - Enable/Disable macsec TX/RX based on the request
 *  - Update the is_macsec_enabled flag in osi_core accordingly
 *  - Release the macsec_fpe lock
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. Used param mac,
 *                      is_fpe_enabled, is macsec_enabled
 * @param[in] enable: macsec enable or disable flag
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: Yes
 * - Run time: Yes
 * - De-initialization: Yes
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t macsec_enable(struct osi_core_priv_data *const osi_core,
			     nveu32_t enable)
{
	nveu32_t val;
	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;
	nve32_t ret = 0;

	osi_lock_irq_enabled(&osi_core->macsec_fpe_lock);

	/* MACSEC and FPE cannot coexist on MGBE refer bug 3484034 */
	if ((osi_core->mac == OSI_MAC_HW_MGBE) &&
	    (((enable & OSI_MACSEC_TX_EN) != OSI_NONE) ||
	    ((enable & OSI_MACSEC_RX_EN) != OSI_NONE)) &&
	    (osi_core->is_fpe_enabled == OSI_ENABLE)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "MACSE and FPE cannot coexist on MGBE\n", 0ULL);
		ret = -1;
		goto exit;
	}

	val = osi_readla(osi_core, base + MACSEC_CONTROL0);
	MACSEC_LOG("Read MACSEC_CONTROL0: 0x%x \n", val);

	if ((enable & OSI_MACSEC_TX_EN) == OSI_MACSEC_TX_EN) {
		MACSEC_LOG("Enabling macsec TX\n");
		val |= (MACSEC_TX_EN);
	} else {
		MACSEC_LOG("Disabling macsec TX\n");
		val &= ~(MACSEC_TX_EN);
	}

	if ((enable & OSI_MACSEC_RX_EN) == OSI_MACSEC_RX_EN) {
		MACSEC_LOG("Enabling macsec RX\n");
		val |= (MACSEC_RX_EN);
	} else {
		MACSEC_LOG("Disabling macsec RX\n");
		val &= ~(MACSEC_RX_EN);
	}

	if (((enable & OSI_MACSEC_TX_EN) != OSI_NONE) ||
	    ((enable & OSI_MACSEC_RX_EN) != OSI_NONE)) {
		osi_core->is_macsec_enabled = OSI_ENABLE;
	} else {
		osi_core->is_macsec_enabled = OSI_DISABLE;
	}

	MACSEC_LOG("Write MACSEC_CONTROL0: 0x%x\n", val);
	osi_writela(osi_core, val, base + MACSEC_CONTROL0);

exit:
	osi_unlock_irq_enabled(&osi_core->macsec_fpe_lock);
	return ret;
}

#ifdef MACSEC_KEY_PROGRAM
/**
 * @brief poll_for_kt_update - Query the status of a KT update.
 *
 * @param[in] osi_core: OSI Core private data structure.
 *
 * @retval 0 on Success
 * @retval -1 on Failure
 */
static inline nve32_t poll_for_kt_update(struct osi_core_priv_data *osi_core)
{
	/* half sec timeout */
	nveu32_t retry = RETRY_COUNT;
	nveu32_t kt_config;
	nveu32_t count;
	nve32_t cond = 1;

	count = 0;
	while (cond == 1) {
		if (count > retry) {
			OSI_CORE_ERR(osi_core->osd,
				OSI_LOG_ARG_HW_FAIL,
				"KT update timed out\n",
				0ULL);
			return -1;
		}

		count++;

		kt_config = osi_readla(osi_core,
				       (nveu8_t *)osi_core->tz_base +
				       MACSEC_GCM_KEYTABLE_CONFIG);
		if ((kt_config & MACSEC_KT_CONFIG_UPDATE) == OSI_NONE) {
			/* exit loop */
			cond = 0;
		} else {
			/* wait on UPDATE bit to reset */
			osi_core->osd_ops.udelay(RETRY_DELAY);
		}
	}

	return 0;
}

static nve32_t kt_key_read(struct osi_core_priv_data *const osi_core,
			struct osi_macsec_kt_config *const kt_config)
{
	nveu32_t kt_key[MACSEC_KT_DATA_REG_CNT] = {0};
	nveu32_t i, j;

	for (i = 0; i < MACSEC_KT_DATA_REG_CNT; i++) {
		kt_key[i] = osi_readla(osi_core,
				       (nveu8_t *)osi_core->tz_base +
				       MACSEC_GCM_KEYTABLE_DATA(i));
	}

	if ((kt_key[MACSEC_KT_DATA_REG_CNT - 1U] & MACSEC_KT_ENTRY_VALID) ==
	     MACSEC_KT_ENTRY_VALID) {
		kt_config->flags |= OSI_LUT_FLAGS_ENTRY_VALID;
	}

	for (i = 0; i < MACSEC_KT_DATA_REG_SAK_CNT; i++) {
		for (j = 0; j < INTEGER_LEN; j++) {
			kt_config->entry.sak[(i * 4U) + j] =
			(nveu8_t)((kt_key[i] >> (j * 8U)) & 0xFFU);
		}
	}

	for (i = 0; i < MACSEC_KT_DATA_REG_H_CNT; i++) {
		for (j = 0; j < INTEGER_LEN; j++) {
			kt_config->entry.h[(i * 4U) + j] =
			(nveu8_t)((kt_key[i + MACSEC_KT_DATA_REG_SAK_CNT] >> (j * 8U))
			 & 0xFFU);
		}
	}

	return 0;
}

static nve32_t kt_key_write(struct osi_core_priv_data *const osi_core,
			    const struct osi_macsec_kt_config *const kt_config)
{
	nveu32_t kt_key[MACSEC_KT_DATA_REG_CNT] = {0};
	struct osi_kt_entry entry = kt_config->entry;
	nveu32_t i, j;

	/* write SAK */
	for (i = 0; i < MACSEC_KT_DATA_REG_SAK_CNT; i++) {
		/* 4-bytes in each register */
		for (j = 0; j < INTEGER_LEN; j++) {
			kt_key[i] |= ((nveu32_t)(entry.sak[(i * 4U) + j]) <<
				      (j * 8U));
		}
	}
	/* write H-key */
	for (i = 0; i < MACSEC_KT_DATA_REG_H_CNT; i++) {
		/* 4-bytes in each register */
		for (j = 0; j < INTEGER_LEN; j++) {
			kt_key[i + MACSEC_KT_DATA_REG_SAK_CNT] |=
				((nveu32_t)(entry.h[(i * 4U) + j]) << (j * 8U));
		}
	}

	if ((kt_config->flags & OSI_LUT_FLAGS_ENTRY_VALID) ==
	     OSI_LUT_FLAGS_ENTRY_VALID) {
		kt_key[MACSEC_KT_DATA_REG_CNT - 1U] |= MACSEC_KT_ENTRY_VALID;
	}

	for (i = 0; i < MACSEC_KT_DATA_REG_CNT; i++) {
		osi_writela(osi_core, kt_key[i],
			    (nveu8_t *)osi_core->tz_base +
			    MACSEC_GCM_KEYTABLE_DATA(i));
	}

	return 0;
}

static nve32_t validate_kt_config(const struct osi_macsec_kt_config *const kt_config)
{
	nve32_t ret = 0;

	/* Validate KT config */
	if ((kt_config->table_config.ctlr_sel > OSI_CTLR_SEL_MAX) ||
	    (kt_config->table_config.rw > OSI_RW_MAX) ||
	    (kt_config->table_config.index > OSI_TABLE_INDEX_MAX)) {
		ret = -1;
		goto err;
	}
err:
	return ret;

}

static nve32_t macsec_kt_config(struct osi_core_priv_data *const osi_core,
			    struct osi_macsec_kt_config *const kt_config)
{
	nve32_t ret = 0;
	nveu32_t kt_config_reg = 0;
	nveu8_t *base = (nveu8_t *)osi_core->tz_base;

	ret = validate_kt_config(kt_config);
	if (ret < 0) {
		goto err;
	}

	kt_config_reg = osi_readla(osi_core, base + MACSEC_GCM_KEYTABLE_CONFIG);
	if (kt_config->table_config.ctlr_sel != OSI_NONE) {
		kt_config_reg |= MACSEC_KT_CONFIG_CTLR_SEL;
	} else {
		kt_config_reg &= ~MACSEC_KT_CONFIG_CTLR_SEL;
	}

	if (kt_config->table_config.rw != OSI_NONE) {
		kt_config_reg |= MACSEC_KT_CONFIG_RW;
		/* For write operation, load the lut_data registers */
		ret = kt_key_write(osi_core, kt_config);
		if (ret < 0) {
			goto err;
		}
	} else {
		kt_config_reg &= ~MACSEC_KT_CONFIG_RW;
	}

	kt_config_reg &= ~MACSEC_KT_CONFIG_INDEX_MASK;
	kt_config_reg |= (kt_config->table_config.index);

	kt_config_reg |= MACSEC_KT_CONFIG_UPDATE;
	osi_writela(osi_core, kt_config_reg, base + MACSEC_GCM_KEYTABLE_CONFIG);

	/* Wait for this KT update to finish */
	ret = poll_for_kt_update(osi_core);
	if (ret < 0) {
		goto err;
	}

	if (kt_config->table_config.rw == OSI_NONE) {
		ret = kt_key_read(osi_core, kt_config);
		if (ret < 0) {
			goto err;
		}
	}
err:
	return ret;
}
#endif /* MACSEC_KEY_PROGRAM */

/**
 * @brief poll_for_lut_update - Query the status of a LUT update.
 *
 * @note
 * Algorithm:
 *  - Check if MACSEC_LUT_CONFIG_UPDATE is reset by waiting for 1 micro second
 *    for 1000 iterations
 *  - Return -1 if maximum iterations are reached
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. Used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static inline nve32_t poll_for_lut_update(struct osi_core_priv_data *osi_core)
{
	/* half sec timeout */
	nveu32_t retry = RETRY_COUNT;
	nveu32_t lut_config;
	nveu32_t count;
	nve32_t cond = 1;
	nve32_t ret = 0;

	count = 0;
	while (cond == 1) {
		if (count > retry) {
			OSI_CORE_ERR(osi_core->osd,
				OSI_LOG_ARG_HW_FAIL,
				"LUT update timed out\n",
				0ULL);
			ret = -1;
			goto exit;
		}

		count++;

		lut_config = osi_readla(osi_core,
					(nveu8_t *)osi_core->macsec_base +
					MACSEC_LUT_CONFIG);
		if ((lut_config & MACSEC_LUT_CONFIG_UPDATE) == OSI_NONE) {
			/* exit loop */
			cond = 0;
		} else {
			/* wait on UPDATE bit to reset */
			osi_core->osd_ops.udelay(RETRY_DELAY);
		}
	}
exit:
	return ret;
}

/**
 * @brief read_lut_data - Read LUT data
 *
 * @note
 * Algorithm:
 *  - Read LUT data from MACSEC_LUT_DATA and fill lut_data buffer
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. Used param macsec_base
 * @param[out] lut_data: Read lut_data stored in this buffer
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void read_lut_data(struct osi_core_priv_data *const osi_core,
				 nveu32_t *const lut_data)
{
	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;
	nveu32_t i;

	/* Commit the LUT entry to HW */
	for (i = 0; i < MACSEC_LUT_DATA_REG_CNT; i++) {
		lut_data[i] = osi_readla(osi_core, base + MACSEC_LUT_DATA(i));
	}
}

/**
 * @brief lut_read_inputs_DA - Read LUT data an fill destination address and flags
 *
 * @note
 * Algorithm:
 *  - Read LUT data for mac DA and fill the flags and lut_inputs accordingly
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_data: Read lut_data stored in this buffer
 * @param[out] flags: Flags to indicate if the byte is valid
 * @param[out] entry: Update the DA to lut_inputs from lut_data
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void lut_read_inputs_DA(const nveu32_t *const lut_data,
			       nveu32_t *flags,
			       struct osi_lut_inputs *const entry)
{
	/* MAC DA */
	if ((lut_data[1] & MACSEC_LUT_DA_BYTE0_INACTIVE) !=
	    MACSEC_LUT_DA_BYTE0_INACTIVE) {
		entry->da[0] = (nveu8_t)(lut_data[0] & 0xFFU);
		*flags |= OSI_LUT_FLAGS_DA_BYTE0_VALID;
	}

	if ((lut_data[1] & MACSEC_LUT_DA_BYTE1_INACTIVE) !=
	    MACSEC_LUT_DA_BYTE1_INACTIVE) {
		entry->da[1] = (nveu8_t)((lut_data[0] >> 8) & 0xFFU);
		*flags |= OSI_LUT_FLAGS_DA_BYTE1_VALID;
	}

	if ((lut_data[1] & MACSEC_LUT_DA_BYTE2_INACTIVE) !=
	    MACSEC_LUT_DA_BYTE2_INACTIVE) {
		entry->da[2] = (nveu8_t)((lut_data[0] >> 16) & 0xFFU);
		*flags |= OSI_LUT_FLAGS_DA_BYTE2_VALID;
	}

	if ((lut_data[1] & MACSEC_LUT_DA_BYTE3_INACTIVE) !=
	    MACSEC_LUT_DA_BYTE3_INACTIVE) {
		entry->da[3] = (nveu8_t)((lut_data[0] >> 24) & 0xFFU);
		*flags |= OSI_LUT_FLAGS_DA_BYTE3_VALID;
	}

	if ((lut_data[1] & MACSEC_LUT_DA_BYTE4_INACTIVE) !=
	    MACSEC_LUT_DA_BYTE4_INACTIVE) {
		entry->da[4] = (nveu8_t)(lut_data[1] & 0xFFU);
		*flags |= OSI_LUT_FLAGS_DA_BYTE4_VALID;
	}

	if ((lut_data[1] & MACSEC_LUT_DA_BYTE5_INACTIVE) !=
	    MACSEC_LUT_DA_BYTE5_INACTIVE) {
		entry->da[5] = (nveu8_t)((lut_data[1] >> 8) & 0xFFU);
		*flags |= OSI_LUT_FLAGS_DA_BYTE5_VALID;
	}

}

/**
 * @brief lut_read_inputs_SA - Read LUT data an fill source addresss and flags
 *
 * @note
 * Algorithm:
 *  - Read LUT data for mac SA and fill the flags and lut_inputs accordingly
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_data: Read lut_data stored in this buffer
 * @param[out] flags: Flags to indicate if the byte is valid
 * @param[out] entry: Update the SA to lut_inputs from lut_data
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void lut_read_inputs_SA(const nveu32_t *const lut_data,
			       nveu32_t *flags,
			       struct osi_lut_inputs *const entry)
{
	/* MAC SA */
	if ((lut_data[3] & MACSEC_LUT_SA_BYTE0_INACTIVE) !=
	    MACSEC_LUT_SA_BYTE0_INACTIVE) {
		entry->sa[0] = (nveu8_t)((lut_data[1] >> 22) & 0xFFU);
		*flags |= OSI_LUT_FLAGS_SA_BYTE0_VALID;
	}

	if ((lut_data[3] & MACSEC_LUT_SA_BYTE1_INACTIVE) !=
	    MACSEC_LUT_SA_BYTE1_INACTIVE) {
		entry->sa[1] = (nveu8_t)((lut_data[1] >> 30) |
			      ((lut_data[2] & 0x3FU) << 2));
		*flags |= OSI_LUT_FLAGS_SA_BYTE1_VALID;
	}

	if ((lut_data[3] & MACSEC_LUT_SA_BYTE2_INACTIVE) !=
	    MACSEC_LUT_SA_BYTE2_INACTIVE) {
		entry->sa[2] = (nveu8_t)((lut_data[2] >> 6) & 0xFFU);
		*flags |= OSI_LUT_FLAGS_SA_BYTE2_VALID;
	}

	if ((lut_data[3] & MACSEC_LUT_SA_BYTE3_INACTIVE) !=
	    MACSEC_LUT_SA_BYTE3_INACTIVE) {
		entry->sa[3] = (nveu8_t)((lut_data[2] >> 14) & 0xFFU);
		*flags |= OSI_LUT_FLAGS_SA_BYTE3_VALID;
	}

	if ((lut_data[3] & MACSEC_LUT_SA_BYTE4_INACTIVE) !=
	    MACSEC_LUT_SA_BYTE4_INACTIVE) {
		entry->sa[4] = (nveu8_t)((lut_data[2] >> 22) & 0xFFU);
		*flags |= OSI_LUT_FLAGS_SA_BYTE4_VALID;
	}

	if ((lut_data[3] & MACSEC_LUT_SA_BYTE5_INACTIVE) !=
	    MACSEC_LUT_SA_BYTE5_INACTIVE) {
		entry->sa[5] = (nveu8_t)((lut_data[2] >> 30) |
			      ((lut_data[3] & 0x3FU) << 2));
		*flags |= OSI_LUT_FLAGS_SA_BYTE5_VALID;
	}

}

/**
 * @brief lut_read_inputs_vlan - Read LUT data and fill VLAN fields and flags
 *
 * @note
 * Algorithm:
 *  - Read LUT data for mac VLAN PCP and ID and fill the flags and
 *    lut_inputs accordingly
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_data: Read lut_data stored in this buffer
 * @param[out] flags: Flags to indicate if the byte is valid
 * @param[out] entry: Update the vlan_pcp and vlan_id to lut_inputs from lut_data
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void lut_read_inputs_vlan(const nveu32_t *const lut_data,
				 nveu32_t *flags,
				 struct osi_lut_inputs *const entry)
{
	/* VLAN */
	if ((lut_data[4] & MACSEC_LUT_VLAN_ACTIVE) == MACSEC_LUT_VLAN_ACTIVE) {
		*flags |= OSI_LUT_FLAGS_VLAN_VALID;
		/* VLAN PCP */
		if ((lut_data[4] & MACSEC_LUT_VLAN_PCP_INACTIVE) !=
		    MACSEC_LUT_VLAN_PCP_INACTIVE) {
			*flags |= OSI_LUT_FLAGS_VLAN_PCP_VALID;
			entry->vlan_pcp = lut_data[3] >> 29U;
		}

		/* VLAN ID */
		if ((lut_data[4] & MACSEC_LUT_VLAN_ID_INACTIVE) !=
		    MACSEC_LUT_VLAN_ID_INACTIVE) {
			*flags |= OSI_LUT_FLAGS_VLAN_ID_VALID;
			entry->vlan_id = (lut_data[4] >> 1) & 0xFFFU;
		}
	}
}

/**
 * @brief lut_read_inputs - Read LUT data and fill lut_inputs accordingly
 *
 * @note
 * Algorithm:
 *  - Read LUT data and fill the flags and lut_inputs accordingly
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_data: Read lut_data stored in this buffer
 * @param[out] lut_config: Update the lut_config from lut_data
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t lut_read_inputs(struct osi_macsec_lut_config *const lut_config,
			       const nveu32_t *const lut_data)
{
	struct osi_lut_inputs entry = {0};
	nveu32_t flags = 0;

	lut_read_inputs_DA(lut_data, &flags, &entry);
	lut_read_inputs_SA(lut_data, &flags, &entry);

	/* Ether type */
	if ((lut_data[3] & MACSEC_LUT_ETHTYPE_INACTIVE) !=
	    MACSEC_LUT_ETHTYPE_INACTIVE) {
		entry.ethtype[0] = (nveu8_t)((lut_data[3] >> 12) & (0xFFU));
		entry.ethtype[1] = (nveu8_t)((lut_data[3] >> 20) & (0xFFU));
		flags |= OSI_LUT_FLAGS_ETHTYPE_VALID;
	}

	lut_read_inputs_vlan(lut_data, &flags, &entry);

	/* Byte patterns */
	if ((lut_data[4] & MACSEC_LUT_BYTE0_PATTERN_INACTIVE) !=
	    MACSEC_LUT_BYTE0_PATTERN_INACTIVE) {
		flags |= OSI_LUT_FLAGS_BYTE0_PATTERN_VALID;
		entry.byte_pattern[0] = (nveu8_t)((lut_data[4] >> 15) & 0xFFU);
		entry.byte_pattern_offset[0] = (nveu8_t)((lut_data[4] >> 23) &
						0x3FU);
	}
	if ((lut_data[5] & MACSEC_LUT_BYTE1_PATTERN_INACTIVE) !=
	    MACSEC_LUT_BYTE1_PATTERN_INACTIVE) {
		flags |= OSI_LUT_FLAGS_BYTE1_PATTERN_VALID;
		entry.byte_pattern[1] = (nveu8_t)((lut_data[4] >> 30) |
					((lut_data[5] & 0x3FU) << 2));
		entry.byte_pattern_offset[1] = (nveu8_t)((lut_data[5] >> 6) &
						0x3FU);
	}
	if ((lut_data[5] & MACSEC_LUT_BYTE2_PATTERN_INACTIVE) !=
	    MACSEC_LUT_BYTE2_PATTERN_INACTIVE) {
		flags |= OSI_LUT_FLAGS_BYTE2_PATTERN_VALID;
		entry.byte_pattern[2] = (nveu8_t)((lut_data[5] >> 13) & 0xFFU);
		entry.byte_pattern_offset[2] = (nveu8_t)((lut_data[5] >> 21) &
						0x3FU);
	}
	if ((lut_data[6] & MACSEC_LUT_BYTE3_PATTERN_INACTIVE) !=
	    MACSEC_LUT_BYTE3_PATTERN_INACTIVE) {
		flags |= OSI_LUT_FLAGS_BYTE3_PATTERN_VALID;
		entry.byte_pattern[3] = (nveu8_t)((lut_data[5] >> 28) |
					((lut_data[6] & 0xFU) << 4));
		entry.byte_pattern_offset[3] = (nveu8_t)((lut_data[6] >> 4) &
						0x3FU);
	}

	/* Preempt mask */
	if ((lut_data[6] & MACSEC_LUT_PREEMPT_INACTIVE) !=
	    MACSEC_LUT_PREEMPT_INACTIVE) {
		flags |= OSI_LUT_FLAGS_PREEMPT_VALID;
		if ((lut_data[6] & MACSEC_LUT_PREEMPT) == MACSEC_LUT_PREEMPT) {
			flags |= OSI_LUT_FLAGS_PREEMPT;
		}
	}

	lut_config->lut_in = entry;
	lut_config->flags = flags;

	return 0;
}

/**
 * @brief byp_lut_read - Read BYP LUT data and fill lut_config accordingly
 *
 * @note
 * Algorithm:
 *  - Read LUT data and fill the flags and lut_config accordingly
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. Used param macsec_base
 * @param[out] lut_config: Update the lut_config from BYP LUT
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t byp_lut_read(struct osi_core_priv_data *const osi_core,
			struct osi_macsec_lut_config *const lut_config)
{
	nveu32_t lut_data[MACSEC_LUT_DATA_REG_CNT] = {0};
	nveu32_t flags = 0;
	nveu32_t val = 0;
	nveu32_t index = lut_config->table_config.index;
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu8_t *paddr = OSI_NULL;
	nve32_t ret = 0;

	read_lut_data(osi_core, lut_data);

	if (lut_read_inputs(lut_config, lut_data) != 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "LUT inputs error\n", 0ULL);
		ret = -1;
		goto err;
	}

	/* Lookup output */
	if ((lut_data[6] & MACSEC_LUT_CONTROLLED_PORT) ==
	    MACSEC_LUT_CONTROLLED_PORT) {
		flags |= OSI_LUT_FLAGS_CONTROLLED_PORT;
	}

	if ((lut_data[6] & MACSEC_BYP_LUT_DVLAN_PKT) ==
	    MACSEC_BYP_LUT_DVLAN_PKT) {
		flags |= OSI_LUT_FLAGS_DVLAN_PKT;
	}

	if ((lut_data[6] & BYP_LUT_DVLAN_OUTER_INNER_TAG_SEL) ==
		BYP_LUT_DVLAN_OUTER_INNER_TAG_SEL) {
		flags |= OSI_LUT_FLAGS_DVLAN_OUTER_INNER_TAG_SEL;
	}

	switch (lut_config->table_config.ctlr_sel) {
	case OSI_CTLR_SEL_TX:
		paddr = addr + MACSEC_TX_BYP_LUT_VALID;
		break;
	case OSI_CTLR_SEL_RX:
		paddr = addr + MACSEC_RX_BYP_LUT_VALID;
		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Unknown controller select\n", 0ULL);
		ret = -1;
		break;
	}
	if (ret == OSI_NONE_SIGNED) {
		val = osi_readla(osi_core, paddr);
		if ((val & ((nveu32_t)(1U) << (index & 0x1FU))) != OSI_NONE) {
			flags |= OSI_LUT_FLAGS_ENTRY_VALID;
		}
		lut_config->flags |= flags;
	}
err:
	return ret;
}

/**
 * @brief tx_sci_lut_read - Read LUT_data and fill tx sci lut_config accordingly
 *
 * @note
 * Algorithm:
 *  - Read LUT data and fill the flags and lut_config accordingly
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. Used param macsec_base
 * @param[in] lut_data: lut_data read from h/w registers
 * @param[out] lut_config: Update the lut_config from lut_data
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void tx_sci_lut_read(struct osi_core_priv_data *const osi_core,
			    struct osi_macsec_lut_config *const lut_config,
			    const nveu32_t *const lut_data)
{
	nveu32_t val = 0;
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu32_t index = lut_config->table_config.index;

		if ((lut_data[6] & MACSEC_LUT_AN0_VALID) ==
		    MACSEC_LUT_AN0_VALID) {
			lut_config->sci_lut_out.an_valid |= OSI_AN0_VALID;
		}
		if ((lut_data[6] & MACSEC_LUT_AN1_VALID) ==
		    MACSEC_LUT_AN1_VALID) {
			lut_config->sci_lut_out.an_valid |= OSI_AN1_VALID;
		}
		if ((lut_data[6] & MACSEC_LUT_AN2_VALID) ==
		    MACSEC_LUT_AN2_VALID) {
			lut_config->sci_lut_out.an_valid |= OSI_AN2_VALID;
		}
		if ((lut_data[6] & MACSEC_LUT_AN3_VALID) ==
		    MACSEC_LUT_AN3_VALID) {
			lut_config->sci_lut_out.an_valid |= OSI_AN3_VALID;
		}

		lut_config->sci_lut_out.sc_index = (lut_data[6] >> 17) & 0xFU;

		if ((lut_data[6] & MACSEC_TX_SCI_LUT_DVLAN_PKT) ==
		    MACSEC_TX_SCI_LUT_DVLAN_PKT) {
			lut_config->flags |= OSI_LUT_FLAGS_DVLAN_PKT;
		}
		if ((lut_data[6] & MACSEC_TX_SCI_LUT_DVLAN_OUTER_INNER_TAG_SEL) ==
			MACSEC_TX_SCI_LUT_DVLAN_OUTER_INNER_TAG_SEL) {
			lut_config->flags |=
				OSI_LUT_FLAGS_DVLAN_OUTER_INNER_TAG_SEL;
		}

		val = osi_readla(osi_core, addr+MACSEC_TX_SCI_LUT_VALID);
		if ((val & ((nveu32_t)(1U) << (index & 0xFFU))) != OSI_NONE) {
			lut_config->flags |= OSI_LUT_FLAGS_ENTRY_VALID;
		}
}

/**
 * @brief sci_lut_read - Read SCI LUT data
 *
 * @note
 * Algorithm:
 *  - Return -1 if the index is not valid
 *  - Read SCI Lut data from h/w registers to lut_data
 *  - Read Tx/Rx SCI lut data
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. Used param macsec_base
 * @param[out] lut_config: Update the lut_config from h/w registers
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t sci_lut_read(struct osi_core_priv_data *const osi_core,
			struct osi_macsec_lut_config *const lut_config)
{
	nveu32_t lut_data[MACSEC_LUT_DATA_REG_CNT] = {0};
	nveu32_t flags = 0;
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu32_t val = 0;
	nveu32_t index = lut_config->table_config.index;
	nve32_t ret = 0;

	if (index > OSI_SC_LUT_MAX_INDEX) {
		ret = -1;
		goto exit;
	}
	read_lut_data(osi_core, lut_data);

	switch (lut_config->table_config.ctlr_sel) {
	case OSI_CTLR_SEL_TX:
		if (lut_read_inputs(lut_config, lut_data) != 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "LUT inputs error\n", 0ULL);
			ret = -1;
			goto exit;
		}
		tx_sci_lut_read(osi_core, lut_config, lut_data);
		break;
	case OSI_CTLR_SEL_RX:
		lut_config->sci_lut_out.sci[0] = (nveu8_t)(lut_data[0] & 0xFFU);
		lut_config->sci_lut_out.sci[1] = (nveu8_t)((lut_data[0] >> 8) & 0xFFU);
		lut_config->sci_lut_out.sci[2] = (nveu8_t)((lut_data[0] >> 16) & 0xFFU);
		lut_config->sci_lut_out.sci[3] = (nveu8_t)((lut_data[0] >> 24) & 0xFFU);
		lut_config->sci_lut_out.sci[4] = (nveu8_t)(lut_data[1] & 0xFFU);
		lut_config->sci_lut_out.sci[5] = (nveu8_t)((lut_data[1] >> 8) & 0xFFU);
		lut_config->sci_lut_out.sci[6] = (nveu8_t)((lut_data[1] >> 16) & 0xFFU);
		lut_config->sci_lut_out.sci[7] = (nveu8_t)((lut_data[1] >> 24) & 0xFFU);

		lut_config->sci_lut_out.sc_index = (lut_data[2] >> 10) & 0xFU;
		if ((lut_data[2] & MACSEC_RX_SCI_LUT_PREEMPT_INACTIVE) !=
		    MACSEC_RX_SCI_LUT_PREEMPT_INACTIVE) {
			flags |= OSI_LUT_FLAGS_PREEMPT_VALID;
			if ((lut_data[2] & MACSEC_RX_SCI_LUT_PREEMPT) ==
			    MACSEC_RX_SCI_LUT_PREEMPT) {
				flags |= OSI_LUT_FLAGS_PREEMPT;
			}
		}

		val = osi_readla(osi_core, addr+MACSEC_RX_SCI_LUT_VALID);
		if ((val & ((nveu32_t)(1U) << index)) != OSI_NONE) {
			lut_config->flags |= OSI_LUT_FLAGS_ENTRY_VALID;
		}
		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Unknown controller selected\n", 0ULL);
		ret = -1;
		break;
	}
exit:
	return ret;
}

/**
 * @brief sc_param_lut_read - Read SC Param LUT data
 *
 * @note
 * Algorithm:
 *  - Read SC param Lut data from h/w registers to lut_data
 *  - Update the Tx/Rx SC param data to  lut_config
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[out] lut_config: Update the lut_config from h/w registers
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t sc_param_lut_read(struct osi_core_priv_data *const osi_core,
			     struct osi_macsec_lut_config *const lut_config)
{
	nveu32_t lut_data[MACSEC_LUT_DATA_REG_CNT] = {0};
	nve32_t ret = 0;

	read_lut_data(osi_core, lut_data);

	switch (lut_config->table_config.ctlr_sel) {
	case OSI_CTLR_SEL_TX:
		lut_config->sc_param_out.key_index_start = lut_data[0] & 0x1FU;
		lut_config->sc_param_out.pn_max = (lut_data[0] >> 5) |
						   (lut_data[1] << 27);
		lut_config->sc_param_out.pn_threshold = (lut_data[1] >> 5) |
							(lut_data[2] << 27);
		lut_config->sc_param_out.tci = (nveu8_t)((lut_data[2] >> 5) & 0x3U);
		lut_config->sc_param_out.sci[0] = (nveu8_t)((lut_data[2] >> 8) & 0xFFU);
		lut_config->sc_param_out.sci[1] = (nveu8_t)((lut_data[2] >> 16) & 0xFFU);
		lut_config->sc_param_out.sci[2] = (nveu8_t)((lut_data[2] >> 24) & 0xFFU);
		lut_config->sc_param_out.sci[3] = (nveu8_t)(lut_data[3] & 0xFFU);
		lut_config->sc_param_out.sci[4] = (nveu8_t)((lut_data[3] >> 8) & 0xFFU);
		lut_config->sc_param_out.sci[5] = (nveu8_t)((lut_data[3] >> 16) & 0xFFU);
		lut_config->sc_param_out.sci[6] = (nveu8_t)((lut_data[3] >> 24) & 0xFFU);
		lut_config->sc_param_out.sci[7] = (nveu8_t)(lut_data[4] & 0xFFU);
		lut_config->sc_param_out.vlan_in_clear =
						(nveu8_t)((lut_data[4] >> 8) & 0x1U);
		break;
	case OSI_CTLR_SEL_RX:
		lut_config->sc_param_out.key_index_start = lut_data[0] & 0x1FU;
		lut_config->sc_param_out.pn_window = (lut_data[0] >> 5) |
						     (lut_data[1] << 27);
		lut_config->sc_param_out.pn_max = (lut_data[1] >> 5) |
						     (lut_data[2] << 27);
		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Unknown controller selected\n", 0ULL);
		ret = -1;
		break;
	}

	return ret;
}

/**
 * @brief sc_state_lut_read - Read SC state LUT data
 *
 * @note
 * Algorithm:
 *  - Read SC state Lut data from h/w registers to lut_data
 *  - Update the curr_an to lut_config
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[out] lut_config: Update the lut_config from h/w registers
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t sc_state_lut_read(struct osi_core_priv_data *const osi_core,
			     struct osi_macsec_lut_config *const lut_config)
{
	nveu32_t lut_data[MACSEC_LUT_DATA_REG_CNT] = {0};

	read_lut_data(osi_core, lut_data);
	lut_config->sc_state_out.curr_an = lut_data[0];

	return 0;
}

/**
 * @brief sa_state_lut_read - Read Sa state LUT data
 *
 * @note
 * Algorithm:
 *  - Read Sa state Lut data from h/w registers to lut_data
 *  - Update the flags and next_pn and lowest_pn to lut_config for TX/RX
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[out] lut_config: Update the lut_config from h/w registers
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t sa_state_lut_read(struct osi_core_priv_data *const osi_core,
			     struct osi_macsec_lut_config *const lut_config)
{
	nveu32_t lut_data[MACSEC_LUT_DATA_REG_CNT] = {0};
	nve32_t ret = 0;

	read_lut_data(osi_core, lut_data);

	switch (lut_config->table_config.ctlr_sel) {
	case OSI_CTLR_SEL_TX:
		lut_config->sa_state_out.next_pn = lut_data[0];
		if ((lut_data[1] & MACSEC_SA_STATE_LUT_ENTRY_VALID) ==
		    MACSEC_SA_STATE_LUT_ENTRY_VALID) {
			lut_config->flags |= OSI_LUT_FLAGS_ENTRY_VALID;
		}
		break;
	case OSI_CTLR_SEL_RX:
		lut_config->sa_state_out.next_pn = lut_data[0];
		lut_config->sa_state_out.lowest_pn = lut_data[1];
		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Unknown controller selected\n", 0ULL);
		ret = -1;
		break;
	}

	/* Lookup output */
	return ret;
}

/**
 * @brief lut_data_read - Read different types of LUT data
 *
 * @note
 * Algorithm:
 *  - Read byp/SCI/SC param/SC state/SA state lut data to lut_config
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[out] lut_config: Update the lut_config from h/w registers
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t lut_data_read(struct osi_core_priv_data *const osi_core,
			 struct osi_macsec_lut_config *const lut_config)
{
	nve32_t ret = 0;

	switch (lut_config->lut_sel) {
	case OSI_LUT_SEL_BYPASS:
		ret = byp_lut_read(osi_core, lut_config);
		break;
	case OSI_LUT_SEL_SCI:
		ret = sci_lut_read(osi_core, lut_config);
		break;
	case OSI_LUT_SEL_SC_PARAM:
		ret = sc_param_lut_read(osi_core, lut_config);
		break;
	case OSI_LUT_SEL_SC_STATE:
		ret = sc_state_lut_read(osi_core, lut_config);
		break;
	case OSI_LUT_SEL_SA_STATE:
		ret = sa_state_lut_read(osi_core, lut_config);
		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Unsupported LUT\n", 0ULL);
		ret = -1;
		break;
	}

	return ret;
}

/**
 * @brief commit_lut_data - Write lut_data to h/w registers
 *
 * @note
 * Algorithm:
 *  - Write lut_data to h/w registers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] lut_data: data to be pushed to h/w registers
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void commit_lut_data(struct osi_core_priv_data *const osi_core,
				   nveu32_t const *const lut_data)
{
	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;
	nveu32_t i;

	/* Commit the LUT entry to HW */
	for (i = 0; i < MACSEC_LUT_DATA_REG_CNT; i++) {
		osi_writela(osi_core, lut_data[i], base + MACSEC_LUT_DATA(i));
	}
}

/**
 * @brief rx_sa_state_lut_config - update lut_data from lut_config sa_state
 *
 * @note
 * Algorithm:
 *  - update lut_data from lut_config sa_state
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: sa_state from lut_config is used. Param used sa_state_out
 * @param[out] lut_data: buffer to which sa_state is updated
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void rx_sa_state_lut_config(
				const struct osi_macsec_lut_config *const lut_config,
				nveu32_t *const lut_data)
{
	struct osi_sa_state_outputs entry = lut_config->sa_state_out;

	lut_data[0] |= entry.next_pn;
	lut_data[1] |= entry.lowest_pn;
}

/**
 * @brief tx_sa_state_lut_config - update lut_data from lut_config sa_state
 *
 * @note
 * Algorithm:
 *  - update lut_data from lut_config sa_state
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: sa_state from lut_config is used. Param used sa_state_out
 * @param[out] lut_data: buffer to which sa_state is updated
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void tx_sa_state_lut_config(const struct osi_macsec_lut_config *const lut_config,
				   nveu32_t *const lut_data)
{
	nveu32_t flags = lut_config->flags;
	struct osi_sa_state_outputs entry = lut_config->sa_state_out;

	lut_data[0] |= entry.next_pn;
	if ((flags & OSI_LUT_FLAGS_ENTRY_VALID) == OSI_LUT_FLAGS_ENTRY_VALID) {
		lut_data[1] |= MACSEC_SA_STATE_LUT_ENTRY_VALID;
	}

}

/**
 * @brief sa_state_lut_config - update lut_data from lut_config sa_state
 *
 * @note
 * Algorithm:
 *  - update lut_data from lut_config sa_state for Tx/Rx
 *  - program the lut_data to h/w
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] lut_config: sa_state from lut_config is used. Param used table_config
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t sa_state_lut_config(struct osi_core_priv_data *const osi_core,
				   const struct osi_macsec_lut_config *const lut_config)
{
	nveu32_t lut_data[MACSEC_LUT_DATA_REG_CNT] = {0};
	struct osi_macsec_table_config table_config = lut_config->table_config;
	nve32_t ret = 0;

	switch (table_config.ctlr_sel) {
	case OSI_CTLR_SEL_TX:
		tx_sa_state_lut_config(lut_config, lut_data);
		break;
	case OSI_CTLR_SEL_RX:
		rx_sa_state_lut_config(lut_config, lut_data);
		break;
	default:
		ret = -1;
		break;
	}

	commit_lut_data(osi_core, lut_data);

	return ret;
}

/**
 * @brief sc_state_lut_config - update lut_data from lut_config sc_state
 *
 * @note
 * Algorithm:
 *  - update lut_data from lut_config sc_state for Tx/Rx
 *  - program the lut_data to h/w
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] lut_config: sc_state from lut_config is used. Param used sc_state_out
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t sc_state_lut_config(struct osi_core_priv_data *const osi_core,
				   const struct osi_macsec_lut_config *const lut_config)
{
	nveu32_t lut_data[MACSEC_LUT_DATA_REG_CNT] = {0};
	struct osi_sc_state_outputs entry = lut_config->sc_state_out;

	lut_data[0] |= entry.curr_an;
	commit_lut_data(osi_core, lut_data);

	return 0;
}

/**
 * @brief rx_sc_param_lut_config - update lut_data from lut_config rx_sc_param
 *
 * @note
 * Algorithm:
 *  - update lut_data from lut_config sc_param for Rx
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: sa_state from lut_config is used. Param used sc_param_out
 * @param[out] lut_data: rx_sc_params are updated to lut_data buffer
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void rx_sc_param_lut_config(
			const struct osi_macsec_lut_config *const lut_config,
			nveu32_t *const lut_data)
{
	struct osi_sc_param_outputs entry = lut_config->sc_param_out;

	lut_data[0] |= entry.key_index_start;
	lut_data[0] |= entry.pn_window << 5;
	lut_data[1] |= entry.pn_window >> 27;
	lut_data[1] |= entry.pn_max << 5;
	lut_data[2] |= entry.pn_max >> 27;
}

/**
 * @brief tx_sc_param_lut_config - update lut_data from lut_config tx_sc_param
 *
 * @note
 * Algorithm:
 *  - update lut_data from lut_config sc_param for Tx
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: sa_state from lut_config is used. Param used sc_param_out
 * @param[out] lut_data: tx_sc_params are updated to lut_data buffer
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void tx_sc_param_lut_config(
			const struct osi_macsec_lut_config *const lut_config,
			nveu32_t *const lut_data)
{
	struct osi_sc_param_outputs entry = lut_config->sc_param_out;

	lut_data[0] |= entry.key_index_start;
	lut_data[0] |= entry.pn_max << 5;
	lut_data[1] |= entry.pn_max >> 27;
	lut_data[1] |= entry.pn_threshold << 5;
	lut_data[2] |= entry.pn_threshold >> 27;
	lut_data[2] |= (nveu32_t)(entry.tci) << 5;
	lut_data[2] |= ((nveu32_t)entry.sci[0]) << 8;
	lut_data[2] |= ((nveu32_t)entry.sci[1]) << 16;
	lut_data[2] |= ((nveu32_t)entry.sci[2]) << 24;
	lut_data[3] |= ((nveu32_t)entry.sci[3]);
	lut_data[3] |= ((nveu32_t)entry.sci[4]) << 8;
	lut_data[3] |= ((nveu32_t)entry.sci[5]) << 16;
	lut_data[3] |= ((nveu32_t)entry.sci[6]) << 24;
	lut_data[4] |= ((nveu32_t)entry.sci[7]);
	lut_data[4] |= ((nveu32_t)entry.vlan_in_clear) << 8;
}

/**
 * @brief sc_param_lut_config - update lut_data from lut_config sc_param
 *
 * @note
 * Algorithm:
 *  - Return -1 for invalid index
 *  - update lut_data from lut_config sc_param for Tx/Rx
 *  - commit the lut_data to h/w
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] lut_config: sc_param from lut_config is used. Param used sc_param_out
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t sc_param_lut_config(struct osi_core_priv_data *const osi_core,
				   const struct osi_macsec_lut_config *const lut_config)
{
	nveu32_t lut_data[MACSEC_LUT_DATA_REG_CNT] = {0};
	struct osi_macsec_table_config table_config = lut_config->table_config;
	struct osi_sc_param_outputs entry = lut_config->sc_param_out;
	nve32_t ret = 0;

	if (entry.key_index_start > OSI_KEY_INDEX_MAX) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Invalid Key Index\n", 0ULL);
		ret = -1;
		goto exit;
	}

	switch (table_config.ctlr_sel) {
	case OSI_CTLR_SEL_TX:
		tx_sc_param_lut_config(lut_config, lut_data);
		break;
	case OSI_CTLR_SEL_RX:
		rx_sc_param_lut_config(lut_config, lut_data);
		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Unknown controller selected\n", 0ULL);
		ret = -1;
		break;
	}

	commit_lut_data(osi_core, lut_data);
exit:
	return ret;
}

/**
 * @brief lut_config_MAC_SA - update lut_data from lut_config source address
 *
 * @note
 * Algorithm:
 *  - update lut_data from lut_config mac_SA and flags
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: mac SA from lut_config is used. Param used lut_in
 * @param[out] lut_data: lut_data is updated with MAC SA
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void lut_config_MAC_SA(const struct osi_macsec_lut_config *const lut_config,
			     nveu32_t *const lut_data)
{
	struct osi_lut_inputs entry = lut_config->lut_in;
	nveu32_t flags = lut_config->flags;

	/* MAC SA */
	if ((flags & OSI_LUT_FLAGS_SA_BYTE0_VALID) ==
	    OSI_LUT_FLAGS_SA_BYTE0_VALID) {
		lut_data[1] |= ((nveu32_t)entry.sa[0]) << 22;
		lut_data[3] &= ~MACSEC_LUT_SA_BYTE0_INACTIVE;
	} else {
		lut_data[3] |= MACSEC_LUT_SA_BYTE0_INACTIVE;
	}

	if ((flags & OSI_LUT_FLAGS_SA_BYTE1_VALID) ==
	    OSI_LUT_FLAGS_SA_BYTE1_VALID) {
		lut_data[1] |= ((nveu32_t)entry.sa[1]) << 30;
		lut_data[2] |= (((nveu32_t)(entry.sa[1])) >> 2);
		lut_data[3] &= ~MACSEC_LUT_SA_BYTE1_INACTIVE;
	} else {
		lut_data[3] |= MACSEC_LUT_SA_BYTE1_INACTIVE;
	}

	if ((flags & OSI_LUT_FLAGS_SA_BYTE2_VALID) ==
	    OSI_LUT_FLAGS_SA_BYTE2_VALID) {
		lut_data[2] |= ((nveu32_t)entry.sa[2]) << 6;
		lut_data[3] &= ~MACSEC_LUT_SA_BYTE2_INACTIVE;
	} else {
		lut_data[3] |= MACSEC_LUT_SA_BYTE2_INACTIVE;
	}

	if ((flags & OSI_LUT_FLAGS_SA_BYTE3_VALID) ==
	    OSI_LUT_FLAGS_SA_BYTE3_VALID) {
		lut_data[2] |= ((nveu32_t)entry.sa[3]) << 14;
		lut_data[3] &= ~MACSEC_LUT_SA_BYTE3_INACTIVE;
	} else {
		lut_data[3] |= MACSEC_LUT_SA_BYTE3_INACTIVE;
	}

	if ((flags & OSI_LUT_FLAGS_SA_BYTE4_VALID) ==
	    OSI_LUT_FLAGS_SA_BYTE4_VALID) {
		lut_data[2] |= ((nveu32_t)(entry.sa[4])) << 22;
		lut_data[3] &= ~MACSEC_LUT_SA_BYTE4_INACTIVE;
	} else {
		lut_data[3] |= MACSEC_LUT_SA_BYTE4_INACTIVE;
	}

	if ((flags & OSI_LUT_FLAGS_SA_BYTE5_VALID) ==
	    OSI_LUT_FLAGS_SA_BYTE5_VALID) {
		lut_data[2] |= ((nveu32_t)entry.sa[5]) << 30;
		lut_data[3] |= (((nveu32_t)entry.sa[5]) >> 2);
		lut_data[3] &= ~MACSEC_LUT_SA_BYTE5_INACTIVE;
	} else {
		lut_data[3] |= MACSEC_LUT_SA_BYTE5_INACTIVE;
	}

}

/**
 * @brief lut_config_MAC_DA - update lut_data from lut_config destination address
 *
 * @note
 * Algorithm:
 *  - update lut_data from lut_config mac_DA and flags
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: mac DA from lut_config is used. Param used lut_in
 * @param[out] lut_data: lut_data is updated with MAC DA
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void lut_config_MAC_DA(const struct osi_macsec_lut_config *const lut_config,
			     nveu32_t *const lut_data)
{
	struct osi_lut_inputs entry = lut_config->lut_in;
	nveu32_t flags = lut_config->flags;

	/* MAC DA */
	if ((flags & OSI_LUT_FLAGS_DA_BYTE0_VALID) ==
	    OSI_LUT_FLAGS_DA_BYTE0_VALID) {
		lut_data[0] |= ((nveu32_t)entry.da[0]);
		lut_data[1] &= ~MACSEC_LUT_DA_BYTE0_INACTIVE;
	} else {
		lut_data[1] |= MACSEC_LUT_DA_BYTE0_INACTIVE;
	}

	if ((flags & OSI_LUT_FLAGS_DA_BYTE1_VALID) ==
	    OSI_LUT_FLAGS_DA_BYTE1_VALID) {
		lut_data[0] |= ((nveu32_t)entry.da[1]) << 8;
		lut_data[1] &= ~MACSEC_LUT_DA_BYTE1_INACTIVE;
	} else {
		lut_data[1] |= MACSEC_LUT_DA_BYTE1_INACTIVE;
	}

	if ((flags & OSI_LUT_FLAGS_DA_BYTE2_VALID) ==
	    OSI_LUT_FLAGS_DA_BYTE2_VALID) {
		lut_data[0] |= ((nveu32_t)entry.da[2]) << 16;
		lut_data[1] &= ~MACSEC_LUT_DA_BYTE2_INACTIVE;
	} else {
		lut_data[1] |= MACSEC_LUT_DA_BYTE2_INACTIVE;
	}

	if ((flags & OSI_LUT_FLAGS_DA_BYTE3_VALID) ==
	    OSI_LUT_FLAGS_DA_BYTE3_VALID) {
		lut_data[0] |= ((nveu32_t)entry.da[3]) << 24;
		lut_data[1] &= ~MACSEC_LUT_DA_BYTE3_INACTIVE;
	} else {
		lut_data[1] |= MACSEC_LUT_DA_BYTE3_INACTIVE;
	}

	if ((flags & OSI_LUT_FLAGS_DA_BYTE4_VALID) ==
	    OSI_LUT_FLAGS_DA_BYTE4_VALID) {
		lut_data[1] |= ((nveu32_t)entry.da[4]);
		lut_data[1] &= ~MACSEC_LUT_DA_BYTE4_INACTIVE;
	} else {
		lut_data[1] |= MACSEC_LUT_DA_BYTE4_INACTIVE;
	}

	if ((flags & OSI_LUT_FLAGS_DA_BYTE5_VALID) ==
	    OSI_LUT_FLAGS_DA_BYTE5_VALID) {
		lut_data[1] |= ((nveu32_t)entry.da[5]) << 8;
		lut_data[1] &= ~MACSEC_LUT_DA_BYTE5_INACTIVE;
	} else {
		lut_data[1] |= MACSEC_LUT_DA_BYTE5_INACTIVE;
	}

}

/**
 * @brief lut_config_ether_type - update lut_data from lut_config ether type
 *
 * @note
 * Algorithm:
 *  - update lut_data from lut_config ether_type and flags
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: ether_type from lut_config is used. Param used lut_in
 * @param[out] lut_data: lut_data is updated with ether_type
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void lut_config_ether_type(const struct osi_macsec_lut_config *const lut_config,
			     nveu32_t *const lut_data)
{
	struct osi_lut_inputs entry = lut_config->lut_in;
	nveu32_t flags = lut_config->flags;

	/* Ether type */
	if ((flags & OSI_LUT_FLAGS_ETHTYPE_VALID) ==
	    OSI_LUT_FLAGS_ETHTYPE_VALID) {
		lut_data[3] |= ((nveu32_t)entry.ethtype[0]) << 12;
		lut_data[3] |= ((nveu32_t)entry.ethtype[1]) << 20;
		lut_data[3] &= ~MACSEC_LUT_ETHTYPE_INACTIVE;
	} else {
		lut_data[3] |= MACSEC_LUT_ETHTYPE_INACTIVE;
	}
}

/**
 * @brief lut_config_vlan - update lut_data from lut_config vlan params
 *
 * @note
 * Algorithm:
 *  - update lut_data from lut_config vlan pcp, id and flags
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: vlan params from lut_config is used. Param used lut_in
 * @param[out] lut_data: lut_data is updated with vlan params
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void lut_config_vlan(const struct osi_macsec_lut_config *const lut_config,
			     nveu32_t *const lut_data)
{
	struct osi_lut_inputs entry = lut_config->lut_in;
	nveu32_t flags = lut_config->flags;

	/* VLAN */
	if ((flags & OSI_LUT_FLAGS_VLAN_VALID) == OSI_LUT_FLAGS_VLAN_VALID) {
		/* VLAN PCP */
		if ((flags & OSI_LUT_FLAGS_VLAN_PCP_VALID) ==
		    OSI_LUT_FLAGS_VLAN_PCP_VALID) {
			lut_data[3] |= entry.vlan_pcp << 29;
			lut_data[4] &= ~MACSEC_LUT_VLAN_PCP_INACTIVE;
		} else {
			lut_data[4] |= MACSEC_LUT_VLAN_PCP_INACTIVE;
		}

		/* VLAN ID */
		if ((flags & OSI_LUT_FLAGS_VLAN_ID_VALID) ==
		    OSI_LUT_FLAGS_VLAN_ID_VALID) {
			lut_data[4] |= entry.vlan_id << 1;
			lut_data[4] &= ~MACSEC_LUT_VLAN_ID_INACTIVE;
		} else {
			lut_data[4] |= MACSEC_LUT_VLAN_ID_INACTIVE;
		}
		lut_data[4] |= MACSEC_LUT_VLAN_ACTIVE;
	} else {
		lut_data[4] |= MACSEC_LUT_VLAN_PCP_INACTIVE;
		lut_data[4] |= MACSEC_LUT_VLAN_ID_INACTIVE;
		lut_data[4] &= ~MACSEC_LUT_VLAN_ACTIVE;
	}
}

/**
 * @brief lut_config_byte_pattern - update lut_data from lut_config byte pattern
 *
 * @note
 * Algorithm:
 *  - update lut_data from lut_config byte pattern and flags for 4 bytes
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: byte pattern from lut_config is used. Param used lut_in
 * @param[out] lut_data: lut_data is updated with byte patterns
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void lut_config_byte_pattern(const struct osi_macsec_lut_config *const lut_config,
			     nveu32_t *const lut_data)
{
	struct osi_lut_inputs entry = lut_config->lut_in;
	nveu32_t flags = lut_config->flags;

	/* Byte patterns */
	if ((flags & OSI_LUT_FLAGS_BYTE0_PATTERN_VALID) ==
	    OSI_LUT_FLAGS_BYTE0_PATTERN_VALID) {
		lut_data[4] |= ((nveu32_t)entry.byte_pattern[0]) << 15;
		lut_data[4] |= entry.byte_pattern_offset[0] << 23;
		lut_data[4] &= ~MACSEC_LUT_BYTE0_PATTERN_INACTIVE;
	} else {
		lut_data[4] |= MACSEC_LUT_BYTE0_PATTERN_INACTIVE;
	}
	if ((flags & OSI_LUT_FLAGS_BYTE1_PATTERN_VALID) ==
	    OSI_LUT_FLAGS_BYTE1_PATTERN_VALID) {
		lut_data[4] |= ((nveu32_t)entry.byte_pattern[1]) << 30;
		lut_data[5] |= ((nveu32_t)entry.byte_pattern[1]) >> 2;
		lut_data[5] |= entry.byte_pattern_offset[1] << 6;
		lut_data[5] &= ~MACSEC_LUT_BYTE1_PATTERN_INACTIVE;
	} else {
		lut_data[5] |= MACSEC_LUT_BYTE1_PATTERN_INACTIVE;
	}

	if ((flags & OSI_LUT_FLAGS_BYTE2_PATTERN_VALID) ==
	    OSI_LUT_FLAGS_BYTE2_PATTERN_VALID) {
		lut_data[5] |= ((nveu32_t)entry.byte_pattern[2]) << 13;
		lut_data[5] |= entry.byte_pattern_offset[2] << 21;
		lut_data[5] &= ~MACSEC_LUT_BYTE2_PATTERN_INACTIVE;
	} else {
		lut_data[5] |= MACSEC_LUT_BYTE2_PATTERN_INACTIVE;
	}

	if ((flags & OSI_LUT_FLAGS_BYTE3_PATTERN_VALID) ==
	    OSI_LUT_FLAGS_BYTE3_PATTERN_VALID) {
		lut_data[5] |= ((nveu32_t)entry.byte_pattern[3]) << 28;
		lut_data[6] |= ((nveu32_t)entry.byte_pattern[3]) >> 4;
		lut_data[6] |= entry.byte_pattern_offset[3] << 4;
		lut_data[6] &= ~MACSEC_LUT_BYTE3_PATTERN_INACTIVE;
	} else {
		lut_data[6] |= MACSEC_LUT_BYTE3_PATTERN_INACTIVE;
	}
}

/**
 * @brief lut_config_preempt_mask - update lut_data from lut_config preempt mask
 *
 * @note
 * Algorithm:
 *  - update lut_data from lut_config preempt mask and flags
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: preempt mask from lut_config is used.
 * @param[out] lut_data: lut_data is updated with preempt mask
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void lut_config_preempt_mask(const struct osi_macsec_lut_config *const lut_config,
			     nveu32_t *const lut_data)
{
	nveu32_t flags = lut_config->flags;

	/* Preempt mask */
	if ((flags & OSI_LUT_FLAGS_PREEMPT_VALID) ==
	    OSI_LUT_FLAGS_PREEMPT_VALID) {
		if ((flags & OSI_LUT_FLAGS_PREEMPT) == OSI_LUT_FLAGS_PREEMPT) {
			lut_data[6] |= MACSEC_LUT_PREEMPT;
		} else {
			lut_data[6] &= ~MACSEC_LUT_PREEMPT;
		}
		lut_data[6] &= ~MACSEC_LUT_PREEMPT_INACTIVE;
	} else {
		lut_data[6] |= MACSEC_LUT_PREEMPT_INACTIVE;
	}
}

/**
 * @brief lut_config_inputs - update lut_data from lut_config attributes
 *
 * @note
 * Algorithm:
 *  - Return -1 for invalid byte pattern offset
 *  - Return -1 for invalid vlan params
 *  - update the lut_data with mac_DA, mac_SA, ether_type,
 *    vlan params, byte_pattern and preempt mask
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: attributes from lut_config is used.
 * @param[out] lut_data: lut_data is updated with attributes from lut_config
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t lut_config_inputs(const struct osi_macsec_lut_config *const lut_config,
			     nveu32_t *const lut_data)
{
	struct osi_lut_inputs entry = lut_config->lut_in;
	nveu32_t flags = lut_config->flags;
	nveu32_t i, j = OSI_LUT_FLAGS_BYTE0_PATTERN_VALID;
	nve32_t ret = 0;

	for (i = 0; i < OSI_LUT_BYTE_PATTERN_MAX; i++) {
		if ((flags & j) == j) {
			if (entry.byte_pattern_offset[i] >
			    OSI_LUT_BYTE_PATTERN_MAX_OFFSET) {
				ret = -1;
				goto exit;
			}
		}
		j <<= 1;
	}

	if ((flags & OSI_LUT_FLAGS_BYTE0_PATTERN_VALID) ==
		    OSI_LUT_FLAGS_BYTE0_PATTERN_VALID) {
		if (entry.byte_pattern_offset[0] >
		    OSI_LUT_BYTE_PATTERN_MAX_OFFSET) {
			ret = -1;
			goto exit;
		}
	}

	if ((flags & OSI_LUT_FLAGS_VLAN_VALID) == OSI_LUT_FLAGS_VLAN_VALID) {
		if ((entry.vlan_pcp > OSI_VLAN_PCP_MAX) ||
		    (entry.vlan_id > OSI_VLAN_ID_MAX)) {
			ret = -1;
			goto exit;
		}
	}

	lut_config_MAC_DA(lut_config, lut_data);
	lut_config_MAC_SA(lut_config, lut_data);
	lut_config_ether_type(lut_config, lut_data);
	lut_config_vlan(lut_config, lut_data);
	lut_config_byte_pattern(lut_config, lut_data);
	lut_config_preempt_mask(lut_config, lut_data);
exit:
	return ret;
}

/**
 * @brief rx_sci_lut_config - update lut_data from lut_config for sci_lut
 *
 * @note
 * Algorithm:
 *  - Return -1 for invalid sc_index
 *  - update the lut_data with sci, preempt mask and index
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: attributes from lut_config is used.
 * @param[out] lut_data: lut_data is updated with attributes from lut_config
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t rx_sci_lut_config(
			const struct osi_macsec_lut_config *const lut_config,
			nveu32_t *const lut_data)
{
	nveu32_t flags = lut_config->flags;
	struct osi_sci_lut_outputs entry = lut_config->sci_lut_out;
	nve32_t ret = 0;

	if (entry.sc_index > OSI_SC_INDEX_MAX) {
		ret = -1;
		goto exit;
	}

	lut_data[0] |= ((nveu32_t)(entry.sci[0]) |
			(((nveu32_t)entry.sci[1]) << 8) |
			(((nveu32_t)entry.sci[2]) << 16) |
			(((nveu32_t)entry.sci[3]) << 24));
	lut_data[1] |= (((nveu32_t)entry.sci[4]) |
			(((nveu32_t)entry.sci[5]) << 8) |
			(((nveu32_t)entry.sci[6]) << 16) |
			(((nveu32_t)entry.sci[7]) << 24));

	/* Preempt mask */
	if ((flags & OSI_LUT_FLAGS_PREEMPT_VALID) ==
	    OSI_LUT_FLAGS_PREEMPT_VALID) {
		if ((flags & OSI_LUT_FLAGS_PREEMPT) == OSI_LUT_FLAGS_PREEMPT) {
			lut_data[2] |= MACSEC_RX_SCI_LUT_PREEMPT;
		} else {
			lut_data[2] &= ~MACSEC_RX_SCI_LUT_PREEMPT;
		}
		lut_data[2] &= ~MACSEC_RX_SCI_LUT_PREEMPT_INACTIVE;
	} else {
		lut_data[2] |= MACSEC_RX_SCI_LUT_PREEMPT_INACTIVE;
	}

	lut_data[2] |= entry.sc_index << 10;
exit:
	return ret;
}

/**
 * @brief tx_sci_lut_config - update lut_data from lut_config for sci_lut
 *
 * @note
 * Algorithm:
 *  - update the lut_data with inputs such as DA, SA, ether_type and other params
 *  - Update valid an mask in lut_data
 *  - Update dvlan tags in lut_data
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: attributes from lut_config is used.
 * @param[out] lut_data: lut_data is updated with attributes from lut_config
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t tx_sci_lut_config(
			const struct osi_macsec_lut_config *const lut_config,
			nveu32_t *const lut_data)
{
	nveu32_t flags = lut_config->flags;
	struct osi_sci_lut_outputs entry = lut_config->sci_lut_out;
	nveu32_t an_valid = entry.an_valid;
	nve32_t ret = 0;

	if (lut_config_inputs(lut_config, lut_data) != 0) {
		ret = -1;
		goto exit;
	}

	/* Lookup result fields */
	if ((an_valid & OSI_AN0_VALID) == OSI_AN0_VALID) {
		lut_data[6] |= MACSEC_LUT_AN0_VALID;
	}
	if ((an_valid & OSI_AN1_VALID) == OSI_AN1_VALID) {
		lut_data[6] |= MACSEC_LUT_AN1_VALID;
	}
	if ((an_valid & OSI_AN2_VALID) == OSI_AN2_VALID) {
		lut_data[6] |= MACSEC_LUT_AN2_VALID;
	}
	if ((an_valid & OSI_AN3_VALID) == OSI_AN3_VALID) {
		lut_data[6] |= MACSEC_LUT_AN3_VALID;
	}

	lut_data[6] |= entry.sc_index << 17;

	if ((flags & OSI_LUT_FLAGS_DVLAN_PKT) == OSI_LUT_FLAGS_DVLAN_PKT) {
		lut_data[6] |= MACSEC_TX_SCI_LUT_DVLAN_PKT;
	}

	if ((flags & OSI_LUT_FLAGS_DVLAN_OUTER_INNER_TAG_SEL) ==
		OSI_LUT_FLAGS_DVLAN_OUTER_INNER_TAG_SEL) {
		lut_data[6] |= MACSEC_TX_SCI_LUT_DVLAN_OUTER_INNER_TAG_SEL;
	}
exit:
	return ret;
}

/**
 * @brief sci_lut_config - update hardware registers with Tx/Rx sci lut params
 *
 * @note
 * Algorithm:
 *  - Return -1 for invalid index
 *  - Update the Tx/Rx sci lut_data to h/w registers and update the flags to
 *    h/w registers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: attributes from lut_config is used. Used params
 *                        table_config, sci_lut_out
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t sci_lut_config(struct osi_core_priv_data *const osi_core,
			      const struct osi_macsec_lut_config *const lut_config)
{
	nveu32_t lut_data[MACSEC_LUT_DATA_REG_CNT] = {0};
	struct osi_macsec_table_config table_config = lut_config->table_config;
	struct osi_sci_lut_outputs entry = lut_config->sci_lut_out;
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu32_t val = 0;
	nveu32_t index = lut_config->table_config.index;
	nve32_t ret = 0;

	if ((entry.sc_index > OSI_SC_INDEX_MAX) ||
		(lut_config->table_config.index > OSI_SC_LUT_MAX_INDEX)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "SCI LUT config err - Invalid Index\n", 0ULL);
		ret = -1;
		goto exit;
	}

	switch (table_config.ctlr_sel) {
	case OSI_CTLR_SEL_TX:
		if (tx_sci_lut_config(lut_config, lut_data) < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "Failed to config tx sci LUT\n", 0ULL);
			ret = -1;
			goto exit;
		}
		commit_lut_data(osi_core, lut_data);

		if ((lut_config->flags & OSI_LUT_FLAGS_ENTRY_VALID) ==
			OSI_LUT_FLAGS_ENTRY_VALID) {
			val = osi_readla(osi_core, addr +
					 MACSEC_TX_SCI_LUT_VALID);
			val |= ((nveu32_t)(1U) << index);
			osi_writela(osi_core, val, addr +
				    MACSEC_TX_SCI_LUT_VALID);
		} else {
			val = osi_readla(osi_core, addr +
					 MACSEC_TX_SCI_LUT_VALID);
			val &= ~((nveu32_t)(1U) << index);
			osi_writela(osi_core, val, addr +
				    MACSEC_TX_SCI_LUT_VALID);
		}

		break;
	case OSI_CTLR_SEL_RX:
		if (rx_sci_lut_config(lut_config, lut_data) < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "Failed to config rx sci LUT\n", 0ULL);
			ret = -1;
			goto exit;
		}
		commit_lut_data(osi_core, lut_data);

		if ((lut_config->flags & OSI_LUT_FLAGS_ENTRY_VALID) ==
			OSI_LUT_FLAGS_ENTRY_VALID) {
			val = osi_readla(osi_core, addr +
					 MACSEC_RX_SCI_LUT_VALID);
			val |= ((nveu32_t)(1U) << index);
			osi_writela(osi_core, val, addr +
				    MACSEC_RX_SCI_LUT_VALID);
		} else {
			val = osi_readla(osi_core, addr +
					 MACSEC_RX_SCI_LUT_VALID);
			val &= ~((nveu32_t)(1U) << index);
			osi_writela(osi_core, val, addr +
				    MACSEC_RX_SCI_LUT_VALID);
		}

		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Unknown controller select\n", 0ULL);
		ret = -1;
		break;
	}
exit:
	return ret;
}

/**
 * @brief byp_lut_config - update hardware registers with Tx/Rx byp lut params
 *
 * @note
 * Algorithm:
 *  - Update the Tx/Rx bypass lut_data to h/w registers and update the flags to
 *    h/w registers
 *  - Update the flags with valid or invalid entries
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: attributes from lut_config is used. Used params table_config
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t byp_lut_config(struct osi_core_priv_data *const osi_core,
			      const struct osi_macsec_lut_config *const lut_config)
{
	nveu32_t lut_data[MACSEC_LUT_DATA_REG_CNT] = {0};
	nveu32_t flags = lut_config->flags;
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu32_t val = 0;
	nveu32_t index = lut_config->table_config.index;
	nve32_t ret = 0;

	if (lut_config_inputs(lut_config, lut_data) != 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "LUT inputs error\n", 0ULL);
		ret = -1;
		goto exit;
	}

	/* Lookup output */
	if ((flags & OSI_LUT_FLAGS_CONTROLLED_PORT) ==
		OSI_LUT_FLAGS_CONTROLLED_PORT) {
		lut_data[6] |= MACSEC_LUT_CONTROLLED_PORT;
	}

	if ((flags & OSI_LUT_FLAGS_DVLAN_PKT) == OSI_LUT_FLAGS_DVLAN_PKT) {
		lut_data[6] |= MACSEC_BYP_LUT_DVLAN_PKT;
	}

	if ((flags & OSI_LUT_FLAGS_DVLAN_OUTER_INNER_TAG_SEL) ==
		OSI_LUT_FLAGS_DVLAN_OUTER_INNER_TAG_SEL) {
		lut_data[6] |= BYP_LUT_DVLAN_OUTER_INNER_TAG_SEL;
	}

	commit_lut_data(osi_core, lut_data);

	switch (lut_config->table_config.ctlr_sel) {
	case OSI_CTLR_SEL_TX:
		if ((flags & OSI_LUT_FLAGS_ENTRY_VALID) ==
		     OSI_LUT_FLAGS_ENTRY_VALID) {
			val = osi_readla(osi_core, addr +
					 MACSEC_TX_BYP_LUT_VALID);
			val |= ((nveu32_t)(1U) << (index & 0x1FU));
			osi_writela(osi_core, val, addr +
				    MACSEC_TX_BYP_LUT_VALID);
		} else {
			val = osi_readla(osi_core, addr +
					 MACSEC_TX_BYP_LUT_VALID);
			val &= ~((nveu32_t)(1U) << (index & 0x1FU));
			osi_writela(osi_core, val, addr +
				    MACSEC_TX_BYP_LUT_VALID);
		}
		break;

	case OSI_CTLR_SEL_RX:
		if ((flags & OSI_LUT_FLAGS_ENTRY_VALID) ==
		     OSI_LUT_FLAGS_ENTRY_VALID) {
			val = osi_readla(osi_core, addr +
					 MACSEC_RX_BYP_LUT_VALID);
			val |= ((nveu32_t)(1U) << (index & 0x1FU));
			osi_writela(osi_core, val, addr +
				    MACSEC_RX_BYP_LUT_VALID);
		} else {
			val = osi_readla(osi_core, addr +
					 MACSEC_RX_BYP_LUT_VALID);
			val &= ~((nveu32_t)(1U) << (index & 0x1FU));
			osi_writela(osi_core, val, addr +
				    MACSEC_RX_BYP_LUT_VALID);
		}

		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Unknown controller select\n", 0ULL);
		ret = -1;
		break;
	}
exit:
	return ret;
}

/**
 * @brief lut_data_write - update hardware registers with different LUT params
 *
 * @note
 * Algorithm:
 *  - Update the h/w registers for different LUT types
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] lut_config: attributes from lut_config is used
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static inline nve32_t lut_data_write(struct osi_core_priv_data *const osi_core,
				     const struct osi_macsec_lut_config *const lut_config)
{
	nve32_t ret = 0;

	switch (lut_config->lut_sel) {
	case OSI_LUT_SEL_BYPASS:
		ret = byp_lut_config(osi_core, lut_config);
		break;
	case OSI_LUT_SEL_SCI:
		ret = sci_lut_config(osi_core, lut_config);
		break;
	case OSI_LUT_SEL_SC_PARAM:
		ret = sc_param_lut_config(osi_core, lut_config);
		break;
	case OSI_LUT_SEL_SC_STATE:
		ret = sc_state_lut_config(osi_core, lut_config);
		break;
	case OSI_LUT_SEL_SA_STATE:
		ret = sa_state_lut_config(osi_core, lut_config);
		break;
	default:
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Unsupported LUT\n", 0ULL);
		ret = -1;
		break;
	}

	return ret;
}

/**
 * @brief validate_lut_conf - validate the lut_config params
 *
 * @note
 * Algorithm:
 *  - Return -1 if any of the lut_config attributes are invalid
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] lut_config: attributes from lut_config is used
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t validate_lut_conf(const struct osi_macsec_lut_config *const lut_config)
{
	nve32_t ret = 0;

	/* Validate LUT config */
	if ((lut_config->table_config.ctlr_sel > OSI_CTLR_SEL_MAX) ||
	    (lut_config->table_config.rw > OSI_RW_MAX) ||
	    (lut_config->table_config.index > OSI_TABLE_INDEX_MAX) ||
	    (lut_config->lut_sel > OSI_LUT_SEL_MAX)) {
		MACSEC_LOG("Validating LUT config failed. ctrl: %hu,"
			" rw: %hu, index: %hu, lut_sel: %hu",
			lut_config->table_config.ctlr_sel,
			lut_config->table_config.rw,
			lut_config->table_config.index, lut_config->lut_sel);
		ret = -1;
		goto exit;
	}
exit:
	return ret;
}

/**
 * @brief macsec_lut_config - update hardware registers with different LUT params
 *
 * @note
 * Algorithm:
 *  - Validate if params are fine else return -1
 *  - Poll for the previous update to be finished
 *  - Select the controller based on lut_config
 *  - Update the h/w registers for different LUT types if write attribute is
 *    passed through lut_config
 *  - Poll for the h/w confirmation on the lut_update
 *  - If the lut_config has read attribute read the lut and return -1 on failure
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] lut_config: attributes from lut_config is used. Param used table_config
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t macsec_lut_config(struct osi_core_priv_data *const osi_core,
			     struct osi_macsec_lut_config *const lut_config)
{
	nve32_t ret = 0;
	nveu32_t lut_config_reg;
	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;

	if (validate_lut_conf(lut_config) < 0) {
		ret = -1;
		goto exit;
	}

	/* Wait for previous LUT update to finish */
	ret = poll_for_lut_update(osi_core);
	if (ret < 0) {
		goto exit;
	}

	lut_config_reg = osi_readla(osi_core, base + MACSEC_LUT_CONFIG);
	if (lut_config->table_config.ctlr_sel != OSI_NONE) {
		lut_config_reg |= MACSEC_LUT_CONFIG_CTLR_SEL;
	} else {
		lut_config_reg &= ~MACSEC_LUT_CONFIG_CTLR_SEL;
	}

	if (lut_config->table_config.rw != OSI_NONE) {
		lut_config_reg |= MACSEC_LUT_CONFIG_RW;
		/* For write operation, load the lut_data registers */
		ret = lut_data_write(osi_core, lut_config);
		if (ret < 0) {
			goto exit;
		}
	} else {
		lut_config_reg &= ~MACSEC_LUT_CONFIG_RW;
	}

	lut_config_reg &= ~MACSEC_LUT_CONFIG_LUT_SEL_MASK;
	lut_config_reg |= ((nveu32_t)(lut_config->lut_sel) <<
			   MACSEC_LUT_CONFIG_LUT_SEL_SHIFT);

	lut_config_reg &= ~MACSEC_LUT_CONFIG_INDEX_MASK;
	lut_config_reg |= (nveu32_t)(lut_config->table_config.index);

	lut_config_reg |= MACSEC_LUT_CONFIG_UPDATE;
	osi_writela(osi_core, lut_config_reg, base + MACSEC_LUT_CONFIG);

	/* Wait for this LUT update to finish */
	ret = poll_for_lut_update(osi_core);
	if (ret < 0) {
		goto exit;
	}

	if (lut_config->table_config.rw == OSI_NONE) {
		ret = lut_data_read(osi_core, lut_config);
		if (ret < 0) {
			goto exit;
		}
	}
exit:
	return ret;
}

/**
 * @brief handle_rx_sc_invalid_key - Handles the Rx sc invalid key interrupt
 *
 * @note
 * Algorithm:
 *  - Clears MACSEC_RX_SC_KEY_INVALID_STS0_0 status register
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void handle_rx_sc_invalid_key(
		struct osi_core_priv_data *const osi_core)
{
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu32_t clear = 0;

	MACSEC_LOG("%s()\n", __func__);

	/** check which SC/AN had triggered and clear */
	/* rx_sc0_7 */
	clear = osi_readla(osi_core, addr + MACSEC_RX_SC_KEY_INVALID_STS0_0);
	osi_writela(osi_core, clear, addr + MACSEC_RX_SC_KEY_INVALID_STS0_0);
	/* rx_sc8_15 */
	clear = osi_readla(osi_core, addr + MACSEC_RX_SC_KEY_INVALID_STS1_0);
	osi_writela(osi_core, clear, addr + MACSEC_RX_SC_KEY_INVALID_STS1_0);
}

/**
 * @brief handle_tx_sc_invalid_key - Handles the Tx sc invalid key interrupt
 *
 * @note
 * Algorithm:
 *  - Clears MACSEC_TX_SC_KEY_INVALID_STS0_0 status register
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void handle_tx_sc_invalid_key(
			struct osi_core_priv_data *const osi_core)
{
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu32_t clear = 0;

	MACSEC_LOG("%s()\n", __func__);

	/** check which SC/AN had triggered and clear */
	/* tx_sc0_7 */
	clear = osi_readla(osi_core, addr + MACSEC_TX_SC_KEY_INVALID_STS0_0);
	osi_writela(osi_core, clear, addr + MACSEC_TX_SC_KEY_INVALID_STS0_0);
	/* tx_sc8_15 */
	clear = osi_readla(osi_core, addr + MACSEC_TX_SC_KEY_INVALID_STS1_0);
	osi_writela(osi_core, clear, addr + MACSEC_TX_SC_KEY_INVALID_STS1_0);
}

/**
 * @brief handle_safety_err_irq - Safety Error handler
 *
 * @note
 * Algorithm:
 *  - Nothing is handled
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void handle_safety_err_irq(
				const struct osi_core_priv_data *const osi_core)
{
	(void) osi_core;
	OSI_CORE_INFO(osi_core->osd, OSI_LOG_ARG_INVALID,
		      "Safety Error Handler \n", 0ULL);
	MACSEC_LOG("%s()\n", __func__);
}

/**
 * @brief handle_rx_sc_replay_err - Rx SC replay error handler
 *
 * @note
 * Algorithm:
 *  - Clears MACSEC_RX_SC_REPLAY_ERROR_STATUS0_0 status register
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void handle_rx_sc_replay_err(
				struct osi_core_priv_data *const osi_core)
{
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu32_t clear = 0;

	/* rx_sc0_7 */
	clear = osi_readla(osi_core, addr +
			   MACSEC_RX_SC_REPLAY_ERROR_STATUS0_0);
	osi_writela(osi_core, clear, addr +
		    MACSEC_RX_SC_REPLAY_ERROR_STATUS0_0);
	/* rx_sc8_15 */
	clear = osi_readla(osi_core, addr +
			   MACSEC_RX_SC_REPLAY_ERROR_STATUS1_0);
	osi_writela(osi_core, clear, addr +
		    MACSEC_RX_SC_REPLAY_ERROR_STATUS1_0);
}

/**
 * @brief handle_rx_pn_exhausted - Rx PN exhaustion handler
 *
 * @note
 * Algorithm:
 *  - Clears MACSEC_RX_SC_PN_EXHAUSTED_STATUS0_0 status register
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void handle_rx_pn_exhausted(
			struct osi_core_priv_data *const osi_core)
{
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu32_t clear = 0;

	/* Check which SC/AN had triggered and clear */
	/* rx_sc0_7 */
	clear = osi_readla(osi_core, addr +
			   MACSEC_RX_SC_PN_EXHAUSTED_STATUS0_0);
	osi_writela(osi_core, clear, addr +
		    MACSEC_RX_SC_PN_EXHAUSTED_STATUS0_0);
	/* rx_sc8_15 */
	clear = osi_readla(osi_core, addr +
			   MACSEC_RX_SC_PN_EXHAUSTED_STATUS1_0);
	osi_writela(osi_core, clear, addr +
		    MACSEC_RX_SC_PN_EXHAUSTED_STATUS1_0);
}

/**
 * @brief handle_tx_sc_err - Tx SC error handler
 *
 * @note
 * Algorithm:
 *  - Clears MACSEC_TX_SC_ERROR_INTERRUPT_STATUS_0 status register
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void handle_tx_sc_err(struct osi_core_priv_data *const osi_core)
{
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu32_t clear = 0;

	clear = osi_readla(osi_core, addr +
			  MACSEC_TX_SC_ERROR_INTERRUPT_STATUS_0);
	osi_writela(osi_core, clear, addr +
		    MACSEC_TX_SC_ERROR_INTERRUPT_STATUS_0);

}

/**
 * @brief handle_tx_pn_threshold - Tx PN Threshold handler
 *
 * @note
 * Algorithm:
 *  - Clears MACSEC_TX_SC_PN_THRESHOLD_STATUS0_0 status register
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void handle_tx_pn_threshold(
			struct osi_core_priv_data *const osi_core)
{
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu32_t clear = 0;

	/* check which SC/AN had triggered and clear */
	/* tx_sc0_7 */
	clear = osi_readla(osi_core, addr +
			   MACSEC_TX_SC_PN_THRESHOLD_STATUS0_0);
	osi_writela(osi_core, clear, addr +
		    MACSEC_TX_SC_PN_THRESHOLD_STATUS0_0);
	/* tx_sc8_15 */
	clear = osi_readla(osi_core, addr +
			   MACSEC_TX_SC_PN_THRESHOLD_STATUS1_0);
	osi_writela(osi_core, clear, addr +
		    MACSEC_TX_SC_PN_THRESHOLD_STATUS1_0);
}

/**
 * @brief handle_tx_pn_exhausted - Tx PN exhaustion handler
 *
 * @note
 * Algorithm:
 *  - Clears MACSEC_TX_SC_PN_EXHAUSTED_STATUS0_0 status register
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void handle_tx_pn_exhausted(
			struct osi_core_priv_data *const osi_core)
{
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu32_t clear = 0;

	/* check which SC/AN had triggered and clear */
	/* tx_sc0_7 */
	clear = osi_readla(osi_core, addr +
			   MACSEC_TX_SC_PN_EXHAUSTED_STATUS0_0);
	osi_writela(osi_core, clear, addr +
		    MACSEC_TX_SC_PN_EXHAUSTED_STATUS0_0);
	/* tx_sc8_15 */
	clear = osi_readla(osi_core, addr +
			   MACSEC_TX_SC_PN_EXHAUSTED_STATUS1_0);
	osi_writela(osi_core, clear, addr +
		    MACSEC_TX_SC_PN_EXHAUSTED_STATUS1_0);
}

/**
 * @brief handle_dbg_evt_capture_done - Debug event handler
 *
 * @note
 * Algorithm:
 *  - Clears the Tx/Rx debug status register
 *  - Disabled the trigger events
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] ctrl_sel: Controller selected
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void handle_dbg_evt_capture_done(
			struct osi_core_priv_data *const osi_core,
			nveu16_t ctrl_sel)
{
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nveu32_t trigger_evts = 0;

	if (ctrl_sel == OSI_CTLR_SEL_TX) {
		trigger_evts = osi_readla(osi_core, addr +
					  MACSEC_TX_DEBUG_STATUS_0);
		osi_writela(osi_core, trigger_evts, addr +
			    MACSEC_TX_DEBUG_STATUS_0);
		/* clear all trigger events */
		trigger_evts = 0U;
		osi_writela(osi_core, trigger_evts,
			    addr + MACSEC_TX_DEBUG_TRIGGER_EN_0);
	} else if (ctrl_sel == OSI_CTLR_SEL_RX) {
		trigger_evts = osi_readla(osi_core, addr +
					  MACSEC_RX_DEBUG_STATUS_0);
		osi_writela(osi_core, trigger_evts, addr +
			    MACSEC_RX_DEBUG_STATUS_0);
		/* clear all trigger events */
		trigger_evts = 0U;
		osi_writela(osi_core, trigger_evts,
			    addr + MACSEC_RX_DEBUG_TRIGGER_EN_0);
	} else {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Invalid ctrl selected\n", 0ULL);
	}
}

/**
 * @brief handle_tx_irq - Handles all Tx interrupts
 *
 * @note
 * Algorithm:
 *  - Clears the Below Interrupt status
 *    - Tx DBG buffer capture done
 *    - Tx MTU check fail
 *    - Tx AES GCM overflow
 *    - Tx SC AN Not valid
 *    - Tx MAC CRC Error
 *      - If HSi is enabled and threshold is met, hsi report counters
 *        are incremented
 *    - Tx PN Threshold reached
 *    - Tx PN Exhausted
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void handle_tx_irq(struct osi_core_priv_data *const osi_core)
{
	nveu32_t tx_isr, clear = 0;
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
#ifdef HSI_SUPPORT
	nveu64_t tx_crc_err = 0;
#endif

	tx_isr = osi_readla(osi_core, addr + MACSEC_TX_ISR);
	MACSEC_LOG("%s(): tx_isr 0x%x\n", __func__, tx_isr);
	if ((tx_isr & MACSEC_TX_DBG_BUF_CAPTURE_DONE) ==
	    MACSEC_TX_DBG_BUF_CAPTURE_DONE) {
		handle_dbg_evt_capture_done(osi_core, OSI_CTLR_SEL_TX);
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.tx_dbg_capture_done);
		clear |= MACSEC_TX_DBG_BUF_CAPTURE_DONE;
	}

	if ((tx_isr & MACSEC_TX_MTU_CHECK_FAIL) == MACSEC_TX_MTU_CHECK_FAIL) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.tx_mtu_check_fail);
		clear |= MACSEC_TX_MTU_CHECK_FAIL;
	}

	if ((tx_isr & MACSEC_TX_AES_GCM_BUF_OVF) == MACSEC_TX_AES_GCM_BUF_OVF) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.tx_aes_gcm_buf_ovf);
		clear |= MACSEC_TX_AES_GCM_BUF_OVF;
	}

	if ((tx_isr & MACSEC_TX_SC_AN_NOT_VALID) == MACSEC_TX_SC_AN_NOT_VALID) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.tx_sc_an_not_valid);
		handle_tx_sc_err(osi_core);
		clear |= MACSEC_TX_SC_AN_NOT_VALID;
	}

	if ((tx_isr & MACSEC_TX_MAC_CRC_ERROR) == MACSEC_TX_MAC_CRC_ERROR) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.tx_mac_crc_error);
		clear |= MACSEC_TX_MAC_CRC_ERROR;
#ifdef HSI_SUPPORT
		if (osi_core->hsi.enabled == OSI_ENABLE) {
			tx_crc_err = osi_core->macsec_irq_stats.tx_mac_crc_error /
				osi_core->hsi.err_count_threshold;
			if (osi_core->hsi.macsec_tx_crc_err_count < tx_crc_err) {
				osi_core->hsi.macsec_tx_crc_err_count = tx_crc_err;
				osi_core->hsi.macsec_report_count_err[MACSEC_TX_CRC_ERR_IDX] =
					OSI_ENABLE;
			}

			osi_core->hsi.macsec_err_code[MACSEC_TX_CRC_ERR_IDX] =
				OSI_MACSEC_TX_CRC_ERR;
			osi_core->hsi.macsec_report_err = OSI_ENABLE;
		}
#endif
	}

	if ((tx_isr & MACSEC_TX_PN_THRSHLD_RCHD) == MACSEC_TX_PN_THRSHLD_RCHD) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.tx_pn_threshold);
		handle_tx_pn_threshold(osi_core);
		clear |= MACSEC_TX_PN_THRSHLD_RCHD;
	}

	if ((tx_isr & MACSEC_TX_PN_EXHAUSTED) == MACSEC_TX_PN_EXHAUSTED) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.tx_pn_exhausted);
		handle_tx_pn_exhausted(osi_core);
		clear |= MACSEC_TX_PN_EXHAUSTED;
	}
	if (clear != OSI_NONE) {
		osi_writela(osi_core, clear, addr + MACSEC_TX_ISR);
	}
}

/**
 * @brief handle_rx_irq - Handles all Rx interrupts
 *
 * @note
 * Algorithm:
 *  - Clears the Below Interrupt status
 *    - Rx DBG buffer capture done
 *    - Rx ICV check fail
 *      - If HSi is enabled and threshold is met, hsi report counters
 *        are incremented
 *    - Rx Replay error
 *    - Rx MTU check fail
 *    - Rx AES GCM overflow
 *    - Rx MAC CRC check failed
 *      - If HSi is enabled and threshold is met, hsi report counters
 *        are incremented
 *    - Rx PN Exhausted
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void handle_rx_irq(struct osi_core_priv_data *const osi_core)
{
	nveu32_t rx_isr;
	nveu32_t clear = 0;
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
#ifdef HSI_SUPPORT
	nveu64_t rx_crc_err = 0;
	nveu64_t rx_icv_err = 0;
#endif

	rx_isr = osi_readla(osi_core, addr + MACSEC_RX_ISR);
	MACSEC_LOG("%s(): rx_isr 0x%x\n", __func__, rx_isr);

	if ((rx_isr & MACSEC_RX_DBG_BUF_CAPTURE_DONE) ==
	    MACSEC_RX_DBG_BUF_CAPTURE_DONE) {
		handle_dbg_evt_capture_done(osi_core, OSI_CTLR_SEL_RX);
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.rx_dbg_capture_done);
		clear |= MACSEC_RX_DBG_BUF_CAPTURE_DONE;
	}

	if ((rx_isr & MACSEC_RX_ICV_ERROR) == MACSEC_RX_ICV_ERROR) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.rx_icv_err_threshold);
		clear |= MACSEC_RX_ICV_ERROR;
#ifdef HSI_SUPPORT
		if (osi_core->hsi.enabled == OSI_ENABLE) {
			rx_icv_err = osi_core->macsec_irq_stats.rx_icv_err_threshold /
				osi_core->hsi.err_count_threshold;
			if (osi_core->hsi.macsec_rx_icv_err_count < rx_icv_err) {
				osi_core->hsi.macsec_rx_icv_err_count = rx_icv_err;
				osi_core->hsi.macsec_report_count_err[MACSEC_RX_ICV_ERR_IDX] =
					OSI_ENABLE;
			}
			osi_core->hsi.macsec_err_code[MACSEC_RX_ICV_ERR_IDX] =
					OSI_MACSEC_RX_ICV_ERR;
			osi_core->hsi.macsec_report_err = OSI_ENABLE;
		}
#endif
	}

	if ((rx_isr & MACSEC_RX_REPLAY_ERROR) == MACSEC_RX_REPLAY_ERROR) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.rx_replay_error);
		handle_rx_sc_replay_err(osi_core);
		clear |= MACSEC_RX_REPLAY_ERROR;
	}

	if ((rx_isr & MACSEC_RX_MTU_CHECK_FAIL) == MACSEC_RX_MTU_CHECK_FAIL) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.rx_mtu_check_fail);
		clear |= MACSEC_RX_MTU_CHECK_FAIL;
	}

	if ((rx_isr & MACSEC_RX_AES_GCM_BUF_OVF) == MACSEC_RX_AES_GCM_BUF_OVF) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.rx_aes_gcm_buf_ovf);
		clear |= MACSEC_RX_AES_GCM_BUF_OVF;
	}

	if ((rx_isr & MACSEC_RX_MAC_CRC_ERROR) == MACSEC_RX_MAC_CRC_ERROR) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.rx_mac_crc_error);
		clear |= MACSEC_RX_MAC_CRC_ERROR;
#ifdef HSI_SUPPORT
		if (osi_core->hsi.enabled == OSI_ENABLE) {
			rx_crc_err = osi_core->macsec_irq_stats.rx_mac_crc_error /
				osi_core->hsi.err_count_threshold;
			if (osi_core->hsi.macsec_rx_crc_err_count < rx_crc_err) {
				osi_core->hsi.macsec_rx_crc_err_count = rx_crc_err;
				osi_core->hsi.macsec_report_count_err[MACSEC_RX_CRC_ERR_IDX] =
					OSI_ENABLE;
			}
			osi_core->hsi.macsec_err_code[MACSEC_RX_CRC_ERR_IDX] =
					OSI_MACSEC_RX_CRC_ERR;
			osi_core->hsi.macsec_report_err = OSI_ENABLE;
		}
#endif
	}

	if ((rx_isr & MACSEC_RX_PN_EXHAUSTED) == MACSEC_RX_PN_EXHAUSTED) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.rx_pn_exhausted);
		handle_rx_pn_exhausted(osi_core);
		clear |= MACSEC_RX_PN_EXHAUSTED;
	}
	if (clear != OSI_NONE) {
		osi_writela(osi_core, clear, addr + MACSEC_RX_ISR);
	}
}

/**
 * @brief handle_common_irq - Common interrupt handler
 *
 * @note
 * Algorithm:
 *  - Clears the Below Interrupt status
 *    - Secure register access violation
 *    - Rx Uninititalized key slot error
 *    - Rx Lookup miss event
 *    - Tx Uninititalized key slot error
 *    - Tx Lookup miss event
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static inline void handle_common_irq(struct osi_core_priv_data *const osi_core)
{
	nveu32_t common_isr;
	nveu32_t clear = 0;
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;

	common_isr = osi_readla(osi_core, addr + MACSEC_COMMON_ISR);
	MACSEC_LOG("%s(): common_isr 0x%x\n", __func__, common_isr);

	if ((common_isr & MACSEC_SECURE_REG_VIOL) == MACSEC_SECURE_REG_VIOL) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.secure_reg_viol);
		clear |= MACSEC_SECURE_REG_VIOL;
#ifdef HSI_SUPPORT
		if (osi_core->hsi.enabled == OSI_ENABLE) {
			osi_core->hsi.macsec_err_code[MACSEC_REG_VIOL_ERR_IDX] =
				OSI_MACSEC_REG_VIOL_ERR;
			osi_core->hsi.macsec_report_err = OSI_ENABLE;
		}
#endif
	}

	if ((common_isr & MACSEC_RX_UNINIT_KEY_SLOT) ==
	    MACSEC_RX_UNINIT_KEY_SLOT) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.rx_uninit_key_slot);
		clear |= MACSEC_RX_UNINIT_KEY_SLOT;
		handle_rx_sc_invalid_key(osi_core);
	}

	if ((common_isr & MACSEC_RX_LKUP_MISS) == MACSEC_RX_LKUP_MISS) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.rx_lkup_miss);
		clear |= MACSEC_RX_LKUP_MISS;
	}

	if ((common_isr & MACSEC_TX_UNINIT_KEY_SLOT) ==
	    MACSEC_TX_UNINIT_KEY_SLOT) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.tx_uninit_key_slot);
		clear |= MACSEC_TX_UNINIT_KEY_SLOT;
		handle_tx_sc_invalid_key(osi_core);
	}

	if ((common_isr & MACSEC_TX_LKUP_MISS) == MACSEC_TX_LKUP_MISS) {
		CERT_C__POST_INC__U64(osi_core->macsec_irq_stats.tx_lkup_miss);
		clear |= MACSEC_TX_LKUP_MISS;
	}
	if (clear != OSI_NONE) {
		osi_writela(osi_core, clear, addr + MACSEC_COMMON_ISR);
	}
}

/**
 * @brief macsec_handle_irq - Macsec interrupt handler
 *
 * @note
 * Algorithm:
 *  - Handles below non-secure interrupts
 *    - Handles Tx interrupts
 *    - Handles Rx interrupts
 *    - Handles Safety interrupts
 *    - Handles common interrupts
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void macsec_handle_irq(struct osi_core_priv_data *const osi_core)
{
	nveu32_t irq_common_sr, common_isr;
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;

	irq_common_sr = osi_readla(osi_core, addr + MACSEC_INTERRUPT_COMMON_SR);
	MACSEC_LOG("%s(): common_sr 0x%x\n", __func__, irq_common_sr);
	if ((irq_common_sr & MACSEC_COMMON_SR_TX) == MACSEC_COMMON_SR_TX) {
		handle_tx_irq(osi_core);
	}

	if ((irq_common_sr & MACSEC_COMMON_SR_RX) == MACSEC_COMMON_SR_RX) {
		handle_rx_irq(osi_core);
	}

	if ((irq_common_sr & MACSEC_COMMON_SR_SFTY_ERR) ==
	    MACSEC_COMMON_SR_SFTY_ERR) {
		handle_safety_err_irq(osi_core);
	}

	common_isr = osi_readla(osi_core, addr + MACSEC_COMMON_ISR);
	if (common_isr != OSI_NONE) {
		handle_common_irq(osi_core);
	}
}

/**
 * @brief macsec_cipher_config - Configures the cipher type
 *
 * @note
 * Algorithm:
 *  - Configures the AES type to h/w registers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] cipher: Cipher type
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t macsec_cipher_config(struct osi_core_priv_data *const osi_core,
				 nveu32_t cipher)
{
	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;
	nveu32_t val;
	nve32_t ret = 0;

	val = osi_readla(osi_core, base + MACSEC_GCM_AES_CONTROL_0);

	val &= ~MACSEC_TX_AES_MODE_MASK;
	val &= ~MACSEC_RX_AES_MODE_MASK;
	if (cipher == OSI_MACSEC_CIPHER_AES128) {
		val |= MACSEC_TX_AES_MODE_AES128;
		val |= MACSEC_RX_AES_MODE_AES128;
	} else if (cipher == OSI_MACSEC_CIPHER_AES256) {
		val |= MACSEC_TX_AES_MODE_AES256;
		val |= MACSEC_RX_AES_MODE_AES256;
	} else {
		ret = -1;
		goto exit;
	}

	osi_writela(osi_core, val, base + MACSEC_GCM_AES_CONTROL_0);
exit:
	return ret;
}

#ifdef DEBUG_MACSEC
/**
 * @brief macsec_loopback_config - Configures the loopback mode
 *
 * @note
 * Algorithm:
 *  - Configures the loopback mode to h/w registers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] enable: Enable or disable the loopback
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t macsec_loopback_config(
				struct osi_core_priv_data *const osi_core,
				nveu32_t enable)
{
	nveu8_t *base = (nveu8_t *)osi_core->macsec_base;
	nveu32_t val;
	nve32_t ret = 0;

	val = osi_readla(osi_core, base + MACSEC_CONTROL1);

	if (enable == OSI_ENABLE) {
		val |= MACSEC_LOOPBACK_MODE_EN;
	} else if (enable == OSI_DISABLE) {
		val &= ~MACSEC_LOOPBACK_MODE_EN;
	} else {
		ret = -1;
		goto exit;
	}

	osi_writela(osi_core, val, base + MACSEC_CONTROL1);
exit:
	return ret;
}
#endif /* DEBUG_MACSEC */

/**
 * @brief clear_byp_lut - Clears the bypass lut
 *
 * @note
 * Algorithm:
 *  - Clears the bypass lut for all the indices in both Tx and Rx controllers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t clear_byp_lut(struct osi_core_priv_data *const osi_core)
{
	struct osi_macsec_lut_config lut_config = {0};
	struct osi_macsec_table_config *table_config = &lut_config.table_config;
	nveu16_t i, j;
	nve32_t ret = 0;

	table_config->rw = OSI_LUT_WRITE;
	/* Tx/Rx BYP LUT */
	lut_config.lut_sel = OSI_LUT_SEL_BYPASS;
	for (i = 0; i <= OSI_CTLR_SEL_MAX; i++) {
		table_config->ctlr_sel = i;
		for (j = 0; j <= OSI_BYP_LUT_MAX_INDEX; j++) {
			table_config->index = j;
			ret = macsec_lut_config(osi_core, &lut_config);
			if (ret < 0) {
				OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
					     "Error clearing CTLR:BYPASS LUT:INDEX: \n", j);
				goto exit;
			}
		}
	}
exit:
	return ret;
}

/**
 * @brief clear_sci_lut - Clears the sci lut
 *
 * @note
 * Algorithm:
 *  - Clears the sci lut for all the indices in both Tx and Rx controllers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t clear_sci_lut(struct osi_core_priv_data *const osi_core)
{
	struct osi_macsec_lut_config lut_config = {0};
	struct osi_macsec_table_config *table_config = &lut_config.table_config;
	nveu16_t i, j;
	nve32_t ret = 0;

	table_config->rw = OSI_LUT_WRITE;
	/* Tx/Rx SCI LUT */
	lut_config.lut_sel = OSI_LUT_SEL_SCI;
	for (i = 0; i <= OSI_CTLR_SEL_MAX; i++) {
		table_config->ctlr_sel = i;
		for (j = 0; j <= OSI_SC_LUT_MAX_INDEX; j++) {
			table_config->index = j;
			ret = macsec_lut_config(osi_core, &lut_config);
			if (ret < 0) {
				OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
					     "Error clearing CTLR:SCI LUT:INDEX: \n", j);
				goto exit;
			}
		}
	}
exit:
	return ret;
}

/**
 * @brief clear_sc_param_lut - Clears the sc param lut
 *
 * @note
 * Algorithm:
 *  - Clears the sc param lut for all the indices in both Tx and Rx controllers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t clear_sc_param_lut(struct osi_core_priv_data *const osi_core)
{
	struct osi_macsec_lut_config lut_config = {0};
	struct osi_macsec_table_config *table_config = &lut_config.table_config;
	nveu16_t i, j;
	nve32_t ret = 0;

	table_config->rw = OSI_LUT_WRITE;
	/* Tx/Rx SC param LUT */
	lut_config.lut_sel = OSI_LUT_SEL_SC_PARAM;
	for (i = 0; i <= OSI_CTLR_SEL_MAX; i++) {
		table_config->ctlr_sel = i;
		for (j = 0; j <= OSI_SC_LUT_MAX_INDEX; j++) {
			table_config->index = j;
			ret = macsec_lut_config(osi_core, &lut_config);
			if (ret < 0) {
				OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
					     "Error clearing CTLR:SC PARAM LUT:INDEX: \n", j);
				goto exit;
			}
		}
	}
exit:
	return ret;

}

/**
 * @brief clear_sc_state_lut - Clears the sc state lut
 *
 * @note
 * Algorithm:
 *  - Clears the sc state lut for all the indices in both Tx and Rx controllers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t clear_sc_state_lut(struct osi_core_priv_data *const osi_core)
{
	struct osi_macsec_lut_config lut_config = {0};
	struct osi_macsec_table_config *table_config = &lut_config.table_config;
	nveu16_t i, j;
	nve32_t ret = 0;

	table_config->rw = OSI_LUT_WRITE;
	/* Tx/Rx SC state */
	lut_config.lut_sel = OSI_LUT_SEL_SC_STATE;
	for (i = 0; i <= OSI_CTLR_SEL_MAX; i++) {
		table_config->ctlr_sel = i;
		for (j = 0; j <= OSI_SC_LUT_MAX_INDEX; j++) {
			table_config->index = j;
			ret = macsec_lut_config(osi_core, &lut_config);
			if (ret < 0) {
				OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
					     "Error clearing CTLR:SC STATE LUT:INDEX: \n", j);
				goto exit;
			}
		}
	}
exit:
	return ret;

}

/**
 * @brief clear_sa_state_lut - Clears the sa state lut
 *
 * @note
 * Algorithm:
 *  - Clears the sa state lut for all the indices in both Tx and Rx controllers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t clear_sa_state_lut(struct osi_core_priv_data *const osi_core)
{
	struct osi_macsec_lut_config lut_config = {0};
	struct osi_macsec_table_config *table_config = &lut_config.table_config;
	nveu16_t j;
	nve32_t ret = 0;

	table_config->rw = OSI_LUT_WRITE;
	/* Tx SA state LUT */
	lut_config.lut_sel = OSI_LUT_SEL_SA_STATE;
	table_config->ctlr_sel = OSI_CTLR_SEL_TX;
	for (j = 0; j <= OSI_SA_LUT_MAX_INDEX; j++) {
		table_config->index = j;
		ret = macsec_lut_config(osi_core, &lut_config);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "Error clearing TX CTLR:SA STATE LUT:INDEX: \n", j);
			goto exit;
		}
	}

	/* Rx SA state LUT */
	lut_config.lut_sel = OSI_LUT_SEL_SA_STATE;
	table_config->ctlr_sel = OSI_CTLR_SEL_RX;
	for (j = 0; j <= OSI_SA_LUT_MAX_INDEX; j++) {
		table_config->index = j;
		ret = macsec_lut_config(osi_core, &lut_config);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "Error clearing RX CTLR:SA STATE LUT:INDEX: \n", j);
			goto exit;
		}
	}
exit:
	return ret;
}

/**
 * @brief clear_lut - Clears all the LUTs
 *
 * @note
 * Algorithm:
 *  - Clears all of the below LUTs
 *    - SCI LUT
 *    - SC param LUT
 *    - SC state LUT
 *    - SA state LUT
 *    - key for all the SAs
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t clear_lut(struct osi_core_priv_data *const osi_core)
{
	struct osi_macsec_lut_config lut_config = {0};
#ifdef MACSEC_KEY_PROGRAM
	struct osi_macsec_kt_config kt_config = {0};
	nveu16_t i, j;
#endif
	struct osi_macsec_table_config *table_config = &lut_config.table_config;
	nve32_t ret = 0;

	table_config->rw = OSI_LUT_WRITE;
	/* Clear all the LUT's which have a dedicated LUT valid bit per entry */
	ret = clear_byp_lut(osi_core);
	if (ret < 0) {
		goto exit;
	}
	ret = clear_sci_lut(osi_core);
	if (ret < 0) {
		goto exit;
	}
	ret = clear_sc_param_lut(osi_core);
	if (ret < 0) {
		goto exit;
	}
	ret = clear_sc_state_lut(osi_core);
	if (ret < 0) {
		goto exit;
	}
	ret = clear_sa_state_lut(osi_core);
	if (ret < 0) {
		goto exit;
	}

#ifdef MACSEC_KEY_PROGRAM
	/* Key table */
	table_config = &kt_config.table_config;
	table_config->rw = OSI_LUT_WRITE;
	for (i = 0; i <= OSI_CTLR_SEL_MAX; i++) {
		table_config->ctlr_sel = i;
		for (j = 0; j <= OSI_TABLE_INDEX_MAX; j++) {
			table_config->index = j;
			ret = macsec_kt_config(osi_core, &kt_config);
			if (ret < 0) {
				OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
					     "Error clearing KT LUT:INDEX: \n", j);
				goto exit;
			}
		}
	}
#endif /* MACSEC_KEY_PROGRAM */
exit:
	return ret;
}

/**
 * @brief macsec_deinit - Deinitializes the macsec
 *
 * @note
 * Algorithm:
 *  - Clears the lut_status buffer
 *  - Programs the mac IPG and MTL_EST values with MACSEC disabled set of values
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0
 */
static nve32_t macsec_deinit(struct osi_core_priv_data *const osi_core)
{
	nveu32_t i;
#if defined(MACSEC_SUPPORT) && !defined(OSI_STRIPPED_LIB)
	const struct core_local *l_core = (void *)osi_core;
#endif

	for (i = OSI_CTLR_SEL_TX; i <= OSI_CTLR_SEL_RX; i++) {
		osi_memset(&osi_core->macsec_lut_status[i], OSI_NONE,
			   sizeof(struct osi_macsec_lut_status));
	}

#if defined(MACSEC_SUPPORT) && !defined(OSI_STRIPPED_LIB)
	/* Update MAC as per macsec requirement */
	if (l_core->ops_p->macsec_config_mac != OSI_NULL) {
		l_core->ops_p->macsec_config_mac(osi_core, OSI_DISABLE);
	} else {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Failed config MAC per macsec\n", 0ULL);
	}
#endif
	return 0;
}

/**
 * @brief macsec_update_mtu - Updates macsec MTU
 *
 * @note
 * Algorithm:
 *  - Returns if invalid mtu received
 *  - Programs the tx and rx MTU to macsec h/w registers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] mtu: mtu to be programmed
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t macsec_update_mtu(struct osi_core_priv_data *const osi_core,
				 nveu32_t mtu)
{
	nveu32_t val = 0;
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nve32_t ret = 0;

	if (mtu > OSI_MAX_MTU_SIZE) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Invalid MTU received!!\n", mtu);
		ret = -1;
		goto exit;
	}
	/* Set MTU */
	val = osi_readla(osi_core, addr + MACSEC_TX_MTU_LEN);
	MACSEC_LOG("Read MACSEC_TX_MTU_LEN: 0x%x\n", val);
	val &= ~(MTU_LENGTH_MASK);
	val |= (mtu & MTU_LENGTH_MASK);
	MACSEC_LOG("Write MACSEC_TX_MTU_LEN: 0x%x\n", val);
	osi_writela(osi_core, val, addr + MACSEC_TX_MTU_LEN);

	val = osi_readla(osi_core, addr + MACSEC_RX_MTU_LEN);
	MACSEC_LOG("Read MACSEC_RX_MTU_LEN: 0x%x\n", val);
	val &= ~(MTU_LENGTH_MASK);
	val |= (mtu & MTU_LENGTH_MASK);
	MACSEC_LOG("Write MACSEC_RX_MTU_LEN: 0x%x\n", val);
	osi_writela(osi_core, val, addr + MACSEC_RX_MTU_LEN);
exit:
	return ret;
}

/**
 * @brief set_byp_lut - Sets bypass lut
 *
 * @note
 * Algorithm:
 *  - Adds broadcast address to the Tx and Rx Bypass luts
 *  - Adds the mkpdu multi case address to the Tx and Rx Bypass luts
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t set_byp_lut(struct osi_core_priv_data *const osi_core)
{
	struct osi_macsec_lut_config lut_config = {0};
	struct osi_macsec_table_config *table_config = &lut_config.table_config;
	/* Store MAC address in reverse, per HW design */
	const nveu8_t mac_da_mkpdu[OSI_ETH_ALEN] = {0x3, 0x0, 0x0,
					      0xC2, 0x80, 0x01};
	const nveu8_t mac_da_bc[OSI_ETH_ALEN] = {0xFF, 0xFF, 0xFF,
					   0xFF, 0xFF, 0xFF};
	nve32_t ret = 0;
	nveu16_t i, j;

	/* Set default BYP for MKPDU/BC packets */
	table_config->rw = OSI_LUT_WRITE;
	lut_config.lut_sel = OSI_LUT_SEL_BYPASS;
	lut_config.flags |= (OSI_LUT_FLAGS_DA_VALID |
			     OSI_LUT_FLAGS_ENTRY_VALID);
	for (j = 0; j < OSI_ETH_ALEN; j++) {
		lut_config.lut_in.da[j] = mac_da_bc[j];
	}

	for (i = OSI_CTLR_SEL_TX; i <= OSI_CTLR_SEL_RX; i++) {
		table_config->ctlr_sel = i;
		table_config->index =
				osi_core->macsec_lut_status[i].next_byp_idx;
		ret = macsec_lut_config(osi_core, &lut_config);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				      "Failed to set BYP for BC addr\n", (nveul64_t)ret);
			goto exit;
		} else {
			osi_core->macsec_lut_status[i].next_byp_idx = (nveu16_t )
				((osi_core->macsec_lut_status[i].next_byp_idx & 0xFFU) + 1U);
		}
	}

	for (j = 0; j < OSI_ETH_ALEN; j++) {
		lut_config.lut_in.da[j] = mac_da_mkpdu[j];
	}

	for (i = OSI_CTLR_SEL_TX; i <= OSI_CTLR_SEL_RX; i++) {
		table_config->ctlr_sel = i;
		table_config->index =
				osi_core->macsec_lut_status[i].next_byp_idx;
		ret = macsec_lut_config(osi_core, &lut_config);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			   "Failed to set BYP for MKPDU multicast DA\n", (nveul64_t)ret);

			goto exit;
		} else {
			osi_core->macsec_lut_status[i].next_byp_idx = (nveu16_t )
				((osi_core->macsec_lut_status[i].next_byp_idx & 0xFFU) + 1U);
		}
	}
exit:
	return ret;
}

#ifdef DEBUG_MACSEC
static void macsec_intr_config(struct osi_core_priv_data *const osi_core, nveu32_t enable)
{
	nveu32_t val = 0;
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;

	if (enable == OSI_ENABLE) {
		val = osi_readla(osi_core, addr + MACSEC_TX_IMR);
		MACSEC_LOG("Read MACSEC_TX_IMR: 0x%x\n", val);
		val |= (MACSEC_TX_DBG_BUF_CAPTURE_DONE_INT_EN |
			MACSEC_TX_MTU_CHECK_FAIL_INT_EN |
			MACSEC_TX_SC_AN_NOT_VALID_INT_EN |
			MACSEC_TX_AES_GCM_BUF_OVF_INT_EN |
			MACSEC_TX_PN_EXHAUSTED_INT_EN |
			MACSEC_TX_PN_THRSHLD_RCHD_INT_EN);
		osi_writela(osi_core, val, addr + MACSEC_TX_IMR);
		MACSEC_LOG("Write MACSEC_TX_IMR: 0x%x\n", val);

		val = osi_readla(osi_core, addr + MACSEC_RX_IMR);
		MACSEC_LOG("Read MACSEC_RX_IMR: 0x%x\n", val);

		val |= (MACSEC_RX_DBG_BUF_CAPTURE_DONE_INT_EN |
			RX_REPLAY_ERROR_INT_EN |
			MACSEC_RX_MTU_CHECK_FAIL_INT_EN |
			MACSEC_RX_AES_GCM_BUF_OVF_INT_EN |
			MACSEC_RX_PN_EXHAUSTED_INT_EN
		       );
		osi_writela(osi_core, val, addr + MACSEC_RX_IMR);
		MACSEC_LOG("Write MACSEC_RX_IMR: 0x%x\n", val);

		val = osi_readla(osi_core, addr + MACSEC_COMMON_IMR);
		MACSEC_LOG("Read MACSEC_COMMON_IMR: 0x%x\n", val);
		val |= (MACSEC_RX_UNINIT_KEY_SLOT_INT_EN |
			MACSEC_RX_LKUP_MISS_INT_EN |
			MACSEC_TX_UNINIT_KEY_SLOT_INT_EN |
			MACSEC_TX_LKUP_MISS_INT_EN);
		osi_writela(osi_core, val, addr + MACSEC_COMMON_IMR);
		MACSEC_LOG("Write MACSEC_COMMON_IMR: 0x%x\n", val);
	} else {
		val = osi_readla(osi_core, addr + MACSEC_TX_IMR);
		MACSEC_LOG("Read MACSEC_TX_IMR: 0x%x\n", val);
		val &= (~MACSEC_TX_DBG_BUF_CAPTURE_DONE_INT_EN &
			~MACSEC_TX_MTU_CHECK_FAIL_INT_EN &
			~MACSEC_TX_SC_AN_NOT_VALID_INT_EN &
			~MACSEC_TX_AES_GCM_BUF_OVF_INT_EN &
			~MACSEC_TX_PN_EXHAUSTED_INT_EN &
			~MACSEC_TX_PN_THRSHLD_RCHD_INT_EN);
		osi_writela(osi_core, val, addr + MACSEC_TX_IMR);
		MACSEC_LOG("Write MACSEC_TX_IMR: 0x%x\n", val);

		val = osi_readla(osi_core, addr + MACSEC_RX_IMR);
		MACSEC_LOG("Read MACSEC_RX_IMR: 0x%x\n", val);
		val &= (~MACSEC_RX_DBG_BUF_CAPTURE_DONE_INT_EN &
			~RX_REPLAY_ERROR_INT_EN &
			~MACSEC_RX_MTU_CHECK_FAIL_INT_EN &
			~MACSEC_RX_AES_GCM_BUF_OVF_INT_EN &
			~MACSEC_RX_PN_EXHAUSTED_INT_EN
		       );
		osi_writela(osi_core, val, addr + MACSEC_RX_IMR);
		MACSEC_LOG("Write MACSEC_RX_IMR: 0x%x\n", val);

		val = osi_readla(osi_core, addr + MACSEC_COMMON_IMR);
		MACSEC_LOG("Read MACSEC_COMMON_IMR: 0x%x\n", val);
		val &= (~MACSEC_RX_UNINIT_KEY_SLOT_INT_EN &
			~MACSEC_RX_LKUP_MISS_INT_EN &
			~MACSEC_TX_UNINIT_KEY_SLOT_INT_EN &
			~MACSEC_TX_LKUP_MISS_INT_EN);
		osi_writela(osi_core, val, addr + MACSEC_COMMON_IMR);
		MACSEC_LOG("Write MACSEC_COMMON_IMR: 0x%x\n", val);
	}
}
#endif /* DEBUG_MACSEC */

/**
 * @brief macsec_initialize - Inititlizes macsec
 *
 * @note
 * Algorithm:
 *  - Configures mac IPG and MTL_EST with MACSEC enabled values
 *  - Sets the macsec MTU
 *  - If the mac type is eqos sets the Start of Transmission delays
 *  - Enables below interrupts
 *    - Tx/Rx Lookup miss
 *    - Validate frames to strict
 *    - Rx replay protection enable
 *    - Tx/Rx MTU check enable
 *    - Tx LUT priority to bypass lut
 *    - Enable Stats roll-over
 *    - Enables Tx interrupts
 *    - Enables Rx interrupts
 *    - Enables common interrupts
 *  - Clears all the luts, return -1 on failure
 *  - Sets the bypass lut, return -1 on failure
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] mtu: mtu to be programmed
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 for success
 * @retval -1 for failure
 */
static nve32_t macsec_initialize(struct osi_core_priv_data *const osi_core, nveu32_t mtu)
{
	nveu32_t val = 0;
#if defined(MACSEC_SUPPORT) && !defined(OSI_STRIPPED_LIB)
	const struct core_local *l_core = (void *)osi_core;
#endif
	nveu8_t *addr = (nveu8_t *)osi_core->macsec_base;
	nve32_t ret = 0;

#if defined(MACSEC_SUPPORT) && !defined(OSI_STRIPPED_LIB)
	/* Update MAC value as per macsec requirement */
	if (l_core->ops_p->macsec_config_mac != OSI_NULL) {
		l_core->ops_p->macsec_config_mac(osi_core, OSI_ENABLE);
	} else {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Failed to config mac per macsec\n", 0ULL);
	}
#endif
	/* Set MTU */
	ret = macsec_update_mtu(osi_core, mtu);
	if (ret < 0) {
		goto exit;
	}

	/* set TX/RX SOT, as SOT value different for eqos.
	 * default value matches for MGBE
	 */
	if (osi_core->mac == OSI_MAC_HW_EQOS) {
		val = osi_readla(osi_core, addr + MACSEC_TX_SOT_DELAY);
		MACSEC_LOG("Read MACSEC_TX_SOT_DELAY: 0x%x\n", val);
		val &= ~(SOT_LENGTH_MASK);
		val |= (EQOS_MACSEC_SOT_DELAY & SOT_LENGTH_MASK);
		MACSEC_LOG("Write MACSEC_TX_SOT_DELAY: 0x%x\n", val);
		osi_writela(osi_core, val, addr + MACSEC_TX_SOT_DELAY);

		val = osi_readla(osi_core, addr + MACSEC_RX_SOT_DELAY);
		MACSEC_LOG("Read MACSEC_RX_SOT_DELAY: 0x%x\n", val);
		val &= ~(SOT_LENGTH_MASK);
		val |= (EQOS_MACSEC_SOT_DELAY & SOT_LENGTH_MASK);
		MACSEC_LOG("Write MACSEC_RX_SOT_DELAY: 0x%x\n", val);
		osi_writela(osi_core, val, addr + MACSEC_RX_SOT_DELAY);
	}

	/* Set essential MACsec control configuration */
	val = osi_readla(osi_core, addr + MACSEC_CONTROL0);
	MACSEC_LOG("Read MACSEC_CONTROL0: 0x%x\n", val);
	val |= (MACSEC_TX_LKUP_MISS_NS_INTR | MACSEC_RX_LKUP_MISS_NS_INTR |
		MACSEC_TX_LKUP_MISS_BYPASS | MACSEC_RX_LKUP_MISS_BYPASS);
	val &= ~(MACSEC_VALIDATE_FRAMES_MASK);
	val |= MACSEC_VALIDATE_FRAMES_STRICT;
	val |= MACSEC_RX_REPLAY_PROT_EN;
	MACSEC_LOG("Write MACSEC_CONTROL0: 0x%x\n", val);
	osi_writela(osi_core, val, addr + MACSEC_CONTROL0);

	val = osi_readla(osi_core, addr + MACSEC_CONTROL1);
	MACSEC_LOG("Read MACSEC_CONTROL1: 0x%x\n", val);
	val |= (MACSEC_RX_MTU_CHECK_EN | MACSEC_TX_LUT_PRIO_BYP |
		MACSEC_TX_MTU_CHECK_EN);
	MACSEC_LOG("Write MACSEC_CONTROL1: 0x%x\n", val);
	osi_writela(osi_core, val, addr + MACSEC_CONTROL1);

	val = osi_readla(osi_core, addr + MACSEC_STATS_CONTROL_0);
	MACSEC_LOG("Read MACSEC_STATS_CONTROL_0: 0x%x\n", val);
	/* set STATS rollover bit */
	val |= MACSEC_STATS_CONTROL0_CNT_RL_OVR_CPY;
	MACSEC_LOG("Write MACSEC_STATS_CONTROL_0: 0x%x\n", val);
	osi_writela(osi_core, val, addr + MACSEC_STATS_CONTROL_0);

	/* Enable default HSI related interrupts needed */
	val = osi_readla(osi_core, addr + MACSEC_TX_IMR);
	MACSEC_LOG("Read MACSEC_TX_IMR: 0x%x\n", val);
	val |= MACSEC_TX_MAC_CRC_ERROR_INT_EN;
	MACSEC_LOG("Write MACSEC_TX_IMR: 0x%x\n", val);
	osi_writela(osi_core, val, addr + MACSEC_TX_IMR);

	/* set ICV error threshold to 1 */
	osi_writela(osi_core, 1U, addr + MACSEC_RX_ICV_ERR_CNTRL);
	/* Enabling interrupts only related to HSI */
	val = osi_readla(osi_core, addr + MACSEC_RX_IMR);
	MACSEC_LOG("Read MACSEC_RX_IMR: 0x%x\n", val);
	val |= (MACSEC_RX_ICV_ERROR_INT_EN |
		MACSEC_RX_MAC_CRC_ERROR_INT_EN);
	MACSEC_LOG("Write MACSEC_RX_IMR: 0x%x\n", val);
	osi_writela(osi_core, val, addr + MACSEC_RX_IMR);

	val = osi_readla(osi_core, addr + MACSEC_COMMON_IMR);
	val |= MACSEC_SECURE_REG_VIOL_INT_EN;
	osi_writela(osi_core, val, addr + MACSEC_COMMON_IMR);

	/* Set AES mode
	 * Default power on reset is AES-GCM128, leave it.
	 */

	/* Invalidate LUT entries */
	ret = clear_lut(osi_core);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			      "Invalidating all LUT's failed\n", (nveul64_t)ret);
		goto exit;
	}
	ret = set_byp_lut(osi_core);
exit:
	return ret;
}

/**
 * @brief find_existing_sc - Find the existing sc
 *
 * @note
 * Algorithm:
 *  - Compare the received sci with the existing sci and return sc if found
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] sc: Pointer to the sc which needs to be found
 * @param[in] ctlr: Controller to be selected
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval Pointer to sc on success
 * @retval NULL on failure
 */
static struct osi_macsec_sc_info *find_existing_sc(
				struct osi_core_priv_data *const osi_core,
				struct osi_macsec_sc_info *const sc,
				nveu16_t ctlr)
{
	struct osi_macsec_lut_status *lut_status_ptr =
					&osi_core->macsec_lut_status[ctlr];
	struct osi_macsec_sc_info *sc_found = OSI_NULL;
	nveu32_t i;

	for (i = 0; i < OSI_MAX_NUM_SC; i++) {
		if (osi_memcmp(lut_status_ptr->sc_info[i].sci, sc->sci,
			       (nve32_t)OSI_SCI_LEN) == OSI_NONE_SIGNED) {
			sc_found = &lut_status_ptr->sc_info[i];
		}
	}

	return sc_found;
}

/**
 * @brief get_avail_sc_idx - Find the available SC Index
 *
 * @note
 * Algorithm:
 *  - Return Index of the SC where valid an is 0
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] ctlr: Controller to be selected
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval index of the free sc
 */
static nveu32_t get_avail_sc_idx(const struct osi_core_priv_data *const osi_core,
				   nveu16_t ctlr)
{
	const struct osi_macsec_lut_status *lut_status_ptr =
					&osi_core->macsec_lut_status[ctlr];
	nveu32_t i;

	for (i = 0; i < OSI_MAX_NUM_SC; i++) {
		if (lut_status_ptr->sc_info[i].an_valid == OSI_NONE) {
			break;
		}
	}
	return i;
}

/**
 * @brief macsec_get_key_index - gets the key index for given sci
 *
 * @note
 * Algorithm:
 *  - Return -1 for invalid input arguments
 *  - Find the existing SC for a given sci
 *  - Derive the key index for the SC found
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] sci: Pointer of sci that needds to be found
 * @param[out] key_index: Pointer to the key index to be filled once SCI is found
 * @param[in] ctlr: Controller to be selected
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t macsec_get_key_index(struct osi_core_priv_data *const osi_core,
				nveu8_t *sci, nveu32_t *key_index, nveu16_t ctlr)
{
	struct osi_macsec_sc_info sc;
	const struct osi_macsec_sc_info *sc_info = OSI_NULL;
	nve32_t ret = 0;

	/* Validate inputs */
	if ((sci == OSI_NULL) || (key_index == OSI_NULL) ||
	    (ctlr > OSI_CTLR_SEL_MAX)) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Params validation failed\n", 0ULL);
		ret = -1;
		goto exit;
	}

	ret = osi_memcpy(sc.sci, sci, OSI_SCI_LEN);
	if (ret < OSI_NONE_SIGNED) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "memcpy failed\n", 0ULL);
		ret = -1;
		goto exit;
	}
	sc_info = find_existing_sc(osi_core, &sc, ctlr);
	if (sc_info == OSI_NULL) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "SCI Not found\n", 0ULL);
		ret = -1;
		goto exit;
	}

	*key_index = (sc_info->sc_idx_start * OSI_MAX_NUM_SA);
exit:
	return ret;
}

/**
 * @brief del_upd_sc - deletes or updates SC
 *
 * @note
 * Algorithm:
 *  - If the current SA of existing SC is same as passed SA
 *    - Clear the SCI LUT for the given SC
 *    - Clear the SC param LUT for the given SC
 *    - Clear the SC State LUT for the gien SC
 *  - Clear SA State LUT for the given SC
 *  - If key programming is enabled clear the key LUT for the given SC
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] existing_sc: Pointer to the existing sc
 * @param[in] sc: Pointer to the sc which need to be deleted or updated
 * @param[in] ctlr: Controller to be selected
 * @param[out] kt_idx: Key index to be passed to osd
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t del_upd_sc(struct osi_core_priv_data *const osi_core,
			  struct osi_macsec_sc_info *existing_sc,
			  const struct osi_macsec_sc_info *const sc,
			  nveu16_t ctlr, nveu16_t *kt_idx)
{
#ifdef MACSEC_KEY_PROGRAM
	struct osi_macsec_kt_config kt_config = {0};
#endif /* MACSEC_KEY_PROGRAM */
	struct osi_macsec_lut_config lut_config = {0};
	struct osi_macsec_table_config *table_config;
	nve32_t ret = 0;

	/* All input/output fields are already zero'd in declaration.
	 * Write all 0's to LUT index to clear everything
	 */
	table_config = &lut_config.table_config;
	table_config->ctlr_sel = ctlr;
	table_config->rw = OSI_LUT_WRITE;

	/* If curr_an of existing SC is same as AN being deleted, then remove
	 * SCI LUT entry as well. If not, it means some other AN is already
	 * enabled, so leave the SC configuration as is.
	 */
	if (existing_sc->curr_an == sc->curr_an) {
		/* 1. SCI LUT */
		lut_config.lut_sel = OSI_LUT_SEL_SCI;
		table_config->index = (nveu16_t)(existing_sc->sc_idx_start & 0xFFU);
		ret = macsec_lut_config(osi_core, &lut_config);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "Failed to del SCI LUT idx\n",
				     sc->sc_idx_start);
			ret = -1;
			goto exit;
		}

		/* 2. SC Param LUT */
		lut_config.lut_sel = OSI_LUT_SEL_SC_PARAM;
		ret = macsec_lut_config(osi_core, &lut_config);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "Failed to del SC param\n", (nveul64_t)ret);
			ret = -1;
			goto exit;
		}

		/* 3. SC state LUT */
		lut_config.lut_sel = OSI_LUT_SEL_SC_STATE;
		ret = macsec_lut_config(osi_core, &lut_config);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "Failed to del SC state\n", (nveul64_t)ret);
			ret = -1;
			goto exit;
		}
	}

	/* 4. SA State LUT */
	lut_config.lut_sel = OSI_LUT_SEL_SA_STATE;
	table_config->index = (nveu16_t)(((existing_sc->sc_idx_start * OSI_MAX_NUM_SA) +
			       sc->curr_an) & (0xFFU));
	ret = macsec_lut_config(osi_core, &lut_config);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Failed to del SA state\n", (nveul64_t)ret);
		ret = -1;
		goto exit;
	}

	/* Store key table index returned to osd */
	*kt_idx = (nveu16_t)(((existing_sc->sc_idx_start * OSI_MAX_NUM_SA) +
		   sc->curr_an) & (0xFFU));
#ifdef MACSEC_KEY_PROGRAM
	/* 5. Key LUT */
	table_config = &kt_config.table_config;
	table_config->ctlr_sel = ctlr;
	table_config->rw = OSI_LUT_WRITE;
	/* Each SC has OSI_MAX_NUM_SA's supported in HW */
	table_config->index = *kt_idx;
	ret = macsec_kt_config(osi_core, &kt_config);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Failed to del SAK\n", (nveul64_t)ret);
		ret = -1;
		goto exit;
	}
#endif /* MACSEC_KEY_PROGRAM */

	existing_sc->an_valid &= ~OSI_BIT(sc->curr_an);
exit:
	return ret;
}

/**
 * @brief print_error - Print error on failure
 *
 * @note
 * Algorithm:
 *  - Print error if there is a failure
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] ret: value to judge if there is a failure
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void print_error(const struct osi_core_priv_data *const osi_core,
			   nve32_t ret)
{
	(void) osi_core;
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Failed to config macsec\n", (nveul64_t)ret);
	}
}

/**
 * @brief copy_rev_order - Helper function to copy from one buffer to the other
 *
 * @note
 * Algorithm:
 *  - Copy from source buffer to dest buffer in reverse order
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[out] dst_buff: pointer to dest buffer
 * @param[in] src_buff: pointer to source buffer
 * @param[in] len: no. of bytes to be copied
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void copy_rev_order(nveu8_t *dst_buff, const nveu8_t *src_buff, nveu16_t len)
{
	nveu16_t i;

	/* Program in reverse order as per HW design */
	for (i = 0; i < len; i++) {
		dst_buff[i] = src_buff[len - 1U - i];
	}
}

/**
 * @brief add_upd_sc_err_cleanup - Helper function to handle error conditions in add_upd_sc
 *
 * @note
 * Algorithm:
 *  - Depending on the error_mask passed clear the LUTs
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] mask: Error mask that indicate which LUTs need to be cleared
 * @param[in] ctlr: Controller to be selected
 * @param[in] sc: Pointer to the SC that was intended to be added
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
static void add_upd_sc_err_cleanup(struct osi_core_priv_data *const osi_core,
				 nveu8_t mask, nveu16_t ctlr,
				 const struct osi_macsec_sc_info *const sc)
{
	struct osi_macsec_lut_config lut_config = {0};
	struct osi_macsec_table_config *table_config;
	nve32_t ret_fail = 0;
	nveu8_t error_mask = mask;

	if ((error_mask & OSI_BIT(3)) != OSI_NONE) {
		/* Cleanup SCI LUT */
		error_mask &= ((~OSI_BIT(3)) & (0xFFU));
		osi_memset(&lut_config, 0, sizeof(lut_config));
		table_config = &lut_config.table_config;
		table_config->ctlr_sel = ctlr;
		table_config->rw = OSI_LUT_WRITE;
		lut_config.lut_sel = OSI_LUT_SEL_SCI;
		table_config->index = (nveu16_t)(sc->sc_idx_start & 0xFFU);
		ret_fail = macsec_lut_config(osi_core, &lut_config);
		print_error(osi_core, ret_fail);
	}
	if ((error_mask & OSI_BIT(2)) != OSI_NONE) {
		/* cleanup SC param */
		error_mask &= ((~OSI_BIT(2)) & (0xFFU));
		osi_memset(&lut_config, 0, sizeof(lut_config));
		table_config = &lut_config.table_config;
		table_config->ctlr_sel = ctlr;
		lut_config.lut_sel = OSI_LUT_SEL_SC_PARAM;
		table_config->index = (nveu16_t)(sc->sc_idx_start & 0xFFU);
		ret_fail = macsec_lut_config(osi_core, &lut_config);
		print_error(osi_core, ret_fail);
	}
	if ((error_mask & OSI_BIT(1)) != OSI_NONE) {
		/* Cleanup SA state LUT */
		error_mask &= ((~OSI_BIT(1)) & (0xFFU));
		osi_memset(&lut_config, 0, sizeof(lut_config));
		table_config = &lut_config.table_config;
		table_config->ctlr_sel = ctlr;
		table_config->rw = OSI_LUT_WRITE;
		lut_config.lut_sel = OSI_LUT_SEL_SA_STATE;
		table_config->index = (nveu16_t)(((sc->sc_idx_start & 0xFU) *
				       OSI_MAX_NUM_SA) + sc->curr_an);
		ret_fail = macsec_lut_config(osi_core, &lut_config);
		print_error(osi_core, ret_fail);
	}
#ifdef MACSEC_KEY_PROGRAM
	if ((error_mask & OSI_BIT(0)) != OSI_NONE) {
		error_mask &= ((~OSI_BIT(0)) & (0xFFU));
		osi_memset(&kt_config, 0, sizeof(kt_config));
		table_config = &kt_config.table_config;
		table_config->ctlr_sel = ctlr;
		table_config->rw = OSI_LUT_WRITE;
		table_config->index = *kt_idx;
		ret_fail = macsec_kt_config(osi_core, &kt_config);
		print_error(osi_core, ret_fail);
	}
#endif /* MACSEC_KEY_PROGRAM */
}

/**
 * @brief add_upd_sc - add or update an SC
 *
 * @note
 * Algorithm:
 *  - If key programming is enabled, program the key if the command
 *    is to create SA
 *  - Create SA state lut
 *  - Create SC param lut
 *  - Create SCI lut
 *  - Create SC state lut if the command is to enable SA
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] sc: Pointer to the existing sc
 * @param[in] ctlr: Controller to be selected
 * @param[out] kt_idx: Key index to be passed to osd
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t add_upd_sc(struct osi_core_priv_data *const osi_core,
			  const struct osi_macsec_sc_info *const sc,
			  nveu16_t ctlr, nveu16_t *kt_idx)
{
	struct osi_macsec_lut_config lut_config = {0};
	struct osi_macsec_table_config *table_config;
	nve32_t ret = 0;
	nveu32_t i;
	nveu8_t error_mask = 0;
#ifdef MACSEC_KEY_PROGRAM
	struct osi_macsec_kt_config kt_config = {0};
#endif /* MACSEC_KEY_PROGRAM */

	/* Store key table index returned to osd */
	*kt_idx = (nveu16_t)(((sc->sc_idx_start & 0xFFU) * OSI_MAX_NUM_SA) + sc->curr_an);

#ifdef MACSEC_KEY_PROGRAM
	/* 1. Key LUT */
	if (sc->flags == OSI_CREATE_SA) {
		table_config = &kt_config.table_config;
		table_config->ctlr_sel = ctlr;
		table_config->rw = OSI_LUT_WRITE;
		/* Each SC has OSI_MAX_NUM_SA's supported in HW */
		table_config->index = *kt_idx;
		kt_config.flags |= OSI_LUT_FLAGS_ENTRY_VALID;

		copy_rev_order(kt_config.entry.sak, sc->sak, OSI_KEY_LEN_128);
		copy_rev_order(kt_config.entry.h, sc->hkey, OSI_KEY_LEN_128);

		ret = macsec_kt_config(osi_core, &kt_config);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "Failed to set SAK\n", (nveul64_t)ret);
			ret = -1;
			goto exit;
		}
	}
#endif /* MACSEC_KEY_PROGRAM */

	table_config = &lut_config.table_config;
	table_config->ctlr_sel = ctlr;
	table_config->rw = OSI_LUT_WRITE;

	/* 2. SA state LUT */
	lut_config.lut_sel = OSI_LUT_SEL_SA_STATE;
	table_config->index = (nveu16_t)(((sc->sc_idx_start & 0xFU) *
			       OSI_MAX_NUM_SA) + sc->curr_an);
	lut_config.sa_state_out.next_pn = sc->next_pn;
	lut_config.sa_state_out.lowest_pn = sc->lowest_pn;
	lut_config.flags |= OSI_LUT_FLAGS_ENTRY_VALID;
	ret = macsec_lut_config(osi_core, &lut_config);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Failed to set SA state\n", (nveul64_t)ret);
		error_mask |= OSI_BIT(0);
		goto exit;
	}

	/* 3. SC param LUT */
	lut_config.flags = OSI_NONE;
	lut_config.lut_sel = OSI_LUT_SEL_SC_PARAM;
	table_config->index = (nveu16_t)(sc->sc_idx_start & 0xFFU);
	copy_rev_order(lut_config.sc_param_out.sci, sc->sci, OSI_SCI_LEN);
	lut_config.sc_param_out.key_index_start =
					((sc->sc_idx_start & 0xFU) *
					OSI_MAX_NUM_SA);
	lut_config.sc_param_out.pn_max = OSI_PN_MAX_DEFAULT;
	lut_config.sc_param_out.pn_threshold = OSI_PN_THRESHOLD_DEFAULT;
	lut_config.sc_param_out.pn_window = sc->pn_window;
	lut_config.sc_param_out.tci = OSI_TCI_DEFAULT;
	lut_config.sc_param_out.vlan_in_clear = OSI_VLAN_IN_CLEAR_DEFAULT;
	ret = macsec_lut_config(osi_core, &lut_config);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Failed to set SC param\n", (nveul64_t)ret);
		error_mask |= OSI_BIT(1);
		goto exit;
	}

	/* 4. SCI LUT */
	lut_config.flags = OSI_NONE;
	lut_config.lut_sel = OSI_LUT_SEL_SCI;
	table_config->index = (nveu16_t)(sc->sc_idx_start);
	/* Extract the mac sa from the SCI itself */
	copy_rev_order(lut_config.lut_in.sa, sc->sci, OSI_ETH_ALEN);
	lut_config.flags |= OSI_LUT_FLAGS_SA_VALID;
	lut_config.sci_lut_out.sc_index = sc->sc_idx_start;
	for (i = 0; i < OSI_SCI_LEN; i++) {
		lut_config.sci_lut_out.sci[i] = sc->sci[OSI_SCI_LEN - 1U - i];
	}
	lut_config.sci_lut_out.an_valid = sc->an_valid;

	lut_config.flags |= OSI_LUT_FLAGS_ENTRY_VALID;
	ret = macsec_lut_config(osi_core, &lut_config);
	if (ret < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Failed to set SCI LUT\n", (nveul64_t)ret);
		error_mask |= OSI_BIT(2);
		goto exit;
	}

	if (sc->flags == OSI_ENABLE_SA) {
		/* 5. SC state LUT */
		lut_config.flags = OSI_NONE;
		lut_config.lut_sel = OSI_LUT_SEL_SC_STATE;
		table_config->index = (nveu16_t)(sc->sc_idx_start);
		lut_config.sc_state_out.curr_an = sc->curr_an;
		ret = macsec_lut_config(osi_core, &lut_config);
		if (ret < 0) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "Failed to set SC state\n", (nveul64_t)ret);
			error_mask |= OSI_BIT(3);
			goto exit;
		}
	}
exit:
	add_upd_sc_err_cleanup(osi_core, error_mask, ctlr, sc);
	return ret;
}

/**
 * @brief macsec_config_validate_inputs - Helper function to validate inputs
 *
 * @note
 * Algorithm:
 *  - Returns -1 if the validation fails else returns 0
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] enable: parameter to enable/disable
 * @param[in] ctlr: Parameter to indicate the controller
 * @param[in] kt_idx: Pointer to kt_index
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t macsec_config_validate_inputs(nveu32_t enable, nveu16_t ctlr,
					     const nveu16_t *kt_idx)
{
	nve32_t ret = 0;

	/* Validate inputs */
	if (((enable != OSI_ENABLE) && (enable != OSI_DISABLE)) ||
	    ((ctlr != OSI_CTLR_SEL_TX) && (ctlr != OSI_CTLR_SEL_RX)) ||
	    (kt_idx == OSI_NULL)) {
		ret = -1;
	}
	return ret;
}

/**
 * @brief memcpy_sci_sak_hkey - Helper function to copy SC params
 *
 * @note
 * Algorithm:
 *  - Copy SCI, sak and hkey from src to dst SC
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] src_sc: Pointer to source SC
 * @param[out] dst_sc: Pointer to dest SC
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t memcpy_sci_sak_hkey(struct osi_macsec_sc_info *dst_sc,
				   struct osi_macsec_sc_info *src_sc)
{
	nve32_t ret = 0;

	ret = osi_memcpy(dst_sc->sci, src_sc->sci, OSI_SCI_LEN);
	if (ret < OSI_NONE_SIGNED) {
		goto failure;
	}
	ret = osi_memcpy(dst_sc->sak, src_sc->sak, OSI_KEY_LEN_128);
	if (ret < OSI_NONE_SIGNED) {
		goto failure;
	}
#ifdef MACSEC_KEY_PROGRAM
	ret = osi_memcpy(dst_sc->hkey, src_sc->hkey, OSI_KEY_LEN_128);
	if (ret < OSI_NONE_SIGNED) {
		goto failure;
	}
#endif /* MACSEC_KEY_PROGRAM */

failure:
	return ret;

}

/**
 * @brief add_new_sc - Helper function to add new SC
 *
 * @note
 * Algorithm:
 *  - Return -1 if new SC cannot be added because of max check
 *  - Return -1 if there is no available lot for storing new SC
 *  - Copy all the SC reltated parameters
 *  - Add a new SC to the LUTs, if failed return -1
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] sc: Pointer to the sc that need to be added
 * @param[in] ctlr: Controller to be selected
 * @param[out] kt_idx: Key index to be passed to osd
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t add_new_sc(struct osi_core_priv_data *const osi_core,
			  struct osi_macsec_sc_info *const sc,
			  nveu16_t ctlr, nveu16_t *kt_idx)
{
	nve32_t ret = 0;
	struct osi_macsec_lut_status *lut_status_ptr;
	nveu32_t avail_sc_idx = 0;
	struct osi_macsec_sc_info *new_sc = OSI_NULL;

	lut_status_ptr = &osi_core->macsec_lut_status[ctlr];

	if (lut_status_ptr->num_of_sc_used >= OSI_MAX_NUM_SC) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
		  "Err: Reached max SC LUT entries!\n", 0ULL);
		ret = -1;
		goto exit;
	}

	avail_sc_idx = get_avail_sc_idx(osi_core, ctlr);
	if (avail_sc_idx == OSI_MAX_NUM_SC) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Err: NO free SC Index\n", 0ULL);
		ret = -1;
		goto exit;
	}
	new_sc = &lut_status_ptr->sc_info[avail_sc_idx];
	ret = memcpy_sci_sak_hkey(new_sc, sc);
	if (ret < OSI_NONE_SIGNED) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "memcpy Failed\n", 0ULL);
		ret = -1;
		goto exit;
	}
	new_sc->curr_an = sc->curr_an;
	new_sc->next_pn = sc->next_pn;
	new_sc->pn_window = sc->pn_window;
	new_sc->flags = sc->flags;

	new_sc->sc_idx_start = avail_sc_idx;
	new_sc->an_valid |= OSI_BIT((sc->curr_an & 0xFU));

	if (add_upd_sc(osi_core, new_sc, ctlr, kt_idx) !=
		       OSI_NONE_SIGNED) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "failed to add new SC\n", 0ULL);
		ret = -1;
		goto exit;
	} else {
		/* Update lut status */
		lut_status_ptr->num_of_sc_used++;
		MACSEC_LOG("%s: Added new SC ctlr: %u "
		       "Total active SCs: %u",
		       __func__, ctlr,
		       lut_status_ptr->num_of_sc_used);
	}
exit:
	return ret;
}

/**
 * @brief macsec_configure - API to update LUTs for addition/deletion of SC/SA
 *
 * @note
 * Algorithm:
 *  - Return -1 if inputs are invalid
 *  - Check if the Passed SC is already enabled
 *    - If not found
 *      - Add new SC if the request is to enable
 *      - Return failure if the request is to disable
 *    - If found
 *      - Update existing SC if request is to enable
 *      - Delete sc if the request is to disable
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 * @param[in] sc: Pointer to the sc that need to be added/deleted/updated
 * @param[in] enable: enable or disable
 * @param[in] ctlr: Controller to be selected
 * @param[out] kt_idx: Key index to be passed to osd
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
static nve32_t macsec_configure(struct osi_core_priv_data *const osi_core,
				struct osi_macsec_sc_info *const sc,
				nveu32_t enable, nveu16_t ctlr,
				nveu16_t *kt_idx)
{
	struct osi_macsec_sc_info *existing_sc = OSI_NULL;
	struct osi_macsec_sc_info tmp_sc;
	struct osi_macsec_sc_info *tmp_sc_p = &tmp_sc;
	struct osi_macsec_lut_status *lut_status_ptr;
	nve32_t ret = 0;

	if (macsec_config_validate_inputs(enable, ctlr, kt_idx) < 0) {
		OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
			     "Input validation failed\n", 0ULL);
		ret = -1;
		goto exit;
	}

	lut_status_ptr = &osi_core->macsec_lut_status[ctlr];
	/* 1. Find if SC is already existing in HW */
	existing_sc = find_existing_sc(osi_core, sc, ctlr);
	if (existing_sc == OSI_NULL) {
		if (enable == OSI_DISABLE) {
			OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
				     "trying to delete non-existing SC ?\n",
				     0ULL);
			ret = -1;
			goto exit;
		} else {
			MACSEC_LOG("%s: Adding new SC/SA: ctlr: %hu", __func__, ctlr);
			ret = add_new_sc(osi_core, sc, ctlr, kt_idx);
			goto exit;
		}
	} else {
		MACSEC_LOG("%s: Updating existing SC", __func__);
		if (enable == OSI_DISABLE) {
			MACSEC_LOG("%s: Deleting existing SA", __func__);
			if (del_upd_sc(osi_core, existing_sc, sc, ctlr, kt_idx) !=
			    OSI_NONE_SIGNED) {
				OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
					     "failed to del SA\n", 0ULL);
				ret = -1;
				goto exit;
			} else {
				if ((existing_sc->an_valid == OSI_NONE) &&
				    (lut_status_ptr->num_of_sc_used != OSI_NONE)) {
					lut_status_ptr->num_of_sc_used--;
					osi_memset(existing_sc, OSI_NONE,
						   sizeof(*existing_sc));
				}

				goto exit;
			}
		} else {
			/* Take backup copy.
			 * Don't directly commit SC changes until LUT's are
			 * programmed successfully
			 */
			*tmp_sc_p = *existing_sc;
			ret = memcpy_sci_sak_hkey(tmp_sc_p, sc);
			if (ret < OSI_NONE_SIGNED) {
				OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
					     "memcpy Failed\n", 0ULL);
				ret = -1;
				goto exit;
			}
			tmp_sc_p->curr_an = sc->curr_an;
			tmp_sc_p->next_pn = sc->next_pn;
			tmp_sc_p->pn_window = sc->pn_window;
			tmp_sc_p->flags = sc->flags;

			tmp_sc_p->an_valid |= OSI_BIT(sc->curr_an & 0x1FU);

			if (add_upd_sc(osi_core, tmp_sc_p, ctlr, kt_idx) !=
				       OSI_NONE_SIGNED) {
				OSI_CORE_ERR(osi_core->osd, OSI_LOG_ARG_HW_FAIL,
					     "failed to add new SA\n", 0ULL);
				ret = -1;
				goto exit;
			} else {
				MACSEC_LOG("%s: Updated new SC ctlr: %u "
				       "Total active SCs: %u",
				       __func__, ctlr,
				       lut_status_ptr->num_of_sc_used);
				/* Now commit the changes */
				*existing_sc = *tmp_sc_p;
			}
		}
	}
exit:
	return ret;
}

/**
 * @brief osi_init_macsec_ops - macsec initialize operations
 *
 * @note
 * Algorithm:
 *  - If virtualization is enabled initialize virt ops
 *  - Else
 *    - If macsec base is null return -1
 *    - initialize with macsec ops
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure. used param macsec_base
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_init_macsec_ops(struct osi_core_priv_data *const osi_core)
{
	static struct osi_macsec_core_ops virt_macsec_ops;
	nve32_t ret = 0;
	static struct osi_macsec_core_ops macsec_ops = {
		.init = macsec_initialize,
		.deinit = macsec_deinit,
		.handle_irq = macsec_handle_irq,
		.lut_config = macsec_lut_config,
#ifdef MACSEC_KEY_PROGRAM
		.kt_config = macsec_kt_config,
#endif /* MACSEC_KEY_PROGRAM */
		.cipher_config = macsec_cipher_config,
		.macsec_en = macsec_enable,
		.config = macsec_configure,
		.read_mmc = macsec_read_mmc,
		.get_sc_lut_key_index = macsec_get_key_index,
		.update_mtu = macsec_update_mtu,
#ifdef DEBUG_MACSEC
		.loopback_config = macsec_loopback_config,
		.dbg_buf_config = macsec_dbg_buf_config,
		.dbg_events_config = macsec_dbg_events_config,
		.intr_config = macsec_intr_config,
#endif
	};

	if (osi_core->use_virtualization == OSI_ENABLE) {
		osi_core->macsec_ops = &virt_macsec_ops;
		ivc_init_macsec_ops(osi_core->macsec_ops);
	} else {
		if (osi_core->macsec_base == OSI_NULL) {
			ret = -1;
			goto exit;
		}
		osi_core->macsec_ops = &macsec_ops;
	}
exit:
	return ret;
}

/**
 * @brief osi_macsec_init - Initialize the macsec controller
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Configure MTU, controller configs, interrupts, clear all LUT's and
 *    set BYP LUT entries for MKPDU and BC packets
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure.
 * @param[in] mtu: mtu to be programmed
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_init(struct osi_core_priv_data *const osi_core,
			nveu32_t mtu)
{
	nve32_t ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->init != OSI_NULL)) {
		ret = osi_core->macsec_ops->init(osi_core, mtu);
	}

	return ret;
}

/**
 * @brief osi_macsec_deinit - De-Initialize the macsec controller
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Resets macsec global data structured and restores the mac confirguration
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_deinit(struct osi_core_priv_data *const osi_core)
{
	nve32_t ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->deinit != OSI_NULL)) {
		ret = osi_core->macsec_ops->deinit(osi_core);
	}
	return ret;
}

/**
 * @brief osi_macsec_isr - macsec irq handler
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - handles macsec interrupts
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 */
void osi_macsec_isr(struct osi_core_priv_data *const osi_core)
{
	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->handle_irq != OSI_NULL)) {
		osi_core->macsec_ops->handle_irq(osi_core);
	}
}

/**
 * @brief osi_macsec_config_lut - Read or write to macsec LUTs
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Reads or writes to MACSEC LUTs
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[out] lut_config: Pointer to the lut configuration
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_config_lut(struct osi_core_priv_data *const osi_core,
			  struct osi_macsec_lut_config *const lut_config)
{
	nve32_t ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->lut_config != OSI_NULL)) {
		ret = osi_core->macsec_ops->lut_config(osi_core, lut_config);
	}

	return ret;
}

/**
 * @brief osi_macsec_get_sc_lut_key_index - API to get key index for a given SCI
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - gets the key index for the given sci
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] sci: Pointer to sci that needs to be found
 * @param[out] key_index: Pointer to key_index
 * @param[in] ctlr: macsec controller selected
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_get_sc_lut_key_index(struct osi_core_priv_data *const osi_core,
					nveu8_t *sci, nveu32_t *key_index,
					nveu16_t ctlr)
{
	nve32_t ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->get_sc_lut_key_index != OSI_NULL)) {
		ret = osi_core->macsec_ops->get_sc_lut_key_index(osi_core, sci, key_index,
								  ctlr);
	}

	return ret;
}

/**
 * @brief osi_macsec_update_mtu - Update the macsec mtu in run-time
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Updates the macsec mtu
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] mtu: mtu that needs to be programmed
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_update_mtu(struct osi_core_priv_data *const osi_core,
			      nveu32_t mtu)
{
	nve32_t ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->update_mtu != OSI_NULL)) {
		ret = osi_core->macsec_ops->update_mtu(osi_core, mtu);
	}

	return ret;
}

#ifdef MACSEC_KEY_PROGRAM
/**
 * @brief osi_macsec_config_kt - API to read or update the keys
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Read or write the keys
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] kt_config: Keys that needs to be programmed
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_config_kt(struct osi_core_priv_data *const osi_core,
			 struct osi_macsec_kt_config *const kt_config)
{
	nve32_t ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->kt_config != OSI_NULL) &&
	    (kt_config != OSI_NULL)) {
		ret = osi_core->macsec_ops->kt_config(osi_core, kt_config);
	}

	return ret;
}
#endif /* MACSEC_KEY_PROGRAM */

/**
 * @brief osi_macsec_cipher_config - API to update the cipher
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Updates cipher to use
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] cipher: Cipher suit to be used
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_cipher_config(struct osi_core_priv_data *const osi_core,
			      nveu32_t cipher)
{
	nve32_t ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->cipher_config != OSI_NULL)) {
		ret = osi_core->macsec_ops->cipher_config(osi_core, cipher);
	}

	return ret;
}

#ifdef DEBUG_MACSEC
/**
 * @brief osi_macsec_loopback - API to enable/disable macsec loopback
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Enables/disables macsec loopback
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] enable: parameter to enable or disable
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_loopback(struct osi_core_priv_data *const osi_core,
			nveu32_t enable)
{
	nve32_t ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->loopback_config != OSI_NULL)) {
		ret = osi_core->macsec_ops->loopback_config(osi_core, enable);
	}

	return ret;
}
#endif /* DEBUG_MACSEC */

/**
 * @brief osi_macsec_en - API to enable/disable macsec
 *
 * @note
 * Algorithm:
 *  - Return -1 if passed enable param is invalid
 *  - Return -1 if osi core or ops is null
 *  - Enables/disables macsec
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] enable: parameter to enable or disable
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_en(struct osi_core_priv_data *const osi_core,
		  nveu32_t enable)
{
	nve32_t ret = -1;

	if (((enable & OSI_MACSEC_TX_EN) != OSI_MACSEC_TX_EN) &&
	    ((enable & OSI_MACSEC_RX_EN) != OSI_MACSEC_RX_EN) &&
	    (enable != OSI_DISABLE)) {
		goto exit;
	}

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->macsec_en != OSI_NULL)) {
		ret = osi_core->macsec_ops->macsec_en(osi_core, enable);
	}
exit:
	return ret;
}

/**
 * @brief osi_macsec_config - Updates SC or SA in the macsec
 *
 * @note
 * Algorithm:
 *  - Return -1 if passed params are invalid
 *  - Return -1 if osi core or ops is null
 *  - Update/add/delete SC/SA
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] sc: Pointer to the sc that needs to be added/deleted/updated
 * @param[in] enable: enable or disable
 * @param[in] ctlr: Controller selected
 * @param[out] kt_idx: Pointer to the kt_index passed to OSD
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_config(struct osi_core_priv_data *const osi_core,
		      struct osi_macsec_sc_info *const sc,
		      nveu32_t enable, nveu16_t ctlr,
		      nveu16_t *kt_idx)
{
	nve32_t ret = -1;

	if (((enable != OSI_ENABLE) && (enable != OSI_DISABLE)) ||
	    (ctlr > OSI_CTLR_SEL_MAX) || (kt_idx == OSI_NULL)) {
		goto exit;
	}

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->config != OSI_NULL) && (sc != OSI_NULL)) {
		ret = osi_core->macsec_ops->config(osi_core, sc,
						    enable, ctlr, kt_idx);
	}
exit:
	return ret;
}

/**
 * @brief osi_macsec_read_mmc - Updates the mmc counters
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Updates the mcc counters in osi_core structure
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[out] osi_core: OSI core private data structure
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_read_mmc(struct osi_core_priv_data *const osi_core)
{
	nve32_t ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->read_mmc != OSI_NULL)) {
		osi_core->macsec_ops->read_mmc(osi_core);
		ret = 0;
	}
	return ret;
}

#ifdef DEBUG_MACSEC
/**
 * @brief osi_macsec_config_dbg_buf - Reads the debug buffer captured
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Reads the dbg buffers captured
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[out] dbg_buf_config: dbg buffer data captured
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_config_dbg_buf(
		struct osi_core_priv_data *const osi_core,
		struct osi_macsec_dbg_buf_config *const dbg_buf_config)
{
	nve32_t ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->dbg_buf_config != OSI_NULL)) {
		ret = osi_core->macsec_ops->dbg_buf_config(osi_core,
							dbg_buf_config);
	}

	return ret;
}

/**
 * @brief osi_macsec_dbg_events_config - Enables debug buffer events
 *
 * @note
 * Algorithm:
 *  - Return -1 if osi core or ops is null
 *  - Enables specific events to capture debug buffers
 *  - Refer to MACSEC column of <<******, (sequence diagram)>> for API details.
 *  - TraceID: ***********
 *
 * @param[in] osi_core: OSI core private data structure
 * @param[in] dbg_buf_config: dbg buffer data captured
 *
 * @pre MACSEC needs to be out of reset and proper clock configured.
 *
 * @note
 * API Group:
 * - Initialization: No
 * - Run time: Yes
 * - De-initialization: No
 *
 * @retval 0 on success
 * @retval -1 on failure
 */
nve32_t osi_macsec_dbg_events_config(
		struct osi_core_priv_data *const osi_core,
		struct osi_macsec_dbg_buf_config *const dbg_buf_config)
{
	nve32_t ret = -1;

	if ((osi_core != OSI_NULL) && (osi_core->macsec_ops != OSI_NULL) &&
	    (osi_core->macsec_ops->dbg_events_config != OSI_NULL)) {
		ret = osi_core->macsec_ops->dbg_events_config(osi_core,
							dbg_buf_config);
	}

	return ret;
}

#endif /* DEBUG_MACSEC */
#endif /* MACSEC_SUPPORT */
