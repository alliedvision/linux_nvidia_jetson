// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021-2022 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 */

#define pr_fmt(msg) "Safety I2S: " msg

#include <linux/module.h>
#include <sound/core.h>
#include <sound/dmaengine_pcm.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/dmaengine.h>
#include <linux/clk.h>
#include <linux/reset.h>

#include "tegra_i2s.h"

#define CHECK_AND_RET_ERR(x, err) \
do { \
	if ((x)) { \
		pr_err("Check failed: %s %d\n", __func__, __LINE__); \
		return err; \
	} \
} while (0)

struct safety_audio_priv {
	struct snd_card *card;
};

static int safety_i2s_trigger(struct snd_pcm_substream *substream, int cmd);

static const char * const clk_names[] = {
	"pll_a_out0",
	"i2s7", "i2s7_clk_parent", "i2s7_ext_audio_sync",
	"i2s7_audio_sync", "i2s7_sync_input",
	"i2s8", "i2s8_clk_parent", "i2s8_ext_audio_sync",
	"i2s8_audio_sync", "i2s8_sync_input",
};

static const char * const reset_names[] = {
	"i2s7_reset", "i2s8_reset"
};

//TODO: Either encapsulate it or allocate dynamically
static struct i2s_dev i2s[NUM_SAFETY_I2S_INST];
static unsigned int enabled_i2s_mask[NUM_SAFETY_I2S_INST];
static struct safety_audio_priv *priv;

static const struct snd_pcm_hardware t234_pcm_hardware = {
	.rates =	    SNDRV_PCM_RATE_48000,
	.rate_min =	 48000,
	.rate_max =	 48000,
	.channels_min =     1,
	.channels_max =     16,
	.info			= SNDRV_PCM_INFO_MMAP |
				  SNDRV_PCM_INFO_MMAP_VALID |
				  SNDRV_PCM_INFO_PAUSE |
				  SNDRV_PCM_INFO_RESUME |
				  SNDRV_PCM_INFO_INTERLEAVED,
	.formats		= SNDRV_PCM_FMTBIT_S8 |
				  SNDRV_PCM_FMTBIT_S16_LE |
				  SNDRV_PCM_FMTBIT_S32_LE,
	.period_bytes_min	= PAGE_SIZE * 16,
	.period_bytes_max	= PAGE_SIZE * 32,
	.periods_min		= 1,
	.periods_max		= 4,
	.fifo_size		= 256,
	.buffer_bytes_max	= PAGE_SIZE * 128,
};

static const struct i2s_config i2s_defaults = {
	.srate = 48000,
	.channels = 8,
	.fsync_width = 255,
	.bclk_ratio = 1,
	.pcm_mask_bits = 0,
	.highz_ctrl = 0,
	.bit_size = 32,
	.total_slots = 8
};

struct i2s_dev *safety_i2s_get_priv(void)
{
	return i2s;
}

static int loopback_control_put(struct snd_kcontrol *kctl,
				struct snd_ctl_elem_value *uc)
{
	int id = (int)kctl->private_value;
	int enable = uc->value.integer.value[0];

	return i2s_set_loopback(id, enable);
}

static int loopback_control_get(struct snd_kcontrol *kctl,
				struct snd_ctl_elem_value *uc)
{
	return 0;
}

static int snd_myctl_mono_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_BOOLEAN;
	uinfo->count = 1;
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 1;

	return 0;
}

static const struct snd_kcontrol_new controls[] = {
	{
		.iface		= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name		= "I2S7 Loopback",
		.index		= 0,
		.access		= SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.put		= loopback_control_put,
		.get		= loopback_control_get,
		.info		= snd_myctl_mono_info,
		.private_value	= 0
	},
	{
		.iface		= SNDRV_CTL_ELEM_IFACE_MIXER,
		.name		= "I2S8 Loopback",
		.index		= 0,
		.access		= SNDRV_CTL_ELEM_ACCESS_READWRITE,
		.put		= loopback_control_put,
		.get		= loopback_control_get,
		.info		= snd_myctl_mono_info,
		.private_value	= 1
	}
};

