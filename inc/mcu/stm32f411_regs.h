/*
 * stm32f411_regs.h
 * 
 * Core and peripheral register definitions for STM32F411.
 * 
 * This is free and unencumbered software released into the public domain.
 */

#pragma once

#include <stdint.h>

/* Power control */
struct pwr {
    uint32_t cr;       /* 00: Power control */
    uint32_t csr;      /* 04: Power control/status */
};

#define PWR_BASE 0x40007000

/* Flash memory interface */
struct flash_bank {
    uint32_t sr;       /* +00: Flash status */
    uint32_t cr;       /* +04: Flash control */
    uint32_t ar;       /* +08: Flash address */
};
struct flash {
    uint32_t acr;      /* 00: Access control */
    uint32_t keyr;     /* 04: Key */
    uint32_t optkeyr;  /* 08: Option key */
    uint32_t sr;       /* 0C: Status */
    uint32_t cr;       /* 10: Control */
    uint32_t optcr;    /* 14: Option control */
};

#define FLASH_SR_EOP         (1u<< 0)
#define FLASH_SR_OPERR       (1u<< 1)
#define FLASH_SR_WRPERR      (1u<< 4)
#define FLASH_SR_PGAERR      (1u<< 5)
#define FLASH_SR_PGPERR      (1u<< 6)
#define FLASH_SR_PGSERR      (1u<< 7)
#define FLASH_SR_BSY         (1u<<16)

#define FLASH_CR_PG          (1u<< 0)
#define FLASH_CR_SER         (1u<< 1)
#define FLASH_CR_MER         (1u<< 2)
#define FLASH_CR_SNB(x)      ((x)<< 3)
#define FLASH_CR_STRT        (1u<<16)
#define FLASH_CR_EOPIE       (1u<<24)
#define FLASH_CR_ERRIE       (1u<<25)
#define FLASH_CR_LOCK        (1u<<31)

#define FLASH_ACR_DCRST      (1u<<12)
#define FLASH_ACR_ICRST      (1u<<11)
#define FLASH_ACR_DCEN       (1u<<10)
#define FLASH_ACR_ICEN       (1u<< 9)
#define FLASH_ACR_PRFTEN     (1u<< 8)
#define FLASH_ACR_LATENCY(x) ((x)<<0)

#define FLASH_BASE 0x40023c00

/* Reset and clock control */
struct rcc {
    uint32_t cr;       /* 00: Clock control */
    uint32_t pllcfgr;  /* 04: PLL configuration */
    uint32_t cfgr;     /* 08: Clock configuration */
    uint32_t cir;      /* 0C: Clock interrupt */
    uint32_t ahb1rstr; /* 10: AHB1 peripheral reset */
    uint32_t ahb2rstr; /* 14: AHB2 peripheral reset */
    uint32_t ahb3rstr; /* 18: AHB3 peripheral reset */
    uint32_t _unused0; /* 1C: - */
    uint32_t apb1rstr; /* 20: APB1 peripheral reset */
    uint32_t apb2rstr; /* 24: APB2 peripheral reset */
    uint32_t _unused1[2]; /* 28-2C: - */
    uint32_t ahb1enr;  /* 30: AHB1 peripheral clock enable */
    uint32_t ahb2enr;  /* 34: AHB2 peripheral clock enable */
    uint32_t ahb3enr;  /* 38: AHB3 peripheral clock enable */
    uint32_t _unused2; /* 3C: - */
    uint32_t apb1enr;  /* 40: APB1 peripheral clock enable */
    uint32_t apb2enr;  /* 44: APB2 peripheral clock enable */
    uint32_t _unused3[2]; /* 48-4C: - */
    uint32_t ahb1lpenr;/* 50: AHB1 peripheral clock enable (low-power mode) */
    uint32_t ahb2lpenr;/* 54: AHB2 peripheral clock enable (low-power mode) */
    uint32_t ahb3lpenr;/* 58: AHB3 peripheral clock enable (low-power mode) */
    uint32_t _unused4; /* 5C: - */
    uint32_t apb1lpenr;/* 60: APB1 peripheral clock enable (low-power mode) */
    uint32_t apb2lpenr;/* 64: APB2 peripheral clock enable (low-power mode) */
    uint32_t _unused5[2]; /* 68-6C: - */
    uint32_t bdcr;     /* 70: Backup domain control */
    uint32_t csr;      /* 74: Clock control & status */
};

#define RCC_CR_HSION         (1u<< 0)
#define RCC_CR_HSIRDY        (1u<< 1)
#define RCC_CR_HSEON         (1u<<16)
#define RCC_CR_HSERDY        (1u<<17)
#define RCC_CR_HSEBYP        (1u<<18)
#define RCC_CR_PLLON         (1u<<24)
#define RCC_CR_PLLRDY        (1u<<25)

#define RCC_PLLCFGR_PLLSRC_HSE (1<<22)
#define RCC_PLLCFGR_PLLN(x)  ((x)<<6)
#define RCC_PLLCFGR_PLLM(x)  ((x)<<0)
#define RCC_PLLCFGR_PLLP(x)  ((x)<<16)
#define RCC_PLLCFGR_PLLQ(x)  ((x)<<24)

#define RCC_CFGR_MCO1(x)     ((x)<<21)
#define RCC_CFGR_PPRE2(x)    ((x)<<13)
#define RCC_CFGR_PPRE1(x)    ((x)<<10)
#define RCC_CFGR_HPRE(x)     ((x)<< 4)
#define RCC_CFGR_SWS(x)      ((x)<< 2)
#define RCC_CFGR_SW(x)       ((x)<< 0)

#define RCC_AHB1ENR_OTGFSEN  (1u<<27)
#define RCC_AHB1ENR_DMA2EN   (1u<<22)
#define RCC_AHB1ENR_DMA1EN   (1u<<21)
#define RCC_AHB1ENR_CRCEN    (1u<<12)
#define RCC_AHB1ENR_GPIOHEN  (1u<< 7)
#define RCC_AHB1ENR_GPIOEEN  (1u<< 4)
#define RCC_AHB1ENR_GPIODEN  (1u<< 3)
#define RCC_AHB1ENR_GPIOCEN  (1u<< 2)
#define RCC_AHB1ENR_GPIOBEN  (1u<< 1)
#define RCC_AHB1ENR_GPIOAEN  (1u<< 0)

#define RCC_AHB2ENR_OTGFSEN (1u<< 7)

#define RCC_APB1ENR_PWREN    (1u<<28)
#define RCC_APB1ENR_I2C2EN   (1u<<22)
#define RCC_APB1ENR_I2C1EN   (1u<<21)
#define RCC_APB1ENR_USART3EN (1u<<18)
#define RCC_APB1ENR_USART2EN (1u<<17)
#define RCC_APB1ENR_SPI3EN   (1u<<15)
#define RCC_APB1ENR_SPI2EN   (1u<<14)
#define RCC_APB1ENR_TIM5EN   (1u<< 3)
#define RCC_APB1ENR_TIM4EN   (1u<< 2)
#define RCC_APB1ENR_TIM3EN   (1u<< 1)
#define RCC_APB1ENR_TIM2EN   (1u<< 0)

#define RCC_APB2ENR_USART6EN (1u<< 5)
#define RCC_APB2ENR_USART1EN (1u<< 4)
#define RCC_APB2ENR_SPI1EN   (1u<<12)
#define RCC_APB2ENR_SYSCFGEN (1u<<14)
#define RCC_APB2ENR_TIM1EN   (1u<< 0)

#define RCC_BDCR_BDRST       (1u<<16)
#define RCC_BDCR_RTCEN       (1u<<15)
#define RCC_BDCR_RTCSEL(x)   ((x)<<8)
#define RCC_BDCR_LSEDRV(x)   ((x)<<3)
#define RCC_BDCR_LSEBYP      (1u<< 2)
#define RCC_BDCR_LSERDY      (1u<< 1)
#define RCC_BDCR_LSEON       (1u<< 0)

#define RCC_CSR_LPWRRSTF     (1u<<31)
#define RCC_CSR_WWDGRSTF     (1u<<30)
#define RCC_CSR_IWDGRSTF     (1u<<29)
#define RCC_CSR_SFTRSTF      (1u<<28)
#define RCC_CSR_PORRSTF      (1u<<27)
#define RCC_CSR_PINRSTF      (1u<<26)
#define RCC_CSR_BORRSTF      (1u<<25)
#define RCC_CSR_RMVF         (1u<<24)
#define RCC_CSR_LSIRDY       (1u<< 1)
#define RCC_CSR_LSION        (1u<< 0)

#define RCC_BASE 0x40023800

/* General-purpose I/O */
struct gpio {
    uint32_t moder;   /* 00: Port mode */
    uint32_t otyper;  /* 04: Port output type */
    uint32_t ospeedr; /* 08: Port output speed */
    uint32_t pupdr;   /* 0C: Port pull-up/pull-down */
    uint32_t idr;     /* 10: Port input data */
    uint32_t odr;     /* 14: Port output data */
    uint32_t bsrr;    /* 18: Port bit set/reset */
    uint32_t lckr;    /* 1C: Port configuration lock */
    uint32_t afrl;    /* 20: Alternate function low */
    uint32_t afrh;    /* 24: Alternate function high */
};

