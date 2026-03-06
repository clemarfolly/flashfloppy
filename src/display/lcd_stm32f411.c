/*
 * lcd_stm32f411.c
 *
 * 1. HD44780 LCD controller via a PCF8574 I2C backpack.
 * 2. SSD1306 OLED controller driving 128x32 bitmap display.
 *
 * Written & released by Keir Fraser <keir.xen@gmail.com>
 * Adapted for STM32F411 (I2C v1 peripheral)
 *
 * This is free and unencumbered software released into the public domain.
 * See the file COPYING for more details, or visit <http://unlicense.org>.
 */

#include <stdint.h>
#include "mcu/stm32f411.h"
#include "mcu/stm32f411_regs.h"
#include "types.h"

/* PCF8574 pin assignment: D7-D6-D5-D4-BL-EN-RW-RS */
#define _D7 (1u<<7)
#define _D6 (1u<<6)
#define _D5 (1u<<5)
#define _D4 (1u<<4)
#define _BL (1u<<3)
#define _EN (1u<<2)
#define _RW (1u<<1)
#define _RS (1u<<0)

/* HD44780 commands */
#define CMD_ENTRYMODE    0x04
#define CMD_DISPLAYCTL   0x08
#define CMD_DISPLAYSHIFT 0x10
#define CMD_FUNCTIONSET  0x20
#define CMD_SETCGRADDR   0x40
#define CMD_SETDDRADDR   0x80
#define FS_2LINE         0x08

/* FF OSD command set */
#define OSD_BACKLIGHT    0x00
#define OSD_DATA         0x02
#define OSD_ROWS         0x10
#define OSD_HEIGHTS      0x20
#define OSD_BUTTONS      0x30
#define OSD_COLUMNS      0x40
struct packed i2c_osd_info {
    uint8_t protocol_ver;
    uint8_t fw_major, fw_minor;
    uint8_t buttons;
};

/* STM32 I2C peripheral. */
#define i2c i2c2

#define i2c_gpio gpiob
#define SCL 10      // PB10
#define SCL_AF 4    // Alternate function 4
#define SDA 3       // PB3
#define SDA_AF 9    // Alternate function 9

/* I2C error ISR. */
#define I2C_ERROR_IRQ 34
void IRQ_34(void) __attribute__((alias("IRQ_i2c_error")));

/* I2C event ISR. */
#define I2C_EVENT_IRQ 33
void IRQ_33(void) __attribute__((alias("IRQ_i2c_event")));

/* DMA1 Stream 7, Channel 7 -> I2C2_TX */
#define DMA_TX_STREAM 7
#define DMA_TX_CH 7
#define i2c_tx_dma dma1->stream[DMA_TX_STREAM]

/* DMA1 Stream 3, Channel 7 -> I2C2_RX */
#define DMA_RX_STREAM 3
#define DMA_RX_CH 7
#define i2c_rx_dma dma1->stream[DMA_RX_STREAM]

/* SR1 error flags */
#define I2C_SR1_ERRORS (I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF | \
                        I2C_SR1_OVR  | I2C_SR1_TIMEOUT)

/* APB1 = 50MHz on STM32F411 @ 100MHz.
 * Standard mode 100kHz: CCR = 50MHz / (2 * 100kHz) = 250.
 * TRISE = 50MHz * 1000ns + 1 = 51. */
#define I2C_CCR_100k  I2C_CCR_CCR(250)
#define I2C_TRISE_100k 51

/* Fast mode 400kHz, DUTY=0: CCR = 50MHz / (3 * 400kHz) = 42.
 * TRISE = 50MHz * 300ns + 1 = 16. */
#define I2C_CCR_400k  (I2C_CCR_FS | I2C_CCR_CCR(42))
#define I2C_TRISE_400k 16

bool_t has_osd;
uint8_t osd_buttons_tx;
uint8_t osd_buttons_rx;
#define OSD_no    0
#define OSD_read  1
#define OSD_write 2
static uint8_t in_osd, osd_ver;
#define OSD_I2C_ADDR 0x10

static uint8_t _bl;
static uint8_t i2c_addr;
static uint8_t i2c_dead;
static uint8_t i2c_row;
static bool_t is_oled_display;
static uint8_t oled_height;

/* Tracks whether current DMA transfer is RX or TX, for the event ISR. */
static bool_t i2c_dma_rd;

#define OLED_ADDR 0x3c
enum { OLED_unknown, OLED_ssd1306, OLED_sh1106 };
static uint8_t oled_model;
static void oled_init(void);
static unsigned int oled_prep_buffer(void);

#define I2C_RD TRUE
#define I2C_WR FALSE
static void i2c_start(uint8_t a, unsigned int nr, bool_t rd);

static void i2c_tx_tc(void);
static void i2c_rx_tc(void);

static volatile uint8_t refresh_count;

static uint8_t buffer[256] aligned(4);
static char text[4][40];

uint8_t lcd_columns, lcd_rows;

uint8_t display_mode = DM_banner;
#define menu_mode (display_mode == DM_menu)

#define DMA_TIMEOUT time_ms(200)
static struct timer timeout_timer;
static void timeout_fn(void *unused)
{
    IRQx_set_pending(I2C_ERROR_IRQ);
}

/* I2C Error ISR: Reset the peripheral and reinit everything. */
static void IRQ_i2c_error(void)
{
    /* Read and clear SR1 error flags (write 0 to clear). */
    uint16_t sr1 = i2c->sr1;
    printk("I2C: Error (%04x)\n", (uint16_t)(sr1 & I2C_SR1_ERRORS));
    i2c->sr1 = sr1 & ~I2C_SR1_ERRORS;

    /* Reset the I2C peripheral via SWRST. */
    i2c->cr1 = I2C_CR1_SWRST;
    i2c->cr1 = 0;

    /* Clear DMA channels. */
    i2c_tx_dma.cr = i2c_rx_dma.cr = 0;

    timer_cancel(&timeout_timer);

    lcd_init();
}

/*
 * I2C Event ISR.
 *
 * On I2C v1 the DMA handles the data bytes. We only need the event ISR to:
 *  - detect ADDR (start+address ACKed): clear by reading SR1 then SR2
 *  - detect BTF/STOPF after last DMA byte (DMA sets CR2_LAST so the
 *    peripheral generates NACK+STOP automatically on the final RX byte,
 *    and we get a BTF on TX after the last byte).
 *  - detect AF (NACK from slave) as an error.
 */
static void IRQ_i2c_event(void)
{
    uint16_t sr1 = i2c->sr1;

    if (sr1 & I2C_SR1_ERRORS) {
        IRQx_set_pending(I2C_ERROR_IRQ);
        return;
    }

    if (sr1 & I2C_SR1_ADDR) {
        /* Clear ADDR flag by reading SR1 (done) then SR2. */
        (void)i2c->sr2;
        return;
    }

    if (sr1 & I2C_SR1_BTF) {
        /* TX: all bytes sent, DMA complete - generate STOP. */
        if (!i2c_dma_rd) {
            i2c->cr1 |= I2C_CR1_STOP;
            i2c_tx_tc();
        }
        return;
    }

    if (sr1 & I2C_SR1_STOPF) {
        /* RX stop detected (set by hardware after NACK on last byte). */
        /* Clear STOPF: read SR1 (done above) then write CR1. */
        i2c->cr1 |= I2C_CR1_PE;
        i2c_rx_tc();
    }
}

/* Start an I2C DMA sequence. */
static void dma_start(unsigned int sz)
{
    unsigned int addr = in_osd ? OSD_I2C_ADDR : i2c_addr;

    ASSERT(sz <= sizeof(buffer));

    i2c_dma_rd = (in_osd == OSD_read);

    /* Disable I2C before reconfiguring. */
    i2c->cr1 &= ~I2C_CR1_PE;

    if (i2c_dma_rd) {

        /* RX path */
        i2c_rx_dma.cr = 0;
        i2c_rx_dma.ndtr = sz;
        i2c_rx_dma.m0ar = (uint32_t)(unsigned long)buffer;
        i2c_rx_dma.par  = (uint32_t)(unsigned long)&i2c->dr;
        i2c_rx_dma.cr   = (DMA_CR_CHSEL(DMA_RX_CH)   |
                           DMA_SxCR_MSIZE_8BIT  |
                           DMA_SxCR_PSIZE_8BIT  |
                           DMA_SxCR_MINC        |
                           DMA_SxCR_DIR_P2M     |
                           DMA_SxCR_EN);


        /* Enable I2C with DMA, ACK, LAST (auto-NACK on last byte). */
        i2c->cr1 = (I2C_CR1_PE | I2C_CR1_ACK);
        i2c->cr2 = (I2C_CR2_FREQ(50) | I2C_CR2_DMAEN |
                    I2C_CR2_LAST    | I2C_CR2_ITEVTEN |
                    I2C_CR2_ITERREN);

    } else {

        /* TX path */
        i2c_tx_dma.cr = 0;
        i2c_tx_dma.ndtr = sz;
        i2c_tx_dma.m0ar = (uint32_t)(unsigned long)buffer;
        i2c_tx_dma.par  = (uint32_t)(unsigned long)&i2c->dr;
        i2c_tx_dma.cr   = (DMA_CR_CHSEL(DMA_TX_CH)   |
                           DMA_SxCR_MSIZE_8BIT  |
                           DMA_SxCR_PSIZE_8BIT  |
                           DMA_SxCR_MINC        |
                           DMA_SxCR_DIR_M2P     |
                           DMA_SxCR_EN);

        /* Enable I2C with DMA. */
        i2c->cr1 = I2C_CR1_PE;
        i2c->cr2 = (I2C_CR2_FREQ(50) | I2C_CR2_DMAEN |
                    I2C_CR2_ITEVTEN  | I2C_CR2_ITERREN);
    }

    /* Kick off the START condition (triggers SB -> ADDR -> DMA). */
    i2c_start(addr, sz, i2c_dma_rd);

    timer_set(&timeout_timer, time_now() + DMA_TIMEOUT);
}

