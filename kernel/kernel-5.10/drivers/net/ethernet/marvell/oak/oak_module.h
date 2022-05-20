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

static int	oak_init_module(void) ;
static void	oak_exit_module(void) ;

static int	oak_probe(struct pci_dev* pdev, const struct pci_device_id* dev_id);
static void	oak_remove(struct pci_dev* pdev);

module_init(oak_init_module);
module_exit(oak_exit_module);

extern int	debug ;
extern int	rxs ;		/* receive descriptor ring size as a power of two */
extern int	txs ;		/* transmit descriptor ring size as a power of two */
extern int	chan ;		/* number of channels */
extern int	rto ;		/* receive timeout in usecs */
extern int	mhdr ;		/* receive timeout in usecs */
extern int	port_speed ;

struct oak_tStruct ;

module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "OAK debug level");

/* RX and TX ring sizes are given as a power of 2 e.g:
 * x=[0-7] :: ring-size=2^(4+x), where x is the specified load parameter.
 */
module_param(rxs, int, 0);
MODULE_PARM_DESC(rxs, "Receive ring size");

module_param(txs, int, 0);
MODULE_PARM_DESC(txs, "Transmit ring size");

module_param(chan, int, 0);
MODULE_PARM_DESC(chan, "Number of (tx/rx) channels ");

module_param(rto, int, 0);
MODULE_PARM_DESC(rto, "Receive descriptor timeout in usec");

module_param(mhdr, int, 0);
MODULE_PARM_DESC(mhdr, "Marvell header generation");

module_param(port_speed, int, 0);
MODULE_PARM_DESC(mhdr, "Unimac 11 Port speed");

MODULE_LICENSE("GPL");


