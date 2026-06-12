// SPDX-License-Identifier: GPL-2.0-only
/*
 * panel-rpi-dsi-display.c — Unified DRM panel driver for Pi DSI displays
 *
 * Supports multiple panels via descriptors.
 * Based on HYJEF/RPI_DSI_Displays, extended for P040B019 (ST7701P).
 *
 * Copyright (C) CNflysky. All rights reserved.
 * Copyright (C) 2026 psy/p040b019 project.
 */

#include <drm/drm_mipi_dsi.h>
#include <drm/drm_modes.h>
#include <drm/drm_panel.h>
#include <linux/gpio/consumer.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/backlight.h>
#include <linux/version.h>
#include <video/mipi_display.h>

/* Kernel version compatibility shims */
#ifndef BACKLIGHT_POWER_OFF
#define BACKLIGHT_POWER_OFF 0
#endif

/* ===== Kernel version compatibility shims ===== */

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 4, 0)
/*
 * mipi_dsi_multi_context and _multi() helpers were added in 6.4.
 * Provide fallbacks for older kernels.
 */
struct mipi_dsi_multi_context {
	struct mipi_dsi_device *dsi;
	int accum_err;
};

#define mipi_dsi_dcs_write_seq_multi(ctx, cmd, seq...) \
({ \
	const u8 d[] = { cmd, seq }; \
	struct mipi_dsi_multi_context *__ctx = (ctx); \
	int __ret = mipi_dsi_dcs_write_buffer(__ctx->dsi, d, ARRAY_SIZE(d)); \
	if (__ret < 0) \
		__ctx->accum_err = __ret; \
})

static inline void mipi_dsi_dcs_soft_reset_multi(struct mipi_dsi_multi_context *ctx)
{
	int ret = mipi_dsi_dcs_soft_reset(ctx->dsi);
	if (ret < 0)
		ctx->accum_err = ret;
}

static inline void mipi_dsi_dcs_exit_sleep_mode_multi(struct mipi_dsi_multi_context *ctx)
{
	int ret = mipi_dsi_dcs_exit_sleep_mode(ctx->dsi);
	if (ret < 0)
		ctx->accum_err = ret;
}

static inline void mipi_dsi_dcs_enter_sleep_mode_multi(struct mipi_dsi_multi_context *ctx)
{
	int ret = mipi_dsi_dcs_enter_sleep_mode(ctx->dsi);
	if (ret < 0)
		ctx->accum_err = ret;
}

static inline void mipi_dsi_dcs_set_display_on_multi(struct mipi_dsi_multi_context *ctx)
{
	int ret = mipi_dsi_dcs_set_display_on(ctx->dsi);
	if (ret < 0)
		ctx->accum_err = ret;
}

static inline void mipi_dsi_dcs_set_display_off_multi(struct mipi_dsi_multi_context *ctx)
{
	int ret = mipi_dsi_dcs_set_display_off(ctx->dsi);
	if (ret < 0)
		ctx->accum_err = ret;
}

static inline void mipi_dsi_dcs_set_tear_on_multi(struct mipi_dsi_multi_context *ctx,
						   enum mipi_dsi_dcs_tear_mode mode)
{
	int ret = mipi_dsi_dcs_set_tear_on(ctx->dsi, mode);
	if (ret < 0)
		ctx->accum_err = ret;
}
#endif /* < 6.4 */


struct power_on_timing {
	unsigned long post_reset;
	unsigned long reset_low;
	unsigned long after_reset;
	unsigned long slpout;
};

struct rpi_dsi_display_desc {
	const struct drm_display_mode *mode;
	unsigned int lanes;
	unsigned long flags;
	enum mipi_dsi_pixel_format format;
	int (*init_sequence)(struct mipi_dsi_device *dsi);
	const struct power_on_timing *pwr_timing;
	bool do_sw_reset;
};

struct rpi_dsi_display {
	struct drm_panel panel;
	struct mipi_dsi_device *dsi;
	const struct rpi_dsi_display_desc *desc;
	struct gpio_desc *reset;
	enum drm_panel_orientation orientation;
};

static inline struct rpi_dsi_display *
to_rpi_dsi_display(struct drm_panel *panel)
{
	return container_of(panel, struct rpi_dsi_display, panel);
}

/* ===== Panel: w280bf036i (1-lane, 480×640) ===== */

static int w280bf036i_init_sequence(struct mipi_dsi_device *dsi)
{
	struct mipi_dsi_multi_context ctx = { .dsi = dsi };
	/* Wait for panel power to stabilize */
	msleep(50);


	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x13);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xEF, 0x08);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x10);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC0, 0x4f, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC1, 0x10, 0x0c);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC2, 0x07, 0x14);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xCC, 0x10);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB0, 0x0a, 0x18, 0x1e, 0x12, 0x16,
				     0x0c, 0x0e, 0x0d, 0x0c, 0x29, 0x06, 0x14,
				     0x13, 0x29, 0x33, 0x1c);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB1, 0x0a, 0x19, 0x21, 0x0a, 0x0c,
				     0x00, 0x0c, 0x03, 0x03, 0x23, 0x01, 0x0e,
				     0x0c, 0x27, 0x2b, 0x1c);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x11);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB0, 0x5d);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB1, 0x61);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB2, 0x84);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB3, 0x80);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB5, 0x4d);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB7, 0x85);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB8, 0x20);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC1, 0x78);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC2, 0x78);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xD0, 0x88);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE0, 0x00, 0x00, 0x02);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE1, 0x06, 0xa0, 0x08, 0xa0, 0x05,
				     0xa0, 0x07, 0xa0, 0x00, 0x44, 0x44);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE2, 0x20, 0x20, 0x44, 0x44, 0x96,
				     0xa0, 0x00, 0x00, 0x96, 0xa0, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE3, 0x00, 0x00, 0x22, 0x22);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE4, 0x44, 0x44);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE5, 0x0d, 0x91, 0xa0, 0xa0, 0x0f,
				     0x93, 0xa0, 0xa0, 0x09, 0x8d, 0xa0, 0xa0,
				     0x0b, 0x8f, 0xa0, 0xa0);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE6, 0x00, 0x00, 0x22, 0x22);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE7, 0x44, 0x44);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE8, 0x0c, 0x90, 0xa0, 0xa0, 0x0e,
				     0x92, 0xa0, 0xa0, 0x08, 0x8c, 0xa0, 0xa0,
				     0x0a, 0x8e, 0xa0, 0xa0);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE9, 0x36, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xEB, 0x00, 0x01, 0xe4, 0xe4, 0x44,
				     0x88, 0x40);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xED, 0xff, 0x45, 0x67, 0xfa, 0x01,
				     0x2b, 0xcf, 0xff, 0xff, 0xfc, 0xb2, 0x10,
				     0xaf, 0x76, 0x54, 0xff);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xEF, 0x10, 0x0d, 0x04, 0x08, 0x3f,
				     0x1f);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_set_tear_on_multi(&ctx, MIPI_DSI_DCS_TEAR_MODE_VBLANK);

	return ctx.accum_err;
}

/* ===== Panel: tdo_qhd0500d5 (2-lane, 540×960) ===== */

static int tdo_qhd0500d5_init_sequence(struct mipi_dsi_device *dsi)
{
	struct mipi_dsi_multi_context ctx = { .dsi = dsi };
	/* Wait for panel power to stabilize */
	msleep(50);


	mipi_dsi_dcs_write_seq_multi(&ctx, MIPI_DCS_WRITE_CONTROL_DISPLAY, 0x2C);

	return ctx.accum_err;
}

/* ===== Panel: P040B019 (ST7701P, 2-lane, 480×800) ===== */

static int p040b019_init_sequence(struct mipi_dsi_device *dsi)
{
	struct mipi_dsi_multi_context ctx = { .dsi = dsi };
	/* Wait for panel power to stabilize */
	msleep(50);


	/* BK3: system setting */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x13);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xEF, 0x08);

	/* BK0: display setting */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x10);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC0, 0x63, 0x00);	/* display line = 480 */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC1, 0x09, 0x02);	/* porch control */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC2, 0x21, 0x02);	/* inversion + frame rate */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xCC, 0x18);	/* panel setting */

	/* Gamma positive voltage */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB0, 0x40, 0x0E, 0x51, 0x0F, 0x11,
				     0x07, 0x00, 0x09, 0x06, 0x1E, 0x04, 0x12,
				     0x11, 0x64, 0x29, 0xDF);
	/* Gamma negative voltage */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB1, 0x40, 0x07, 0x4C, 0x0A, 0x0E,
				     0x04, 0x00, 0x08, 0x09, 0x1D, 0x01, 0x0E,
				     0x0C, 0x6A, 0x34, 0xDF);

	/* BK1: power & voltage */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x11);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB0, 0x30);	/* Vop = 4.8V */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB1, 0x40);	/* VCOM */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB2, 0x80);	/* VGH = 16V */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB3, 0x80);	/* TEST */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB5, 0x4F);	/* VGL = -12.25V */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB7, 0x85);	/* power control 1 */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB8, 0x23);	/* power control 2 */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xB9, 0x22, 0x13);	/* power control 3 */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xBB, 0x03);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xBC, 0x10);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC0, 0x89);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC1, 0x78);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xC2, 0x78);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xD0, 0x88);

	/* GIP timing */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE0, 0x00, 0x00, 0x02);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE1, 0x04, 0x00, 0x00, 0x00, 0x05,
				     0x00, 0x00, 0x00, 0x00, 0x10, 0x10);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE2, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
				     0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE3, 0x00, 0x00, 0x33, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE4, 0x22, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE5, 0x03, 0x34, 0xAF, 0xB3, 0x05,
				     0x34, 0xAF, 0xB3, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE6, 0x00, 0x00, 0x33, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE7, 0x22, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE8, 0x04, 0x34, 0xAF, 0xB3, 0x06,
				     0x34, 0xAF, 0xB3, 0x00, 0x00, 0x00, 0x00,
				     0x00, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xEB, 0x02, 0x00, 0x40, 0x40, 0x00,
				     0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xEC, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xED, 0xFA, 0x45, 0x0B, 0xFF, 0xFF,
				     0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
				     0xFF, 0xB0, 0x54, 0xAF);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xEF, 0x08, 0x08, 0x08, 0x45, 0x3F,
				     0x54);

	/* BK3: ESD protection */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x13);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE6, 0x16, 0x7C);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE8, 0x00, 0x0E);


	return ctx.accum_err;
}

/* ===== DRM panel callbacks ===== */

static int rpi_dsi_display_prepare(struct drm_panel *panel)
{
	struct rpi_dsi_display *rpi_dsi_display = to_rpi_dsi_display(panel);
	struct mipi_dsi_multi_context ctx = { .dsi = rpi_dsi_display->dsi };

	dev_info(panel->dev, "panel prepare: starting\n");

	if (rpi_dsi_display->reset) {
		dev_info(panel->dev, "panel prepare: toggling reset GPIO\n");
		/*
		 * GPIO configured with GPIO_ACTIVE_LOW (DT flag 1).
		 * gpiod_set_value(1) = assert (physical low = in reset).
		 * gpiod_set_value(0) = de-assert (physical high-Z, external pull-up).
		 *
		 * Initial state from devm_gpiod_get_optional(GPIOD_OUT_HIGH)
		 * is asserted.  De-assert first, then do the proper reset pulse.
		 */
		gpiod_set_value_cansleep(rpi_dsi_display->reset, 0);	/* de-assert */
		msleep(1);

		gpiod_set_value_cansleep(rpi_dsi_display->reset, 1);	/* assert */
		msleep(rpi_dsi_display->desc->pwr_timing->reset_low);

		gpiod_set_value_cansleep(rpi_dsi_display->reset, 0);	/* de-assert */
		msleep(rpi_dsi_display->desc->pwr_timing->after_reset);
	}

	if (rpi_dsi_display->desc->do_sw_reset) {
		dev_info(panel->dev, "panel prepare: soft reset\n");
		mipi_dsi_dcs_soft_reset_multi(&ctx);
		msleep(rpi_dsi_display->desc->pwr_timing->after_reset);
	}

	if (rpi_dsi_display->desc->init_sequence) {
		dev_info(panel->dev, "panel prepare: init sequence\n");
		int ret = rpi_dsi_display->desc->init_sequence(
				rpi_dsi_display->dsi);
		if (ret) {
			dev_err(panel->dev,
				"Failed to send init sequence to panel: %d",
				ret);
			return ret;
		}
	}
	/* Sleep Out + post-Sleep-Out sequence (vendor required) */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_exit_sleep_mode_multi(&ctx);
	msleep(120);
	/* BK3 delay trigger */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x13);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE8, 0x00, 0x0C);
	msleep(10);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xE8, 0x00, 0x00);
	/* TE ON */
	mipi_dsi_dcs_write_seq_multi(&ctx, 0xFF, 0x77, 0x01, 0x00, 0x00, 0x00);
	mipi_dsi_dcs_write_seq_multi(&ctx, 0x35, 0x00);
	msleep(rpi_dsi_display->desc->pwr_timing->slpout);
	dev_info(panel->dev, "panel prepare: done\n");

	return ctx.accum_err;
}