/* 0-1: MODE, 2: OTYPE, 3-4:OSPEED, 5-6:PUPD, 7:OUTPUT_LEVEL */
#define GPI_analog    0x3u
#define GPI(pupd)     (0x0u|((pupd)<<5))
#define PUPD_none     0
#define PUPD_up       1
#define PUPD_down     2
#define GPI_floating  GPI(PUPD_none)
#define GPI_pull_down GPI(PUPD_down)
#define GPI_pull_up   GPI(PUPD_up)

#define GPO_pushpull(speed,level)  (0x1u|((speed)<<3)|((level)<<7))
#define GPO_opendrain(speed,level) (0x5u|((speed)<<3)|((level)<<7))
#define AFI(pupd)                  (0x2u|((pupd)<<5))
#define AFO_pushpull(speed)        (0x2u|((speed)<<3))
#define _AFO_pushpull(speed,level) (0x2u|((speed)<<3)|((level)<<7))
#define AFO_opendrain(speed)       (0x6u|((speed)<<3))
#define _2MHz 0
#define _10MHz 1
#define _50MHz 2
#define LOW  0
#define HIGH 1

#define GPIOA_BASE 0x40020000
#define GPIOB_BASE 0x40020400
#define GPIOC_BASE 0x40020800
#define GPIOD_BASE 0x40020C00
#define GPIOE_BASE 0x40021000
#define GPIOH_BASE 0x40021C00

/* System configuration controller */
struct syscfg {
    uint32_t memrmp;     /* 00: Memory remap */
    uint32_t pmc;        /* 04: Peripheral mode configuration */
    uint32_t exticr[4];  /* 08-14: External interrupt configuration #1-4 */
    uint32_t _pad[2];
    uint32_t cmpcr;      /* 20: Compensation cell control */
};

#define SYSCFG_BASE 0x40013800

/* EXTI */
#define EXTI_BASE 0x40013c00

/* DMA */
#define DMA1_Stream2_IRQn 12
#define DMA1_Stream4_IRQn 14
#define DMA2_Stream5_IRQn 68
#define DMA2_Stream6_IRQn 69

#define DMA_LIFCR_CFEIF(x)  (1u << ((x) * 6))
#define DMA_LIFCR_CDMEIF(x) (1u << ((x) * 6 + 2))
#define DMA_LIFCR_CTEIF(x)  (1u << ((x) * 6 + 3))
#define DMA_LIFCR_CHTIF(x)  (1u << ((x) * 6 + 4))
#define DMA_LIFCR_CTCIF(x)  (1u << ((x) * 6 + 5))

#define DMA_HIFCR_CFEIF(x)  (1u << (((x) - 4) * 6))
#define DMA_HIFCR_CDMEIF(x) (1u << (((x) - 4) * 6 + 2))
#define DMA_HIFCR_CTEIF(x)  (1u << (((x) - 4) * 6 + 3))
#define DMA_HIFCR_CHTIF(x)  (1u << (((x) - 4) * 6 + 4))
#define DMA_HIFCR_CTCIF(x)  (1u << (((x) - 4) * 6 + 5))

/* ===================== DMA_SxCR bit definitions ===================== */

/* Enable */
#define DMA_SxCR_EN            (1U << 0)

/* Interrupt enables */
#define DMA_SxCR_TCIE          (1U << 4)
#define DMA_SxCR_HTIE          (1U << 3)

/* Data transfer direction (DIR[1:0]) */
#define DMA_SxCR_DIR_P2M       (0U << 6)
#define DMA_SxCR_DIR_M2P       (1U << 6)
#define DMA_SxCR_DIR_M2M       (2U << 6)

/* Circular mode */
#define DMA_SxCR_CIRC          (1U << 8)

/* Peripheral increment */
#define DMA_SxCR_PINC          (1U << 9)

/* Memory increment */
#define DMA_SxCR_MINC          (1U << 10)

/* Peripheral size (PSIZE[1:0]) */
#define DMA_SxCR_PSIZE_8BIT    (0U << 11)
#define DMA_SxCR_PSIZE_16BIT   (1U << 11)
#define DMA_SxCR_PSIZE_32BIT   (2U << 11)