static int safety_i2s_add_kcontrols(struct snd_card *card, int id)
{
	return snd_ctl_add(card, snd_ctl_new1(&controls[id], i2s));
}

static int prealloc_dma_buff(struct snd_pcm *pcm, unsigned int stream, size_t size)
{
	struct snd_pcm_substream *substream = pcm->streams[stream].substream;
	struct snd_dma_buffer *buff = &substream->dma_buffer;

	buff->area = dma_alloc_coherent(pcm->card->dev, size,
						&buff->addr, GFP_KERNEL);
	if (!buff->area) {
		pr_alert("Buffer allocation failed\n");
		return -ENOMEM;
	}

	buff->private_data = NULL;
	buff->dev.type = SNDRV_DMA_TYPE_DEV;
	buff->dev.dev = pcm->card->dev;
	buff->bytes = size;

	return 0;
}


static int setup_plls(struct device *dev)
{
	/* Do nothing for now */

	return 0;
}

static int i2s_reset_init_and_deassert(struct device *dev, unsigned int id)
{
	struct reset_control *reset;
	int ret;

	reset = of_reset_control_get(dev->of_node, reset_names[id]);
	if (IS_ERR(reset)) {
		pr_alert("No reset information found in DT, skipping...");
		return PTR_ERR(reset);
	}

	i2s[id].reset = reset;
	pr_alert("Clearing reset for i2s%d... ", id + 7);
	ret = reset_control_deassert(reset);
	if (ret)
		pr_alert("Failed!\n");
	else
		pr_alert("Success!\n");

	return ret;
}

static int i2s_clock_init(struct device *dev, unsigned int id)
{
	i2s[id].audio_sync_input = devm_clk_get(dev,
		clk_names[id * CLK_NUM_ENTRIES + CLK_AUDIO_INPUT_SYNC]);
	if (IS_ERR(i2s[id].audio_sync_input)) {
		pr_alert("Could not get audio_sync_input clock from DT\n");
		return -EINVAL;
	}

	i2s[id].audio_sync = devm_clk_get(dev,
			clk_names[id * CLK_NUM_ENTRIES + CLK_AUDIO_SYNC]);
	if (IS_ERR(i2s[id].audio_sync)) {
		pr_alert("Could not get audio_sync clock from DT\n");
		return -EINVAL;
	}

	i2s[id].i2s_sync = devm_clk_get(dev,
			clk_names[id * CLK_NUM_ENTRIES + CLK_I2S_SYNC]);
	if (IS_ERR(i2s[id].i2s_sync)) {
		pr_alert("Could not get i2s_sync clock from DT\n");
		return -EINVAL;
	}

	i2s[id].clk_i2s = devm_clk_get(dev,
				clk_names[id * CLK_NUM_ENTRIES + CLK_I2S]);
	if (IS_ERR(i2s[id].clk_i2s)) {
		pr_alert("Could not get clk_i2s clock from DT\n");
		return -EINVAL;
	}

	i2s[id].clk_i2s_src = devm_clk_get(dev,
			clk_names[id * CLK_NUM_ENTRIES + CLK_I2S_SOURCE]);
	if (IS_ERR(i2s[id].clk_i2s_src)) {
		pr_alert("Could not get clk_i2s_src clock from DT\n");
		return -EINVAL;
	}

	return 0;
}

static int32_t
i2s_get_mode(const void *mode, uint32_t *tx_mode, uint32_t *data_offset)
{
	uint32_t i;
	struct {
		char *name;
		uint32_t mode;
		uint32_t data_offset;
	} i2s_mode[] = {
		{"dsp_a", 1, 1},
		{"dsp_b", 1, 0},
		{"i2s", 0, 1},
	};

	for (i = 0; i < ARRAY_SIZE(i2s_mode); i++) {
		if (strcmp(mode, i2s_mode[i].name) == 0) {
			*tx_mode = i2s_mode[i].mode;
			*data_offset = i2s_mode[i].data_offset;
			return 0;
		}
	}
	return -1;
}