static void emit4(uint8_t **p, uint8_t val)
{
    *(*p)++ = val;
    *(*p)++ = val | _EN;
    *(*p)++ = val;
}

static void emit8(uint8_t **p, uint8_t val, uint8_t signals)
{
    signals |= _bl;
    emit4(p, (val & 0xf0) | signals);
    emit4(p, (val << 4) | signals);
}

static unsigned int osd_prep_buffer(void)
{
    uint16_t order = menu_mode ? 0x7903 : 0x7183;
    char *p;
    uint8_t *q = buffer;
    unsigned int row, rows, heights;
    int i;

    if (++in_osd == OSD_read) {
        memset(buffer, 0x11, sizeof(struct i2c_osd_info));
        return sizeof(struct i2c_osd_info);
    }

    if ((ff_cfg.osd_display_order != DORD_default)
        && (display_mode == DM_normal))
        order = ff_cfg.osd_display_order;

    heights = rows = 0;
    for (i = 3; i >= 0; i--) {
        row = order >> (i<<2);
        if ((rows == 0) && ((row&7) == 7))
            continue;
        rows++;
        heights <<= 1;
        if (row & 8)
            heights |= 1;
    }

    *q++ = OSD_BACKLIGHT | !!_bl;
    *q++ = OSD_COLUMNS | lcd_columns;
    *q++ = OSD_ROWS | rows;
    *q++ = OSD_HEIGHTS | heights;
    *q++ = OSD_BUTTONS | osd_buttons_tx;
    *q++ = OSD_DATA;
    for (row = 0; row < rows; row++) {
        p = text[(order >> (row * DORD_shift)) & DORD_row];
        memcpy(q, p, lcd_columns);
        q += lcd_columns;
    }

    if (i2c_addr == 0)
        refresh_count++;

    in_osd = OSD_write;

    return q - buffer;
}

static unsigned int lcd_prep_buffer(void)
{
    const static uint8_t row_offs[] = { 0x00, 0x40, 0x14, 0x54 };
    uint16_t order;
    char *p;
    uint8_t *q = buffer;
    unsigned int i, row;

    if (i2c_row == lcd_rows) {
        i2c_row++;
        if (has_osd)
            return osd_prep_buffer();
    }

    if (i2c_row > lcd_rows) {
        i2c_row = 0;
        refresh_count++;
    }

    order = (lcd_rows == 2) ? 0x7710 : 0x2103;
    if ((ff_cfg.display_order != DORD_default) && (display_mode == DM_normal))
        order = ff_cfg.display_order;

    row = (order >> (i2c_row * DORD_shift)) & DORD_row;
    p = (_bl && row < ARRAY_SIZE(text)) ? text[row] : NULL;

    emit8(&q, CMD_SETDDRADDR | row_offs[i2c_row], 0);
    for (i = 0; i < lcd_columns; i++)
        emit8(&q, p ? *p++ : ' ', _RS);

    i2c_row++;

    return q - buffer;
}

static void i2c_tx_tc(void)
{
    unsigned int dma_sz;

    in_osd = OSD_no;
    if (i2c_addr == 0) {
        dma_sz = osd_prep_buffer();
    } else {
        dma_sz = is_oled_display ? oled_prep_buffer() : lcd_prep_buffer();
    }
    dma_start(dma_sz);
}

static void i2c_rx_tc(void)
{
    struct i2c_osd_info *info = (struct i2c_osd_info *)buffer;

    osd_buttons_rx = info->buttons;

    dma_start(osd_prep_buffer());
}

/* Wait for SR1 flag(s) while checking for errors. */
static bool_t i2c_wait(uint16_t s)
{
    stk_time_t t = stk_now();
    
    while ((i2c->sr1 & s) != s) {
        if (i2c->sr1 & I2C_SR1_ERRORS) {
            i2c->sr1 = i2c->sr1 & ~I2C_SR1_ERRORS;
            return FALSE;
        }
        if (stk_diff(t, stk_now()) > stk_ms(10)) {
            i2c_dead = TRUE;
            return FALSE;
        }
    }
    return TRUE;
}

/*
 * Initiate an I2C transfer (synchronous START + address phase).
 * On I2C v1: set START, wait for SB, write address byte to DR,
 * wait for ADDR, then clear by reading SR2.
 */
static void i2c_start(uint8_t a, unsigned int nr, bool_t rd)
{
    /* Generate START condition. */
    i2c->cr1 |= I2C_CR1_START;

    /* Wait for SB (start bit sent). */
    if (!i2c_wait(I2C_SR1_SB))
        return;

    /* Send address byte: LSB=1 for read, LSB=0 for write. */
    i2c->dr = (a << 1) | (rd ? 1 : 0);

    /* Wait for ADDR (address ACKed by slave). */
    if (!i2c_wait(I2C_SR1_ADDR))
        return;

    /* Clear ADDR flag: read SR1 (done in i2c_wait) then SR2. */
    (void)i2c->sr2;
}

/* Synchronously generate STOP and wait for bus idle. */
static bool_t i2c_stop(void)
{
    stk_time_t t = stk_now();

    i2c->cr1 |= I2C_CR1_STOP;
    /* Wait until STOP condition is cleared by hardware (bus free). */
    while (i2c->cr1 & I2C_CR1_STOP) {
        if (stk_diff(t, stk_now()) > stk_ms(10)) {
            i2c_dead = TRUE;
            return FALSE;
        }
    }
    return TRUE;
}

/* Synchronously transmit one byte. */
static bool_t i2c_sync_write(uint8_t b)
{
    if (!i2c_wait(I2C_SR1_TXE))
        return FALSE;
    i2c->dr = b;
    return TRUE;
}

/* Synchronously receive one byte. */
static bool_t i2c_sync_read(uint8_t *pb)
{
    if (!i2c_wait(I2C_SR1_RXNE))
        return FALSE;
    *pb = i2c->dr;
    return TRUE;
}

static bool_t i2c_sync_write_txn(uint8_t addr, uint8_t *cmds, unsigned int nr)
{
    unsigned int i;

    /* Set up for synchronous TX (no DMA, no IRQ). */
    i2c->cr1 = I2C_CR1_PE;
    i2c->cr2 = I2C_CR2_FREQ(50);

    i2c_start(addr, nr, I2C_WR);

    for (i = 0; i < nr; i++)
        if (!i2c_sync_write(*cmds++))
            return FALSE;

    /* Wait for BTF (last byte fully shifted out) before STOP. */
    if (!i2c_wait(I2C_SR1_BTF))
        return FALSE;

    return i2c_stop();
}

static bool_t i2c_sync_read_txn(uint8_t addr, uint8_t *rsp, unsigned int nr)
{
    unsigned int i;

    i2c->cr1 = (I2C_CR1_PE | I2C_CR1_ACK);
    i2c->cr2 = I2C_CR2_FREQ(50);

    i2c_start(addr, nr, I2C_RD);

    for (i = 0; i < nr; i++) {
        /* On last byte: clear ACK and set STOP before reading DR,
         * so the peripheral NACKs the last byte as required by I2C. */
        if (i == (nr - 1))
            i2c->cr1 = (I2C_CR1_PE | I2C_CR1_STOP);
        if (!i2c_sync_read(rsp + i))
            return FALSE;
    }

    return TRUE;
}

static void write4(uint8_t val)
{
    i2c_sync_write(val);
    i2c_sync_write(val | _EN);
    i2c_sync_write(val);
}

static bool_t i2c_probe(uint8_t a)
{
    i2c->cr1 = I2C_CR1_PE;
    i2c->cr2 = I2C_CR2_FREQ(50);

    i2c->cr1 |= I2C_CR1_START;
    if (!i2c_wait(I2C_SR1_SB))
        return FALSE;

    /* Send address for write; if slave NACKs, AF will be set. */
    i2c->dr = (a << 1);
    if (!i2c_wait(I2C_SR1_ADDR)) {
        /* Clear AF. */
        i2c->sr1 = i2c->sr1 & ~I2C_SR1_AF;
        i2c->cr1 |= I2C_CR1_STOP;
        return FALSE;
    }
    (void)i2c->sr2;

    i2c->cr1 |= I2C_CR1_STOP;
    return TRUE;
}

