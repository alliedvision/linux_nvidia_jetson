/*=============================================================================
  Copyright (C) 2020 Allied Vision Technologies.  All Rights Reserved.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

  -----------------------------------------------------------------------------

File:        libcsi_ioctl.h

version:     1.7.10
=============================================================================*/



////////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////////
#ifndef LIBCSI_IOCTL_H
#define LIBCSI_IOCTL_H

#include <linux/videodev2.h>

/* Version of the libcsi - driver interface spec */
#define LIBCSI_DRV_SPEC_VERSION_MAJOR       1
#define LIBCSI_DRV_SPEC_VERSION_MINOR       0
#define LIBCSI_DRV_SPEC_VERSION_PATCH       8

/* Buffer status reported by driver for returned frames */
#define V4L2_BUF_FLAG_INCOMPLETE            0x10000000
#define V4L2_BUF_FLAG_UNUSED                0x20000000
#define V4L2_BUF_FLAG_VALID                 0x40000000
#define V4L2_BUF_FLAG_INVALID               0x80000000
#define V4L2_BUF_FLAG_INVALIDINCOMPLETE     (V4L2_BUF_FLAG_INCOMPLETE | V4L2_BUF_FLAG_INVALID)
/* Driver capabilities flags. See v4l2_csi_driver_info */
#define AVT_DRVCAP_USRPTR                   0x00000001  
#define AVT_DRVCAP_MMAP                     0x00000002

////////////////////////////////////////////////////////////////////////////////
// ENUMS
////////////////////////////////////////////////////////////////////////////////
enum v4l2_lane_counts
{
    V4L2_LANE_COUNT_1_LaneSupport       = 0x1,
    V4L2_LANE_COUNT_2_LaneSupport       = 0x2,
    V4L2_LANE_COUNT_3_LaneSupport       = 0x4,
    V4L2_LANE_COUNT_4_LaneSupport       = 0x8,
};

enum v4l2_statistics_capability
{
    V4L2_STATISTICS_CAPABILITY_FrameCount           = 0x1,
    V4L2_STATISTICS_CAPABILITY_PacketCRCError       = 0x2,
    V4L2_STATISTICS_CAPABILITY_FramesUnderrun       = 0x4,
    V4L2_STATISTICS_CAPABILITY_FramesIncomplete     = 0x8,
    V4L2_STATISTICS_CAPABILITY_CurrentFrameCount    = 0x10,
    V4L2_STATISTICS_CAPABILITY_CurrentFrameInterval = 0x20,
};

enum gencp_handshake_state
{
    GENCP_HANDSHAKE_BUFFER_CLEARED      = 0x0,
    GENCP_HANDSHAKE_BUFFER_VALID        = 0x1,
    GENCP_HANDSHAKE_BUFFER_PROCESSED    = 0x2,
};

enum manufacturer_id
{
    MANUFACTURER_ID_NXP                 = 0x00,
    MANUFACTURER_ID_NVIDIA              = 0x01,
};                                      
                                        
enum soc_family_id                      
{                                       
    SOC_FAMILY_ID_IMX6                  = 0x00,
    SOC_FAMILY_ID_TEGRA                 = 0x01,
    SOC_FAMILY_ID_IMX8                  = 0x02,
    SOC_FAMILY_ID_IMX8M                 = 0x03,
    SOC_FAMILY_ID_IMX8X                 = 0x04,
};                                      
                                        
enum imx6_driver_id                     
{                                       
    IMX6_DRIVER_ID_NITROGEN             = 0x00,
    IMX6_DRIVER_ID_WANDBOARD            = 0x01,
};                                      
                                        
enum tegra_driver_id                    
{                                       
    TEGRA_DRIVER_ID_DEFAULT             = 0x00,
};                                      
                                        
enum imx8_driver_id                     
{                                       
    IMX8_DRIVER_ID_DEFAULT              = 0x00,
};                                      
                                        
enum imx8m_driver_id                    
{                                       
    IMX8M_DRIVER_ID_DEFAULT             = 0x00,
};                                      
                                        
enum imx8x_driver_id                    
{                                       
    IMX8X_DRIVER_ID_DEFAULT             = 0x00,
};

enum v4l2_triggeractivation
{
    V4L2_TRIGGER_ACTIVATION_RISING_EDGE  = 0,
    V4L2_TRIGGER_ACTIVATION_FALLING_EDGE = 1,
    V4L2_TRIGGER_ACTIVATION_ANY_EDGE     = 2,
    V4L2_TRIGGER_ACTIVATION_LEVEL_HIGH   = 3,
    V4L2_TRIGGER_ACTIVATION_LEVEL_LOW    = 4
};

enum v4l2_triggersource
{
    V4L2_TRIGGER_SOURCE_SOFTWARE = 0,
    V4L2_TRIGGER_SOURCE_LINE0    = 1,
    V4L2_TRIGGER_SOURCE_LINE1    = 2,
    V4L2_TRIGGER_SOURCE_LINE2    = 3,
    V4L2_TRIGGER_SOURCE_LINE3    = 4,
};

