#
# (C) Copyright 2008-2017 Fuzhou Rockchip Electronics Co., Ltd
#
# SPDX-License-Identifier:	GPL-2.0+
#

obj-y += rockchip_display.o rockchip_crtc.o rockchip_phy.o \
		rockchip_vop.o rockchip_vop_reg.o bmp_helper.o

obj-$(CONFIG_DRM_ROCKCHIP_MIPI_DSI)	+= rockchip_mipi_dsi.o
obj-$(CONFIG_DRM_ROCKCHIP_DW_MIPI_DSI) += rockchip-dw-mipi-dsi.o rockchip-inno-mipi-dphy.o
obj-$(CONFIG_DRM_ROCKCHIP_ANALOGIX_DP) += rockchip_analogix_dp.o rockchip_analogix_dp_reg.o
obj-$(CONFIG_DRM_ROCKCHIP_LVDS) += rockchip_lvds.o
obj-$(CONFIG_DRM_ROCKCHIP_PANEL) += rockchip_panel.o panel_simple.o \
					rockchip_dsi_panel.o
