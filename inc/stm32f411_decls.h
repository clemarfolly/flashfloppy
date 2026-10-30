/*
 * stm32f411_decls.h
 *
 * STM32F411 (BlackPill) MCU declarations used by the FlashFloppy core.
 *
 * This is free and unencumbered software released into the public domain.
 * See the file COPYING for more details, or visit <http://unlicense.org>.
 */

#pragma once

/* DMA - STM32F411 (streams with channel select) - RM0383 section 9.5 */
struct dma_stream {
    uint32_t cr;         /* +00: Configuration */
    uint32_t ndtr;       /* +04: Number of data */
    uint32_t par;        /* +08: Peripheral address */
    uint32_t m0ar;       /* +0C: Memory 0 address */
    uint32_t m1ar;       /* +10: Memory 1 address */
    uint32_t fcr;        /* +14: FIFO control */
};
struct dma {
    uint32_t lisr;       /* 00: Low interrupt status (streams 0-3) */
    uint32_t hisr;       /* 04: High interrupt status (streams 4-7) */
    uint32_t lifcr;      /* 08: Low interrupt flag clear (streams 0-3) */
    uint32_t hifcr;      /* 0C: High interrupt flag clear (streams 4-7) */
    struct dma_stream stream[8];
};

#include "mcu/stm32f411_regs.h"
#include "mcu/stm32f411.h"

#define DMA_CR_CHSEL(x)  DMA_SxCR_CHSEL(x)

/* Clear all interrupt flags of DMA stream @s. */
#define dma_clear_flags(dma, s) do {                    \
    if ((s) < 4)                                        \
        (dma)->lifcr = DMA_SxIFCR_CALL(s);                \
    else                                                \
        (dma)->hifcr = DMA_SxIFCR_CALL(s);                \
} while (0)

/* Disable DMA stream and wait for it to stop (RM0383 9.3.17). */
#define dma_stream_disable(st) do {                     \
    (st).cr &= ~DMA_SxCR_EN;                            \
    while ((st).cr & DMA_SxCR_EN)                       \
        continue;                                       \
} while (0)

#define gpio_data gpioa

/* WDATA: PA8 = TIM1_CH1 (AF1) -> DMA2 Stream6 Channel0 (RM0383 Table 28) */
#define pin_wdata        8
#define pin_wdata_af     1
#define tim_wdata        (tim1)
#define dma_wdata_ctrl   dma2
#define dma_wdata_stream 6
#define dma_wdata        (dma_wdata_ctrl->stream[dma_wdata_stream])
#define dma_wdata_ch     0
#define dma_wdata_irq    DMA2_Stream6_IRQn

/* RDATA: PA7 = TIM3_CH2 (AF2); TIM3_UP -> DMA1 Stream2 Channel5 (Table 27) */
#define pin_rdata        7
#define pin_rdata_af     2
#define tim_rdata        (tim3)
#define dma_rdata_ctrl   dma1
#define dma_rdata_stream 2
#define dma_rdata        (dma_rdata_ctrl->stream[dma_rdata_stream])
#define dma_rdata_ch     5
#define dma_rdata_irq    DMA1_Stream2_IRQn

/* Clear DMA peripheral interrupts. */
#define clear_dma_interrupts_rdata() \
    dma_clear_flags(dma_rdata_ctrl, dma_rdata_stream)
#define clear_dma_interrupts_wdata() \
    dma_clear_flags(dma_wdata_ctrl, dma_wdata_stream)

/* KC30 select pin: not present on BlackPill boards. Defined for build only. */
#define kc30_sel_gpio gpioc
#define kc30_sel_pin  14
