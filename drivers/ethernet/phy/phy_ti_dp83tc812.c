/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Driver for the TI DP83TC812 100BASE-T1 Automotive Ethernet PHY.
 *
 * Initialization follows the SNLA389G application note (Tables 3-1 and 3-2)
 * with the full vendor interoperability register sequence for both master and
 * slave modes.  Link state is polled via the standard Clause-22 BMSR register.
 */

#define DT_DRV_COMPAT ti_dp83tc812

#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <zephyr/net/phy.h>
#include <zephyr/net/mii.h>
#include <zephyr/net/mdio.h>
#include <zephyr/drivers/mdio.h>
#include <zephyr/random/random.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(phy_ti_dp83tc812, CONFIG_PHY_LOG_LEVEL);

/* DP83TC812 PHY identifier — revision nibble masked out */
#define DP83TC812_PHY_ID      0x2000A270U
#define DP83TC812_PHY_ID_MASK 0xFFFFFFF0U

/* Retry parameters while waiting for the PHY to come out of reset */
#define DP83TC812_RESET_POLL_DELAY_US 1000U
#define DP83TC812_RESET_RETRY_COUNT   500U

/*
 * Vendor reset/control register (MMD 0x1F, reg 0x001F) — SNLA389G Table 3-1.
 * Both bits are self-clearing.  Hard reset requires ≥2 ms SMI silence afterwards
 * (SNLA431 Table 4-7, T6.2).
 */
#define DP83TC812_VND_RESET_REG     0x001FU
#define DP83TC812_VND_HARD_RESET    BIT(15)
#define DP83TC812_VND_SOFT_RESET    BIT(14)

/* TX holdoff during configuration (SNLA389G Table 3-1) */
#define DP83TC812_TX_CTRL_REG       0x0523U
#define DP83TC812_TX_CTRL_DISABLE   BIT(0)

/* master-slave enum indices match the DT binding enum order */
#define MASTER_SLAVE_MASTER 0
#define MASTER_SLAVE_SLAVE  1
#define MASTER_SLAVE_AUTO   2

struct dp83tc812_config {
	const struct device *mdio;
	uint8_t phy_addr;
	uint8_t master_slave;
};

struct dp83tc812_data {
	const struct device *dev;
	struct phy_link_state state;
	phy_callback_t cb;
	void *cb_data;
	struct k_work_delayable phy_work;
	/* AUTO mode state: current role and when to next attempt a flip */
	k_timepoint_t auto_flip_timeout;
	bool auto_is_master;
};

/* ------------------------------------------------------------------ */
/* Low-level MDIO helpers                                               */
/* ------------------------------------------------------------------ */

static int dp83tc812_c22_read(const struct device *dev, uint16_t reg, uint16_t *val)
{
	const struct dp83tc812_config *cfg = dev->config;

	return mdio_read(cfg->mdio, cfg->phy_addr, reg, val);
}

static int dp83tc812_c22_write(const struct device *dev, uint16_t reg, uint16_t val)
{
	const struct dp83tc812_config *cfg = dev->config;

	return mdio_write(cfg->mdio, cfg->phy_addr, reg, val);
}

/*
 * Clause 45 access via Clause 22 indirect MMD registers (0x0D / 0x0E).
 * The STM32 MDIO driver only exposes Clause 22 read/write, so we use the
 * standard IEEE 802.3 MMD indirect access mechanism instead of direct C45.
 */
static int dp83tc812_c45_read(const struct device *dev, uint16_t devad, uint16_t reg,
			      uint16_t *val)
{
	int ret;

	ret = dp83tc812_c22_write(dev, MII_MMD_ACR,
				  MII_MMD_ACR_ADDR | (devad & MII_MMD_ACR_DEVAD_MASK));
	if (ret < 0) {
		return ret;
	}
	ret = dp83tc812_c22_write(dev, MII_MMD_AADR, reg);
	if (ret < 0) {
		return ret;
	}
	ret = dp83tc812_c22_write(dev, MII_MMD_ACR,
				  MII_MMD_ACR_DATA_NO_POS_INC | (devad & MII_MMD_ACR_DEVAD_MASK));
	if (ret < 0) {
		return ret;
	}
	return dp83tc812_c22_read(dev, MII_MMD_AADR, val);
}

static int dp83tc812_c45_write(const struct device *dev, uint16_t devad, uint16_t reg,
			       uint16_t val)
{
	int ret;

	ret = dp83tc812_c22_write(dev, MII_MMD_ACR,
				  MII_MMD_ACR_ADDR | (devad & MII_MMD_ACR_DEVAD_MASK));
	if (ret < 0) {
		return ret;
	}
	ret = dp83tc812_c22_write(dev, MII_MMD_AADR, reg);
	if (ret < 0) {
		return ret;
	}
	ret = dp83tc812_c22_write(dev, MII_MMD_ACR,
				  MII_MMD_ACR_DATA_NO_POS_INC | (devad & MII_MMD_ACR_DEVAD_MASK));
	if (ret < 0) {
		return ret;
	}
	return dp83tc812_c22_write(dev, MII_MMD_AADR, val);
}

/* ------------------------------------------------------------------ */
/* SNLA389G init tables (all writes to MMD 0x1F / VENDOR_SPECIFIC2)   */
/* ------------------------------------------------------------------ */

struct dp83tc812_reg_val {
	uint16_t reg;
	uint16_t val;
};

/* Master-mode interoperability registers (SNLA389G Table 3-1) */
static const struct dp83tc812_reg_val dp83tc812_master_cfg[] = {
	{ 0x081C, 0x0FE2 },
	{ 0x0873, 0x0021 },
	{ 0x089E, 0x0010 },
	{ 0x0874, 0x6866 },
	{ 0x0875, 0x6868 },
	{ 0x0812, 0x00EE },
	{ 0x0816, 0x0300 },
	{ 0x0806, 0x293A },
	{ 0x0807, 0x3348 },
	{ 0x0808, 0x3D56 },
	{ 0x083E, 0x045F },
	{ 0x0834, 0x8000 },
	{ 0x0862, 0x0330 },
	{ 0x0896, 0x32CB },
	{ 0x003E, 0x0009 },
	{ 0x0848, 0x0030 },
	{ 0x0830, 0x0143 },
	{ 0x080A, 0x0015 },
	{ 0x0820, 0x03AA },
	{ 0x0826, 0x1407 },
	{ 0x083D, 0x0047 },
	{ 0x086C, 0x1336 },
	{ 0x0856, 0x1000 },
	{ 0x0842, 0xBAB8 },
};

/* Slave-mode interoperability registers (SNLA389G Table 3-2) */
static const struct dp83tc812_reg_val dp83tc812_slave_cfg[] = {
	{ 0x0862, 0x0330 },
	{ 0x086E, 0x1868 },
	{ 0x0812, 0x00F4 },
	{ 0x0816, 0x0300 },
	{ 0x0873, 0x0021 },
	{ 0x0896, 0x22FF },
	{ 0x089E, 0x0000 },
};

/*
 * Registers common to both master and slave (SNLA389G Tables 3-1, 3-2).
 * Written after the mode-specific registers.  Entry 0x018B = 0x144B
 * enables autonomous link-up (bit 6) alongside TC10 configuration.
 */
static const struct dp83tc812_reg_val dp83tc812_common_cfg[] = {
	{ 0x08F3, 0x0015 },
	{ 0x08AD, 0x0019 },
	{ 0x08ED, 0x001D },
	{ 0x08EF, 0x0021 },
	{ 0x08F0, 0x0025 },
	{ 0x08F1, 0x0029 },
	{ 0x08F2, 0x002D },
	{ 0x0456, 0x0021 }, /* reduce MAC I/O slew rate */
	{ 0x085A, 0x3000 }, /* RF immunity */
	{ 0x085B, 0x3000 }, /* RF immunity */
	{ 0x0189, 0x0018 }, /* TC10 interoperability */
	{ 0x018B, 0x144B }, /* TC10 + autonomous mode enable (bit 6) */
	{ 0x0154, 0x0220 },
};

/* ------------------------------------------------------------------ */
/* PHY operations                                                       */
/* ------------------------------------------------------------------ */

static int dp83tc812_read_id(const struct device *dev, uint32_t *id)
{
	uint16_t val;

	if (dp83tc812_c22_read(dev, MII_PHYID1R, &val) < 0) {
		return -EIO;
	}
	*id = (uint32_t)val << 16;

	if (dp83tc812_c22_read(dev, MII_PHYID2R, &val) < 0) {
		return -EIO;
	}
	*id |= val;

	return 0;
}

/*
 * Trigger a vendor hard (bit 15) or soft (bit 14) reset via MMD 0x1F
 * register 0x001F and poll until the self-clearing bit is cleared.
 */
static int dp83tc812_vnd_reset(const struct device *dev, uint16_t reset_bit)
{
	const struct dp83tc812_config *cfg = dev->config;
	uint16_t val;
	int retries = DP83TC812_RESET_RETRY_COUNT;

	if (dp83tc812_c45_write(dev, MDIO_MMD_VENDOR_SPECIFIC2,
				DP83TC812_VND_RESET_REG, reset_bit) < 0) {
		return -EIO;
	}

	if (reset_bit & DP83TC812_VND_HARD_RESET) {
		k_busy_wait(2000U); /* 2 ms post-reset SMI silence (SNLA431 T6.2) */
	}

	do {
		k_busy_wait(DP83TC812_RESET_POLL_DELAY_US);
		if (dp83tc812_c45_read(dev, MDIO_MMD_VENDOR_SPECIFIC2,
				       DP83TC812_VND_RESET_REG, &val) < 0) {
			return -EIO;
		}
	} while ((val & reset_bit) && --retries > 0);

	if (retries == 0) {
		LOG_ERR("DP83TC812 (addr %u): vendor reset (bit 0x%04X) timed out",
			cfg->phy_addr, reset_bit);
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * Apply the full SNLA389G initialization sequence for the requested role.
 * Must be called after power-up and whenever the master/slave role changes.
 *
 * Sequence per SNLA389G Section 3:
 *  1. Hard reset
 *  2. Assert TX holdoff (reg 0x0523 = 1)
 *  3. Set IEEE 802.3bw master/slave (MMD 1, reg 0x0834)
 *  4. Write mode-specific interop registers (MMD 0x1F)
 *  5. Write common interop registers (MMD 0x1F), including autonomous-mode bit
 *  6. Soft reset to latch configuration
 *  7. Release TX holdoff (reg 0x0523 = 0)
 */
static int dp83tc812_apply_config(const struct device *dev, bool master)
{
	const struct dp83tc812_config *cfg = dev->config;
	const struct dp83tc812_reg_val *mode_tbl;
	size_t mode_len;
	size_t i;
	int ret;

	ret = dp83tc812_vnd_reset(dev, DP83TC812_VND_HARD_RESET);
	if (ret < 0) {
		return ret;
	}

	ret = dp83tc812_c45_write(dev, MDIO_MMD_VENDOR_SPECIFIC2,
				  DP83TC812_TX_CTRL_REG, DP83TC812_TX_CTRL_DISABLE);
	if (ret < 0) {
		return ret;
	}

	ret = dp83tc812_c45_write(dev, MDIO_MMD_PMAPMD, MDIO_PMA_PMD_BT1_CTRL,
				  master ? 0xC000U : 0x8000U);
	if (ret < 0) {
		return ret;
	}

	if (master) {
		mode_tbl = dp83tc812_master_cfg;
		mode_len = ARRAY_SIZE(dp83tc812_master_cfg);
	} else {
		mode_tbl = dp83tc812_slave_cfg;
		mode_len = ARRAY_SIZE(dp83tc812_slave_cfg);
	}

	for (i = 0; i < mode_len; i++) {
		ret = dp83tc812_c45_write(dev, MDIO_MMD_VENDOR_SPECIFIC2,
					  mode_tbl[i].reg, mode_tbl[i].val);
		if (ret < 0) {
			return ret;
		}
	}

	for (i = 0; i < ARRAY_SIZE(dp83tc812_common_cfg); i++) {
		ret = dp83tc812_c45_write(dev, MDIO_MMD_VENDOR_SPECIFIC2,
					  dp83tc812_common_cfg[i].reg,
					  dp83tc812_common_cfg[i].val);
		if (ret < 0) {
			return ret;
		}
	}

	ret = dp83tc812_vnd_reset(dev, DP83TC812_VND_SOFT_RESET);
	if (ret < 0) {
		return ret;
	}

	ret = dp83tc812_c45_write(dev, MDIO_MMD_VENDOR_SPECIFIC2,
				  DP83TC812_TX_CTRL_REG, 0);
	if (ret < 0) {
		return ret;
	}

	LOG_DBG("DP83TC812 (addr %u): SNLA389G config applied (%s)",
		cfg->phy_addr, master ? "master" : "slave");

	return 0;
}

/* ------------------------------------------------------------------ */
/* PHY API                                                              */
/* ------------------------------------------------------------------ */

static int dp83tc812_get_link_state(const struct device *dev, struct phy_link_state *state)
{
	struct dp83tc812_data *data = dev->data;

	state->speed = data->state.speed;
	state->is_up = data->state.is_up;

	return 0;
}

static int dp83tc812_link_cb_set(const struct device *dev, phy_callback_t cb, void *user_data)
{
	struct dp83tc812_data *data = dev->data;

	data->cb = cb;
	data->cb_data = user_data;

	/* Fire the callback immediately with current state */
	if (cb != NULL) {
		cb(dev, &data->state, user_data);
	}

	return 0;
}

static int dp83tc812_reg_read(const struct device *dev, uint16_t reg_addr, uint32_t *out)
{
	return dp83tc812_c22_read(dev, reg_addr, (uint16_t *)out);
}

static int dp83tc812_reg_write(const struct device *dev, uint16_t reg_addr, uint32_t val)
{
	return dp83tc812_c22_write(dev, reg_addr, (uint16_t)val);
}

/* ------------------------------------------------------------------ */
/* Link monitor work                                                    */
/* ------------------------------------------------------------------ */

static void dp83tc812_work_handler(struct k_work *work)
{
	struct k_work_delayable *dwork = k_work_delayable_from_work(work);
	struct dp83tc812_data *data = CONTAINER_OF(dwork, struct dp83tc812_data, phy_work);
	const struct device *dev = data->dev;
	const struct dp83tc812_config *cfg = dev->config;
	uint16_t bmsr;
	bool link_up;

	/* BMSR bit 2 is latched-low; read twice to get the current value */
	if (dp83tc812_c22_read(dev, MII_BMSR, &bmsr) < 0 ||
	    dp83tc812_c22_read(dev, MII_BMSR, &bmsr) < 0) {
		goto reschedule;
	}

	link_up = (bmsr & MII_BMSR_LINK_STATUS) != 0;

	/* In AUTO mode, flip master/slave and re-apply full SNLA389G config */
	if (cfg->master_slave == MASTER_SLAVE_AUTO && !link_up &&
	    sys_timepoint_expired(data->auto_flip_timeout)) {

		data->auto_flip_timeout =
			sys_timepoint_calc(K_MSEC(1000 + (sys_rand32_get() % 2000)));
		data->auto_is_master = !data->auto_is_master;

		if (dp83tc812_apply_config(dev, data->auto_is_master) < 0) {
			LOG_WRN("DP83TC812 (addr %u): AUTO flip to %s failed",
				cfg->phy_addr,
				data->auto_is_master ? "master" : "slave");
		}
	}

	if (link_up != data->state.is_up) {
		data->state.is_up = link_up;
		LOG_INF("DP83TC812 (addr %u): link %s", cfg->phy_addr,
			link_up ? "up  (100BASE-T1 full-duplex)" : "down");

		if (data->cb != NULL) {
			data->cb(dev, &data->state, data->cb_data);
		}
	}

reschedule:
	k_work_reschedule(&data->phy_work, K_MSEC(CONFIG_PHY_MONITOR_PERIOD));
}

/* ------------------------------------------------------------------ */
/* Initialisation                                                       */
/* ------------------------------------------------------------------ */

static int dp83tc812_init(const struct device *dev)
{
	const struct dp83tc812_config *cfg = dev->config;
	struct dp83tc812_data *data = dev->data;
	uint32_t phy_id = 0;
	int ret;

	data->dev = dev;
	data->cb = NULL;
	data->state.is_up = false;
	data->state.speed = LINK_FULL_100BASE;

	if (!device_is_ready(cfg->mdio)) {
		LOG_ERR("MDIO bus device not ready");
		return -ENODEV;
	}

	ret = WAIT_FOR(dp83tc812_read_id(dev, &phy_id) == 0 &&
		       (phy_id & DP83TC812_PHY_ID_MASK) == DP83TC812_PHY_ID,
		       DP83TC812_RESET_RETRY_COUNT * DP83TC812_RESET_POLL_DELAY_US,
		       k_busy_wait(DP83TC812_RESET_POLL_DELAY_US));
	if (ret == 0) {
		LOG_ERR("DP83TC812 (addr %u): PHY ID mismatch (got 0x%08X)", cfg->phy_addr,
			phy_id);
		return -ENODEV;
	}

	bool start_as_master = (cfg->master_slave != MASTER_SLAVE_SLAVE);

	if (cfg->master_slave == MASTER_SLAVE_AUTO) {
		data->auto_is_master = true;
		data->auto_flip_timeout = sys_timepoint_calc(K_MSEC(1000));
	}

	ret = dp83tc812_apply_config(dev, start_as_master);
	if (ret < 0) {
		LOG_ERR("DP83TC812 (addr %u): SNLA389G init failed (%d)", cfg->phy_addr, ret);
		return ret;
	}

	k_work_init_delayable(&data->phy_work, dp83tc812_work_handler);
	k_work_reschedule(&data->phy_work, K_NO_WAIT);

	return 0;
}

static DEVICE_API(ethphy, dp83tc812_api) = {
	.get_link    = dp83tc812_get_link_state,
	.link_cb_set = dp83tc812_link_cb_set,
	.read        = dp83tc812_reg_read,
	.write       = dp83tc812_reg_write,
};

#define DP83TC812_INIT(n)                                                       \
	static const struct dp83tc812_config dp83tc812_config_##n = {           \
		.phy_addr    = DT_INST_REG_ADDR(n),                             \
		.mdio        = DEVICE_DT_GET(DT_INST_BUS(n)),                   \
		.master_slave = DT_INST_ENUM_IDX(n, master_slave),              \
	};                                                                      \
	static struct dp83tc812_data dp83tc812_data_##n;                        \
	DEVICE_DT_INST_DEFINE(n, dp83tc812_init, NULL,                          \
			      &dp83tc812_data_##n, &dp83tc812_config_##n,       \
			      POST_KERNEL, CONFIG_PHY_INIT_PRIORITY,             \
			      &dp83tc812_api);

DT_INST_FOREACH_STATUS_OKAY(DP83TC812_INIT)
