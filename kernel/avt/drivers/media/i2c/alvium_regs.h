/*=============================================================================
  Copyright (C) 2022 Allied Vision Technologies.  All Rights Reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

  -----------------------------------------------------------------------------

File:        alvium_regs.h

version:     1.8.0
=============================================================================*/

////////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////////
#ifndef ALVIUM_REGS_H
#define ALVIUM_REGS_H

// Version of the GenCP over CSI spec
#define GENCP_OVER_CCI_SPEC_VERSION_MAJOR           1
#define GENCP_OVER_CCI_SPEC_VERSION_MINOR           1
#define GENCP_OVER_CCI_SPEC_VERSION_PATCH           31

// Version of the BCRM spec
#define BCRM_SPEC_VERSION_MAJOR                     1
#define BCRM_SPEC_VERSION_MINOR                     1
#define BCRM_SPEC_VERSION_PATCH                     16

// CCI registers
#define CCI_REG_LAYOUT_VER_32R                      0x0000
#define CCI_DEVICE_CAP_64R                          0x0008
#define CCI_GCPRM_16R                               0x0010
#define CCI_BCRM_16R                                0x0014
#define CCI_DEVICE_GUID_512R                        0x0018
#define CCI_MANUF_NAME_512R                         0x0058
#define CCI_MODEL_NAME_512R                         0x0098
#define CCI_FAMILY_NAME_512R                        0x00D8
#define CCI_DEVICE_VERSION_512R                     0x0118
#define CCI_MANUF_INFO_512R                         0x0158
#define CCI_SERIAL_NUM_512R                         0x0198
#define CCI_USER_DEF_NAME_512R                      0x01D8
#define CCI_CHECKSUM_32R                            0x0218
#define CCI_CHANGE_MODE_8W                          0x021C
#define CCI_CURRENT_MODE_8R                         0x021D
#define CCI_SOFT_RESET_8W                           0x021E
#define CCI_HEARTBEAT_8RW                           0x021F
#define CCI_CAMERA_I2C_ADDRESS_8RW                  0x0220

// GCPRM register offsets
#define GCPRM_LAYOUT_VERSION_32R                    0x0000
#define GCPRM_GENCP_OUTBUF_ADDR_16R                 0x0004
#define GCPRM_GENCP_OUTBUF_SIZE_16R                 0x0008
#define GCPRM_GENCP_INBUF_ADDR_16R                  0x000C
#define GCPRM_GENCP_INBUF_SIZE_16R                  0x0010
#define GCPRM_GENCP_CHECKSUM_32R                    0x0014
#define GCPRM_GENCP_OUTHANDSHAKE_8RW                0x0018
#define GCPRM_GENCP_INHANDSHAKE_8RW                 0x001C
#define GCPRM_GENCP_OUT_SIZE_W16                    0x0020
#define GCPRM_GENCP_IN_SIZE_R16                     0x0024

// SBRM register offsets
#define SBRM_VERSION_32R                            0x0000
#define SBRM_GENCP_CCI_DEV_CAP_64R                  0x0004
#define SBRM_NUM_OF_STREAM_CHAN_32R                 0x000C
#define SBRM_SUPP_CSI2_LANE_COUNTS_8R               0x0010
#define SBRM_CSI2_LANE_COUNT_8RW                    0x0014
#define SBRM_MIN_SUPP_CSI2_CLK_FREQ_32R             0x0018
#define SBRM_MAX_SUPP_CSI2_CLK_FREQ_32R             0x001C
#define SBRM_CSI2_CLK_FREQ_32RW                     0x0020
#define SBRM_SIRM_ADDR_64R                          0x0024
#define SBRM_SIRM_LENGTH_32R                        0x002C

