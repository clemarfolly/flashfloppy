/*
 * mcu_stm32f411.c
 *
 * Core and peripheral registers.
 *
 * Written & released by Keir Fraser <keir.xen@gmail.com>
 * Adapted for STM32F411 (BlackPill)
 *
 * This is free and unencumbered software released into the public domain.
 * See the file COPYING for more details, or visit <http://unlicense.org>.
 */

#include "stm32f411_decls.h"

bool_t is_artery_mcu = FALSE;
unsigned int flash_page_size = FLASH_PAGE_SIZE;
unsigned int ram_kb = 128;

static void clock_init(void)
{
    /* Flash: 2 wait states for 64-90MHz at 2.7-3.6V (RM0383 Table 6). */
    flash->acr = (FLASH_ACR_LATENCY(2) | FLASH_ACR_ICEN
                  | FLASH_ACR_DCEN | FLASH_ACR_PRFTEN);

    /* Start up the external oscillator (25MHz on BlackPill). */
    rcc->cr |= RCC_CR_HSEON;
    while (!(rcc->cr & RCC_CR_HSERDY))
        cpu_relax();

    /* PLL: VCO = 25MHz / 25 * 144 = 144MHz.
     *  SYSCLK = VCO / 2 = 72MHz (PLLP=0 means /2)
     *  USB    = VCO / 3 = 48MHz */
    rcc->pllcfgr = (RCC_PLLCFGR_PLLSRC_HSE |
                    RCC_PLLCFGR_PLLM(25) |
                    RCC_PLLCFGR_PLLN(144) |
                    RCC_PLLCFGR_PLLP(0) |
                    RCC_PLLCFGR_PLLQ(3));

    /* Bus divisors. */
    rcc->cfgr = (RCC_CFGR_PPRE2(0) | /* APB2 = SYSCLK/1 = 72MHz */
                 RCC_CFGR_PPRE1(4) | /* APB1 = SYSCLK/2 = 36MHz */
                 RCC_CFGR_HPRE(0));  /* AHB  = SYSCLK/1 = 72MHz */

    /* Enable and stabilise the PLL. */
    rcc->cr |= RCC_CR_PLLON;
    while (!(rcc->cr & RCC_CR_PLLRDY))
        cpu_relax();

    /* Switch to the externally-driven PLL for system clock. */
    rcc->cfgr |= RCC_CFGR_SW(2);
    while ((rcc->cfgr & RCC_CFGR_SWS(3)) != RCC_CFGR_SWS(2))
        cpu_relax();

    /* Internal oscillator no longer needed. */
    rcc->cr &= ~RCC_CR_HSION;
}

static void peripheral_init(void)
{
    /* Enable basic GPIO clocks, DMA, and SYSCFG. */
    rcc->ahb1enr |= (RCC_AHB1ENR_DMA1EN  |
                     RCC_AHB1ENR_DMA2EN  |
                     RCC_AHB1ENR_GPIOHEN |
                     RCC_AHB1ENR_GPIOCEN |
                     RCC_AHB1ENR_GPIOBEN |
                     RCC_AHB1ENR_GPIOAEN);
    (void)rcc->ahb1enr;
    
    rcc->apb1enr |= (RCC_APB1ENR_TIM2EN |
                     RCC_APB1ENR_TIM3EN |
                     RCC_APB1ENR_TIM4EN |
                     RCC_APB1ENR_TIM5EN);
    (void)rcc->apb1enr;

    rcc->apb2enr |= (RCC_APB2ENR_SYSCFGEN |
                     RCC_APB2ENR_TIM1EN);
    (void)rcc->apb2enr;   

    /* STM32F411 does not have DMAMUX; DMA request mapping is fixed. */

    /* Release JTAG pins. */
    gpio_configure_pin(gpioa, 15, GPI_floating);
    gpio_configure_pin(gpiob,  3, GPI_floating);
    gpio_configure_pin(gpiob,  4, GPI_floating);
}

void stm32_init(void)
{
    cortex_init();
    clock_init();
    peripheral_init();
    cpu_sync();
}

void gpio_configure_pin(GPIO gpio, unsigned int pin, unsigned int mode)
{
    gpio_write_pin(gpio, pin, mode >> 7);
    gpio->moder   = (gpio->moder   & ~(3u << (pin << 1))) | ((mode & 3u) << (pin << 1));
    mode >>= 2;
    gpio->otyper  = (gpio->otyper  & ~(1u << pin))        | ((mode & 1u) << pin);
    mode >>= 1;
    /* STM32 uses OSPEEDR (2-bit speed) in place of AT32 ODRVR. */
    gpio->ospeedr = (gpio->ospeedr & ~(3u << (pin << 1))) | ((mode & 3u) << (pin << 1));
    mode >>= 2;
    gpio->pupdr   = (gpio->pupdr   & ~(3u << (pin << 1))) | ((mode & 3u) << (pin << 1));
}

void gpio_set_af(GPIO gpio, unsigned int pin, unsigned int af)
{
    if (pin < 8) {
        gpio->afrl = (gpio->afrl & ~(15u << (pin << 2))) | (af << (pin << 2));
    } else {
        pin -= 8;
        gpio->afrh = (gpio->afrh & ~(15u << (pin << 2))) | (af << (pin << 2));
    }
}

void _exti_route(unsigned int px, unsigned int pin)
{
    unsigned int n = pin >> 2;
    unsigned int s = (pin & 3) << 2;
    uint32_t exticr = syscfg->exticr[n];
    ASSERT(!in_exception()); /* no races please */
    exticr &= ~(0xf << s);
    exticr |= px << s;
    syscfg->exticr[n] = exticr;
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
