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
#ifndef H_OAK
#define H_OAK

#include "oak_unimac.h" /* Include for relation to classifier oak_unimac */
#include "oak_net.h" /* Include for relation to classifier oak_net */
#include "oak_ethtool.h" /* Include for relation to classifier oak_ethtool */
#include "oak_debug.h"
#include "oak_module.h" /* Include for relation to classifier oak_module */
#include "oak_chksum.h"
#include "oak_dpm.h"

#define OAK_DRIVER_NAME "oak"

#define OAK_DRIVER_STRING "Marvell PCIe Switch Driver"

#define OAK_DRIVER_VERSION "0.03.0000"

#define OAK_DRIVER_COPYRIGHT "Copyright (c) Marvell - 2018"

#define OAK_MAX_JUMBO_FRAME_SIZE (10 * 1024)


int debug;
int txs;
int rxs;
int chan;
int rto;
int mhdr;
int port_speed;

/* Name      : get_msix_resources
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_get_msix_resources(struct pci_dev* pdev);

/* Name      : release_hardware
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_release_hardware(struct pci_dev* pdev);

/* Name      : init_hardware
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_init_hardware(struct pci_dev* pdev);

/* Name      : init_pci
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_init_pci(struct pci_dev* pdev);

/* Name      : init_software
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_init_software(struct pci_dev* pdev);

/* Name      : release_software
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_release_software(struct pci_dev* pdev);

/* Name      : start_hardware
 * Returns   : int
 * Parameters: 
 *  */
int oak_start_hardware(void);

/* Name      : start_software
 * Returns   : int
 * Parameters:  struct pci_dev * pdev
 *  */
int oak_start_software(struct pci_dev* pdev);

/* Name      : stop_hardware
 * Returns   : void
 * Parameters: 
 *  */
void oak_stop_hardware(void);

/* Name      : stop_software
 * Returns   : void
 * Parameters:  struct pci_dev * pdev
 *  */
void oak_stop_software(struct pci_dev* pdev);

#endif /* #ifndef H_OAK */