// SIRM register offsets
#define SIRM_STREAM_ENABLE_8RW                      0x0000
#define SIRM_LEADER_SIZE_32R                        0x0004
#define SIRM_PAYLOAD_SIZE_64R                       0x0008
#define SIRM_TRAILER_SIZE_32R                       0x0010
#define SIRM_CSI2_DATA_ID_INQ1_64R                  0x0014
#define SIRM_CSI2_DATA_ID_INQ2_64R                  0x001C
#define SIRM_CSI2_DATA_ID_INQ3_64R                  0x0024
#define SIRM_CSI2_DATA_ID_INQ4_64R                  0x002C
#define SIRM_CSI2_DATA_ID_8RW                       0x0034
#define SIRM_IPU_X_MIN_32W                          0x0038
#define SIRM_IPU_X_MAX_32W                          0x003C
#define SIRM_IPU_X_INC_32W                          0x0040
#define SIRM_IPU_Y_MIN_32W                          0x0044
#define SIRM_IPU_Y_MAX_32W                          0x0048
#define SIRM_IPU_Y_INC_32W                          0x004C
#define SIRM_IPU_X_32R                              0x0050
#define SIRM_IPU_Y_32R                              0x0054
#define SIRM_GENCP_IMAGE_SIZE_X_32R                 0x0058
#define SIRM_GENCP_IMAGE_SIZE_Y_32R                 0x005C
#define SIRM_GENCP_IMAGE_OFFSET_X_32R               0x0060
#define SIRM_GENCP_IMAGE_OFFSET_Y_32R               0x0064
#define SIRM_GENCP_IMAGE_PADDING_X_16R              0x0068
#define SIRM_GENCP_IMAGE_PIXEL_FORMAT_32R           0x006C
#define SIRM_GENCP_IMAGE_PAYLOAD_TYPE_16R           0x0070
#define SIRM_GENCP_VALID_PAYLOAD_SIZE_64R           0x0074
#define SIRM_GENCP_CHUNK_LAYOUT_ID_32R              0x007C

