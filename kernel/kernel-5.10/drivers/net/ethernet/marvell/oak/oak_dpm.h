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

#if CONFIG_PM
#ifndef H_OAK_DPM
#define H_OAK_DPM

#include <linux/pm_runtime.h>
#include "oak_unimac.h"

/* Name        : oak_dpm_create_sysfs
 * Returns     : void
 * Parameters  : oak_t *np
 * Description : This function creates sysfs entry for setting device power
 *               states D0, D1, D2 and D3.
 * */
void oak_dpm_create_sysfs(oak_t *np);

/* Name        : oak_dpm_remove_sysfs
 * Returns     : void
 * Parameters  : oak_t *np
 * Description : This function removes sysfs entry of device power states
 * */
void oak_dpm_remove_sysfs(oak_t *np);

#if CONFIG_PM_SLEEP

/* Name        : oak_dpm_suspend
 * Returns     : int
 * Parameters  : struct device* dev
 * Description : This function is called when system goes into suspend state
 *               It puts the device into sleep state
 * */
int __maybe_unused oak_dpm_suspend(struct device* dev);

/* Name        : oak_dpm_resume
 * Returns     : int
 * Parameters  : struct device* dev
 * Description : This function called when system goes into resume state and put
 *               the device into active state
 * */
int __maybe_unused oak_dpm_resume(struct device* dev);

/* Name        : oak_dpm_set_power_state
 * Returns     : void
 * Parameters  : struct device *dev, pci_power_t state
 * Description : This function set the device power state
 * */
void oak_dpm_set_power_state(struct device *dev, pci_power_t state);

#endif /* End of PM_SLEEP */
#endif /* End of H_OAK_DPM */
#endif /* End of PM */
