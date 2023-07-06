/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2021-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#ifndef _TEGRA_EPL_H_
#define _TEGRA_EPL_H_

#include <uapi/linux/tegra-epl.h>

/**
 * @brief Error report frame
 */
struct epl_error_report_frame {
	/* Error code indicates error reported by corresponding reporter_id */
	uint32_t error_code;

	/* Extra information for SEH to understand error */
	uint32_t error_attribute;

	/* LSB 32-bit TSC counter when error is detected */
	uint32_t timestamp;

	/* Indicates source of error */
	uint16_t reporter_id;
};

#ifdef CONFIG_TEGRA_EPL
/**
 * @brief API to check if SW error can be reported via Misc EC
 *        by reading and checking Misc EC error status register value.
 *
 * @param[in]   dev                     pointer to the device structure for the kernel driver
 *                                      from where API is called.
 * @param[in]   err_number              Generic SW error number for which status needs to
 *                                      enquired - [0 to 4].
 * @param[out]  status                  out param updated by API as follows:
 *                                      true - SW error can be reported
 *                                      false - SW error can not be reported because previous error
 *                                              is still active. Client needs to retry later.
 *
 * @returns
 *	0			(success)
 *	-EINVAL		(On invalid arguments)
 *	-ENODEV		(On device driver not loaded or Misc EC not configured)
 *	-EACCESS	(On client not allowed to report error via given Misc EC)
 *	-EAGAIN		(On Misc EC busy, client should retry)
 */
int epl_get_misc_ec_err_status(struct device *dev, uint8_t err_number, bool *status);


/**
 * @brief API to report SW error to FSI using Misc Generic SW error lines connected to
 *        the Misc error collator.
 *
 * @param[in]   dev                     pointer to the device structure for the kernel driver
 *                                      from where API is called.
 * @param[in]   err_number              Generic SW error number through which error
 *                                      needs to be reported.
 * @param[in]   sw_error_code           Client Defined Error Code, which will be
 *                                      forwarded to the application on FSI.
 *
 * @returns
 *	0			(success)
 *	-EINVAL		(On invalid arguments)
 *	-ENODEV		(On device driver not loaded or Misc EC not configured)
 *	-EACCESS	(On client not allowed to report error via given Misc EC)
 *	-EAGAIN		(On Misc EC busy, client should retry)
 */
int epl_report_misc_ec_error(struct device *dev, uint8_t err_number, uint32_t sw_error_code);

/**
 * @brief API to report SW error via TOP2 HSP
 *
 * @param[in]	error_report			Error frame to be reported
 *
 * @return
 *	0			(Success)
 *	-ENODEV		(On device driver not loaded or not configured)
 *	-ETIME		(On timeout)
 */
int epl_report_error(struct epl_error_report_frame error_report);
#else
static inline
int epl_get_misc_ec_err_status(struct device *dev, uint8_t err_number, bool *status)
{
	return -ENODEV;
}

static inline
int epl_report_misc_ec_error(struct device *dev, uint8_t err_number, uint32_t sw_error_code)
{
	return -ENODEV;
}

static inline
int epl_report_error(struct epl_error_report_frame error_report)
{
	return -ENODEV;
}
#endif /* CONFIG_TEGRA_EPL */

#endif /* _TEGRA_EPL_H_ */