/* Memory size (MSIZE[1:0]) */
#define DMA_SxCR_MSIZE_8BIT    (0U << 13)
#define DMA_SxCR_MSIZE_16BIT   (1U << 13)
#define DMA_SxCR_MSIZE_32BIT   (2U << 13)

/* Priority level (PL[1:0]) */
#define DMA_SxCR_PL_LOW        (0U << 16)
#define DMA_SxCR_PL_MEDIUM     (1U << 16)
#define DMA_SxCR_PL_HIGH       (2U << 16)
#define DMA_SxCR_PL_VERYHIGH   (3U << 16)

/* Channel selection (CHSEL[2:0]) */
#define DMA_SxCR_CHSEL(n)      ((uint32_t)(n) << 25)

#define DMA1_BASE 0x40026000
#define DMA2_BASE 0x40026400

/* Timer */
#define TIM_CR1_PMEN (1u<<10)

#define TIM1_BASE  0x40010000
#define TIM2_BASE  0x40000000
#define TIM3_BASE  0x40000400
#define TIM4_BASE  0x40000800
#define TIM5_BASE  0x40000c00

/* I2C */
struct i2c {
    uint32_t cr1;     /* 00: Control 1 */
    uint32_t cr2;     /* 04: Control 2 */
    uint32_t oar1;    /* 08: Own address 1 */
    uint32_t oar2;    /* 0C: Own address 2 */
    uint32_t dr;      /* 10: Data */
    uint32_t sr1;     /* 14: Status 1 */
    uint32_t sr2;     /* 18: Status 2 */
    uint32_t ccr;     /* 1C: Clock control */
    uint32_t trise;   /* 20: Rise time */
};

#define I2C_CR1_PE        (1u<< 0)
#define I2C_CR1_ENPEC     (1u<< 5)
#define I2C_CR1_ENGC      (1u<< 6)
#define I2C_CR1_NOSTRETCH (1u<< 7)
#define I2C_CR1_START     (1u<< 8)
#define I2C_CR1_STOP      (1u<< 9)
#define I2C_CR1_ACK       (1u<<10)
#define I2C_CR1_POS       (1u<<11)
#define I2C_CR1_PEC       (1u<<12)
#define I2C_CR1_ALERT     (1u<<13)
#define I2C_CR1_SWRST     (1u<<15)

#define I2C_CR2_FREQ(x)   ((x)<<0)
#define I2C_CR2_ITERREN   (1u<< 8)
#define I2C_CR2_ITEVTEN   (1u<< 9)
#define I2C_CR2_ITBUFEN   (1u<<10)
#define I2C_CR2_DMAEN     (1u<<11)
#define I2C_CR2_LAST      (1u<<12)

#define I2C_SR1_SB        (1u<< 0)
#define I2C_SR1_ADDR      (1u<< 1)
#define I2C_SR1_BTF       (1u<< 2)
#define I2C_SR1_ADD10     (1u<< 3)
#define I2C_SR1_STOPF     (1u<< 4)
#define I2C_SR1_RXNE      (1u<< 6)
#define I2C_SR1_TXE       (1u<< 7)
#define I2C_SR1_BERR      (1u<< 8)
#define I2C_SR1_ARLO      (1u<< 9)
#define I2C_SR1_AF        (1u<<10)
#define I2C_SR1_OVR       (1u<<11)
#define I2C_SR1_PECERR    (1u<<12)
#define I2C_SR1_TIMEOUT   (1u<<14)
#define I2C_SR1_SMBALERT  (1u<<15)

#define I2C_SR2_MSL       (1u<< 0)
#define I2C_SR2_BUSY      (1u<< 1)
#define I2C_SR2_TRA       (1u<< 2)
#define I2C_SR2_GENCALL   (1u<< 4)
#define I2C_SR2_SMBDEFAULT (1u<< 5)
#define I2C_SR2_SMBHOST   (1u<< 6)
#define I2C_SR2_DUALF     (1u<< 7)
#define I2C_SR2_PEC(x)    ((x)>>8)

#define I2C_CCR_CCR(x)    ((x)<<0)
#define I2C_CCR_DUTY      (1u<<14)
#define I2C_CCR_FS       (1u<<15)

#define I2C1_BASE 0x40005400
#define I2C2_BASE 0x40005800

/* SPI */
#define SPI1_BASE 0x40013000
#define SPI2_BASE 0x40003800
#define SPI3_BASE 0x40003C00

/* USART */
#define USART1_BASE 0x40011000
#define USART2_BASE 0x40004400
#define USART3_BASE 0x40004800

/* USB OTG */
#define USB_OTG_BASE 0x50000000

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */