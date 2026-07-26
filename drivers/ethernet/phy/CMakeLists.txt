# SPDX-License-Identifier: Apache-2.0

zephyr_library()
zephyr_library_sources_ifdef(CONFIG_PHY_TI_DP83TC812 phy_ti_dp83tc812.c)
