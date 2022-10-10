/*
 * Allied Vision CSI2 Camera
 *
 * This program is free software; you may redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#ifndef __AVT_CSI2_H__
#define __AVT_CSI2_H__


#include <media/camera_common.h>
#include "alvium_regs.h"
#include "alvium_helper.h"

#define AVT_MAX_CTRLS 50
struct avt_frame_param {
    /* crop settings */
    struct v4l2_rect r;

    /* min/max/step values for frame size */
    uint32_t minh;
    uint32_t maxh;
    uint32_t sh;
    uint32_t minw;
    uint32_t maxw;
    uint32_t sw;
    uint32_t minhoff;
    uint32_t maxhoff;
    uint32_t shoff;
    uint32_t minwoff;
    uint32_t maxwoff;
    uint32_t swoff;
};

struct avt_binning_config {
	enum BCRM_DIGITAL_BINNING_SETTING setting;
	uint32_t width;
	uint32_t height;
};

enum avt_mode {
    AVT_BCRM_MODE,
    AVT_GENCP_MODE,
};
struct avt_csi2_priv {
    struct v4l2_subdev *subdev;
    struct media_pad pad;
    struct i2c_client *client;
    u32 mbus_fmt_code;

    struct v4l2_captureparm streamcap;
    struct v4l2_ctrl_handler hdl;
    struct camera_common_data *s_data;

    struct v4l2_ctrl_config ctrl_cfg[AVT_MAX_CTRLS];
    struct v4l2_ctrl *ctrls[AVT_MAX_CTRLS];

    bool stream_on;
    bool cross_update;
    bool write_handshake_available;
    bool stride_align_enabled;
    bool crop_align_enabled;
    bool trigger_mode;
    bool fallback_app_running;

    uint32_t csi_fixed_lanes;
    uint32_t csi_clk_freq;
    uint32_t host_csi_clk_freq;
    int numlanes;
    struct avt_frame_param frmp;

    struct cci_reg cci_reg;
    struct gencp_reg gencp_reg;

    enum avt_mode mode;

    int32_t *available_fmts;
    uint32_t available_fmts_cnt;


    struct task_struct *trig_thread;
    struct v4l2_trigger_rate *trigger_rate;

    int acquisition_active_invert;

    struct task_struct *value_update_thread;
    wait_queue_head_t value_update_wq;
    atomic_t force_value_update;
    int value_update_interval;

    bool ignore_control_write;

	struct avt_binning_config *available_binnings;
	uint32_t available_binnings_cnt;
	int cur_binning_config;

	s64 link_freqs[1];
};
struct avt_ctrl {
    __u32       id;
    __u32       value0;
    __u32       value1;
};

#define V4L2_AV_CSI2_BASE               0x1000
#define V4L2_AV_CSI2_WIDTH_R            (V4L2_AV_CSI2_BASE+0x0001)
#define V4L2_AV_CSI2_WIDTH_W            (V4L2_AV_CSI2_BASE+0x0002)
#define V4L2_AV_CSI2_WIDTH_MINVAL_R     (V4L2_AV_CSI2_BASE+0x0003)
#define V4L2_AV_CSI2_WIDTH_MAXVAL_R     (V4L2_AV_CSI2_BASE+0x0004)
#define V4L2_AV_CSI2_WIDTH_INCVAL_R     (V4L2_AV_CSI2_BASE+0x0005)
#define V4L2_AV_CSI2_HEIGHT_R           (V4L2_AV_CSI2_BASE+0x0006)
#define V4L2_AV_CSI2_HEIGHT_W           (V4L2_AV_CSI2_BASE+0x0007)
#define V4L2_AV_CSI2_HEIGHT_MINVAL_R    (V4L2_AV_CSI2_BASE+0x0008)
#define V4L2_AV_CSI2_HEIGHT_MAXVAL_R    (V4L2_AV_CSI2_BASE+0x0009)
#define V4L2_AV_CSI2_HEIGHT_INCVAL_R    (V4L2_AV_CSI2_BASE+0x000A)
#define V4L2_AV_CSI2_PIXELFORMAT_R      (V4L2_AV_CSI2_BASE+0x000B)
#define V4L2_AV_CSI2_PIXELFORMAT_W      (V4L2_AV_CSI2_BASE+0x000C)
#define V4L2_AV_CSI2_PALYLOADSIZE_R     (V4L2_AV_CSI2_BASE+0x000D)
#define V4L2_AV_CSI2_STREAMON_W         (V4L2_AV_CSI2_BASE+0x000E)
#define V4L2_AV_CSI2_STREAMOFF_W        (V4L2_AV_CSI2_BASE+0x000F)
#define V4L2_AV_CSI2_ABORT_W            (V4L2_AV_CSI2_BASE+0x0010)
#define V4L2_AV_CSI2_ACQ_STATUS_R       (V4L2_AV_CSI2_BASE+0x0011)
#define V4L2_AV_CSI2_HFLIP_R            (V4L2_AV_CSI2_BASE+0x0012)
#define V4L2_AV_CSI2_HFLIP_W            (V4L2_AV_CSI2_BASE+0x0013)
#define V4L2_AV_CSI2_VFLIP_R            (V4L2_AV_CSI2_BASE+0x0014)
#define V4L2_AV_CSI2_VFLIP_W            (V4L2_AV_CSI2_BASE+0x0015)
#define V4L2_AV_CSI2_OFFSET_X_W         (V4L2_AV_CSI2_BASE+0x0016)
#define V4L2_AV_CSI2_OFFSET_X_R         (V4L2_AV_CSI2_BASE+0x0017)
#define V4L2_AV_CSI2_OFFSET_X_MIN_R     (V4L2_AV_CSI2_BASE+0x0018)
#define V4L2_AV_CSI2_OFFSET_X_MAX_R     (V4L2_AV_CSI2_BASE+0x0019)
#define V4L2_AV_CSI2_OFFSET_X_INC_R     (V4L2_AV_CSI2_BASE+0x001A)
#define V4L2_AV_CSI2_OFFSET_Y_W         (V4L2_AV_CSI2_BASE+0x001B)
#define V4L2_AV_CSI2_OFFSET_Y_R         (V4L2_AV_CSI2_BASE+0x001C)
#define V4L2_AV_CSI2_OFFSET_Y_MIN_R     (V4L2_AV_CSI2_BASE+0x001D)
#define V4L2_AV_CSI2_OFFSET_Y_MAX_R     (V4L2_AV_CSI2_BASE+0x001E)
#define V4L2_AV_CSI2_OFFSET_Y_INC_R     (V4L2_AV_CSI2_BASE+0x001F)
#define V4L2_AV_CSI2_SENSOR_WIDTH_R     (V4L2_AV_CSI2_BASE+0x0020)
#define V4L2_AV_CSI2_SENSOR_HEIGHT_R    (V4L2_AV_CSI2_BASE+0x0021)
#define V4L2_AV_CSI2_MAX_WIDTH_R        (V4L2_AV_CSI2_BASE+0x0022)
#define V4L2_AV_CSI2_MAX_HEIGHT_R       (V4L2_AV_CSI2_BASE+0x0023)
#define V4L2_AV_CSI2_CURRENTMODE_R      (V4L2_AV_CSI2_BASE+0x0024)
#define V4L2_AV_CSI2_CHANGEMODE_W       (V4L2_AV_CSI2_BASE+0x0025)
#define V4L2_AV_CSI2_BAYER_PATTERN_R    (V4L2_AV_CSI2_BASE+0x0026)
#define V4L2_AV_CSI2_BAYER_PATTERN_W    (V4L2_AV_CSI2_BASE+0x0027)

/* Driver release version */
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

