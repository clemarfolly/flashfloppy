/*
 * fpec_stm32f411.c
 * 
 * Flash memory programming controller for STM32F411
 */

#if MCU == MCU_stm32f411

void fpec_init(void)
{
    /* Configurar controlador de flash */
}

void fpec_page_erase(uint32_t addr)
{
    /* Apagar página de flash no endereço */
}

void fpec_write(const void *src, unsigned int len, uint32_t addr)
{
    /* Escrever dados no flash */
}

#endif