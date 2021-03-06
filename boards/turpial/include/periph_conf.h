/*
 * Copyright (C) 2020 Locha Inc
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup         boards_turpial
 * @{
 *
 * @file
 * @brief           Peripheral MCU configuration for Locha Mesh Turpial
 *
 * @author          Jean Pierre Dudey <jeandudey@hotmail.com>
 */

#ifndef PERIPH_CONF_H
#define PERIPH_CONF_H

#include "periph_cpu.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
* @name    Clock configuration
* @{
*/
/* the main clock is fixed to 48MHZ */
#define CLOCK_CORECLOCK     (48000000U)
/** @} */

/**
* @name    Timer configuration
*
* General purpose timers (GPT[0-3]) are configured consecutively and in order
* (without gaps) starting from GPT0, i.e. if multiple timers are enabled.
*
* @{
*/
static const timer_conf_t timer_config[] = {
 {
     .cfg = GPT_CFG_16T,
     .chn = 2,
 },
 {
     .cfg = GPT_CFG_32T,
     .chn = 1,
 },
 {
     .cfg = GPT_CFG_16T,
     .chn = 2,
 },
 {
     .cfg = GPT_CFG_32T,
     .chn = 1,
 }
};

#define TIMER_NUMOF         ARRAY_SIZE(timer_config)
/** @} */

/**
 * @name    UART configuration
 *
 * The used LAUNCHXL-CC1312R board has available a single UART device throught
 * the debugger, so all we need to configure are the RX and TX pins.
 *
 * Optionally we can enable hardware flow control, by setting UART_HW_FLOW_CTRL
 * to 1 and defining pins for cts_pin and rts_pin.
 *
 * Add a second UART configuration if using external pins.
 * @{
 */

static const uart_conf_t uart_config[] = {
 {
     .regs = UART0,
     .tx_pin = 3,
     .rx_pin = 2,
#ifdef MODULE_PERIPH_UART_HW_FC
     .rts_pin = 18,
     .cts_pin = 19,
#endif
     .intn = UART0_IRQN
 },
 {
     .regs = UART1,
     .tx_pin = 11,
     .rx_pin = 12,
#ifdef MODULE_PERIPH_UART_HW_FC
     .rts_pin = 0,
     .cts_pin = 0,
#endif
     .intn = UART1_IRQN
 }
};
#define UART_NUMOF          ARRAY_SIZE(uart_config)
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* PERIPH_CONF_H */
/** @} */
