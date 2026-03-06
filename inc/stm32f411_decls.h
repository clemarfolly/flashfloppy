#pragma once

#include "mcu/stm32f411_regs.h"
#include "mcu/stm32f411.h"

#define gpio_data gpioa

//Timer 1 - CH1
#define pin_wdata   8
#define pin_wdata_af   1
#define tim_wdata   (tim1)
#define dma_wdata_stream 6
#define dma_wdata   (dma2->stream[dma_wdata_stream])
#define dma_wdata_ch 0
#define dma_wdata_irq DMA2_Stream6_IRQn


//Timer 3 - UP
#define pin_rdata   7
#define tim_rdata   (tim3)
#define dma_rdata_stream 2
#define dma_rdata   (dma1->stream[dma_wdata_stream])
#define dma_rdata_ch 5
#define dma_rdata_irq DMA1_Stream2_IRQn

//Clear DMA peripheral interrupts
// rdata → DMA1 Stream2
// Stream2 está no grupo LIFCR (streams 0–3).
#define clear_dma_interrupts_rdata() \
            dma1->lifcr = DMA_LIFCR_CFEIF(dma_rdata_stream) | \
                          DMA_LIFCR_CDMEIF(dma_rdata_stream) | \
                          DMA_LIFCR_CTEIF(dma_rdata_stream) | \
                          DMA_LIFCR_CHTIF(dma_rdata_stream) | \
                          DMA_LIFCR_CTCIF(dma_rdata_stream)

//wdata → DMA2 Stream6
//Stream6 está no grupo HIFCR (streams 4–7).
#define clear_dma_interrupts_wdata() \
            dma2->hifcr = DMA_HIFCR_CFEIF(dma_wdata_stream) | \
                          DMA_HIFCR_CDMEIF(dma_wdata_stream) | \
                          DMA_HIFCR_CTEIF(dma_wdata_stream) | \
                          DMA_HIFCR_CHTIF(dma_wdata_stream) | \
                          DMA_HIFCR_CTCIF(dma_wdata_stream)

#define kc30_sel_gpio gpioc
#define kc30_sel_pin  14

/* DMA - STM32F411 (streams com channel select) -  RM0383 seção 9.5 */
struct dma_stream {
    uint32_t cr;         /* +00: Configuration */
    uint32_t ndtr;       /* +04: Number of data */
    uint32_t par;        /* +08: Peripheral address */
    uint32_t m0ar;       /* +0C: Memory 0 address */
    uint32_t m1ar;       /* +10: Memory 1 address */
    uint32_t fcr;        /* +14: FIFO control */
};
struct dma {
    uint32_t lisr;       /* 00: Low interrupt status */
    uint32_t hisr;       /* 04: High interrupt status */
    uint32_t lifcr;      /* 08: Low interrupt flag clear */
    uint32_t hifcr;      /* 0C: High interrupt flag clear */
    struct dma_stream stream[8];
};
    
#define DMA_CR_CHSEL(x)  ((x) << 25)