/* Driver release version */
#define DRV_VER_MAJOR           5
#define DRV_VER_MINOR           0
#define DRV_VER_PATCH           0
#define DRV_VER_BUILD           0
#define DRIVER_VERSION          STR(DRV_VER_MAJOR) "." STR(DRV_VER_MINOR) "." STR(DRV_VER_PATCH) "." STR(DRV_VER_BUILD)

#define BCRM_DEVICE_VERSION     0x00010000
#define BCRM_MAJOR_VERSION      0x0001
#define BCRM_MINOR_VERSION      0x0000

#define GCPRM_DEVICE_VERSION    0x00010000
#define GCPRM_MAJOR_VERSION     0x0001
#define GCPRM_MINOR_VERSION     0x0000

/* MIPI CSI-2 data types */
#define MIPI_DT_YUV420          0x18 /* YYY.../UYVY.... */
#define MIPI_DT_YUV420_LEGACY   0x1a /* UYY.../VYY...   */
#define MIPI_DT_YUV422          0x1e /* UYVY...         */
#define MIPI_DT_RGB444          0x20
#define MIPI_DT_RGB555          0x21
#define MIPI_DT_RGB565          0x22
#define MIPI_DT_RGB666          0x23
#define MIPI_DT_RGB888          0x24
#define MIPI_DT_RAW6            0x28
#define MIPI_DT_RAW7            0x29
#define MIPI_DT_RAW8            0x2a
#define MIPI_DT_RAW10           0x2b
#define MIPI_DT_RAW12           0x2c
#define MIPI_DT_RAW14           0x2d
#define MIPI_DT_CUSTOM          0x31

enum bayer_format {
    monochrome,/* 0 */
    bayer_gr,
    bayer_rg,
    bayer_gb,
    bayer_bg,
};
struct bcrm_to_v4l2 {
    int64_t min_bcrm;
    int64_t max_bcrm;
    int64_t step_bcrm;
    int32_t min_v4l2;
    int32_t max_v4l2;
    int32_t step_v4l2;
};

enum convert_type {
    min_enum,/* 0 */
    max_enum,
    step_enum,
};

#define CLEAR(x)        memset(&(x), 0, sizeof(x))

#define EXP_ABS         100000UL
#define UHZ_TO_HZ       1000000UL
#define FRAQ_NUM        1000

#define CCI_REG_LAYOUT_MINVER_MASK  (0x0000ffff)
#define CCI_REG_LAYOUT_MINVER_SHIFT (0)
#define CCI_REG_LAYOUT_MAJVER_MASK  (0xffff0000)
#define CCI_REG_LAYOUT_MAJVER_SHIFT (16)

#define CCI_REG_LAYOUT_MINVER   0
#define CCI_REG_LAYOUT_MAJVER   1

#define AV_ATTR_REVERSE_X                       {"Reverse X",                       0}
#define AV_ATTR_REVERSE_Y                       {"Reverse Y",                       1}
#define AV_ATTR_INTENSITY_AUTO                  {"Intensity Auto",                  2}
#define AV_ATTR_BRIGHTNESS                      {"Brightness",                      3}
/* Red & Blue balance features are enabled by default since it doesn't have
 * option in BCRM FEATURE REGISTER
 */