#ifdef SAFETY_I2S_DEBUG
static void dump_config(struct i2s_config *config)
{
#define dump(x) pr_alert("%s = %u\n", #x, x)

	dump(config->mode);
	dump(config->clock_mode);
	dump(config->clock_polarity);
	dump(config->edge_ctrl);
	dump(config->total_slots);
	dump(config->bclk);
	dump(config->bit_size);
	dump(config->channels);
	dump(config->offset);
	dump(config->tx_mask);
	dump(config->rx_mask);
	dump(config->srate);
	dump(config->bclk_ratio);
	dump(config->fsync_width);
	dump(config->pcm_mask_bits);
	dump(config->highz_ctrl);
	dump(config->clock_trim);
}
#endif

static int i2s_parse_dt(struct device *dev, unsigned int id)
{
	char name[5];
	struct device_node *i2s_node;

	struct i2s_config *i2s_config;

	const void *prop;
	int ret = 0;

	ret = sprintf(name, I2S_DT_NODE, I2S_NODE_START_INDEX + id);
	if (ret < 0)
		return -EINVAL;

	i2s_config = &i2s[id].config;
	memcpy(i2s_config, &i2s_defaults, sizeof(i2s_config[0]));

	i2s_config->clock_polarity = 1;

	i2s_node = of_get_child_by_name(dev->of_node, name);
	if (i2s_node == NULL) {
		pr_alert("Invalid device tree node\n");
		return -EINVAL;
	}

	if (of_find_property(i2s_node, "frame-slave", NULL))
		i2s_config->clock_mode = 1;

	prop = of_get_property(i2s_node, "format", NULL);
	if (prop != NULL) {
		i2s_get_mode(prop, &i2s_config->mode, &i2s_config->offset);
		if (strcmp("i2s", prop) == 0)
			i2s_config->clock_polarity = 0;
	}

	if (of_find_property(i2s_node, "bitclock-inversion", NULL))
		i2s_config->edge_ctrl = 1;

	if (of_find_property(i2s_node, "frame-inversion", NULL))
		i2s_config->clock_polarity = !i2s_config->clock_polarity;

	prop = of_get_property(i2s_node, "tx-mask", NULL);
	if (prop != NULL)
		i2s_config->tx_mask = be32_to_cpup(prop);

	prop = of_get_property(i2s_node, "rx-mask", NULL);
	if (prop != NULL)
		i2s_config->rx_mask = be32_to_cpup(prop);

	prop = of_get_property(i2s_node, "clk-trim", NULL);
	if (prop != NULL)
		i2s_config->clock_trim = be32_to_cpup(prop);

	prop = of_get_property(i2s_node, "fsync-width", NULL);
	if (prop != NULL)
		i2s_config->fsync_width = be32_to_cpup(prop);

	prop = of_get_property(i2s_node, "srate", NULL);
	if (prop != NULL)
		i2s_config->srate = be32_to_cpup(prop);

	prop = of_get_property(i2s_node, "num-channel", NULL);
	if (prop != NULL)
		i2s_config->channels = be32_to_cpup(prop);

	prop = of_get_property(i2s_node, "bit-format", NULL);
	if (prop != NULL)
		i2s_config->bit_size = be32_to_cpup(prop);

#ifdef SAFETY_I2S_DEBUG
	dump_config(i2s_config);
#endif
	return 0;
}

static int is_supported_rate(int rate)
{
	return 1;
}