static uint8_t i2c_probe_range(uint8_t s, uint8_t e)
{
    uint8_t a;
    for (a = s; (a <= e) && !i2c_dead; a++)
        if (i2c_probe(a))
            return a;
    return 0;
}

void lcd_clear(void)
{
    memset(text, ' ', sizeof(text));
}

void lcd_write(int col, int row, int min, const char *str)
{
    char c, *p;
    uint32_t oldpri;

    if (min < 0)
        min = lcd_columns;

    p = &text[row][col];

    oldpri = IRQ_save(I2C_IRQ_PRI);

    while ((c = *str++) && (col++ < lcd_columns)) {
        *p++ = c;
        min--;
    }
    while ((min-- > 0) && (col++ < lcd_columns))
        *p++ = ' ';

    IRQ_restore(oldpri);
}

void lcd_backlight(bool_t on)
{
    _bl = on ? _BL : 0;
}

void lcd_sync(void)
{
    uint8_t c = refresh_count;
    while ((uint8_t)(refresh_count - c) < 2)
        cpu_relax();
}

bool_t lcd_init(void)
{
    uint8_t a, *p;
    bool_t reinit = (i2c_addr != 0) || has_osd;

    i2c_dead = FALSE;
    i2c_row = 0;
    in_osd = OSD_no;
    osd_buttons_rx = 0;

    rcc->apb1enr |= RCC_APB1ENR_I2C2EN;

    gpio_configure_pin(i2c_gpio, SCL, GPO_opendrain(_2MHz, HIGH));
    gpio_configure_pin(i2c_gpio, SDA, GPO_opendrain(_2MHz, HIGH));
    delay_us(10);
    if (gpio_read_pin(i2c_gpio, SCL) && !gpio_read_pin(i2c_gpio, SDA)) {
        printk("I2C: SDA held by slave? Fixing... ");
        gpio_write_pin(i2c_gpio, SDA, FALSE);
        gpio_write_pin(i2c_gpio, SCL, FALSE);
        delay_us(10);
        gpio_write_pin(i2c_gpio, SCL, TRUE);
        delay_us(10);
        gpio_write_pin(i2c_gpio, SDA, TRUE);
        delay_us(10);
        printk("%s\n",
               !gpio_read_pin(i2c_gpio, SCL) || !gpio_read_pin(i2c_gpio, SDA)
               ? "Still held" : "Done");
    }

    if (!reinit) {
        bool_t scl, sda;
        gpio_configure_pin(i2c_gpio, SCL, GPI_pull_down);
        gpio_configure_pin(i2c_gpio, SDA, GPI_pull_down);
        delay_us(10);
        scl = gpio_read_pin(i2c_gpio, SCL);
        sda = gpio_read_pin(i2c_gpio, SDA);
        if (!scl || !sda) {
            printk("I2C: Invalid bus SCL=%u SDA=%u\n", scl, sda);
            goto fail;
        }
    }

    gpio_set_af(i2c_gpio, SCL, SCL_AF);
    gpio_set_af(i2c_gpio, SDA, SDA_AF);
    gpio_configure_pin(i2c_gpio, SCL, AFO_opendrain(_2MHz));
    gpio_configure_pin(i2c_gpio, SDA, AFO_opendrain(_2MHz));

    /* Reset and configure I2C for Standard Mode (100kHz).
     * APB1 = 50MHz on STM32F411 @ 100MHz system clock. */
    i2c->cr1 = I2C_CR1_SWRST;
    i2c->cr1 = 0;
    i2c->cr2 = I2C_CR2_FREQ(50);
    i2c->ccr = I2C_CCR_100k;
    i2c->trise = I2C_TRISE_100k;
    i2c->cr1 = I2C_CR1_PE;

    if (!reinit) {

        (void)i2c_probe(0);

        has_osd = i2c_probe(OSD_I2C_ADDR);
        a = i2c_probe_range(0x20, 0x27) ?: i2c_probe_range(0x38, 0x3f);
        if ((a == 0) && (i2c_dead || !has_osd
                         || ((ff_cfg.display_type & 3) != DISPLAY_auto))) {
            printk("I2C: %s\n",
                   i2c_dead ? "Bus locked up?" : "No device found");
            has_osd = FALSE;
            goto fail;
        }

        if (has_osd) {
            (void)i2c_sync_read_txn(OSD_I2C_ADDR, &osd_ver, 1);
            printk("I2C: FF OSD found (ver %x)\n", osd_ver);
        }

        is_oled_display = (ff_cfg.display_type & DISPLAY_oled) ? TRUE
            : (ff_cfg.display_type & DISPLAY_lcd) ? FALSE
            : ((a&~1) == OLED_ADDR);

        if (is_oled_display) {
            oled_height = (ff_cfg.display_type & DISPLAY_oled_64) ? 64 : 32;
            lcd_columns = (ff_cfg.oled_font == FONT_8x16) ? 16
                : (ff_cfg.display_type & DISPLAY_narrower) ? 16
                : (ff_cfg.display_type & DISPLAY_narrow) ? 18 : 21;
            lcd_rows = 4;
        } else {
            lcd_columns = (ff_cfg.display_type >> _DISPLAY_lcd_columns) & 63;
            lcd_rows = (ff_cfg.display_type >> _DISPLAY_lcd_rows) & 7;
        }

        if (a != 0) {
            printk("I2C: %s found at 0x%02x\n",
                   is_oled_display ? "OLED" : "LCD", a);
            i2c_addr = a;
        } else {
            is_oled_display = FALSE;
            lcd_columns = ff_cfg.osd_columns;
        }

        lcd_columns = max_t(uint8_t, lcd_columns, 16);
        lcd_columns = min_t(uint8_t, lcd_columns, 40);
        lcd_rows = max_t(uint8_t, lcd_rows, 2);
        lcd_rows = min_t(uint8_t, lcd_rows, 4);

        lcd_clear();
    }

    /* Enable the Event IRQ. */
    IRQx_set_prio(I2C_EVENT_IRQ, I2C_IRQ_PRI);
    IRQx_clear_pending(I2C_EVENT_IRQ);
    IRQx_enable(I2C_EVENT_IRQ);

    /* Enable the Error IRQ. */
    IRQx_set_prio(I2C_ERROR_IRQ, I2C_IRQ_PRI);
    IRQx_clear_pending(I2C_ERROR_IRQ);
    IRQx_enable(I2C_ERROR_IRQ);

    /* STM32F411: DMA1 ch4=I2C2_TX, ch5=I2C2_RX — no DMAMUX needed. */

    /* Initialise DMA1 channel 4 for I2C2 TX. */
    i2c_tx_dma.m0ar = (uint32_t)(unsigned long)buffer;
    i2c_tx_dma.par = (uint32_t)(unsigned long)&i2c->dr;

    /* Initialise DMA1 channel 5 for I2C2 RX. */
    i2c_rx_dma.m0ar = (uint32_t)(unsigned long)buffer;
    i2c_rx_dma.par = (uint32_t)(unsigned long)&i2c->dr;

    /* Timeout handler. */
    timer_init(&timeout_timer, timeout_fn, NULL);
    timer_set(&timeout_timer, time_now() + DMA_TIMEOUT);

    if (is_oled_display) {
        oled_init();
        return TRUE;
    } else if (i2c_addr == 0) {
        dma_start(osd_prep_buffer());
        return TRUE;
    }

    /* Initialise 4-bit HD44780 interface synchronously. */
    i2c->cr1 = I2C_CR1_PE;
    i2c->cr2 = I2C_CR2_FREQ(50);
    i2c_start(i2c_addr, 4*3, I2C_WR);
    write4(3 << 4);
    delay_us(4100);
    write4(3 << 4);
    delay_us(100);
    write4(3 << 4);
    write4(2 << 4);
    if (!i2c_wait(I2C_SR1_BTF))
        goto fail;
    i2c_stop();

    /* More initialisation. Send by DMA. */
    p = buffer;
    emit8(&p, CMD_FUNCTIONSET | FS_2LINE, 0);
    emit8(&p, CMD_DISPLAYCTL, 0);
    emit8(&p, CMD_ENTRYMODE | 2, 0);
    emit8(&p, CMD_DISPLAYCTL | 4, 0);
    dma_start(p - buffer);

    if (!reinit)
        lcd_backlight(TRUE);

    return TRUE;

fail:
    if (reinit)
        return FALSE;
    IRQx_disable(I2C_EVENT_IRQ);
    IRQx_disable(I2C_ERROR_IRQ);
    i2c->cr1 = I2C_CR1_SWRST;
    i2c->cr1 = 0;
    gpio_configure_pin(i2c_gpio, SCL, GPI_pull_up);
    gpio_configure_pin(i2c_gpio, SDA, GPI_pull_up);
    rcc->apb1enr &= ~RCC_APB1ENR_I2C2EN;
    return FALSE;
}

/* ---- OLED section: unchanged from AT32F435 version ---- */

extern const uint8_t oled_font_6x13[];
static void oled_convert_text_row_6x13(char *pc)
{
    unsigned int i, c;
    const uint8_t *p;
    uint8_t *q = buffer;
    const unsigned int w = 6;

    q[0] = q[128] = 0;
    q++;

    for (i = 0; i < lcd_columns; i++) {
        if ((c = *pc++ - 0x20) > 0x5e)
            c = '.' - 0x20;
        p = &oled_font_6x13[c * w * 2];
        memcpy(q, p, w);
        memcpy(q+128, p+w, w);
        q += w;
    }

    memset(q, 0, 127-lcd_columns*w);
    memset(q+128, 0, 127-lcd_columns*w);
}

#ifdef font_extra
extern const uint8_t oled_font_8x16[];
static void oled_convert_text_row_8x16(char *pc)
{
    unsigned int i, c;
    const uint8_t *p;
    uint8_t *q = buffer;
    const unsigned int w = 8;

    for (i = 0; i < lcd_columns; i++) {
        if ((c = *pc++ - 0x20) > 0x5e)
            c = '.' - 0x20;
        p = &oled_font_8x16[c * w * 2];
        memcpy(q, p, w);
        memcpy(q+128, p+w, w);
        q += w;
    }
}
#endif