////////////////////////////////////////////////////////////////////////////////
// STRUCTS
////////////////////////////////////////////////////////////////////////////////
struct v4l2_i2c
{
    __u32       register_address;       // Register
    __u32       timeout;                // Timeout value
    const char* ptr_buffer;             // I/O buffer
    __u32       register_size;          // Register address size (should be 2 for AVT Alvium 1500 and 1800)
    __u32       num_bytes;              // Bytes to read or write
};

struct v4l2_dma_mem
{
    __u32       index;                  // index of the buffer
    __u32       type;                   // enum v4l2_buf_type
    __u32       memory;                 // enum v4l2_memory
};

struct v4l2_statistics_capabilities
{
    __u64 statistics_capability;        // Bitmask with statistics capabilities enum (v4l2_statistics_capability)
};

struct v4l2_min_announced_frames
{
    __u32 min_announced_frames;         // Minimum number of announced frames
};

struct v4l2_range
{
    __u8  is_valid;                     // Indicates, if values are valid (1) or invalid (0)
    __u32 min;                          // Minimum allowed value
    __u32 max;                          // Maximum allowed value
};

struct v4l2_csi_host_clock_freq_ranges
{
    struct v4l2_range lane_range_1;     // Min and max value for 1 lane
    struct v4l2_range lane_range_2;     // Min and max value for 2 lanes
    struct v4l2_range lane_range_3;     // Min and max value for 3 lanes
    struct v4l2_range lane_range_4;     // Min and max value for 4 lanes
};

struct v4l2_supported_lane_counts
{
    __u32 supported_lane_counts;        // Bitfield with the supported lane counts from v4l2_lane_counts
};

struct v4l2_restriction
{
    __u8  is_valid;                     // Indicates, if values are valid (1) or invalid (0)
    __u32 min;                          // Minimum value
    __u32 max;                          // Maximum value
    __u32 inc;                          // Increment value
};

struct v4l2_ipu_restrictions
{
    struct v4l2_restriction ipu_x;      // X restriction
    struct v4l2_restriction ipu_y;      // Y restriction
};

struct v4l2_streamoff_ex
{
    __u32 timeout;                      // Timeout value in ms
};

struct v4l2_gencp_buffer_sizes
{
    __u32 gencp_in_buffer_size;         // Size in bytes of the GenCP In buffer
    __u32 gencp_out_buffer_size;        // Size in bytes of the GenCP Out buffer
};

struct v4l2_csi_data_identifiers_inq
{
    __u64 data_identifiers_inq_1;       // Inquiry for data identifiers 0-63
    __u64 data_identifiers_inq_2;       // Inquiry for data identifiers 64-127
    __u64 data_identifiers_inq_3;       // Inquiry for data identifiers 128-191
    __u64 data_identifiers_inq_4;       // Inquiry for data identifiers 192-255
};

struct v4l2_stats_t
{
    __u64 frames_count;                 // Total number of frames received
    __u64 packet_crc_error;             // Number of packets with CRC errors
    __u64 frames_underrun;              // Number of frames dropped because of buffer underrun
    __u64 frames_incomplete;            // Number of frames that were not completed
    __u64 current_frame_count;          // Number of frames received within CurrentFrameInterval (nec. to calculate fps value)
    __u64 current_frame_interval;       // Time interval between frames in µs
};

struct v4l2_csi_driver_info
{
    union _id
    {
        __u32 board_id;                 // 32 Bit board id
        struct 
        {
            __u8 manufacturer_id;       // 0x00 = Boundary Devices, 0x01= NVIDIA
            __u8 soc_family_id;         // 0x00 = i.MX6, 0x01=TEGRA, 0x02=i.MX8, 0x03=i.MX8X
            __u8 driver_id;             // Driver identifier for a certain soc family 
            __u8 reserved;              //
        };
    }id;
    __u32 driver_version;               // Driver version
    __u32 driver_interface_version;     // Used driver specification version
    __u32 driver_caps;                  // Driver capabilities flags
    __u32 usrptr_alignment;             // Buffer alignment for user pointer mode in bytes
};

struct v4l2_csi_config
{
    __u8  lane_count;                   // Number of lanes
    __u32 csi_clock;                    // CSI clock in Hz
};

struct v4l2_trigger_status
{
    __u8 trigger_source;                // v4l2_triggersource enum value    
    __u8 trigger_activation;            // v4l2_triggeractivation enum value  
    __u8 trigger_mode_enabled;          // Enable (1) or disable (0) trigger mode
};

struct v4l2_trigger_rate
{
	__u64 frames_per_period;		    // Number of frames per period
	__u64 period_sec;				    // Period in seconds
};

////////////////////////////////////////////////////////////////////////////////
// DEFINES
////////////////////////////////////////////////////////////////////////////////
// Custom ioctl definitions
/* i2c read */
#define VIDIOC_R_I2C                        _IOWR('V', BASE_VIDIOC_PRIVATE + 0, struct v4l2_i2c)

/* i2c write */
#define VIDIOC_W_I2C                        _IOWR('V', BASE_VIDIOC_PRIVATE + 1, struct v4l2_i2c)

/* Memory alloc for a frame */
#define VIDIOC_MEM_ALLOC                    _IOWR('V', BASE_VIDIOC_PRIVATE + 2, struct v4l2_dma_mem)

/* Memory free for a frame */
#define VIDIOC_MEM_FREE                     _IOWR('V', BASE_VIDIOC_PRIVATE + 3, struct v4l2_dma_mem)

/* Flush frames */
#define VIDIOC_FLUSH_FRAMES                 _IO('V', BASE_VIDIOC_PRIVATE + 4)

/* Stream statistics */
#define VIDIOC_STREAMSTAT                   _IOR('V', BASE_VIDIOC_PRIVATE + 5, struct v4l2_stats_t)

/* Reset Stream statistics */
#define VIDIOC_RESET_STREAMSTAT             _IO('V', BASE_VIDIOC_PRIVATE + 6)

/* Custom streamon */
#define VIDIOC_STREAMON_EX                  _IO('V', BASE_VIDIOC_PRIVATE + 7)

/* Custom streamoff */
#define VIDIOC_STREAMOFF_EX                 _IOW('V', BASE_VIDIOC_PRIVATE + 8, struct v4l2_streamoff_ex)

/* Get statistics capability */
#define VIDIOC_G_STATISTICS_CAPABILITIES    _IOR('V', BASE_VIDIOC_PRIVATE + 9, struct v4l2_statistics_capabilities)

/* Get min number of announced frames*/
#define VIDIOC_G_MIN_ANNOUNCED_FRAMES       _IOR('V', BASE_VIDIOC_PRIVATE + 10, struct v4l2_min_announced_frames)

/* Get supported lane value */
#define VIDIOC_G_SUPPORTED_LANE_COUNTS      _IOR('V', BASE_VIDIOC_PRIVATE + 11, struct v4l2_supported_lane_counts)

/* Get CSI Host clock frequencies */
#define VIDIOC_G_CSI_HOST_CLK_FREQ          _IOR('V', BASE_VIDIOC_PRIVATE + 12, struct v4l2_csi_host_clock_freq_ranges)

/* Get IPU restrictions */
#define VIDIOC_G_IPU_RESTRICTIONS           _IOR('V', BASE_VIDIOC_PRIVATE + 13, struct v4l2_ipu_restrictions)

/* Get GenCPIn and GenCPOut buffer sizes */
#define VIDIOC_G_GENCP_BUFFER_SIZES         _IOWR('V', BASE_VIDIOC_PRIVATE + 14, struct v4l2_gencp_buffer_sizes)

/* Retrieving the MIPI Data Identifier */
#define VIDIOC_G_SUPPORTED_DATA_IDENTIFIERS _IOWR('V', BASE_VIDIOC_PRIVATE + 15, struct v4l2_csi_data_identifiers_inq)

/* Retrieving i2c clock frequency */
#define VIDIOC_G_I2C_CLOCK_FREQ             _IOWR('V', BASE_VIDIOC_PRIVATE + 16, int)

/* Retrieving extended driver information */
#define VIDIOC_G_DRIVER_INFO                _IOR('V', BASE_VIDIOC_PRIVATE + 17, struct v4l2_csi_driver_info)

/* Get CSI configuration */
#define VIDIOC_G_CSI_CONFIG                 _IOR('V', BASE_VIDIOC_PRIVATE + 18, struct v4l2_csi_config)

/* Set CSI configuration */
#define VIDIOC_S_CSI_CONFIG                 _IOWR('V', BASE_VIDIOC_PRIVATE + 19, struct v4l2_csi_config)

/* Set the Trigger mode to OFF */
#define VIDIOC_TRIGGER_MODE_OFF             _IO('V', BASE_VIDIOC_PRIVATE + 20)

/* Set the Trigger mode to ON */
#define VIDIOC_TRIGGER_MODE_ON              _IO('V', BASE_VIDIOC_PRIVATE + 21)

/* Set the trigger activation */
#define VIDIOC_S_TRIGGER_ACTIVATION         _IOW('V', BASE_VIDIOC_PRIVATE + 22, int)

/* Get the trigger activation */
#define VIDIOC_G_TRIGGER_ACTIVATION         _IOR('V', BASE_VIDIOC_PRIVATE + 23, int)

/* Set the trigger source */
#define VIDIOC_S_TRIGGER_SOURCE             _IOW('V', BASE_VIDIOC_PRIVATE + 24, int)

/* Get the trigger source */
#define VIDIOC_G_TRIGGER_SOURCE             _IOR('V', BASE_VIDIOC_PRIVATE + 25, int)

/* Execute a software trigger */
#define VIDIOC_TRIGGER_SOFTWARE             _IO('V', BASE_VIDIOC_PRIVATE + 26)


#endif /* LIBCSI_IOCTL_H */