static int i2s_set_rate(unsigned int id, int rate)
{
	unsigned long i2s_clk_freq;

	if (!is_supported_rate(rate))
		return -EINVAL;

	i2s_clk_freq = i2s[id].config.channels * i2s[id].config.srate *
			i2s[id].config.bit_size * i2s[id].config.bclk_ratio;

	if (i2s[id].config.clock_mode == I2S_MASTER) {
		CHECK_AND_RET_ERR(clk_set_parent(i2s[id].audio_sync,
						i2s[id].i2s_sync), -EINVAL);

		CHECK_AND_RET_ERR(clk_set_parent(i2s[id].clk_i2s,
						i2s[id].clk_i2s_src), -EINVAL);
		CHECK_AND_RET_ERR(clk_set_rate(i2s[id].clk_i2s,
						i2s_clk_freq), -EINVAL);
		CHECK_AND_RET_ERR(clk_prepare_enable(
						i2s[id].clk_i2s), -EINVAL);
	}

	CHECK_AND_RET_ERR(clk_set_rate(i2s[id].audio_sync_input,
						i2s_clk_freq), -EINVAL);

	return 0;
}

static void i2s_setup(unsigned int id)
{
	i2s_set_rate(id, i2s[id].config.srate);

	i2s_configure(id, &i2s[id].config);

}

static void safety_i2s_start(struct snd_pcm_substream *substream)
{
	struct dma_data *data = substream->private_data;
	unsigned int id = data->req_sel - 1;
	int rx = (substream->stream == SNDRV_PCM_STREAM_CAPTURE) ? 1 : 0;

	if (rx)
		i2s_enable_rx(id);
	else
		i2s_enable_tx(id);

	i2s_enable(id);
	data->triggered = 1;
}
static void safety_i2s_stop(struct snd_pcm_substream *substream)
{
	struct dma_data *data = substream->private_data;
	unsigned int id = data->req_sel - 1;
	int rx = (substream->stream == SNDRV_PCM_STREAM_CAPTURE) ? 1 : 0;


	if (rx)
		i2s_disable_rx(id);
	else
		i2s_disable_tx(id);

	data->triggered = 0;
}

static int gpcdma_hw_params(struct snd_pcm_substream *substream,
			struct snd_pcm_hw_params *params)
{
	struct dma_slave_config slave_config;
	struct dma_data *dma_data = substream->private_data;
	struct dma_chan *chan;
	int ret;

	chan = snd_dmaengine_pcm_get_chan(substream);
	ret = snd_hwparams_to_dma_slave_config(substream, params,
							&slave_config);
	if (ret) {
		pr_alert("gpcdma hw params failed, err = %d\n", ret);
		return ret;
	}

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		slave_config.dst_addr_width = (dma_data->width == 16) ?
						DMA_SLAVE_BUSWIDTH_2_BYTES :
						DMA_SLAVE_BUSWIDTH_4_BYTES;
		slave_config.dst_addr = dma_data->addr;
		/* MC burst should be multiple of this for proper
		 * stopping of GPCDMA during CYCLIC transfer.
		 * Currently, GPCDMA configures the MC burst
		 * to 2 words unless MMIO supports 64 so we just match it.
		 */
		slave_config.dst_maxburst = 2;
	} else {
		slave_config.src_addr_width = (dma_data->width == 16) ?
						DMA_SLAVE_BUSWIDTH_2_BYTES :
						DMA_SLAVE_BUSWIDTH_4_BYTES;
		slave_config.src_addr = dma_data->addr;
		slave_config.src_maxburst = 2;
	}

	slave_config.slave_id = dma_data->req_sel;

	ret = dmaengine_slave_config(chan, &slave_config);
	if (ret < 0) {
		pr_alert("dma slave config failed, err = %d\n", ret);
		return ret;
	}

	snd_pcm_set_runtime_buffer(substream, &substream->dma_buffer);

	return 0;
}

static int i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	return 0;
}

static int safety_i2s_probe(struct device *dev, unsigned int id)
{
	i2s_parse_dt(dev, id);
	setup_plls(dev);

	i2s_reset_init_and_deassert(dev, id);

	i2s_clock_init(dev, id);
	i2s_setup(id);

	return 0;
}