static int rpi_dsi_display_enable(struct drm_panel *panel)
{
	struct mipi_dsi_multi_context ctx = { .dsi = to_mipi_dsi_device(
						      panel->dev) };
	mipi_dsi_dcs_set_display_on_multi(&ctx);
	msleep(20);

	return ctx.accum_err;
}

static int rpi_dsi_display_disable(struct drm_panel *panel)
{
	struct mipi_dsi_multi_context ctx = { .dsi = to_mipi_dsi_device(
						      panel->dev) };
	mipi_dsi_dcs_set_display_off_multi(&ctx);

	return ctx.accum_err;
}

static int rpi_dsi_display_unprepare(struct drm_panel *panel)
{
	struct rpi_dsi_display *rpi_dsi_display = to_rpi_dsi_display(panel);
	struct mipi_dsi_multi_context ctx = { .dsi = rpi_dsi_display->dsi };

	mipi_dsi_dcs_enter_sleep_mode_multi(&ctx);
	if (rpi_dsi_display->reset)
		gpiod_set_value_cansleep(rpi_dsi_display->reset, 0);

	return ctx.accum_err;
}

static int rpi_dsi_display_get_modes(struct drm_panel *panel,
				     struct drm_connector *connector)
{
	struct rpi_dsi_display *rpi_dsi_display = to_rpi_dsi_display(panel);
	const struct drm_display_mode *desc_mode = rpi_dsi_display->desc->mode;
	struct drm_display_mode *mode;

	mode = drm_mode_duplicate(connector->dev, desc_mode);
	if (!mode) {
		dev_err(&rpi_dsi_display->dsi->dev,
			"failed to add mode %ux%u@%u\n", desc_mode->hdisplay,
			desc_mode->vdisplay, drm_mode_vrefresh(desc_mode));
		return -ENOMEM;
	}

	drm_mode_set_name(mode);
	drm_mode_probed_add(connector, mode);

	connector->display_info.width_mm = desc_mode->width_mm;
	connector->display_info.height_mm = desc_mode->height_mm;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	drm_connector_set_orientation_from_panel(connector, panel);
#else
	drm_connector_set_panel_orientation_with_quirk(connector,
			DRM_MODE_PANEL_ORIENTATION_NORMAL, desc_mode->width_mm,
			desc_mode->height_mm);
#endif
	return 1;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static enum drm_panel_orientation
rpi_dsi_display_get_orientation(struct drm_panel *panel)
{
	struct rpi_dsi_display *rpi_dsi_display = to_rpi_dsi_display(panel);
	return rpi_dsi_display->orientation;
}
#endif

/* ===== DCS backlight (fallback when no DT backlight) ===== */

static int rpi_dsi_display_set_brightness(struct backlight_device *bl)
{
	struct rpi_dsi_display *rpi_dsi_display = bl_get_data(bl);
	struct mipi_dsi_device *dsi = rpi_dsi_display->dsi;
	uint8_t brightness = bl->props.brightness;

	return mipi_dsi_dcs_write(dsi, MIPI_DCS_SET_DISPLAY_BRIGHTNESS,
				  &brightness, sizeof(brightness));
}

static const struct backlight_ops rpi_dsi_display_bl_ops = {
	.update_status = rpi_dsi_display_set_brightness,
};

static const struct drm_panel_funcs rpi_dsi_display_funcs = {
	.disable = rpi_dsi_display_disable,
	.unprepare = rpi_dsi_display_unprepare,
	.prepare = rpi_dsi_display_prepare,
	.enable = rpi_dsi_display_enable,
	.get_modes = rpi_dsi_display_get_modes,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	.get_orientation = rpi_dsi_display_get_orientation,
#endif
};

/* ===== Display modes and descriptors ===== */

static const struct drm_display_mode w280bf036i_mode = {
	.clock = 22572,

	.hdisplay = 480,
	.hsync_start = 480 + 30,	/* HFP */
	.hsync_end = 480 + 30 + 10,	/* + HSync */
	.htotal = 480 + 30 + 10 + 30,	/* + HBP */

	.vdisplay = 640,
	.vsync_start = 640 + 20,	/* VFP */
	.vsync_end = 640 + 20 + 4,	/* + VSync */
	.vtotal = 640 + 20 + 4 + 20,	/* + VBP */

	.width_mm = 43,
	.height_mm = 57,

	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static const struct drm_display_mode tdo_qhd0500d5_mode = {
	.clock = 41496,

	.hdisplay = 540,
	.hsync_start = 540 + 48,	/* HFP */
	.hsync_end = 540 + 48 + 32,	/* + HSync */
	.htotal = 540 + 48 + 32 + 80,	/* + HBP */

	.vdisplay = 960,
	.vsync_start = 960 + 3,	/* VFP */
	.vsync_end = 960 + 3 + 10,	/* + VSync */
	.vtotal = 960 + 3 + 10 + 15,	/* + VBP */

	.width_mm = 65,
	.height_mm = 116,

	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

static const struct drm_display_mode p040b019_mode = {
	.clock = 27000,

	.hdisplay = 480,
	.hsync_start = 480 + 30,	/* hactive + HFP  */
	.hsync_end = 480 + 30 + 10,	/* + hsync-len */
	.htotal = 480 + 30 + 10 + 30,	/* + HBP */

	.vdisplay = 800,
	.vsync_start = 800 + 15,	/* vactive + VFP */
	.vsync_end = 800 + 15 + 4,	/* + vsync-len */
	.vtotal = 800 + 15 + 4 + 12,	/* + VBP */

	.width_mm = 52,
	.height_mm = 86,

	.type = DRM_MODE_TYPE_DRIVER | DRM_MODE_TYPE_PREFERRED,
};

/* ===== Power timing ===== */

static const struct power_on_timing w280bf036i_pwr_timing = {
	.post_reset = 20,
	.reset_low = 20,
	.after_reset = 120,
	.slpout = 120,
};

static const struct power_on_timing tdo_qhd0500d5_pwr_timing = {
	.post_reset = 50,
	.reset_low = 50,
	.after_reset = 120,
	.slpout = 150,
};

static const struct power_on_timing p040b019_pwr_timing = {
	.post_reset = 100,
	.reset_low  = 100,
	.after_reset = 200,
	.slpout     = 200,
};

/* ===== Panel descriptors ===== */

static const struct rpi_dsi_display_desc w280bf036i_desc = {
	.mode = &w280bf036i_mode,
	.lanes = 1,
	.flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
		 MIPI_DSI_MODE_LPM,
	.format = MIPI_DSI_FMT_RGB888,
	.init_sequence = w280bf036i_init_sequence,
	.pwr_timing = &w280bf036i_pwr_timing,
	.do_sw_reset = true,
};

static const struct rpi_dsi_display_desc tdo_qhd0500d5_desc = {
	.mode = &tdo_qhd0500d5_mode,
	.lanes = 2,
	.flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
		 MIPI_DSI_MODE_LPM,
	.format = MIPI_DSI_FMT_RGB888,
	.init_sequence = tdo_qhd0500d5_init_sequence,
	.pwr_timing = &tdo_qhd0500d5_pwr_timing,
	.do_sw_reset = true,
};

static const struct rpi_dsi_display_desc p040b019_desc = {
	.mode = &p040b019_mode,
	.lanes = 2,
	.flags = MIPI_DSI_MODE_VIDEO | MIPI_DSI_MODE_VIDEO_BURST |
		 MIPI_DSI_MODE_LPM,
	.format = MIPI_DSI_FMT_RGB888,
	.init_sequence = p040b019_init_sequence,
	.pwr_timing = &p040b019_pwr_timing,
	.do_sw_reset = true,
};

/* ===== Probe / Remove ===== */

static int rpi_dsi_display_probe(struct mipi_dsi_device *dsi)
{
	struct rpi_dsi_display *rpi_dsi_display;
	const struct rpi_dsi_display_desc *desc;
	int ret;

	dev_info(&dsi->dev, "panel probe starting\n");

	rpi_dsi_display = devm_kzalloc(&dsi->dev, sizeof(*rpi_dsi_display),
				       GFP_KERNEL);
	if (!rpi_dsi_display)
		return -ENOMEM;

	desc = of_device_get_match_data(&dsi->dev);
	dev_info(&dsi->dev, "panel desc found, lanes=%d, mode=%dx%d\n",
		 desc->lanes, desc->mode->hdisplay, desc->mode->vdisplay);
	dsi->mode_flags = desc->flags;
	dsi->format = desc->format;
	dsi->lanes = desc->lanes;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	rpi_dsi_display->panel.prepare_prev_first = true;
#endif
	rpi_dsi_display->reset =
		devm_gpiod_get_optional(&dsi->dev, "reset", GPIOD_OUT_HIGH);
	if (IS_ERR(rpi_dsi_display->reset)) {
		dev_err(&dsi->dev, "Failed to get reset GPIO\n");
		return PTR_ERR(rpi_dsi_display->reset);
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	ret = of_drm_get_panel_orientation(dsi->dev.of_node,
					   &rpi_dsi_display->orientation);
	if (ret < 0) {
		dev_warn(&dsi->dev,
			 "Failed to get orientation, default to normal");
		rpi_dsi_display->orientation =
			DRM_MODE_PANEL_ORIENTATION_NORMAL;
	}
#else
	rpi_dsi_display->orientation =
		DRM_MODE_PANEL_ORIENTATION_NORMAL;
#endif

	drm_panel_init(&rpi_dsi_display->panel, &dsi->dev,
		       &rpi_dsi_display_funcs, DRM_MODE_CONNECTOR_DSI);

	/* Try to get backlight from DT (pwm-backlight node) */
	ret = drm_panel_of_backlight(&rpi_dsi_display->panel);
	if (ret < 0)
		return ret;

	/* Fall back to DCS-based backlight if none in DT */
	if (!rpi_dsi_display->panel.backlight) {
		struct backlight_device *bl;

		dev_info(&dsi->dev,
			 "No DT backlight, using DCS backlight\n");
		bl = devm_backlight_device_register(&dsi->dev,
				"rpi-dsi-display-bl", &dsi->dev,
				rpi_dsi_display, &rpi_dsi_display_bl_ops,
				NULL);
		if (IS_ERR(bl)) {
			dev_err(&dsi->dev,
				"Failed to register DCS backlight\n");
			return PTR_ERR(bl);
		}
		bl->props.max_brightness = 0xFF;
		bl->props.brightness = 0x80;
		bl->props.power = BACKLIGHT_POWER_OFF;
		rpi_dsi_display->panel.backlight = bl;
	}

	drm_panel_add(&rpi_dsi_display->panel);

	mipi_dsi_set_drvdata(dsi, rpi_dsi_display);
	rpi_dsi_display->dsi = dsi;
	rpi_dsi_display->desc = desc;

	ret = mipi_dsi_attach(dsi);
	if (ret) {
		dev_err(&dsi->dev, "mipi_dsi_attach failed: %d\n", ret);
		drm_panel_remove(&rpi_dsi_display->panel);
		return ret;
	}

	dev_info(&dsi->dev, "panel probe success (reset=%s)\n",
		 rpi_dsi_display->reset ? "yes" : "no");
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 5, 0)
#define REMOVE_RET int
#define REMOVE_RETVAL return 0
#else
#define REMOVE_RET void
#define REMOVE_RETVAL
#endif

static REMOVE_RET rpi_dsi_display_remove(struct mipi_dsi_device *dsi)
{
	struct rpi_dsi_display *rpi_dsi_display = mipi_dsi_get_drvdata(dsi);

	mipi_dsi_detach(dsi);
	drm_panel_remove(&rpi_dsi_display->panel);
	REMOVE_RETVAL;
}

static const struct of_device_id rpi_dsi_display_ids[] = {
	{ .compatible = "wlk,w280bf036i", .data = &w280bf036i_desc },
	{ .compatible = "truly,tdo-qhd0500d5", .data = &tdo_qhd0500d5_desc },
	{ .compatible = "boe,p040b019", .data = &p040b019_desc },
	{ /* sentinel */ }
};

MODULE_DEVICE_TABLE(of, rpi_dsi_display_ids);

static struct mipi_dsi_driver rpi_dsi_display_driver = {
	.probe = rpi_dsi_display_probe,
	.remove = rpi_dsi_display_remove,
	.driver = {
		.name = "rpi_dsi_display",
		.of_match_table = rpi_dsi_display_ids,
	},
};

module_mipi_dsi_driver(rpi_dsi_display_driver);
MODULE_AUTHOR("CNflysky <cnflysky@qq.com>");
MODULE_DESCRIPTION("RPi DSI Display Driver (P040B019/ST7701P)");
MODULE_LICENSE("GPL");
