/* r_bsp_pin_config.h -- intentionally empty.
 *
 * The Renesas FIT BSP supports an auto-generated pin-init table here, normally
 * produced by the Smart Configurator GUI. We don't run Smart Configurator on
 * this Pi5; instead all pin-mux (PFS unlock, MPC PSEL, PMR/PDR/PODR ordering)
 * is done explicitly in src/hw_init.c so the values are visible and citable
 * in code review.
 *
 * Leaving this file empty (but present) suppresses r_bsp's "missing pin
 * config" warning while the BSP build looks for it.
 */
#pragma once