static int safety_i2s_open(struct snd_pcm_substream *substream)
{
	struct i2s_dev *i2s = snd_pcm_substream_chip(substream);
	struct dma_data *dma_data =
		(substream->stream == SNDRV_PCM_STREAM_CAPTURE) ?
				&i2s->capture_data : &i2s->playback_data;
	struct snd_pcm_runtime *runtime = substream->runtime;
	struct dma_chan *chan;
	int ret;

	/* overwrite pcm->private_data and
	 * maintain only substram specific data
	 */
	substream->private_data = dma_data;

	runtime->hw.info = t234_pcm_hardware.info;
	runtime->hw.rates = t234_pcm_hardware.rates;
	runtime->hw.rate_min = t234_pcm_hardware.rate_min;
	runtime->hw.rate_max = t234_pcm_hardware.rate_max;
	runtime->hw.formats = t234_pcm_hardware.formats;
	runtime->hw.period_bytes_min = t234_pcm_hardware.period_bytes_min;
	runtime->hw.period_bytes_max = t234_pcm_hardware.period_bytes_max;
	runtime->hw.periods_min = t234_pcm_hardware.periods_min;
	runtime->hw.periods_max = t234_pcm_hardware.periods_max;
	runtime->hw.channels_min = t234_pcm_hardware.channels_min;
	runtime->hw.channels_max = t234_pcm_hardware.channels_max;
	runtime->hw.buffer_bytes_max = t234_pcm_hardware.buffer_bytes_max;
	runtime->hw.fifo_size = t234_pcm_hardware.fifo_size;

	//TODO: Support buffer size update from device tree
	ret = snd_pcm_hw_constraint_step(substream->runtime, 0,
				SNDRV_PCM_HW_PARAM_PERIOD_BYTES, 0x8);
	if (ret) {
		pr_alert("Failed to set constraint %d\n", ret);
		return ret;
	}
	chan = dma_request_slave_channel(substream->pcm->card->dev,
						dma_data->dma_chan_name);
	if (!chan) {
		pr_alert("failed to allocate dma channel\n");
		return -ENODEV;
	}

	ret = snd_dmaengine_pcm_open(substream, chan);
	if (ret) {
		pr_alert("failed to open dmaengine\n");
		return ret;
	}

	return 0;
}

static int safety_i2s_hw_params(struct snd_pcm_substream *substream,
				struct snd_pcm_hw_params *params)
{
	int ret;
	/* configure hardware according to the following params
	 * channels, bits per sample, samples per second, period size
	 * and num periods
	 */
	ret = gpcdma_hw_params(substream, params);
	if (!ret)
		ret = i2s_hw_params(substream, params);

	return ret;
}

static int safety_i2s_prepare(struct snd_pcm_substream *substream)
{
	return 0;
}

static int safety_i2s_close(struct snd_pcm_substream *substream)
{
	struct dma_data *data = substream->private_data;

	if (data->triggered) {
		safety_i2s_trigger(substream, SNDRV_PCM_TRIGGER_STOP);
		data->triggered = 0;
	}

	snd_dmaengine_pcm_close_release_chan(substream);

	return 0;
}

