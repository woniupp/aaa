/*
 * (C) Copyright 2008-2017 Fuzhou Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <config.h>
#include <common.h>
#include <errno.h>
#include <malloc.h>
#include <fdtdec.h>
#include <fdt_support.h>
#include <asm/unaligned.h>
#include <linux/list.h>
#include <dm/device.h>

#include "rockchip_display.h"
#include "rockchip_crtc.h"
#include "rockchip_connector.h"
#include "rockchip_panel.h"

static const struct drm_display_mode auo_b125han03_mode = {
	.clock = 146900,
	.hdisplay = 1920,
	.hsync_start = 1920 + 48,
	.hsync_end = 1920 + 48 + 32,
	.htotal = 1920 + 48 + 32 + 140,
	.vdisplay = 1080,
	.vsync_start = 1080 + 2,
	.vsync_end = 1080 + 2 + 5,
	.vtotal = 1080 + 2 + 5 + 57,
	.vrefresh = 60,
	.flags = DRM_MODE_FLAG_NVSYNC | DRM_MODE_FLAG_NHSYNC,
};

static const struct drm_display_mode lg_lp079qx1_sp0v_mode = {
	.clock = 200000,
	.hdisplay = 1536,
	.hsync_start = 1536 + 12,
	.hsync_end = 1536 + 12 + 16,
	.htotal = 1536 + 12 + 16 + 48,
	.vdisplay = 2048,
	.vsync_start = 2048 + 8,
	.vsync_end = 2048 + 8 + 4,
	.vtotal = 2048 + 8 + 4 + 8,
	.vrefresh = 60,
	.flags = DRM_MODE_FLAG_NVSYNC | DRM_MODE_FLAG_NHSYNC,
};
#if 0
static const struct rockchip_panel simple_panel_data = {
	.funcs = &panel_simple_funcs,
};
#endif

static const struct rockchip_panel simple_panel_dsi_data = {
	.funcs = &rockchip_dsi_panel_funcs,
};
#if 0
static const struct rockchip_panel lg_lp079qx1_sp0v_data = {
	.funcs = &panel_simple_funcs,
	.data = &lg_lp079qx1_sp0v_mode,
};

static const struct rockchip_panel auo_b125han03_data = {
	.funcs = &panel_simple_funcs,
	.data = &auo_b125han03_mode,
};
#endif

static const struct udevice_id rockchip_panel_ids[] = {
#if 0
	}, {
		.compatible = "simple-panel",
		.funcs = &simple_panel_data,
	},
#endif
 {
		.compatible = "simple-panel-dsi",
		.data = (ulong)&simple_panel_dsi_data,
	},
#if 0
{
		.compatible = "lg,lp079qx1-sp0v",
		.funcs = &lg_lp079qx1_sp0v_data,
	}, {
		.compatible = "auo,b125han03",
		.funcs = &auo_b125han03_data,
	},
#endif
 {}
};

static int rockchip_panel_probe(struct udevice *dev)
{
	printf("--->yzq %s %d\n", __func__, __LINE__);
	return 0;
}

static int rockchip_panel_bind(struct udevice *dev)
{
	printf("--->yzq %s %d\n", __func__, __LINE__);
	return 0;
}


U_BOOT_DRIVER(rockchip_panel) = {
	.name = "rockchip_panel",
	.id = UCLASS_PANEL,
	.of_match = rockchip_panel_ids,
	.bind	= rockchip_panel_bind,
	.probe	= rockchip_panel_probe,
};

const struct drm_display_mode *
rockchip_get_display_mode_from_panel(struct display_state *state)
{
	struct panel_state *panel_state = &state->panel_state;
	const struct rockchip_panel *panel = panel_state->panel;

	if (!panel || !panel->data)
		return NULL;

	return (const struct drm_display_mode *)panel->data;
}

#if 0
const struct rockchip_panel *rockchip_get_panel(const void *blob, int node)
{
	const char *name;
	int i;

	name = fdt_stringlist_get(blob, node, "compatible", 0, NULL);

	for (i = 0; i < ARRAY_SIZE(g_panel); i++)
		if (!strcmp(name, g_panel[i].compatible))
			break;

	if (i >= ARRAY_SIZE(g_panel))
		return NULL;

	return &g_panel[i];
}
#endif

int rockchip_panel_init(struct display_state *state)
{
	struct panel_state *panel_state = &state->panel_state;
	const struct rockchip_panel *panel = panel_state->panel;

	if (!panel || !panel->funcs || !panel->funcs->init) {
		printf("%s: failed to find panel init funcs\n", __func__);
		return -ENODEV;
	}

	return panel->funcs->init(state);
}

void rockchip_panel_deinit(struct display_state *state)
{
	struct panel_state *panel_state = &state->panel_state;
	const struct rockchip_panel *panel = panel_state->panel;

	if (!panel || !panel->funcs || !panel->funcs->deinit) {
		printf("%s: failed to find panel deinit funcs\n", __func__);
		return;
	}

	panel->funcs->deinit(state);
}

int rockchip_panel_prepare(struct display_state *state)
{
	struct panel_state *panel_state = &state->panel_state;
	const struct rockchip_panel *panel = panel_state->panel;

printf("--->yzq %s %d\n", __func__, __LINE__);
	if (!panel || !panel->funcs || !panel->funcs->prepare) {
printf("--->yzq %s %d panel=%p\n", __func__, __LINE__, panel);
printf("--->yzq %s %d panel->funcs=%p\n", __func__, __LINE__, panel->funcs);
printf("--->yzq %s %d panel->funcs->prepare=%p\n", __func__, __LINE__, panel->funcs->prepare);
		printf("%s: failed to find panel prepare funcs\n", __func__);
		return -ENODEV;
	}
printf("--->yzq %s %d\n", __func__, __LINE__);

	return panel->funcs->prepare(state);
}

int rockchip_panel_unprepare(struct display_state *state)
{
	struct panel_state *panel_state = &state->panel_state;
	const struct rockchip_panel *panel = panel_state->panel;

	if (!panel || !panel->funcs || !panel->funcs->unprepare) {
		printf("%s: failed to find panel unprepare funcs\n", __func__);
		return -ENODEV;
	}

	return panel->funcs->unprepare(state);
}

int rockchip_panel_enable(struct display_state *state)
{
	struct panel_state *panel_state = &state->panel_state;
	const struct rockchip_panel *panel = panel_state->panel;

	if (!panel || !panel->funcs || !panel->funcs->enable) {
		printf("%s: failed to find panel prepare funcs\n", __func__);
		return -ENODEV;
	}

	return panel->funcs->enable(state);
}

int rockchip_panel_disable(struct display_state *state)
{
	struct panel_state *panel_state = &state->panel_state;
	const struct rockchip_panel *panel = panel_state->panel;

	if (!panel || !panel->funcs || !panel->funcs->disable) {
		printf("%s: failed to find panel disable funcs\n", __func__);
		return -ENODEV;
	}

	return panel->funcs->disable(state);
}
