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
#include "oak_irq.h"

extern int debug;
struct oak_tStruct;

/* Name      : request_ivec
 * Returns   : int
 * Parameters:  struct oak_tStruct * np
 *  */
int oak_irq_request_ivec(struct oak_tStruct* np)
{
    uint32_t i = 0;
    int err = 0;
    uint32_t num_chan_req;
    uint64_t val;
    int grp; /* automatically added for object flow handling */
    int rc_11; /* start of activity code */
	int cpu;

    oak_irq_dis_gicu(np, OAK_GICU_HOST_MASK_0, OAK_GICU_HOST_MASK_1 | OAK_GICU_HOST_MASK_E);

    if (np->num_rx_chan < np->num_tx_chan)
    {
        /* UserCode{EE733D23-9D9A-4547-8E3E-89AEB93BAE06}:16sPaM294b */
        num_chan_req = np->num_tx_chan;
        /* UserCode{EE733D23-9D9A-4547-8E3E-89AEB93BAE06} */
    }
    else
    {
        /* UserCode{FF60B0B8-C23D-4062-B655-D61B3B57C295}:6mU333V37H */
        num_chan_req = np->num_rx_chan;
        /* UserCode{FF60B0B8-C23D-4062-B655-D61B3B57C295} */
    }

    if (num_chan_req <= MAX_NUM_OF_CHANNELS)
    {
        while (i < np->gicu.num_ldg)
        {
            /* UserCode{80B8EB0D-7F55-45a6-8FFA-4F3B588872B2}:9XMwcPq1Ei */
            np->gicu.ldg[i].device = np;
            np->gicu.ldg[i].msi_grp = i;
            np->gicu.ldg[i].msi_tx = 0;
            np->gicu.ldg[i].msi_rx = 0;
            np->gicu.ldg[i].msi_te = 0;
            np->gicu.ldg[i].msi_re = 0;
            np->gicu.ldg[i].msi_ge = 0;
            np->gicu.ldg[i].msiname[0] = '\0';
            ++i;

            /* UserCode{80B8EB0D-7F55-45a6-8FFA-4F3B588872B2} */
        }

        /* UserCode{067B4C3F-F346-4e6b-80BD-591A4B7C0025}:1U9H6FxIzY */
        i = 0;
        grp = 0;
        val = (1ULL << TX_DMA_BIT);

        /* UserCode{067B4C3F-F346-4e6b-80BD-591A4B7C0025} */

        while (i < np->num_tx_chan)
        {
            /* UserCode{110E12D2-8E1C-4bcb-9A08-27914ACE4B68}:141AooKWDr */
            grp = (grp % np->gicu.num_ldg);
            np->gicu.ldg[grp].msi_tx |= val;
            val <<= 4ULL;
            ++grp;
            ++i;
            /* UserCode{110E12D2-8E1C-4bcb-9A08-27914ACE4B68} */
        }

        /* UserCode{6772385C-9756-431b-9CA9-197B67598BDD}:ic72aEIYlP */
        i = 0;
        val = (1ULL << RX_DMA_BIT);
        /* UserCode{6772385C-9756-431b-9CA9-197B67598BDD} */

        while (i < np->num_rx_chan)
        {
            /* UserCode{8A654077-5201-47d8-B598-674A4A199701}:W3a0L0822m */
            grp = (grp % np->gicu.num_ldg);
            np->gicu.ldg[grp].msi_rx |= val;
            val <<= 4ULL;
            ++grp;
            ++i;
            /* UserCode{8A654077-5201-47d8-B598-674A4A199701} */
        }

        /* UserCode{785ADF95-2539-48cd-AA30-3645D2D52EF4}:1AETWAHnL7 */
        i = 0;
        val = (1ULL << TX_ERR_BIT);
        /* UserCode{785ADF95-2539-48cd-AA30-3645D2D52EF4} */

        while (i < np->num_tx_chan)
        {
            /* UserCode{BF12AE4A-DCF5-4f90-B14C-ADEC7149FD17}:1GBOpf86PX */
            grp = (grp % np->gicu.num_ldg);
            np->gicu.ldg[grp].msi_te |= val;
            val <<= 4ULL;
            ++grp;
            ++i;
            /* UserCode{BF12AE4A-DCF5-4f90-B14C-ADEC7149FD17} */
        }

        /* UserCode{22217995-F210-4310-94C8-B700126F28FA}:KaupFbAfSo */
        i = 0;
        val = (1ULL << RX_ERR_BIT);
        /* UserCode{22217995-F210-4310-94C8-B700126F28FA} */

        while (i < np->num_rx_chan)
        {
            /* UserCode{DAE95490-65EB-4c04-8D82-FA318E5347D6}:8aaZbI7a2u */
            grp = (grp % np->gicu.num_ldg);
            np->gicu.ldg[grp].msi_re |= val;
            val <<= 4ULL;
            ++grp;
            ++i;
            /* UserCode{DAE95490-65EB-4c04-8D82-FA318E5347D6} */
        }

/* UserCode{2AD15B07-9300-4a7c-963F-EF2FEFEE9BBE}:63REqDuTGa */
#if 0
grp = np->gicu.num_ldg - 1 ;
val = OAK_GICU_HOST_UNIMAC_P11_IRQ ;
np->gicu.ldg[grp].msi_ge = (val << 32ULL);
#endif
        /* UserCode{2AD15B07-9300-4a7c-963F-EF2FEFEE9BBE} */
    }
    else
    {
        /* UserCode{EB92EEFF-0CE5-40f9-867E-B9D2E3878C89}:17P14WrLMp */
        err = -ENOMEM;
        /* UserCode{EB92EEFF-0CE5-40f9-867E-B9D2E3878C89} */
    }

    if (err == 0)
    {
        /* UserCode{FE12760C-75A6-412c-A465-5F49FA2EB58D}:7233eLMZuK */
        i = 0;
        /* UserCode{FE12760C-75A6-412c-A465-5F49FA2EB58D} */

	cpu = cpumask_first(cpu_online_mask);
        while ((i < np->gicu.num_ldg) && (err == 0))
        {
            /* UserCode{55502965-DB39-40d8-B819-97D92B40FCFB}:X7dOF5g3c6 */
            uint64_t val;
            ldg_t* p = &np->gicu.ldg[i];
            val = (p->msi_tx | p->msi_rx | p->msi_te | p->msi_re | p->msi_ge);
            /* UserCode{55502965-DB39-40d8-B819-97D92B40FCFB} */

            if (val != 0)
            {
		err = oak_irq_request_single_ivec(np, p, val, i, cpu);
		cpu = cpumask_next(cpu, cpu_online_mask);
		if (cpu >= nr_cpu_ids)
			cpu = cpumask_first(cpu_online_mask);

                if (err != 0)
                {
                    /* UserCode{A9543850-8017-4bf7-8464-E9B63524DA31}:1FeBWzfr6O */
                    p->msi_tx = 0;
                    p->msi_rx = 0;
                    p->msi_te = 0;
                    p->msi_re = 0;
                    p->msi_ge = 0;

                    /* UserCode{A9543850-8017-4bf7-8464-E9B63524DA31} */
                }
                else
                {
                }
            }
            else
            {
            }
            /* UserCode{50546313-91B2-434f-AD4E-EC92ED63787B}:pqeK47KwaC */
            ++i;
            /* UserCode{50546313-91B2-434f-AD4E-EC92ED63787B} */
        }
    }
    else
    {
    }

    if (err != 0)
    {
        oak_irq_release_ivec(np);
    }
    else
    {
    }
    /* UserCode{3A25A726-6007-4842-8EE4-BE9C5F34C2ED}:113B1K0otA */
    oakdbg(debug, INTR, "np=%p num_ldg=%d num_chan_req=%d err=%d", np, np->gicu.num_ldg, num_chan_req, err);
    /* UserCode{3A25A726-6007-4842-8EE4-BE9C5F34C2ED} */

    rc_11 = err;
    return rc_11;
}