static int safety_i2s_trigger(struct snd_pcm_substream *substream, int cmd)
{
	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
	case SNDRV_PCM_TRIGGER_RESUME:
	case SNDRV_PCM_TRIGGER_PAUSE_RELEASE:
		snd_dmaengine_pcm_trigger(substream, cmd);
		safety_i2s_start(substream);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
	case SNDRV_PCM_TRIGGER_PAUSE_PUSH:
	case SNDRV_PCM_TRIGGER_SUSPEND:
		safety_i2s_stop(substream);
		snd_dmaengine_pcm_trigger(substream, cmd);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static struct snd_pcm_ops playback_ops = {
	.open		= safety_i2s_open,
	.close		= safety_i2s_close,
	.hw_params	= safety_i2s_hw_params,
	.prepare	= safety_i2s_prepare,
	.pointer	= snd_dmaengine_pcm_pointer,
	.trigger	= safety_i2s_trigger
};

static const struct of_device_id match_table[] = {
	{ .compatible = "nvidia,tegra234-safety-audio" },
	{}
};

static unsigned int parse_enabled_i2s_mask(struct device *dev)
{
	unsigned int num_enabled = 0;
	int i;

	i = of_property_read_variable_u32_array(dev->of_node,
				"enabled-i2s-mask", enabled_i2s_mask,
						NUM_SAFETY_I2S_INST, 0);
	WARN_ON(i != NUM_SAFETY_I2S_INST);
	for (i = 0; i < NUM_SAFETY_I2S_INST; i++)
		num_enabled += !!(enabled_i2s_mask[i]);

	return num_enabled;
}

static int t234_safety_audio_probe(struct platform_device *pdev)
{
	struct snd_card *card;
	int ret, pcm_instance = 0;
	unsigned int i = 0;
	struct snd_pcm *pcm;
	char name[5] = {0};

	if (parse_enabled_i2s_mask(&pdev->dev) == 0) {
		pr_err("No safety-i2s interfaces are available on this board\n");
		return -ENODEV;
	}

	ret = snd_card_new(&pdev->dev, -1, "Safety I2S sound card",
					THIS_MODULE, sizeof(*priv), &card);
	if (ret < 0)
		return ret;

	priv = card->private_data;
	priv->card = card;

	for (i = 0; i < NUM_SAFETY_I2S_INST; i++) {
		struct resource *r;
		size_t buffer_size = t234_pcm_hardware.buffer_bytes_max;
		void __iomem *base;

		if (!enabled_i2s_mask[i])
			continue;

		if ((sprintf(name, I2S_DT_NODE, I2S_NODE_START_INDEX + i)) < 0)
			return -EINVAL;
		ret = snd_pcm_new(card, name, pcm_instance++, 1, 1, &pcm);
		if (ret < 0) {
			pr_alert("Could not register i2s pcm, ret: %d\n", ret);
			return ret;
		}

		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_PLAYBACK, &playback_ops);
		snd_pcm_set_ops(pcm, SNDRV_PCM_STREAM_CAPTURE, &playback_ops);

		base = devm_platform_get_and_ioremap_resource(pdev, i, &r);
		if (IS_ERR(base)) {
			pr_alert("could not remap base\n");
			return -EINVAL;
		}

		i2s[i].base = base;

		safety_i2s_probe(&pdev->dev, i);

		i2s[i].capture_data.addr = r->start + 0x20;
		//TODO: Read from DT later on
		i2s[i].capture_data.size = buffer_size;
		i2s[i].capture_data.width = i2s[i].config.bit_size;
		i2s[i].capture_data.req_sel = i + 1;
		i2s[i].capture_data.dma_chan_name =
					(i ? "i2s8-rx" : "i2s7-rx");

		i2s[i].playback_data.addr = r->start + 0xa0;
		//TODO: Read from DT later on
		i2s[i].playback_data.size = buffer_size;
		i2s[i].playback_data.width = i2s[i].config.bit_size;
		i2s[i].playback_data.req_sel = i + 1;
		i2s[i].playback_data.dma_chan_name =
					(i ? "i2s8-tx" : "i2s7-tx");

		pcm->private_data = &i2s[i];

		//TODO: check the return value
		prealloc_dma_buff(pcm, SNDRV_PCM_STREAM_PLAYBACK, buffer_size);
		prealloc_dma_buff(pcm, SNDRV_PCM_STREAM_CAPTURE, buffer_size);

		safety_i2s_add_kcontrols(card, i);
	}

	ret = snd_card_register(card);
	if (ret < 0)
		pr_alert("Error registering I2S card, ret = %d\n", ret);
	else
		pr_alert("Sound card registered successfully\n");

	return ret;
}

static int t234_safety_audio_remove(struct platform_device *pdev)
{
	snd_card_free(priv->card);

	return 0;
}

static struct platform_driver t234_safety_audio_driver = {
	.probe = t234_safety_audio_probe,
	.remove = t234_safety_audio_remove,
	.driver = {
		.name = "tegra234-safety-audio",
		.owner = THIS_MODULE,
		.of_match_table = match_table,
	},
};

MODULE_DEVICE_TABLE(of, match_table);
MODULE_LICENSE("GPL");

module_platform_driver(t234_safety_audio_driver);