// BCRM register offsets
#define BCRM_VERSION_32R                            0x0000
#define BCRM_FEATURE_INQUIRY_64R                    0x0008
#define BCRM_DEVICE_FIRMWARE_VERSION_64R            0x0010
#define BCRM_WRITE_HANDSHAKE_8RW                    0x0018
#define BCRM_SUPPORTED_CSI2_LANE_COUNTS_8R          0x0040
#define BCRM_CSI2_LANE_COUNT_8RW                    0x0044
#define BCRM_CSI2_CLOCK_MIN_32R                     0x0048
#define BCRM_CSI2_CLOCK_MAX_32R                     0x004C
#define BCRM_CSI2_CLOCK_32RW                        0x0050
#define BCRM_BUFFER_SIZE_32R                        0x0054
#define BCRM_IPU_X_MIN_32W                          0x0058
#define BCRM_IPU_X_MAX_32W                          0x005C
#define BCRM_IPU_X_INC_32W                          0x0060
#define BCRM_IPU_Y_MIN_32W                          0x0064
#define BCRM_IPU_Y_MAX_32W                          0x0068
#define BCRM_IPU_Y_INC_32W                          0x006C
#define BCRM_IPU_X_32R                              0x0070
#define BCRM_IPU_Y_32R                              0x0074
#define BCRM_PHY_RESET_8RW                          0x0078
#define BCRM_ACQUISITION_START_8RW                  0x0080
#define BCRM_ACQUISITION_STOP_8RW                   0x0084
#define BCRM_ACQUISITION_ABORT_8RW                  0x0088
#define BCRM_ACQUISITION_STATUS_8R                  0x008C
#define BCRM_ACQUISITION_FRAME_RATE_64RW            0x0090
#define BCRM_ACQUISITION_FRAME_RATE_MIN_64R         0x0098
#define BCRM_ACQUISITION_FRAME_RATE_MAX_64R         0x00A0
#define BCRM_ACQUISITION_FRAME_RATE_INC_64R         0x00A8
#define BCRM_ACQUISITION_FRAME_RATE_ENABLE_8RW      0x00B0
#define BCRM_FRAME_START_TRIGGER_MODE_8RW           0x00B4
#define BCRM_FRAME_START_TRIGGER_SOURCE_8RW         0x00B8
#define BCRM_FRAME_START_TRIGGER_ACTIVATION_8RW     0x00BC
#define BCRM_FRAME_START_TRIGGER_SOFTWARE_8W        0x00C0
#define BCRM_FRAME_START_TRIGGER_DELAY_32RW         0x00C4
#define BCRM_EXPOSURE_ACTIVE_LINE_MODE_8RW          0x00C8
#define BCRM_EXPOSURE_ACTIVE_OUTPUT_LINE_8RW        0x00CC
#define BCRM_LINE_CONFIGURATION_32RW                0x00D0
#define BCRM_LINE_STATUS_8R                         0x00D4
#define BCRM_IMG_WIDTH_32RW                         0x0100
#define BCRM_IMG_WIDTH_MIN_32R                      0x0104
#define BCRM_IMG_WIDTH_MAX_32R                      0x0108
#define BCRM_IMG_WIDTH_INC_32R                      0x010C
#define BCRM_IMG_HEIGHT_32RW                        0x0110
#define BCRM_IMG_HEIGHT_MIN_32R                     0x0114
#define BCRM_IMG_HEIGHT_MAX_32R                     0x0118
#define BCRM_IMG_HEIGHT_INC_32R                     0x011C
#define BCRM_IMG_OFFSET_X_32RW                      0x0120
#define BCRM_IMG_OFFSET_X_MIN_32R                   0x0124
#define BCRM_IMG_OFFSET_X_MAX_32R                   0x0128
#define BCRM_IMG_OFFSET_X_INC_32R                   0x012C
#define BCRM_IMG_OFFSET_Y_32RW                      0x0130
#define BCRM_IMG_OFFSET_Y_MIN_32R                   0x0134
#define BCRM_IMG_OFFSET_Y_MAX_32R                   0x0138
#define BCRM_IMG_OFFSET_Y_INC_32R                   0x013C
#define BCRM_IMG_MIPI_DATA_FORMAT_32RW              0x0140
#define BCRM_IMG_AVAILABLE_MIPI_DATA_FORMATS_64R    0x0148
#define BCRM_IMG_BAYER_PATTERN_INQUIRY_8R           0x0150
#define BCRM_IMG_BAYER_PATTERN_8RW                  0x0154
#define BCRM_IMG_REVERSE_X_8RW                      0x0158
#define BCRM_IMG_REVERSE_Y_8RW                      0x015C
#define BCRM_SENSOR_WIDTH_32R                       0x0160
#define BCRM_SENSOR_HEIGHT_32R                      0x0164
#define BCRM_WIDTH_MAX_32R                          0x0168
#define BCRM_HEIGHT_MAX_32R                         0x016C
#define BCRM_DIGITAL_BINNIG_INQ_16R                 0x0170
#define BCRM_DIGITAL_BINNIG_SETTING_8RW             0x0174
#define BCRM_DIGITAL_BINNIG_MODE_8RW                0x0178
#define BCRM_EXPOSURE_TIME_64RW                     0x0180
#define BCRM_EXPOSURE_TIME_MIN_64R                  0x0188
#define BCRM_EXPOSURE_TIME_MAX_64R                  0x0190
#define BCRM_EXPOSURE_TIME_INC_64R                  0x0198
#define BCRM_EXPOSURE_AUTO_8RW                      0x01A0
#define BCRM_INTENSITY_AUTO_PRECEDENCE_8RW          0x01A4
#define BCRM_INTENSITY_AUTO_PRECEDENCE_VALUE_32RW   0x01A8
#define BCRM_INTENSITY_AUTO_PRECEDENCE_MIN_32R      0x01AC
#define BCRM_INTENSITY_AUTO_PRECEDENCE_MAX_32R      0x01B0
#define BCRM_INTENSITY_AUTO_PRECEDENCE_INC_32R      0x01B4
#define BCRM_BLACK_LEVEL_32RW                       0x01B8
#define BCRM_BLACK_LEVEL_MIN_32R                    0x01BC
#define BCRM_BLACK_LEVEL_MAX_32R                    0x01C0
#define BCRM_BLACK_LEVEL_INC_32R                    0x01C4
#define BCRM_GAIN_64RW                              0x01C8
#define BCRM_GAIN_MIN_64R                           0x01D0
#define BCRM_GAIN_MAX_64R                           0x01D8
#define BCRM_GAIN_INC_64R                           0x01E0
#define BCRM_GAIN_AUTO_8RW                          0x01E8
#define BCRM_GAMMA_64RW                             0x01F0
#define BCRM_GAMMA_MIN_64R                          0x01F8
#define BCRM_GAMMA_MAX_64R                          0x0200
#define BCRM_GAMMA_INC_64R                          0x0208
#define BCRM_CONTRAST_VALUE_32RW                    0x0214
#define BCRM_CONTRAST_VALUE_MIN_32R                 0x0218
#define BCRM_CONTRAST_VALUE_MAX_32R                 0x021C
#define BCRM_CONTRAST_VALUE_INC_32R                 0x0220
#define BCRM_SATURATION_32RW                        0x0240
#define BCRM_SATURATION_MIN_32R                     0x0244
#define BCRM_SATURATION_MAX_32R                     0x0248
#define BCRM_SATURATION_INC_32R                     0x024C
#define BCRM_HUE_32RW                               0x0250
#define BCRM_HUE_MIN_32R                            0x0254
#define BCRM_HUE_MAX_32R                            0x0258
#define BCRM_HUE_INC_32R                            0x025C
#define BCRM_RED_BALANCE_RATIO_64RW                 0x0280
#define BCRM_RED_BALANCE_RATIO_MIN_64R              0x0288
#define BCRM_RED_BALANCE_RATIO_MAX_64R              0x0290
#define BCRM_RED_BALANCE_RATIO_INC_64R              0x0298
#define BCRM_GREEN_BALANCE_RATIO_64RW               0x02A0
#define BCRM_GREEN_BALANCE_RATIO_MIN_64R            0x02A8
#define BCRM_GREEN_BALANCE_RATIO_MAX_64R            0x02B0
#define BCRM_GREEN_BALANCE_RATIO_INC_64R            0x02B8
#define BCRM_BLUE_BALANCE_RATIO_64RW                0x02C0
#define BCRM_BLUE_BALANCE_RATIO_MIN_64R             0x02C8
#define BCRM_BLUE_BALANCE_RATIO_MAX_64R             0x02D0
#define BCRM_BLUE_BALANCE_RATIO_INC_64R             0x02D8
#define BCRM_WHITE_BALANCE_AUTO_8RW                 0x02E0
#define BCRM_SHARPNESS_32RW                         0x0300
#define BCRM_SHARPNESS_MIN_32R                      0x0304
#define BCRM_SHARPNESS_MAX_32R                      0x0308
#define BCRM_SHARPNESS_INC_32R                      0x030C
#define BCRM_DEVICE_TEMPERATURE_32R                 0x0310
#define BCRM_EXPOSURE_AUTO_MIN_64RW                 0x0330
#define BCRM_EXPOSURE_AUTO_MAX_64RW                 0x0338
#define BCRM_GAIN_AUTO_MIN_64RW                     0x0340
#define BCRM_GAIN_AUTO_MAX_64RW                     0x0348
#define BCRM_AUTO_REGION_WIDTH_32RW                 0x0350
#define BCRM_AUTO_REGION_WIDTH_MIN_32R              0x0354
#define BCRM_AUTO_REGION_WIDTH_MAX_32R              0x0358
#define BCRM_AUTO_REGION_WIDTH_INC_32R              0x035C
#define BCRM_AUTO_REGION_HEIGHT_32RW                0x0360
#define BCRM_AUTO_REGION_HEIGHT_MIN_32R             0x0364
#define BCRM_AUTO_REGION_HEIGHT_MAX_32R             0x0368
#define BCRM_AUTO_REGION_HEIGHT_INC_32R             0x036C
#define BCRM_AUTO_REGION_OFFSET_X_32RW              0x0370
#define BCRM_AUTO_REGION_OFFSET_X_MIN_32R           0x0374
#define BCRM_AUTO_REGION_OFFSET_X_MAX_32R           0x0378
#define BCRM_AUTO_REGION_OFFSET_X_INC_32R           0x037C
#define BCRM_AUTO_REGION_OFFSET_Y_32RW              0x0380
#define BCRM_AUTO_REGION_OFFSET_Y_MIN_32R           0x0384
#define BCRM_AUTO_REGION_OFFSET_Y_MAX_32R           0x0388
#define BCRM_AUTO_REGION_OFFSET_Y_INC_32R           0x038C
#define _BCRM_LAST_ADDR                             BCRM_AUTO_REGION_OFFSET_Y_INC_32R

#define AV_CAM_REG_SIZE                 2
#define AV_CAM_DATA_SIZE_8              1
#define AV_CAM_DATA_SIZE_16             2
#define AV_CAM_DATA_SIZE_32             4
#define AV_CAM_DATA_SIZE_64             8


////////////////////////////////////////////////////////////////////////////////
// ENUMS
////////////////////////////////////////////////////////////////////////////////

/* BCRM_IMG_MIPI_DATA_FORMAT_32RW register values */
enum MIPI_DATA_FORMAT {
    MIPI_DATA_FORMAT_YUV_420_8_LEG      = 0x1A,
    MIPI_DATA_FORMAT_YUV_420_8          = 0x18,
    MIPI_DATA_FORMAT_YUV_420_10         = 0x19,
    MIPI_DATA_FORMAT_YUV_420_8_CSPS     = 0x1C,
    MIPI_DATA_FORMAT_YUV_420_10_CSPS    = 0x1D,
    MIPI_DATA_FORMAT_YUV_422_8          = 0x1E,
    MIPI_DATA_FORMAT_YUV_422_10         = 0x1F,
    MIPI_DATA_FORMAT_RGB888             = 0x24,
    MIPI_DATA_FORMAT_RGB666             = 0x23,
    MIPI_DATA_FORMAT_RGB565             = 0x22,
    MIPI_DATA_FORMAT_RGB555             = 0x21,
    MIPI_DATA_FORMAT_RGB444             = 0x20,
    MIPI_DATA_FORMAT_RAW6               = 0x28,
    MIPI_DATA_FORMAT_RAW7               = 0x29,
    MIPI_DATA_FORMAT_RAW8               = 0x2A,
    MIPI_DATA_FORMAT_RAW10              = 0x2B,
    MIPI_DATA_FORMAT_RAW12              = 0x2C,
    MIPI_DATA_FORMAT_RAW14              = 0x2D,
    MIPI_DATA_FORMAT_JPEG               = 0x30
};

/* BCRM_IMG_BAYER_PATTERN_8RW register values */
enum BAYER_PATTERN {
    BAYER_PATTERN_MONO          = 0,
    BAYER_PATTERN_GR            = 1,
    BAYER_PATTERN_RG            = 2,
    BAYER_PATTERN_GB            = 3,
    BAYER_PATTERN_BG            = 4
};

/* BCRM_IMG_REVERSE_X_8RW */
enum REVERSE_X {
    REVERSE_X_OFF               = 0,
    REVERSE_X_FLIP_H            = 1,
};

/* BCRM_IMG_REVERSE_X_8RW */
enum REVERSE_Y {
    REVERSE_Y_OFF               = 0,
    REVERSE_Y_FLIP_V            = 1,
};

/* BCRM_IMG_BAYER_PATTERN_8RW register values */
enum EXPOSURE_AUTO {
    EXPOSURE_AUTO_OFF           = 0,
    EXPOSURE_AUTO_ONCE          = 1,
    EXPOSURE_AUTO_CONTINUOUS    = 2
};

/* BCRM_GAIN_AUTO_8RW register values */
enum GAIN_AUTO {
    GAIN_AUTO_OFF               = 0,
    GAIN_AUTO_ONCE              = 1,
    GAIN_AUTO_CONTINUOUS        = 2
};

/* BCRM_WHITE_BALANCE_AUTO_8RW register values */
enum WHITEBALANCE_AUTO {
    WHITEBALANCE_AUTO_OFF           = 0,
    WHITEBALANCE_AUTO_ONCE          = 1,
    WHITEBALANCE_AUTO_CONTINUOUS    = 2
};

/* CCI_CHANGE_MODE_8W & CCI_CURRENT_MODE_8R */
enum OPERATION_MODE {
    OPERATION_MODE_BCRM     = 0,
    OPERATION_MODE_GENCP    = 1
};

/* BCRM_ACQUISITION_STATUS_8R register values */
enum ACQUISITION_STATUS {
    ACQUISITION_STATUS_STOPPED = 0,
    ACQUISITION_STATUS_RUNNING = 1
};

/* CCI Device capability String encoding */
enum CCI_STRING_ENC {
    CCI_STRING_ENC_ASCII    = 0,
    CCI_STRING_ENC_UTF8     = 1,
    CCI_STRING_ENC_UTF16    = 2
};

/* BCRM digital binning setting */
enum BCRM_DIGITAL_BINNING_SETTING {
    DIGITAL_BINNING_OFF = 0,
    DIGITAL_BINNING_2X2 = 1,
    DIGITAL_BINNING_3X3 = 2,
    DIGITAL_BINNING_4X4 = 3,
    DIGITAL_BINNING_5X5 = 4,
    DIGITAL_BINNING_6X6 = 5,
    DIGITAL_BINNING_7X7 = 6,
    DIGITAL_BINNING_8X8 = 7
};

/* BCRM digital binning mode */
enum BCRM_DIGITAL_BINNING_MODE {
    DIGITAL_BINNING_MODE_AVG = 0,
    DIGITAL_BINNING_MODE_SUM = 1
};


////////////////////////////////////////////////////////////////////////////////
// UNIONS
////////////////////////////////////////////////////////////////////////////////
/* CCI_DEVICE_CAP_64R regsiter values */
union cci_device_caps_reg {
	struct {
        unsigned long long user_name:1;
        unsigned long long bcrm:1;
        unsigned long long gencp:1;
        unsigned long long reserved:1;
        unsigned long long string_encoding:4;
        unsigned long long family_name:1;
        unsigned long long reserved2:55;
	} caps;

