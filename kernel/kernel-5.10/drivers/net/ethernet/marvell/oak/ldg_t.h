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
#ifndef H_LDG_T
#define H_LDG_T

typedef struct ldg_tStruct
{
    struct napi_struct napi;
    struct oak_tStruct* device;
    uint64_t msi_tx;
    uint64_t msi_rx;
    uint64_t msi_te;
    uint64_t msi_re;
    uint64_t msi_ge;
    uint64_t irq_mask;
    uint32_t irq_first;
    uint32_t irq_count;
    uint32_t msi_grp;
    char msiname[32];
} ldg_t;

#endif /* #ifndef H_LDG_T */

