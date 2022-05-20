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
#ifndef H_OAK_IRQ
#define H_OAK_IRQ

#include "oak_unimac.h" /* Include for relation to classifier oak_unimac */

extern int debug;
struct oak_tStruct;
/* Name      : request_ivec
 * Returns   : int
 * Parameters:  struct oak_tStruct * np
 *  */
int oak_irq_request_ivec(struct oak_tStruct* np);

/* Name      : callback
 * Returns   : irqreturn_t
 * Parameters:  int irq = irq,  void * cookie = cookie
 *  */
irqreturn_t oak_irq_callback(int irq, void* cookie);

/* Name      : request_single_ivec
 * Returns   : int
 * Parameters: struct oak_tStruct * np, ldg_t *ldg, uint64_t val,
 *	       uint32_t idx, int cpu
 *  */
int oak_irq_request_single_ivec(struct oak_tStruct *np, ldg_t *ldg,
				uint64_t val, uint32_t idx, int cpu);

/* Name      : release_ivec
 * Returns   : void
 * Parameters:  struct oak_tStruct * np
 *  */
void oak_irq_release_ivec(struct oak_tStruct* np);

/* Name      : enable_gicu_64
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = np,  uint64_t mask = mask
 *  */
void oak_irq_enable_gicu_64(struct oak_tStruct* np, uint64_t mask);

/* Name      : disable_gicu_64
 * Returns   : void
 * Parameters:  uint64_t mask,  struct oak_tStruct * np
 *  */
void oak_irq_disable_gicu_64(uint64_t mask, struct oak_tStruct* np);

/* Name      : dis_gicu
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = np,  uint32_t mask_0 = mask_0,  uint32_t mask_1 = mask_1
 *  */
void oak_irq_dis_gicu(struct oak_tStruct* np, uint32_t mask_0, uint32_t mask_1);

/* Name      : ena_gicu
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = p,  uint32_t mask_0 = mask_0,  uint32_t mask_1 = mask_1
 *  */
void oak_irq_ena_gicu(struct oak_tStruct* np, uint32_t mask_0, uint32_t mask_1);

/* Name      : ena_general
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = np,  uint32_t enable = enable
 *  */
void oak_irq_ena_general(struct oak_tStruct* np, uint32_t enable);

/* Name      : enable_groups
 * Returns   : int
 * Parameters:  struct oak_tStruct * np = np
 *  */
int oak_irq_enable_groups(struct oak_tStruct* np);

/* Name      : disable_groups
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = np
 *  */
void oak_irq_disable_groups(struct oak_tStruct* np);

#endif /* #ifndef H_OAK_IRQ */