	unsigned long long value;
};

/* BCRM_VERSION_32R register values */
union bcrm_version_reg {
	struct {
        unsigned long minor:16;
        unsigned long major:16;
	} handshake;

	unsigned long value;
};

/* BCRM_DEVICE_FIRMWARE_VERSION_64R register values */
union bcrm_device_firmware_version_reg {
	struct {
        unsigned long long special:8;
        unsigned long long major:8;
        unsigned long long minor:16;
        unsigned long long patch:32;
	} handshake;

	unsigned long long value;
};

/* BCRM_WRITE_HANDSHAKE_8RW register values */
union bcrm_write_done_handshake_reg {
	struct {
        unsigned char finished:1;
        unsigned char reserved:6;
        unsigned char handshake_supported:1;
	} handshake;

	unsigned char value;
};

/* BCRM_FEATURE_INQUIRY_64R register values */
union bcrm_feature_reg {
	struct {
        unsigned long long reverse_x_avail:1;
        unsigned long long reverse_y_avail:1;
        unsigned long long intensity_auto_prcedence_avail:1;
        unsigned long long black_level_avail:1;
        unsigned long long gain_avail:1;
        unsigned long long gamma_avail:1;
        unsigned long long contrast_avail:1;
        unsigned long long saturation_avail:1;
        unsigned long long hue_avail:1;
        unsigned long long white_balance_avail:1;
        unsigned long long sharpness_avail:1;
        unsigned long long exposure_auto:1;
        unsigned long long gain_auto:1;
        unsigned long long white_balance_auto_avail:1;
        unsigned long long device_temperature_avail:1;
        unsigned long long acquisition_abort:1;
        unsigned long long acquisition_frame_rate:1;
        unsigned long long frame_trigger:1;
        unsigned long long exposure_active_line_avail:1;
        unsigned long long reserved:45;
	} feature_inq;

	unsigned long long value;
};

/* BCRM_SUPPORTED_CSI2_LANE_COUNTS_8R register values */
union bcrm_supported_lanecount_reg {
	struct {
        unsigned char one_lane_avail:1;
        unsigned char two_lane_avail:1;
        unsigned char three_lane_avail:1;
        unsigned char four_lane_avail:1;
        unsigned char reserved:4;
	} lane_count;

	unsigned char value;
};

/* BCRM_IMG_AVAILABLE_MIPI_DATA_FORMATS_64R register values */
union bcrm_avail_mipi_reg {
	struct {
        unsigned long long yuv420_8_leg_avail:1;
        unsigned long long yuv420_8_avail:1;
        unsigned long long yuv420_10_avail:1;
        unsigned long long yuv420_8_csps_avail:1;
        unsigned long long yuv420_10_csps_avail:1;
        unsigned long long yuv422_8_avail:1;
        unsigned long long yuv422_10_avail:1;
        unsigned long long rgb888_avail:1;
        unsigned long long rgb666_avail:1;
        unsigned long long rgb565_avail:1;
        unsigned long long rgb555_avail:1;
        unsigned long long rgb444_avail:1;
        unsigned long long raw6_avail:1;
        unsigned long long raw7_avail:1;
        unsigned long long raw8_avail:1;
        unsigned long long raw10_avail:1;
        unsigned long long raw12_avail:1;
        unsigned long long raw14_avail:1;
        unsigned long long jpeg_avail:1;
        unsigned long long reserved:45;
	} avail_mipi;

	unsigned long long value;
};

/* BCRM_IMG_BAYER_PATTERN_INQUIRY_8R register values */
union bcrm_bayer_inquiry_reg {
	struct {
        unsigned char monochrome_avail:1;
        unsigned char bayer_GR_avail:1;
        unsigned char bayer_RG_avail:1;
        unsigned char bayer_GB_avail:1;
        unsigned char bayer_BG_avail:1;
        unsigned char reserved:3;
	} bayer_pattern;

	unsigned char value;
};


union bcrm_digital_binning_inquiry_reg {
	struct {
        unsigned short int digital_binning_2x2:1;
        unsigned short int digital_binning_3x3:1;
        unsigned short int digital_binning_4x4:1;
        unsigned short int digital_binning_5x5:1;
        unsigned short int digital_binning_6x6:1;
        unsigned short int digital_binning_7x7:1;
        unsigned short int digital_binning_8x8:1;
        unsigned short int reserved:9;
	} digital_binning_inquiry;

	unsigned short int value;
};

#endif /* ALVIUM_REGS_H */
