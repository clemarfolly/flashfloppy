/*
 * stm32f411.h
 * 
 * Core and peripheral registers for STM32F411.
 * 
 * This is free and unencumbered software released into the public domain.
 */

#pragma once

/* C pointer types */
#define FLASH_BANK volatile struct flash_bank * const
#define SYSCFG volatile struct syscfg * const

/* C-accessible registers */
static STK stk = (struct stk *)STK_BASE;
static SCB scb = (struct scb *)SCB_BASE;
static NVIC nvic = (struct nvic *)NVIC_BASE;
static DBG dbg = (struct dbg *)DBG_BASE;
static FLASH flash = (struct flash *)FLASH_BASE;
static PWR pwr = (struct pwr *)PWR_BASE;
static RCC rcc = (struct rcc *)RCC_BASE;
static GPIO gpioa = (struct gpio *)GPIOA_BASE;
static GPIO gpiob = (struct gpio *)GPIOB_BASE;
static GPIO gpioc = (struct gpio *)GPIOC_BASE;
static GPIO gpiod = (struct gpio *)GPIOD_BASE;
static GPIO gpioe = (struct gpio *)GPIOE_BASE;
static GPIO gpioh = (struct gpio *)GPIOH_BASE;
static SYSCFG syscfg = (struct syscfg *)SYSCFG_BASE;
static EXTI exti = (struct exti *)EXTI_BASE;
static DMA dma1 = (struct dma *)DMA1_BASE;
static DMA dma2 = (struct dma *)DMA2_BASE;
static TIM tim1 = (struct tim *)TIM1_BASE;
static TIM tim2 = (struct tim *)TIM2_BASE;
static TIM tim3 = (struct tim *)TIM3_BASE;
static TIM tim4 = (struct tim *)TIM4_BASE;
static TIM tim5 = (struct tim *)TIM5_BASE;
static SPI spi1 = (struct spi *)SPI1_BASE;
static SPI spi2 = (struct spi *)SPI2_BASE;
static SPI spi3 = (struct spi *)SPI3_BASE;
static I2C i2c1 = (struct i2c *)I2C1_BASE;
static I2C i2c2 = (struct i2c *)I2C2_BASE;
static USART usart1 = (struct usart *)USART1_BASE;
static USART usart2 = (struct usart *)USART2_BASE;
static USART usart3 = (struct usart *)USART3_BASE;
static USB_OTG usb_otg = (struct usb_otg *)USB_OTG_BASE;

/* Clocks for STM32F411 (BlackPill, 25MHz HSE).
 * FlashFloppy samples flux at SAMPLECLK_MHZ (72MHz), and the RDATA/WDATA timer
 * prescalers require SYSCLK to be a multiple of that. The F411 tops out at
 * 100MHz, so we run at 72MHz (the same as the original STM32F105 Gotek). */
#define SYSCLK_MHZ 72
#define AHB_MHZ    72
#define APB1_MHZ   36
#define APB2_MHZ   72

/* GPIO */
void gpio_set_af(GPIO gpio, unsigned int pin, unsigned int af);

/* Software IRQs: borrow the (unused) SPI4 and SPI5 interrupt lines. */
#define SOFTIRQ_0 84
#define SOFTIRQ_1 85

/*
 * Local variables:
 * mode: C
 * c-file-style: "Linux"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */