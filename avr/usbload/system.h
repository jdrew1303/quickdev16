/*
 * =====================================================================================
 *
 * ________        .__        __    ________               ____  ________
 * \_____  \  __ __|__| ____ |  | __\______ \   _______  _/_   |/  _____/
 *  /  / \  \|  |  \  |/ ___\|  |/ / |    |  \_/ __ \  \/ /|   /   __  \
 * /   \_/.  \  |  /  \  \___|    <  |    `   \  ___/\   / |   \  |__\  \
 * \_____\ \_/____/|__|\___  >__|_ \/_______  /\___  >\_/  |___|\_____  /
 *        \__>             \/     \/        \/     \/                 \/
 *
 *                                  www.optixx.org
 *
 *
 *        Version:  1.0
 *        Created:  07/21/2009 03:32:16 PM
 *         Author:  david@optixx.org
 *
 * =====================================================================================
 */



#ifndef __SYSTEM_H__
#define __SYSTEM_H__



typedef struct system_t {
    enum bus_mode_e   { MODE_AVR, MODE_SNES } bus_mode;
    enum rom_mode_e   { LOROM, HIROM } rom_mode;
    enum reset_line_e { RESET_OFF, RESET_ON } reset_line;
    enum irq_line_e   { IRQ_ON, IRQ_OFF } irq_line;
    enum wr_line_e    { WR_DISABLE, WR_ENABLE } wr_line;
    enum reset_irq_e   { RESET_IRQ_OFF, RESET_IRQ_ON } reset_irq;

    uint8_t snes_reset_count;
    uint8_t avr_reset_count;
} system_t;

void system_init(void);


#endif