/* Name      : callback
 * Returns   : irqreturn_t
 * Parameters:  int irq = irq,  void * cookie = cookie
 *  */
irqreturn_t oak_irq_callback(int irq, void* cookie)
{
    ldg_t* ldg = (ldg_t*)cookie; /* automatically added for object flow handling */
    irqreturn_t rc_4 = IRQ_HANDLED; /* start of activity code */
    oak_unimac_io_write_32(ldg->device, OAK_GICU_INTR_GRP_SET_MASK, ldg->msi_grp | OAK_GICU_INTR_GRP_MASK_ENABLE);
/* UserCode{306D26D3-69E7-4417-8D09-3820AA92313E}:g3TASHyjo9 */
#ifdef DEBUG
    {
        uint32_t mask_0;
        uint32_t mask_1;
        mask_0 = oak_unimac_io_read_32(ldg->device, OAK_GICU_INTR_FLAG_0);
        mask_1 = oak_unimac_io_read_32(ldg->device, OAK_GICU_INTR_FLAG_1);
        oakdbg(debug, INTR, "======= IRQ GRP %d [flag0=0x%0x flag1=0x%0x] ========", ldg->msi_grp, mask_0, mask_1);
    }
#endif

    /* UserCode{306D26D3-69E7-4417-8D09-3820AA92313E} */

    /* UserCode{284AD135-1E62-4ac5-B1E2-5723F01BB43A}:1548M1H9xQ */
    napi_schedule(&ldg->napi);
    /* UserCode{284AD135-1E62-4ac5-B1E2-5723F01BB43A} */

    /* UserCode{25E7A041-C143-4357-AA30-020560D94D29}:1XtUBYvo82 */
    oakdbg(debug, INTR, "==================== IRQ GRP END ====================");
    /* UserCode{25E7A041-C143-4357-AA30-020560D94D29} */

    return rc_4;
}

/* Name      : request_single_ivec
 * Returns   : int
 * Parameters: struct oak_tStruct *np, ldg_t *ldg, uint64_t val, uint32_t idx,
 *	       int cpu
 *  */
int oak_irq_request_single_ivec(struct oak_tStruct *np, ldg_t *ldg,
				uint64_t val, uint32_t idx, int cpu)
{
    int err = 0;
    const char* str = "xx"; /* automatically added for object flow handling */
    int err_1; /* start of activity code */

    if (val == ldg->msi_tx)
    {
        /* UserCode{7E6294ED-DC90-491a-8F0D-AE5132B58046}:HTEEhxLU0W */
        str = "tx";
        /* UserCode{7E6294ED-DC90-491a-8F0D-AE5132B58046} */
    }
    else
    {
    }

    if (val == ldg->msi_rx)
    {
        /* UserCode{54132504-C7C7-4715-850B-D3B86C07F76E}:1Kt1Wd5lmb */
        str = "rx";
        /* UserCode{54132504-C7C7-4715-850B-D3B86C07F76E} */
    }
    else
    {
    }

    if (val == ldg->msi_te)
    {
        /* UserCode{FA89FB7B-9CC8-42b0-8E47-31889C93E1DD}:1dPxluPKDb */
        str = "te";
        /* UserCode{FA89FB7B-9CC8-42b0-8E47-31889C93E1DD} */
    }
    else
    {
    }

    if (val == ldg->msi_re)
    {
        /* UserCode{19D118B4-1DBA-47a5-B5EF-B2093DE7A817}:1VZiB2bkoT */
        str = "re";
        /* UserCode{19D118B4-1DBA-47a5-B5EF-B2093DE7A817} */
    }
    else
    {
    }

    if (val == ldg->msi_ge)
    {
        /* UserCode{8820FEBF-999E-4eaf-A207-0B65A2E13436}:5BsGnDdw6b */
        str = "ge";
        /* UserCode{8820FEBF-999E-4eaf-A207-0B65A2E13436} */
    }
    else
    {
    }

    /* UserCode{AD709810-0D5D-4f5b-8505-9A44C38D0210}:1aUZIA6r5T */
    snprintf(np->gicu.ldg[idx].msiname, sizeof(np->gicu.ldg[idx].msiname) - 1, "%s-%s-%d", np->pdev->driver->name, str, idx);
#ifdef OAK_MSIX_LEGACY
	err = request_irq(np->gicu.msi_vec[idx].vector, oak_irq_callback, 0,
			  np->gicu.ldg[idx].msiname, &np->gicu.ldg[idx]);
	if (err == 0)
		err = irq_set_affinity_hint(np->gicu.msi_vec[idx].vector,
					    get_cpu_mask(cpu));
#else
	err = request_irq(pci_irq_vector(np->pdev, idx), oak_irq_callback, 0,
			  np->gicu.ldg[idx].msiname, &np->gicu.ldg[idx]);
	if (err == 0)
		err = irq_set_affinity_hint(pci_irq_vector(np->pdev, idx),
					    get_cpu_mask(cpu));
#endif

    err_1 = err;
    /* UserCode{344D1DD6-898F-4622-8A0A-8147D3F242C6}:e8WRwYQkXH */
    oakdbg(debug, INTR, "np=%p ivec[%2d]=%2d tx=0x%8llx rx=0x%8llx te=0x%8llx re=0x%8llx ge=%8llx type=%s err=%d",
           np, np->gicu.ldg[idx].msi_grp, np->gicu.msi_vec[idx].vector, ldg->msi_tx, ldg->msi_rx, ldg->msi_te, ldg->msi_re, ldg->msi_ge, str, err);
    /* UserCode{344D1DD6-898F-4622-8A0A-8147D3F242C6} */

    return err_1;
}

/* Name      : release_ivec
 * Returns   : void
 * Parameters:  struct oak_tStruct * np
 *  */
void oak_irq_release_ivec(struct oak_tStruct* np)
{
    /* start of activity code */
    /* UserCode{58CE32E5-1450-4c1a-A5C7-A1A17EE82034}:1Onip9RC6L */
    int i = 0;
    /* UserCode{58CE32E5-1450-4c1a-A5C7-A1A17EE82034} */

    while (i < np->gicu.num_ldg)
    {
        /* UserCode{932757BF-D646-4ffc-AFAE-477A38C1EE49}:KVr3fsDCcz */
        ldg_t* p = &np->gicu.ldg[i];
        /* UserCode{932757BF-D646-4ffc-AFAE-477A38C1EE49} */

        if (((p->msi_tx | p->msi_rx | p->msi_te | p->msi_re | p->msi_ge) != 0))
        {
#ifdef OAK_MSIX_LEGACY
		synchronize_irq(np->gicu.msi_vec[i].vector);
		irq_set_affinity_hint(np->gicu.msi_vec[i].vector, NULL);
		free_irq(np->gicu.msi_vec[i].vector, &np->gicu.ldg[i]);
#else
		synchronize_irq(pci_irq_vector(np->pdev, i));
		irq_set_affinity_hint(pci_irq_vector(np->pdev, i), NULL);
		free_irq(pci_irq_vector(np->pdev, i), &np->gicu.ldg[i]);
#endif
            p->msi_tx = 0;
            p->msi_rx = 0;
            p->msi_te = 0;
            p->msi_re = 0;
            p->msi_ge = 0;

            /* UserCode{E3C81644-90E9-4722-B823-4B9C7CA6E4CD} */
        }
        else
        {
        }
        /* UserCode{D872C9E0-8D98-444e-BC88-499457791192}:pqeK47KwaC */
        ++i;
        /* UserCode{D872C9E0-8D98-444e-BC88-499457791192} */
    }

    return;
}

/* Name      : enable_gicu_64
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = np,  uint64_t mask = mask
 *  */
void oak_irq_enable_gicu_64(struct oak_tStruct* np, uint64_t mask)
{
    /* start of activity code */
    /* UserCode{A8CEF42D-061C-4f70-88DC-1DEF17111356}:16Arom0SCY */
    uint32_t val_0 = (mask & OAK_GICU_HOST_MASK_0);
    uint32_t val_1 = ((mask >> 32) & OAK_GICU_HOST_MASK_1);
    /* UserCode{A8CEF42D-061C-4f70-88DC-1DEF17111356} */

    /* UserCode{4BC1F7E2-B6C8-4ff5-873A-3A07CEDA0862}:FX1hyte0Np */
    oakdbg(debug, INTR, "Enable IRQ mask %016llx", mask);
    /* UserCode{4BC1F7E2-B6C8-4ff5-873A-3A07CEDA0862} */

    oak_irq_ena_gicu(np, val_0, val_1);
    return;
}

/* Name      : disable_gicu_64
 * Returns   : void
 * Parameters:  uint64_t mask,  struct oak_tStruct * np
 *  */
void oak_irq_disable_gicu_64(uint64_t mask, struct oak_tStruct* np)
{
    /* start of activity code */
    /* UserCode{42A73E4B-AE9A-46c3-A487-270E32EEC3A3}:16Arom0SCY */
    uint32_t val_0 = (mask & OAK_GICU_HOST_MASK_0);
    uint32_t val_1 = ((mask >> 32) & OAK_GICU_HOST_MASK_1);
    /* UserCode{42A73E4B-AE9A-46c3-A487-270E32EEC3A3} */

    /* UserCode{2E5D9CE2-C745-4f60-AF25-CAD65A3751DF}:FX1hyte0Np */
    oakdbg(debug, INTR, "Enable IRQ mask %016llx", mask);
    /* UserCode{2E5D9CE2-C745-4f60-AF25-CAD65A3751DF} */

    oak_irq_dis_gicu(np, val_0, val_1);
    return;
}

/* Name      : dis_gicu
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = np,  uint32_t mask_0 = mask_0,  uint32_t mask_1 = mask_1
 *  */
void oak_irq_dis_gicu(struct oak_tStruct* np, uint32_t mask_0, uint32_t mask_1)
{
    /* start of activity code */
    oak_unimac_io_write_32(np, OAK_GICU_HOST_SET_MASK_0, mask_0);
    oak_unimac_io_write_32(np, OAK_GICU_HOST_SET_MASK_1, mask_1);
    return;
}

/* Name      : ena_gicu
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = p,  uint32_t mask_0 = mask_0,  uint32_t mask_1 = mask_1
 *  */
void oak_irq_ena_gicu(struct oak_tStruct* np, uint32_t mask_0, uint32_t mask_1)
{
    /* start of activity code */
    oak_unimac_io_write_32(np, OAK_GICU_HOST_CLR_MASK_0, mask_0);
    oak_unimac_io_write_32(np, OAK_GICU_HOST_CLR_MASK_1, mask_1);
    return;
}

/* Name      : ena_general
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = np,  uint32_t enable = enable
 *  */
void oak_irq_ena_general(struct oak_tStruct* np, uint32_t enable)
{
    /* start of activity code */

    if (enable != 0)
    {
        /* UserCode{0DF992A5-0D3D-49ea-89C9-C90D7116C59D}:1WhbGFtNjj */
        enable = (OAK_UNI_INTR_SEVERE_ERRORS);
        /* UserCode{0DF992A5-0D3D-49ea-89C9-C90D7116C59D} */
    }
    else
    {
    }
    oak_unimac_io_write_32(np, OAK_UNI_IMSK, enable);
    return;
}

/* Name      : enable_groups
 * Returns   : int
 * Parameters:  struct oak_tStruct * np = np
 *  */
int oak_irq_enable_groups(struct oak_tStruct* np)
{
    uint32_t grp = 0;
    uint32_t i;
    uint32_t irq;
    uint64_t irq_val; /* automatically added for object flow handling */
    int err_15 = 0; /* start of activity code */

    while (grp < np->gicu.num_ldg)
    {
        /* UserCode{02A532D8-DF1F-44b5-94E1-755FD064133E}:17ZyNkVzU0 */
        ldg_t* p = &np->gicu.ldg[grp];
        p->irq_mask = (p->msi_tx | p->msi_rx | p->msi_te | p->msi_re | p->msi_ge);
        p->irq_first = 0; /* first bit index from where to start IRQ processing */
        p->irq_count = 0; /* count of irq bits to process */
        irq_val = 1;
        irq = 0;

        /* UserCode{02A532D8-DF1F-44b5-94E1-755FD064133E} */

        while (irq < OAK_MAX_INTR_GRP)
        {

            if (p->irq_mask & irq_val)
            {

                if (p->irq_count == 0)
                {
                    /* UserCode{3F6E483F-72D6-45e9-A31B-85FED37C287A}:118YtXpaEr */
                    p->irq_first = irq;
                    /* UserCode{3F6E483F-72D6-45e9-A31B-85FED37C287A} */
                }
                else
                {
                }
                /* UserCode{36649A69-F30E-43a0-AABC-B15131C5C19B}:iK35K7Ilda */
                ++p->irq_count;

                /* UserCode{36649A69-F30E-43a0-AABC-B15131C5C19B} */

                oak_unimac_io_write_32(np, OAK_GICU_INTR_GRP_NUM(irq), grp);
                /* UserCode{227C1D60-B918-41c3-844E-90C4337FB7FD}:1J2BLQrCQz */
                oakdbg(debug, INTR, "Map IRQ bit %02d => group # %02d (1st=%2d of %2d)", irq, grp, p->irq_first, p->irq_count);
                /* UserCode{227C1D60-B918-41c3-844E-90C4337FB7FD} */
            }
            else
            {
            }
            /* UserCode{6D3D2346-4361-418f-A64D-A9E3E7F64B20}:RmDEQQdZea */
            irq_val <<= 1ULL;
            ++irq;
            /* UserCode{6D3D2346-4361-418f-A64D-A9E3E7F64B20} */
        }

        oak_irq_enable_gicu_64(np, p->irq_mask);
        /* UserCode{54BB4FB1-A3E0-4ba9-8874-C43B41B71134}:1SJ2aDhy3T */
        ++grp;
        /* UserCode{54BB4FB1-A3E0-4ba9-8874-C43B41B71134} */
    }

    oak_irq_ena_gicu(np, 0, OAK_GICU_HOST_UNIMAC_P11_IRQ);
    /* UserCode{A8922B46-7F78-44f2-9DA6-E327D8B20724}:7233eLMZuK */
    i = 0;
    /* UserCode{A8922B46-7F78-44f2-9DA6-E327D8B20724} */

    while (i < np->num_tx_chan)
    {
        oak_unimac_ena_tx_ring_irq(np, i, 1);
        /* UserCode{19D38A81-E031-4572-9CBF-7751C0A9B777}:pqeK47KwaC */
        ++i;
        /* UserCode{19D38A81-E031-4572-9CBF-7751C0A9B777} */
    }

    /* UserCode{75889B6D-8E7F-416a-9CA0-F810E44C272D}:7233eLMZuK */
    i = 0;
    /* UserCode{75889B6D-8E7F-416a-9CA0-F810E44C272D} */

    while (i < np->num_rx_chan)
    {
        oak_unimac_ena_rx_ring_irq(np, i, 1);
        /* UserCode{D5962F5A-507C-4b81-8300-5C6D030C4F71}:pqeK47KwaC */
        ++i;
        /* UserCode{D5962F5A-507C-4b81-8300-5C6D030C4F71} */
    }

    return err_15;
}

/* Name      : disable_groups
 * Returns   : void
 * Parameters:  struct oak_tStruct * np = np
 *  */
void oak_irq_disable_groups(struct oak_tStruct* np)
{
    uint32_t i; /* start of activity code */
    oak_irq_dis_gicu(np, OAK_GICU_HOST_MASK_0, OAK_GICU_HOST_MASK_1);
    /* UserCode{02212929-B0CC-4609-84EB-D3C31364EE7B}:7233eLMZuK */
    i = 0;
    /* UserCode{02212929-B0CC-4609-84EB-D3C31364EE7B} */

    while (i < np->num_rx_chan)
    {
        /* UserCode{EF8D37B4-AB81-4892-9367-8F3949931237}:GnqsL2KXje */
        oak_unimac_ena_rx_ring_irq(np, i, 0);
        /* UserCode{EF8D37B4-AB81-4892-9367-8F3949931237} */

        /* UserCode{24C79F91-EC57-49aa-ADE6-CB387F3E7F3F}:pqeK47KwaC */
        ++i;
        /* UserCode{24C79F91-EC57-49aa-ADE6-CB387F3E7F3F} */
    }

    /* UserCode{12DF6A0C-CBFD-45ab-BE13-41D3103FA497}:7233eLMZuK */
    i = 0;
    /* UserCode{12DF6A0C-CBFD-45ab-BE13-41D3103FA497} */

    while (i < np->num_tx_chan)
    {
        /* UserCode{9EF2D544-25A6-4878-91D4-6C09C7F8A14B}:1cqZeweEYL */
        oak_unimac_ena_tx_ring_irq(np, i, 0);
        /* UserCode{9EF2D544-25A6-4878-91D4-6C09C7F8A14B} */

        /* UserCode{72BBD698-FC5F-41d0-9552-D88293FF5A44}:pqeK47KwaC */
        ++i;
        /* UserCode{72BBD698-FC5F-41d0-9552-D88293FF5A44} */
    }

    return;
}

