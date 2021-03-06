/*
 * Copyright 2017 Rockchip Electronics Co., Ltd
 *
 * SPDX-License-Identifier:    GPL-2.0+
 */

#include <asm/io.h>
#include <asm/arch/clock.h>
#include <common.h>
#include <dm.h>
#include <generic-phy.h>
#include <syscon.h>

#define U2PHY_BIT_WRITEABLE_SHIFT	16
#define CHG_DCD_MAX_RETRIES		6
#define CHG_PRI_MAX_RETRIES		2
#define CHG_DCD_POLL_TIME		100	/* millisecond */
#define CHG_PRIMARY_DET_TIME		40	/* millisecond */
#define CHG_SECONDARY_DET_TIME		40	/* millisecond */

struct rockchip_usb2phy;

enum power_supply_type {
	POWER_SUPPLY_TYPE_UNKNOWN = 0,
	POWER_SUPPLY_TYPE_USB,		/* Standard Downstream Port */
	POWER_SUPPLY_TYPE_USB_DCP,	/* Dedicated Charging Port */
	POWER_SUPPLY_TYPE_USB_CDP,	/* Charging Downstream Port */
	POWER_SUPPLY_TYPE_USB_FLOATING,	/* DCP without shorting D+/D- */
};

enum rockchip_usb2phy_port_id {
	USB2PHY_PORT_OTG,
	USB2PHY_PORT_HOST,
	USB2PHY_NUM_PORTS,
};

struct usb2phy_reg {
	u32	offset;
	u32	bitend;
	u32	bitstart;
	u32	disable;
	u32	enable;
};

/**
 * struct rockchip_chg_det_reg: usb charger detect registers
 * @cp_det: charging port detected successfully.
 * @dcp_det: dedicated charging port detected successfully.
 * @dp_det: assert data pin connect successfully.
 * @idm_sink_en: open dm sink curren.
 * @idp_sink_en: open dp sink current.
 * @idp_src_en: open dm source current.
 * @rdm_pdwn_en: open dm pull down resistor.
 * @vdm_src_en: open dm voltage source.
 * @vdp_src_en: open dp voltage source.
 * @opmode: utmi operational mode.
 */
struct rockchip_chg_det_reg {
	struct usb2phy_reg	cp_det;
	struct usb2phy_reg	dcp_det;
	struct usb2phy_reg	dp_det;
	struct usb2phy_reg	idm_sink_en;
	struct usb2phy_reg	idp_sink_en;
	struct usb2phy_reg	idp_src_en;
	struct usb2phy_reg	rdm_pdwn_en;
	struct usb2phy_reg	vdm_src_en;
	struct usb2phy_reg	vdp_src_en;
	struct usb2phy_reg	opmode;
};

/**
 * struct rockchip_usb2phy_port_cfg: usb-phy port configuration.
 * @phy_sus: phy suspend register.
 * @bvalid_det_en: vbus valid rise detection enable register.
 * @bvalid_det_st: vbus valid rise detection status register.
 * @bvalid_det_clr: vbus valid rise detection clear register.
 * @ls_det_en: linestate detection enable register.
 * @ls_det_st: linestate detection state register.
 * @ls_det_clr: linestate detection clear register.
 * @iddig_output: iddig output from grf.
 * @iddig_en: utmi iddig select between grf and phy,
 *	      0: from phy; 1: from grf
 * @idfall_det_en: id fall detection enable register.
 * @idfall_det_st: id fall detection state register.
 * @idfall_det_clr: id fall detection clear register.
 * @idrise_det_en: id rise detection enable register.
 * @idrise_det_st: id rise detection state register.
 * @idrise_det_clr: id rise detection clear register.
 * @utmi_avalid: utmi vbus avalid status register.
 * @utmi_bvalid: utmi vbus bvalid status register.
 * @utmi_iddig: otg port id pin status register.
 * @utmi_ls: utmi linestate state register.
 * @utmi_hstdet: utmi host disconnect register.
 * @vbus_det_en: vbus detect function power down register.
 */
struct rockchip_usb2phy_port_cfg {
	struct usb2phy_reg	phy_sus;
	struct usb2phy_reg	bvalid_det_en;
	struct usb2phy_reg	bvalid_det_st;
	struct usb2phy_reg	bvalid_det_clr;
	struct usb2phy_reg	ls_det_en;
	struct usb2phy_reg	ls_det_st;
	struct usb2phy_reg	ls_det_clr;
	struct usb2phy_reg	iddig_output;
	struct usb2phy_reg	iddig_en;
	struct usb2phy_reg	idfall_det_en;
	struct usb2phy_reg	idfall_det_st;
	struct usb2phy_reg	idfall_det_clr;
	struct usb2phy_reg	idrise_det_en;
	struct usb2phy_reg	idrise_det_st;
	struct usb2phy_reg	idrise_det_clr;
	struct usb2phy_reg	utmi_avalid;
	struct usb2phy_reg	utmi_bvalid;
	struct usb2phy_reg	utmi_iddig;
	struct usb2phy_reg	utmi_ls;
	struct usb2phy_reg	utmi_hstdet;
	struct usb2phy_reg	vbus_det_en;
};

/**
 * struct rockchip_usb2phy_cfg: usb-phy configuration.
 * @reg: the address offset of grf for usb-phy config.
 * @num_ports: specify how many ports that the phy has.
 * @phy_tuning: phy default parameters tunning.
 * @clkout_ctl: keep on/turn off output clk of phy.
 * @chg_det: charger detection registers.
 */
struct rockchip_usb2phy_cfg {
	u32	reg;
	u32	num_ports;
	int (*phy_tuning)(struct rockchip_usb2phy *);
	struct usb2phy_reg	clkout_ctl;
	const struct rockchip_usb2phy_port_cfg	port_cfgs[USB2PHY_NUM_PORTS];
	const struct rockchip_chg_det_reg	chg_det;
};

/**
 * @dcd_retries: The retry count used to track Data contact
 *		 detection process.
 * @primary_retries: The retry count used to do usb bc detection
 *		     primary stage.
 * @grf: General Register Files register base.
 * @usbgrf_base : USB General Register Files register base.
 * @phy_cfg: phy register configuration, assigned by driver data.
 */
struct rockchip_usb2phy {
	u8		dcd_retries;
	u8		primary_retries;
	void __iomem	*grf_base;
	void __iomem	*usbgrf_base;
	const struct rockchip_usb2phy_cfg	*phy_cfg;
};

static inline void __iomem *get_reg_base(struct rockchip_usb2phy *rphy)
{
	return !rphy->usbgrf_base ? rphy->grf_base : rphy->usbgrf_base;
}

static inline int property_enable(void __iomem *base,
				  const struct usb2phy_reg *reg, bool en)
{
	u32 val, mask, tmp;

	tmp = en ? reg->enable : reg->disable;
	mask = GENMASK(reg->bitend, reg->bitstart);
	val = (tmp << reg->bitstart) | (mask << U2PHY_BIT_WRITEABLE_SHIFT);

	return writel(val, base + reg->offset);
}

static inline bool property_enabled(void __iomem *base,
				    const struct usb2phy_reg *reg)
{
	u32 tmp, orig;
	u32 mask = GENMASK(reg->bitend, reg->bitstart);

	orig = readl(base + reg->offset);

	tmp = (orig & mask) >> reg->bitstart;

	return tmp == reg->enable;
}

static const char *chg_to_string(enum power_supply_type chg_type)
{
	switch (chg_type) {
	case POWER_SUPPLY_TYPE_USB:
		return "USB_SDP_CHARGER";
	case POWER_SUPPLY_TYPE_USB_DCP:
		return "USB_DCP_CHARGER";
	case POWER_SUPPLY_TYPE_USB_CDP:
		return "USB_CDP_CHARGER";
	case POWER_SUPPLY_TYPE_USB_FLOATING:
		return "USB_FLOATING_CHARGER";
	default:
		return "INVALID_CHARGER";
	}
}

static void rockchip_chg_enable_dcd(struct rockchip_usb2phy *rphy,
				    bool en)
{
	void __iomem *base = get_reg_base(rphy);

	property_enable(base, &rphy->phy_cfg->chg_det.rdm_pdwn_en, en);
	property_enable(base, &rphy->phy_cfg->chg_det.idp_src_en, en);
}

static void rockchip_chg_enable_primary_det(struct rockchip_usb2phy *rphy,
					    bool en)
{
	void __iomem *base = get_reg_base(rphy);

	property_enable(base, &rphy->phy_cfg->chg_det.vdp_src_en, en);
	property_enable(base, &rphy->phy_cfg->chg_det.idm_sink_en, en);
}

static void rockchip_chg_enable_secondary_det(struct rockchip_usb2phy *rphy,
					      bool en)
{
	void __iomem *base = get_reg_base(rphy);

	property_enable(base, &rphy->phy_cfg->chg_det.vdm_src_en, en);
	property_enable(base, &rphy->phy_cfg->chg_det.idp_sink_en, en);
}

static bool rockchip_chg_primary_det_retry(struct rockchip_usb2phy *rphy)
{
	bool vout = false;

	while (rphy->primary_retries--) {
		/* voltage source on DP, probe on DM */
		rockchip_chg_enable_primary_det(rphy, true);
		mdelay(CHG_PRIMARY_DET_TIME);
		vout = property_enabled(rphy->grf_base,
					&rphy->phy_cfg->chg_det.cp_det);
		if (vout)
			break;
	}

	return vout;
}

int rockchip_chg_get_type(void)
{
	const struct rockchip_usb2phy_cfg *phy_cfgs;
	enum power_supply_type chg_type;
	struct rockchip_usb2phy rphy;
	struct udevice *dev;
	ofnode u2phy_node, grf_node;
	fdt_size_t size;
	u32 reg, index;
	bool is_dcd, vout;
	int ret;

	u2phy_node = ofnode_null();
	grf_node = ofnode_null();

	u2phy_node = ofnode_path("/usb2-phy");

	if (!ofnode_valid(u2phy_node)) {
		grf_node = ofnode_path("/syscon-usb");
		if (ofnode_valid(grf_node))
			u2phy_node = ofnode_find_subnode(grf_node,
							 "usb2-phy");
	}

	if (!ofnode_valid(u2phy_node)) {
		printf("%s: missing u2phy node\n", __func__);
		return -EINVAL;
	}

	if (ofnode_valid(grf_node)) {
		rphy.grf_base =
			(void __iomem *)ofnode_get_addr_size(grf_node,
							     "reg", &size);
	} else {
		if (ofnode_read_bool(u2phy_node, "rockchip,grf"))
			rphy.grf_base =
				syscon_get_first_range(ROCKCHIP_SYSCON_GRF);
	}

	if (rphy.grf_base <= 0) {
		dev_err(dev, "get syscon grf failed\n");
		return -EINVAL;
	}

	if (ofnode_read_u32(u2phy_node, "reg", &reg)) {
		printf("%s: could not read reg\n", __func__);
		return -EINVAL;
	}

	if (ofnode_read_bool(u2phy_node, "rockchip,usbgrf")) {
		rphy.usbgrf_base =
			syscon_get_first_range(ROCKCHIP_SYSCON_USBGRF);
		if (rphy.usbgrf_base <= 0) {
			dev_err(dev, "get syscon usbgrf failed\n");
			return -EINVAL;
		}
	} else {
		rphy.usbgrf_base = NULL;
	}

	ret = uclass_get_device_by_ofnode(UCLASS_PHY, u2phy_node, &dev);
	if (ret) {
		printf("%s: uclass_get_device_by_ofnode failed: %d\n",
		       __func__, ret);
		return ret;
	}

	phy_cfgs =
		(const struct rockchip_usb2phy_cfg *)dev_get_driver_data(dev);
	if (!phy_cfgs) {
		printf("%s: unable to get phy_cfgs\n", __func__);
		return -EINVAL;
	}

	/* find out a proper config which can be matched with dt. */
	index = 0;
	while (phy_cfgs[index].reg) {
		if (phy_cfgs[index].reg == reg) {
			rphy.phy_cfg = &phy_cfgs[index];
			break;
		}
		++index;
	}

	if (!rphy.phy_cfg) {
		printf("%s: no phy-config can be matched\n", __func__);
		return -EINVAL;
	}

	rphy.dcd_retries = CHG_DCD_MAX_RETRIES;
	rphy.primary_retries = CHG_PRI_MAX_RETRIES;

	/* stage 1, start DCD processing stage */
	rockchip_chg_enable_dcd(&rphy, true);

	while (rphy.dcd_retries--) {
		mdelay(CHG_DCD_POLL_TIME);

		/* get data contact detection status */
		is_dcd = property_enabled(rphy.grf_base,
					  &rphy.phy_cfg->chg_det.dp_det);

		if (is_dcd || !rphy.dcd_retries) {
			/*
			 * stage 2, turn off DCD circuitry, then
			 * voltage source on DP, probe on DM.
			 */
			rockchip_chg_enable_dcd(&rphy, false);
			rockchip_chg_enable_primary_det(&rphy, true);
			break;
		}
	}

	mdelay(CHG_PRIMARY_DET_TIME);
	vout = property_enabled(rphy.grf_base,
				&rphy.phy_cfg->chg_det.cp_det);
	rockchip_chg_enable_primary_det(&rphy, false);
	if (vout) {
		/* stage 3, voltage source on DM, probe on DP */
		rockchip_chg_enable_secondary_det(&rphy, true);
	} else {
		if (!rphy.dcd_retries) {
			/* floating charger found */
			chg_type = POWER_SUPPLY_TYPE_USB_FLOATING;
			goto out;
		} else {
			/*
			 * Retry some times to make sure that it's
			 * really a USB SDP charger.
			 */
			vout = rockchip_chg_primary_det_retry(&rphy);
			if (vout) {
				/* stage 3, voltage source on DM, probe on DP */
				rockchip_chg_enable_secondary_det(&rphy, true);
			} else {
				/* USB SDP charger found */
				chg_type = POWER_SUPPLY_TYPE_USB;
				goto out;
			}
		}
	}

	mdelay(CHG_SECONDARY_DET_TIME);
	vout = property_enabled(rphy.grf_base,
				&rphy.phy_cfg->chg_det.dcp_det);
	/* stage 4, turn off voltage source */
	rockchip_chg_enable_secondary_det(&rphy, false);
	if (vout)
		chg_type = POWER_SUPPLY_TYPE_USB_DCP;
	else
		chg_type = POWER_SUPPLY_TYPE_USB_CDP;

out:
	printf("charger is %s\n", chg_to_string(chg_type));

	return chg_type;
}

static int rockchip_usb2phy_init(struct phy *phy)
{
	struct rockchip_usb2phy *rphy;
	const struct rockchip_usb2phy_port_cfg *port_cfg;
	void __iomem *base;

	rphy = dev_get_priv(phy->dev);
	base = get_reg_base(rphy);

	if (phy->id == USB2PHY_PORT_OTG) {
		port_cfg = &rphy->phy_cfg->port_cfgs[USB2PHY_PORT_OTG];
	} else if (phy->id == USB2PHY_PORT_HOST) {
		port_cfg = &rphy->phy_cfg->port_cfgs[USB2PHY_PORT_HOST];
	} else {
		dev_err(phy->dev, "phy id %lu not support", phy->id);
		return -EINVAL;
	}

	property_enable(base, &port_cfg->phy_sus, false);

	/* waiting for the utmi_clk to become stable */
	udelay(2000);

	return 0;
}

static int rockchip_usb2phy_exit(struct phy *phy)
{
	struct rockchip_usb2phy *rphy;
	const struct rockchip_usb2phy_port_cfg *port_cfg;
	void __iomem *base;

	rphy = dev_get_priv(phy->dev);
	base = get_reg_base(rphy);

	if (phy->id == USB2PHY_PORT_OTG) {
		port_cfg = &rphy->phy_cfg->port_cfgs[USB2PHY_PORT_OTG];
	} else if (phy->id == USB2PHY_PORT_HOST) {
		port_cfg = &rphy->phy_cfg->port_cfgs[USB2PHY_PORT_HOST];
	} else {
		dev_err(phy->dev, "phy id %lu not support", phy->id);
		return -EINVAL;
	}

	property_enable(base, &port_cfg->phy_sus, true);

	return 0;
}

static int rockchip_usb2phy_probe(struct udevice *dev)
{
	const struct rockchip_usb2phy_cfg *phy_cfgs;
	struct rockchip_usb2phy *rphy;
	struct udevice *parent;
	u32 reg, index;

	rphy = dev_get_priv(dev);

	parent = dev_get_parent(dev);
	if (!parent) {
		dev_dbg(dev, "could not find u2phy parent\n");
		if (ofnode_read_bool(dev_ofnode(dev), "rockchip,grf"))
			rphy->usbgrf_base =
				syscon_get_first_range(ROCKCHIP_SYSCON_GRF);
	} else {
		rphy->grf_base =
			(void __iomem *)((uintptr_t)dev_read_addr(parent));
	}

	if (rphy->grf_base <= 0) {
		dev_err(dev, "get syscon grf failed\n");
		return -EINVAL;
	}

	if (ofnode_read_u32(dev_ofnode(dev), "reg", &reg)) {
		dev_err(dev, "could not read reg\n");
		return -EINVAL;
	}

	if (ofnode_read_bool(dev_ofnode(dev), "rockchip,usbgrf")) {
		rphy->usbgrf_base =
			syscon_get_first_range(ROCKCHIP_SYSCON_USBGRF);
		if (rphy->usbgrf_base <= 0) {
			dev_err(dev, "get syscon usbgrf failed\n");
			return -EINVAL;
		}
	} else {
		rphy->usbgrf_base = NULL;
	}

	phy_cfgs =
		(const struct rockchip_usb2phy_cfg *)dev_get_driver_data(dev);
	if (!phy_cfgs) {
		dev_err(dev, "unable to get phy_cfgs\n");
		return -EINVAL;
	}

	/* find out a proper config which can be matched with dt. */
	index = 0;
	while (phy_cfgs[index].reg) {
		if (phy_cfgs[index].reg == reg) {
			rphy->phy_cfg = &phy_cfgs[index];
			break;
		}
		++index;
	}

	if (!rphy->phy_cfg) {
		dev_err(dev, "no phy-config can be matched\n");
		return -EINVAL;
	}

	return 0;
}

static struct phy_ops rockchip_usb2phy_ops = {
	.init = rockchip_usb2phy_init,
	.exit = rockchip_usb2phy_exit,
};

static const struct rockchip_usb2phy_cfg rk312x_phy_cfgs[] = {
	{
		.reg = 0x17c,
		.num_ports	= 2,
		.clkout_ctl	= { 0x0190, 15, 15, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_OTG] = {
				.phy_sus	= { 0x017c, 8, 0, 0, 0x1d1 },
				.bvalid_det_en	= { 0x017c, 14, 14, 0, 1 },
				.bvalid_det_st	= { 0x017c, 15, 15, 0, 1 },
				.bvalid_det_clr	= { 0x017c, 15, 15, 0, 1 },
				.iddig_output	= { 0x017c, 10, 10, 0, 1 },
				.iddig_en	= { 0x017c, 9, 9, 0, 1 },
				.idfall_det_en  = { 0x01a0, 2, 2, 0, 1 },
				.idfall_det_st  = { 0x01a0, 3, 3, 0, 1 },
				.idfall_det_clr = { 0x01a0, 3, 3, 0, 1 },
				.idrise_det_en  = { 0x01a0, 0, 0, 0, 1 },
				.idrise_det_st  = { 0x01a0, 1, 1, 0, 1 },
				.idrise_det_clr = { 0x01a0, 1, 1, 0, 1 },
				.ls_det_en	= { 0x017c, 12, 12, 0, 1 },
				.ls_det_st	= { 0x017c, 13, 13, 0, 1 },
				.ls_det_clr	= { 0x017c, 13, 13, 0, 1 },
				.utmi_bvalid	= { 0x014c, 5, 5, 0, 1 },
				.utmi_iddig	= { 0x014c, 8, 8, 0, 1 },
				.utmi_ls	= { 0x014c, 7, 6, 0, 1 },
			},
			[USB2PHY_PORT_HOST] = {
				.phy_sus	= { 0x0194, 8, 0, 0, 0x1d1 },
				.ls_det_en	= { 0x0194, 14, 14, 0, 1 },
				.ls_det_st	= { 0x0194, 15, 15, 0, 1 },
				.ls_det_clr	= { 0x0194, 15, 15, 0, 1 }
			}
		},
		.chg_det = {
			.opmode		= { 0x017c, 3, 0, 5, 1 },
			.cp_det		= { 0x02c0, 6, 6, 0, 1 },
			.dcp_det	= { 0x02c0, 5, 5, 0, 1 },
			.dp_det		= { 0x02c0, 7, 7, 0, 1 },
			.idm_sink_en	= { 0x0184, 8, 8, 0, 1 },
			.idp_sink_en	= { 0x0184, 7, 7, 0, 1 },
			.idp_src_en	= { 0x0184, 9, 9, 0, 1 },
			.rdm_pdwn_en	= { 0x0184, 10, 10, 0, 1 },
			.vdm_src_en	= { 0x0184, 12, 12, 0, 1 },
			.vdp_src_en	= { 0x0184, 11, 11, 0, 1 },
		},
	},
	{ /* sentinel */ }
};

static const struct rockchip_usb2phy_cfg rk3328_phy_cfgs[] = {
	{
		.reg = 0x100,
		.num_ports	= 2,
		.clkout_ctl	= { 0x108, 4, 4, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_OTG] = {
				.phy_sus	= { 0x0100, 8, 0, 0, 0x1d1 },
				.bvalid_det_en	= { 0x0110, 2, 2, 0, 1 },
				.bvalid_det_st	= { 0x0114, 2, 2, 0, 1 },
				.bvalid_det_clr = { 0x0118, 2, 2, 0, 1 },
				.iddig_output	= { 0x0100, 10, 10, 0, 1 },
				.iddig_en	= { 0x0100, 9, 9, 0, 1 },
				.idfall_det_en	= { 0x0110, 5, 5, 0, 1 },
				.idfall_det_st	= { 0x0114, 5, 5, 0, 1 },
				.idfall_det_clr = { 0x0118, 5, 5, 0, 1 },
				.idrise_det_en	= { 0x0110, 4, 4, 0, 1 },
				.idrise_det_st	= { 0x0114, 4, 4, 0, 1 },
				.idrise_det_clr = { 0x0118, 4, 4, 0, 1 },
				.ls_det_en	= { 0x0110, 0, 0, 0, 1 },
				.ls_det_st	= { 0x0114, 0, 0, 0, 1 },
				.ls_det_clr	= { 0x0118, 0, 0, 0, 1 },
				.utmi_avalid	= { 0x0120, 10, 10, 0, 1 },
				.utmi_bvalid	= { 0x0120, 9, 9, 0, 1 },
				.utmi_iddig	= { 0x0120, 6, 6, 0, 1 },
				.utmi_ls	= { 0x0120, 5, 4, 0, 1 },
				.vbus_det_en	= { 0x001c, 15, 15, 1, 0 },
			},
			[USB2PHY_PORT_HOST] = {
				.phy_sus	= { 0x104, 8, 0, 0, 0x1d1 },
				.ls_det_en	= { 0x110, 1, 1, 0, 1 },
				.ls_det_st	= { 0x114, 1, 1, 0, 1 },
				.ls_det_clr	= { 0x118, 1, 1, 0, 1 },
				.utmi_ls	= { 0x120, 17, 16, 0, 1 },
				.utmi_hstdet	= { 0x120, 19, 19, 0, 1 }
			}
		},
		.chg_det = {
			.opmode		= { 0x0100, 3, 0, 5, 1 },
			.cp_det		= { 0x0120, 24, 24, 0, 1 },
			.dcp_det	= { 0x0120, 23, 23, 0, 1 },
			.dp_det		= { 0x0120, 25, 25, 0, 1 },
			.idm_sink_en	= { 0x0108, 8, 8, 0, 1 },
			.idp_sink_en	= { 0x0108, 7, 7, 0, 1 },
			.idp_src_en	= { 0x0108, 9, 9, 0, 1 },
			.rdm_pdwn_en	= { 0x0108, 10, 10, 0, 1 },
			.vdm_src_en	= { 0x0108, 12, 12, 0, 1 },
			.vdp_src_en	= { 0x0108, 11, 11, 0, 1 },
		},
	},
	{ /* sentinel */ }
};

static const struct rockchip_usb2phy_cfg rv1108_phy_cfgs[] = {
	{
		.reg = 0x100,
		.num_ports	= 2,
		.clkout_ctl	= { 0x108, 4, 4, 1, 0 },
		.port_cfgs	= {
			[USB2PHY_PORT_OTG] = {
				.phy_sus	= { 0x0100, 8, 0, 0, 0x1d1 },
				.bvalid_det_en	= { 0x0680, 3, 3, 0, 1 },
				.bvalid_det_st	= { 0x0690, 3, 3, 0, 1 },
				.bvalid_det_clr = { 0x06a0, 3, 3, 0, 1 },
				.ls_det_en	= { 0x0680, 2, 2, 0, 1 },
				.ls_det_st	= { 0x0690, 2, 2, 0, 1 },
				.ls_det_clr	= { 0x06a0, 2, 2, 0, 1 },
				.utmi_bvalid	= { 0x0804, 10, 10, 0, 1 },
				.utmi_ls	= { 0x0804, 13, 12, 0, 1 },
			},
			[USB2PHY_PORT_HOST] = {
				.phy_sus	= { 0x0104, 8, 0, 0, 0x1d1 },
				.ls_det_en	= { 0x0680, 4, 4, 0, 1 },
				.ls_det_st	= { 0x0690, 4, 4, 0, 1 },
				.ls_det_clr	= { 0x06a0, 4, 4, 0, 1 },
				.utmi_ls	= { 0x0804, 9, 8, 0, 1 },
				.utmi_hstdet	= { 0x0804, 7, 7, 0, 1 }
			}
		},
		.chg_det = {
			.opmode		= { 0x0100, 3, 0, 5, 1 },
			.cp_det		= { 0x0804, 1, 1, 0, 1 },
			.dcp_det	= { 0x0804, 0, 0, 0, 1 },
			.dp_det		= { 0x0804, 2, 2, 0, 1 },
			.idm_sink_en	= { 0x0108, 8, 8, 0, 1 },
			.idp_sink_en	= { 0x0108, 7, 7, 0, 1 },
			.idp_src_en	= { 0x0108, 9, 9, 0, 1 },
			.rdm_pdwn_en	= { 0x0108, 10, 10, 0, 1 },
			.vdm_src_en	= { 0x0108, 12, 12, 0, 1 },
			.vdp_src_en	= { 0x0108, 11, 11, 0, 1 },
		},
	},
	{ /* sentinel */ }
};

static const struct udevice_id rockchip_usb2phy_ids[] = {
	{ .compatible = "rockchip,rk3128-usb2phy", .data = (ulong)&rk312x_phy_cfgs },
	{ .compatible = "rockchip,rk3328-usb2phy", .data = (ulong)&rk3328_phy_cfgs },
	{ .compatible = "rockchip,rv1108-usb2phy", .data = (ulong)&rv1108_phy_cfgs },
	{ }
};

U_BOOT_DRIVER(rockchip_usb2phy) = {
	.name		= "rockchip_usb2phy",
	.id		= UCLASS_PHY,
	.of_match	= rockchip_usb2phy_ids,
	.ops		= &rockchip_usb2phy_ops,
	.probe		= rockchip_usb2phy_probe,
	.priv_auto_alloc_size = sizeof(struct rockchip_usb2phy),
};