#define AV_ATTR_RED_BALANCE                     {"Red Balance",                     3}
#define AV_ATTR_BLUE_BALANCE                    {"Blue Balance",                    3}
#define AV_ATTR_GAIN                            {"Gain",                            4}
#define AV_ATTR_GAMMA                           {"Gamma",                           5}
#define AV_ATTR_CONTRAST                        {"Contrast",                        6}
#define AV_ATTR_SATURATION                      {"Saturation",                      7}
#define AV_ATTR_HUE                             {"Hue",                             8}
#define AV_ATTR_WHITEBALANCE                    {"White Balance",                   9}
#define AV_ATTR_SHARPNESS                       {"Sharpnesss",                      10}
#define AV_ATTR_EXPOSURE_AUTO                   {"Exposure Auto",                   11}
#define AV_ATTR_EXPOSURE_AUTO_MIN               {"Exposure Auto Min",               11}
#define AV_ATTR_EXPOSURE_AUTO_MAX               {"Exposure Auto Max",               11}
#define AV_ATTR_AUTOGAIN                        {"Gain Auto",                       12}
#define AV_ATTR_GAIN_AUTO_MIN                   {"Gain Auto Min",                   12}
#define AV_ATTR_GAIN_AUTO_MAX                   {"Gain Auto Max",                   12}
#define AV_ATTR_EXPOSURE                        {"Exposure",                        0}
#define AV_ATTR_EXPOSURE_ABSOLUTE               {"Exposure Absolute",               0}
#define AV_ATTR_WHITEBALANCE_AUTO               {"Auto White Balance",              13}
#define AV_ATTR_EXPOSURE_ACTIVE_LINE_MODE       {"Exposure Active Line Mode",       18}
#define AV_ATTR_EXPOSURE_ACTIVE_LINE_SELECTOR   {"Exposure Active Line Selector",   18}
#define AV_ATTR_EXPOSURE_ACTIVE_INVERT          {"Exposure Active Invert",          18}

#define AV_ATTR_TRIGGER_MODE                    {"Trigger Mode",                    17}
#define AV_ATTR_TRIGGER_ACTIVATION              {"Trigger Activation",              17}
#define AV_ATTR_TRIGGER_SOURCE                  {"Trigger Source",                  17}
#define AV_ATTR_TRIGGER_SOFTWARE                {"Trigger Software",                17}
#define AV_ATTR_DEVICE_TEMPERATURE              {"Device Temperature",              14}
#define AV_ATTR_BINNING_MODE             		{"Binning Mode",              14}

struct avt_ctrl_mapping {
    u8  reg_size;
    u8  data_size;
    u16 min_offset;
    u16 max_offset;
    u16 reg_offset;
    u16 step_offset;
    u32 id;
    u32 type;
    u32 flags;
    struct {
        s8  *name;
        u8  feature_avail;
    } attr;
    bool disabled_while_streaming : 1;
};

#define V4L2_CID_EXPOSURE_AUTO_MIN              (V4L2_CID_CAMERA_CLASS_BASE+40)
#define V4L2_CID_EXPOSURE_AUTO_MAX              (V4L2_CID_CAMERA_CLASS_BASE+41)
#define V4L2_CID_GAIN_AUTO_MIN                  (V4L2_CID_CAMERA_CLASS_BASE+42)
#define V4L2_CID_GAIN_AUTO_MAX                  (V4L2_CID_CAMERA_CLASS_BASE+43)
#define V4L2_CID_EXPOSURE_ACTIVE_LINE_MODE      (V4L2_CID_CAMERA_CLASS_BASE+44)
#define V4L2_CID_EXPOSURE_ACTIVE_LINE_SELECTOR  (V4L2_CID_CAMERA_CLASS_BASE+45)
#define V4L2_CID_EXPOSURE_ACTIVE_INVERT         (V4L2_CID_CAMERA_CLASS_BASE+46)

/* Trigger mode to ON/OFF */
#define V4L2_CID_TRIGGER_MODE                   (V4L2_CID_CAMERA_CLASS_BASE+47)

/* trigger activation: edge_rising, edge_falling, edge_any, level_high, level_low */
#define V4L2_CID_TRIGGER_ACTIVATION             (V4L2_CID_CAMERA_CLASS_BASE+48)

/* trigger source: software, gpio0, gpio1 */
#define V4L2_CID_TRIGGER_SOURCE                 (V4L2_CID_CAMERA_CLASS_BASE+49)

/* Execute a software trigger */
#define V4L2_CID_TRIGGER_SOFTWARE               (V4L2_CID_CAMERA_CLASS_BASE+50)

/* Camera temperature readout */
#define V4L2_CID_DEVICE_TEMPERATURE             (V4L2_CID_CAMERA_CLASS_BASE+51)

/* Binning mode: avg, sum */
#define V4L2_CID_BINNING_MODE             		(V4L2_CID_CAMERA_CLASS_BASE+52)


const struct avt_ctrl_mapping avt_ctrl_mappings[] = {
    {
        .id             = V4L2_CID_BRIGHTNESS,
        .attr           = AV_ATTR_BRIGHTNESS,
        .min_offset     = BCRM_BLACK_LEVEL_MIN_32R,
        .max_offset     = BCRM_BLACK_LEVEL_MAX_32R,
        .reg_offset     = BCRM_BLACK_LEVEL_32RW,
        .step_offset    = BCRM_BLACK_LEVEL_INC_32R,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_32,
        .type           = V4L2_CTRL_TYPE_INTEGER,
        .flags          = 0,
        .disabled_while_streaming = true,
    },
    {
        .id             = V4L2_CID_CONTRAST,
        .attr           = AV_ATTR_CONTRAST,
        .min_offset     = BCRM_CONTRAST_VALUE_MIN_32R,
        .max_offset     = BCRM_CONTRAST_VALUE_MAX_32R,
        .reg_offset     = BCRM_CONTRAST_VALUE_32RW,
        .step_offset    = BCRM_CONTRAST_VALUE_INC_32R,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_32,
        .type           = V4L2_CTRL_TYPE_INTEGER,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_SATURATION,
        .attr           = AV_ATTR_SATURATION,
        .min_offset     = BCRM_SATURATION_MIN_32R,
        .max_offset     = BCRM_SATURATION_MAX_32R,
        .reg_offset     = BCRM_SATURATION_32RW,
        .step_offset    = BCRM_SATURATION_INC_32R,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_32,
        .type           = V4L2_CTRL_TYPE_INTEGER,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_HUE,
        .attr           = AV_ATTR_HUE,
        .min_offset     = BCRM_HUE_MIN_32R,
        .max_offset     = BCRM_HUE_MAX_32R,
        .reg_offset     = BCRM_HUE_32RW,
        .step_offset    = BCRM_HUE_INC_32R,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_32,
        .type           = V4L2_CTRL_TYPE_INTEGER,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_AUTO_WHITE_BALANCE,
        .attr           = AV_ATTR_WHITEBALANCE_AUTO,
        .reg_offset     = BCRM_WHITE_BALANCE_AUTO_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_BOOLEAN,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_DO_WHITE_BALANCE,
        .attr           = AV_ATTR_WHITEBALANCE,
        .reg_offset     = BCRM_WHITE_BALANCE_AUTO_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_BUTTON,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_RED_BALANCE,
        .attr           = AV_ATTR_RED_BALANCE,
        .min_offset     = BCRM_RED_BALANCE_RATIO_MIN_64R,
        .max_offset     = BCRM_RED_BALANCE_RATIO_MAX_64R,
        .reg_offset     = BCRM_RED_BALANCE_RATIO_64RW,
        .step_offset    = BCRM_RED_BALANCE_RATIO_INC_64R,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_64,
        .type           = V4L2_CTRL_TYPE_INTEGER64,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_BLUE_BALANCE,
        .attr           = AV_ATTR_BLUE_BALANCE,
        .min_offset     = BCRM_BLUE_BALANCE_RATIO_MIN_64R,
        .max_offset     = BCRM_BLUE_BALANCE_RATIO_MAX_64R,
        .reg_offset     = BCRM_BLUE_BALANCE_RATIO_64RW,
        .step_offset    = BCRM_BLUE_BALANCE_RATIO_INC_64R,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_64,
        .type           = V4L2_CTRL_TYPE_INTEGER64,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_GAMMA,
        .attr           = AV_ATTR_GAMMA,
        .min_offset     = BCRM_GAMMA_MIN_64R,
        .max_offset     = BCRM_GAMMA_MAX_64R,
        .reg_offset     = BCRM_GAMMA_64RW,
        .step_offset    = BCRM_GAMMA_INC_64R,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_64,
        .type           = V4L2_CTRL_TYPE_INTEGER64,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_EXPOSURE_ABSOLUTE,
        .attr           = AV_ATTR_EXPOSURE_ABSOLUTE,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_32,
        .type           = V4L2_CTRL_TYPE_INTEGER,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_EXPOSURE,
        .attr           = AV_ATTR_EXPOSURE,
        .min_offset     = BCRM_EXPOSURE_TIME_MIN_64R,
        .max_offset     = BCRM_EXPOSURE_TIME_MAX_64R,
        .reg_offset     = BCRM_EXPOSURE_TIME_64RW,
        .step_offset    = BCRM_EXPOSURE_TIME_INC_64R,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_64,
        .type           = V4L2_CTRL_TYPE_INTEGER64,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_AUTOGAIN,
        .attr           = AV_ATTR_AUTOGAIN,
        .reg_offset     = BCRM_GAIN_AUTO_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_BOOLEAN,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_GAIN,
        .attr           = AV_ATTR_GAIN,
        .min_offset     = BCRM_GAIN_MIN_64R,
        .max_offset     = BCRM_GAIN_MAX_64R,
        .reg_offset     = BCRM_GAIN_64RW,
        .step_offset    = BCRM_GAIN_INC_64R,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_64,
        .type           = V4L2_CTRL_TYPE_INTEGER64,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_HFLIP,
        .attr           = AV_ATTR_REVERSE_X,
        .reg_offset     = BCRM_IMG_REVERSE_X_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_BOOLEAN,
        .flags          = 0,
        .disabled_while_streaming = true,
    },
    {
        .id             = V4L2_CID_VFLIP,
        .attr           = AV_ATTR_REVERSE_Y,
        .reg_offset     = BCRM_IMG_REVERSE_Y_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_BOOLEAN,
        .flags          = 0,
        .disabled_while_streaming = true,
    },
    {
        .id             = V4L2_CID_SHARPNESS,
        .attr           = AV_ATTR_SHARPNESS,
        .min_offset     = BCRM_SHARPNESS_MIN_32R,
        .max_offset     = BCRM_SHARPNESS_MAX_32R,
        .reg_offset     = BCRM_SHARPNESS_32RW,
        .step_offset        = BCRM_SHARPNESS_INC_32R,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_32,
        .type           = V4L2_CTRL_TYPE_INTEGER,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_EXPOSURE_AUTO,
        .attr           = AV_ATTR_EXPOSURE_AUTO,
        .reg_offset     = BCRM_EXPOSURE_AUTO_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_MENU,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_EXPOSURE_AUTO_MIN,
        .attr           = AV_ATTR_EXPOSURE_AUTO_MIN,
        .reg_offset     = BCRM_EXPOSURE_AUTO_MIN_64RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_64,
        .type           = V4L2_CTRL_TYPE_INTEGER64,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_EXPOSURE_AUTO_MAX,
        .attr           = AV_ATTR_EXPOSURE_AUTO_MAX,
        .reg_offset     = BCRM_EXPOSURE_AUTO_MAX_64RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_64,
        .type           = V4L2_CTRL_TYPE_INTEGER64,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_GAIN_AUTO_MIN,
        .attr           = AV_ATTR_GAIN_AUTO_MIN,
        .reg_offset     = BCRM_GAIN_AUTO_MIN_64RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_64,
        .type           = V4L2_CTRL_TYPE_INTEGER64,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_GAIN_AUTO_MAX,
        .attr           = AV_ATTR_GAIN_AUTO_MAX,
        .reg_offset     = BCRM_GAIN_AUTO_MAX_64RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_64,
        .type           = V4L2_CTRL_TYPE_INTEGER64,
        .flags          = 0,
    },
    {
        .id             = V4L2_CID_EXPOSURE_ACTIVE_LINE_MODE,
        .attr           = AV_ATTR_EXPOSURE_ACTIVE_LINE_MODE,
        .reg_offset     = BCRM_EXPOSURE_ACTIVE_LINE_MODE_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_BOOLEAN,
        .flags          = 0,
        .disabled_while_streaming = true,
	},
    {
        .id             = V4L2_CID_EXPOSURE_ACTIVE_LINE_SELECTOR,
        .attr           = AV_ATTR_EXPOSURE_ACTIVE_LINE_SELECTOR,
        .reg_offset     = BCRM_EXPOSURE_ACTIVE_OUTPUT_LINE_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_INTEGER,
        .flags          = 0,
        .disabled_while_streaming = true,
    },
    {
        .id             = V4L2_CID_EXPOSURE_ACTIVE_INVERT,
        .attr           = AV_ATTR_EXPOSURE_ACTIVE_INVERT,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_BOOLEAN,
        .flags          = 0,
        .disabled_while_streaming = true,
    },
    {
        .id             = V4L2_CID_TRIGGER_MODE,
        .attr           = AV_ATTR_TRIGGER_MODE,
        .reg_offset     = BCRM_FRAME_START_TRIGGER_MODE_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_BOOLEAN,
        .flags          = 0,
        .disabled_while_streaming = true,
    },
    {
        .id             = V4L2_CID_TRIGGER_ACTIVATION,
        .attr           = AV_ATTR_TRIGGER_ACTIVATION,
        .reg_offset     = BCRM_FRAME_START_TRIGGER_ACTIVATION_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_MENU,
        .flags          = 0,
        .disabled_while_streaming = true,
    },
    {
        .id             = V4L2_CID_TRIGGER_SOURCE,
        .attr           = AV_ATTR_TRIGGER_SOURCE,
        .reg_offset     = BCRM_FRAME_START_TRIGGER_SOURCE_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_MENU,
        .flags          = 0,
        .disabled_while_streaming = true,
    },
    {
        .id             = V4L2_CID_TRIGGER_SOFTWARE,
        .attr           = AV_ATTR_TRIGGER_SOFTWARE,
        .reg_offset     = BCRM_FRAME_START_TRIGGER_SOURCE_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_BUTTON,
        .flags          = V4L2_CTRL_FLAG_INACTIVE,
    },
    {
        .id             = V4L2_CID_DEVICE_TEMPERATURE,
        .attr           = AV_ATTR_DEVICE_TEMPERATURE,
        .reg_offset     = BCRM_DEVICE_TEMPERATURE_32R,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_32,
        .type           = V4L2_CTRL_TYPE_INTEGER,
        .flags          = V4L2_CTRL_FLAG_VOLATILE | V4L2_CTRL_FLAG_READ_ONLY,
    },
    {
        .id             = V4L2_CID_BINNING_MODE,
        .attr           = AV_ATTR_BINNING_MODE,
        .reg_offset     = BCRM_DIGITAL_BINNIG_MODE_8RW,
        .reg_size       = AV_CAM_REG_SIZE,
        .data_size      = AV_CAM_DATA_SIZE_8,
        .type           = V4L2_CTRL_TYPE_MENU,
        .flags          = 0,
    },
};

#define AVT_TEGRA_TIMEOUT_DEFAULT   CAPTURE_TIMEOUT_MS
#define AVT_TEGRA_TIMEOUT_DISABLED  -1

#define AVT_TEGRA_CID_BASE          (V4L2_CTRL_CLASS_USER | 0x900)

#define AVT_TEGRA_TIMEOUT                   (AVT_TEGRA_CID_BASE + 200)
#define AVT_TEGRA_TIMEOUT_VALUE             (AVT_TEGRA_CID_BASE + 201)
#define AVT_TEGRA_STRIDE_ALIGN              (AVT_TEGRA_CID_BASE + 202)
#define AVT_TEGRA_CROP_ALIGN                (AVT_TEGRA_CID_BASE + 203)
#define AVT_TEGRA_VALUE_UPDATE_INTERVAL     (AVT_TEGRA_CID_BASE + 204)
#define AVT_TEGRA_FORCE_VALUE_UPDATE        (AVT_TEGRA_CID_BASE + 205)

#endif
