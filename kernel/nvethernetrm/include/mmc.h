/*
 * Copyright (c) 2018-2023, NVIDIA CORPORATION. All rights reserved.
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

#ifndef INCLUDED_MMC_H
#define INCLUDED_MMC_H

#include <nvethernet_type.h>
#include "osi_common.h"

#ifdef MACSEC_SUPPORT
/**
 * @brief The structure hold macsec statistics counters
 */
struct osi_macsec_mmc_counters {
	/** This counter provides the number of controller port macsec
	 * untaged packets */
	nveul64_t rx_pkts_no_tag;
	/** This counter provides the number of controller port macsec
	 * untaged packets validateFrame != strict */
	nveul64_t rx_pkts_untagged;
	/** This counter provides the number of invalid tag or icv packets */
	nveul64_t rx_pkts_bad_tag;
	/** This counter provides the number of no sc lookup hit or sc match
	 * packets */
	nveul64_t rx_pkts_no_sa_err;
	/** This counter provides the number of no sc lookup hit or sc match
	 * packets validateFrame != strict */
	nveul64_t rx_pkts_no_sa;
	/** This counter provides the number of late packets
	 *received PN < lowest PN */
	nveul64_t rx_pkts_late[OSI_MACSEC_SC_INDEX_MAX];
	/** This counter provides the number of overrun packets */
	nveul64_t rx_pkts_overrun;
	/** This counter provides the number of octets after IVC passing */
	nveul64_t rx_octets_validated;
	/** This counter provides the number not valid packets */
	nveul64_t rx_pkts_not_valid[OSI_MACSEC_SC_INDEX_MAX];
	/** This counter provides the number of invalid packets */
	nveul64_t in_pkts_invalid[OSI_MACSEC_SC_INDEX_MAX];
	/** This counter provides the number of in packet delayed */
	nveul64_t rx_pkts_delayed[OSI_MACSEC_SC_INDEX_MAX];
	/** This counter provides the number of in packets un checked */
	nveul64_t rx_pkts_unchecked[OSI_MACSEC_SC_INDEX_MAX];
	/** This counter provides the number of in packets ok */
	nveul64_t rx_pkts_ok[OSI_MACSEC_SC_INDEX_MAX];
	/** This counter provides the number of out packets untaged */
	nveul64_t tx_pkts_untaged;
	/** This counter provides the number of out too long */
	nveul64_t tx_pkts_too_long;
	/** This counter provides the number of out packets protected */
	nveul64_t tx_pkts_protected[OSI_MACSEC_SC_INDEX_MAX];
	/** This counter provides the number of out octets protected */
	nveul64_t tx_octets_protected;
};
#endif /* MACSEC_SUPPORT */
#endif /* INCLUDED_MMC_H */
