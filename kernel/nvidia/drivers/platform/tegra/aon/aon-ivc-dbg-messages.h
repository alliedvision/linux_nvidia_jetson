/*
 * Copyright (c) 2015-2022, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AON_IVC_DBG_MESSAGES_H
#define AON_IVC_DBG_MESSAGES_H

#define AON_BOOT			0
#define AON_PING			1
#define AON_QUERY_TAG			2
#define AON_MODS_CASE			3
#define AON_MODS_RESULT			4
#define AON_MODS_CRC			5
#define AON_REQUEST_TYPE_MAX	5

#define AON_DBG_STATUS_OK		0
#define AON_DBG_STATUS_ERROR	1

#define ADCC_NCHANS 6

/**
 * @brief ping message request
 *
 * Structure that describes the ping message request.
 *
 * @challenge	Arbitrarily chosen value. Response to ping is
 *		computed based on this value.
 */
struct aon_ping_req {
	u32 challenge;
};

/**
 * @brief response to the ping request
 *
 * Structure that describes the response to ping request.
 *
 * @reply	Response to ping request with challenge left-shifted
 *		by 1 with carry-bit dropped.
 */
struct aon_ping_resp {
	u32 reply;
};

/**
 * @brief response to the query tag request
 *
 * This struct is used to extract the tag/firmware version of the AON.
 *
 * @tag		array to store tag information.
 */
struct aon_query_tag_resp {
	u8 tag[32];
};

/**
 * @brief mods adcc test request
 *
 * This struct is used to send the adcc configuration to perform the mods
 * adcc test on the target.
 *
 * @chans			adcc channels bit mask for the mods adcc tests
 * @sampling_dur	sampling duration
 * @avg_window		averaging window duration
 * @mode			single shot or continuous mode
 * @clk_src			ADCC clock source
 * @lb_data			ADCC channels loopback data
 */
struct aon_mods_adcc_req {
	u32 chans;
	u32 sampling_dur;
	u32 avg_window;
	u32 mode;
	u32 clk_src;
	u64 lb_data;
};

/**
 * @brief mods test request
 *
 * This struct is used to send the loop count to perform the mods test
 * on the target.
 *
 * @mods_case	mods test type: basic, mem2mem dma, io2mem dma
 * @loops	number of times mods test should be run
 * @dma_chans	dma channels bit mask for the mods dma tests
 * @adccs	mods adcc req config data
 */
struct aon_mods_req {
	u32 mods_case;
	u32 loops;
	u32 dma_chans;
	struct aon_mods_adcc_req adcc;
};

/**
 * @brief mods test adcc response
 *
 * This struct is used to fetch the adcc channels data.
 * Fields:
 *
 * @ch_data	array containing all the channels samples
 */
struct aon_mods_adcc_resp {
	uint32_t  ch_data[ADCC_NCHANS];
};

/**
 * @brief mods test crc response
 *
 * This struct is used to send the CRC32 of the AON text section to the target.
 * Fields:
 *
 * @crc		CRC32 of the text section.
 */
struct aon_mods_crc_resp {
	u32 crc;
};

/**
 * @brief aon dbg request
 *
 * This struct encapsulates the type of the request and the reaonctive
 * data associated with that request.
 *
 * @req_type	indicates the type of the request
 * @data	data needed to send for the request
 */
struct aon_dbg_request {
	u32 req_type;
	union {
		struct aon_ping_req ping_req;
		struct aon_mods_req mods_req;
	} data;
};

/**
 * @brief aon dbg response
 *
 * This struct encapsulates the type of the response and the reaonctive
 * data associated with that response.
 *
 * @resp_type	indicates the type of the response.
 * @status	response in regard to the request i.e success/failure.
 *		In case of mods, this field is the result.
 * @data	data associated with the response to a request.
 */
struct aon_dbg_response {
	u32 resp_type;
	u32 status;
	union {
		struct aon_ping_resp	  ping_resp;
		struct aon_query_tag_resp tag_resp;
		struct aon_mods_crc_resp  crc_resp;
		struct aon_mods_adcc_resp adcc_resp;
	} data;
};

#endif