static void oled_convert_text_row(char *pc)
{
#ifdef font_extra
    if (ff_cfg.oled_font == FONT_8x16)
        oled_convert_text_row_8x16(pc);
    else
#endif
        oled_convert_text_row_6x13(pc);
}

static unsigned int oled_queue_cmds(
    uint8_t *buf, const uint8_t *cmds, unsigned int nr)
{
    uint8_t *p = buf;

    while (nr--) {
        *p++ = 0x80;
        *p++ = *cmds++;
    }

    return p - buf;
}

static void oled_double_height(uint8_t *dst, uint8_t *src, uint8_t mask)
{
    const uint8_t tbl[] = {
        0x00, 0x03, 0x0c, 0x0f, 0x30, 0x33, 0x3c, 0x3f,
        0xc0, 0xc3, 0xcc, 0xcf, 0xf0, 0xf3, 0xfc, 0xff
    };
    uint8_t x, *p, *q;
    unsigned int i;
    if ((mask == 3) && (src == dst)) {
        p = src + 128;
        q = dst + 256;
        for (i = 0; i < 128; i++) {
            x = *--p;
            *--q = tbl[x>>4];
        }
        p = src + 128;
        for (i = 0; i < 128; i++) {
            x = *--p;
            *--q = tbl[x&15];
        }
    } else {
        p = src;
        q = dst;
        if (mask & 1) {
            for (i = 0; i < 128; i++) {
                x = *p++;
                *q++ = tbl[x&15];
            }
        }
        if (mask & 2) {
            p = src;
            for (i = 0; i < 128; i++) {
                x = *p++;
                *q++ = tbl[x>>4];
            }
        }
    }
}

static unsigned int oled_start_i2c(uint8_t *buf)
{
    static const uint8_t ssd1306_addr_cmds[] = {
        0x20, 0,
        0x21, 0, 127,
        0x22,
    }, ztech_addr_cmds[] = {
        0xda, 0x12,
        0x21, 4, 131,
    }, sh1106_addr_cmds[] = {
        0x10
    };

    uint8_t dynamic_cmds[4], *dc = dynamic_cmds;
    uint8_t *p = buf;

    if (oled_model == OLED_sh1106) {
        p += oled_queue_cmds(p, sh1106_addr_cmds, sizeof(sh1106_addr_cmds));
        *dc++ = (oled_height == 64) ? 0x02 : 0x00;
        *dc++ = 0xb0 + i2c_row;
    } else {
        p += oled_queue_cmds(p, ssd1306_addr_cmds, sizeof(ssd1306_addr_cmds));
        *dc++ = i2c_row;
        *dc++ = 7;
    }

    *dc++ = _bl ? 0xaf : 0xae;

    p += oled_queue_cmds(p, dynamic_cmds, dc - dynamic_cmds);

    if (ff_cfg.display_type & DISPLAY_ztech)
        p += oled_queue_cmds(p, ztech_addr_cmds, sizeof(ztech_addr_cmds));

    *p++ = 0x40;

    return p - buf;
}

static int oled_to_lcd_row(int in_row)
{
    uint16_t order;
    int i = 0, row;
    bool_t large = FALSE;

    order = (oled_height == 32) ? 0x7710 : menu_mode ? 0x7903 : 0x7183;
    if ((ff_cfg.display_order != DORD_default) && (display_mode == DM_normal))
        order = ff_cfg.display_order;

    for (;;) {
        large = !!(order & DORD_double);
        i += large ? 2 : 1;
        if (i > in_row)
            break;
        order >>= DORD_shift;
    }

    row = order & DORD_row;
    if (row < lcd_rows) {
        oled_convert_text_row(text[row]);
    } else {
        memset(buffer, 0, 256);
    }

    return large ? i - in_row : 0;
}

static unsigned int oled_prep_buffer(void)
{
    int size;
    uint8_t *p = buffer;

    if (i2c_row == (oled_height / 8)) {
        i2c_row++;
        if (has_osd)
            return osd_prep_buffer();
    }

    if (i2c_row > (oled_height / 8)) {
        i2c_row = 0;
        refresh_count++;
    }

    size = oled_to_lcd_row(i2c_row/2);
    if (size != 0) {
        oled_double_height(&buffer[128], &buffer[(size == 1) ? 128 : 0],
                           (i2c_row & 1) + 1);
    } else {
        if (!(i2c_row & 1))
            memcpy(&buffer[128], &buffer[0], 128);
    }

    p += oled_start_i2c(p);

    memcpy(p, &buffer[128], 128);
    p += 128;

    i2c_row++;

    return p - buffer;
}

static bool_t oled_probe_model(void)
{
    uint8_t cmd1[] = { 0x80, 0x00, 0xc0 };
    uint8_t cmd2[] = { 0x80, 0x00, 0xc0, 0x00 };
    uint8_t rsp[2];
    int i;
    uint8_t x, px = 0;
    uint8_t *rand = (uint8_t *)emit8;

    for (i = 0; i < 3; i++) {
        if (!i2c_sync_write_txn(i2c_addr, cmd1, sizeof(cmd1)))
            goto fail;
        if (!i2c_sync_read_txn(i2c_addr, rsp, sizeof(rsp)))
            goto fail;
        x = rsp[1];
        cmd2[3] = x ^ rand[i];
        if (!i2c_sync_write_txn(i2c_addr, cmd2, sizeof(cmd2)))
            goto fail;
        if (i && (x != px))
            break;
        px = cmd2[3];
    }

    oled_model = (i == 3) ? OLED_sh1106 : OLED_ssd1306;
    printk("OLED: %s\n", (oled_model == OLED_sh1106) ? "SH1106" : "SSD1306");
    return TRUE;

fail:
    return FALSE;
}

static void oled_init_fast_mode(void)
{
    /* Switch to Fast Mode (400kHz). */
    i2c->cr1 = 0;
    i2c->cr2 = I2C_CR2_FREQ(50);
    i2c->ccr = I2C_CCR_400k;
    i2c->trise = I2C_TRISE_400k;
    i2c->cr1 = I2C_CR1_PE;
}

static void oled_init(void)
{
    static const uint8_t init_cmds[] = {
        0xd5, 0x80,
        0xd3, 0x00,
        0x40,
        0x8d, 0x14,
        0xda, 0x02,
        0xd9, 0xf1,
        0xdb, 0x20,
        0xa4,
        0x2e,
    }, norot_cmds[] = {
        0xa1,
        0xc8,
    }, rot_cmds[] = {
        0xa0,
        0xc0,
    };
    uint8_t dynamic_cmds[7], *dc;
    uint8_t *p = buffer;

    if (!(ff_cfg.display_type & DISPLAY_slow))
        oled_init_fast_mode();

    if ((oled_model == OLED_unknown) && !oled_probe_model())
        goto fail;

    p += oled_queue_cmds(p, init_cmds, sizeof(init_cmds));

    dc = dynamic_cmds;
    *dc++ = (ff_cfg.display_type & DISPLAY_inverse) ? 0xa7 : 0xa6;
    *dc++ = 0x81;
    *dc++ = ff_cfg.oled_contrast;
    *dc++ = 0xa8;
    *dc++ = oled_height - 1;
    *dc++ = 0xda;
    *dc++ = (oled_height == 64) ? 0x12 : 0x02;
    p += oled_queue_cmds(p, dynamic_cmds, dc - dynamic_cmds);

    dc = dynamic_cmds;
    memcpy(dc, (ff_cfg.display_type & DISPLAY_rotate) ? rot_cmds : norot_cmds,
           2);
    if (ff_cfg.display_type & DISPLAY_hflip)
        dc[0] ^= 1;
    p += oled_queue_cmds(p, dc, 2);

    p += oled_start_i2c(p);

    dma_start(p - buffer);
    return;

fail:
    IRQx_set_pending(I2C_ERROR_IRQ);
}

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */