/*
 * fpec_stm32f411.c
 *
 * STM32F411 Flash Memory Program/Erase Controller.
 *
 * The F411 Flash is organised in sectors of unequal size, not uniform pages:
 *  Sectors 0-3:  16kB each (0x08000000-0x0800ffff)
 *  Sector  4:    64kB      (0x08010000-0x0801ffff)
 *  Sectors 5-7: 128kB each (0x08020000-0x0807ffff)
 *
 * FlashFloppy erases Flash in FLASH_PAGE_SIZE (16kB) steps. We erase a whole
 * sector when asked to erase the first 16kB page of that sector, and ignore
 * requests for the remaining pages of the sector (already erased). Callers
 * always erase ranges in ascending order from a sector boundary.
 *
 * This is free and unencumbered software released into the public domain.
 * See the file COPYING for more details, or visit <http://unlicense.org>.
 */

#define FLASH_SR_ERRORS (FLASH_SR_OPERR | FLASH_SR_WRPERR | FLASH_SR_PGAERR \
                         | FLASH_SR_PGPERR | FLASH_SR_PGSERR)

static void fpec_wait_and_clear(void)
{
    while (flash->sr & FLASH_SR_BSY)
        continue;
    flash->sr = FLASH_SR_EOP | FLASH_SR_ERRORS;
    flash->cr &= ~(FLASH_CR_PG | FLASH_CR_SER | FLASH_CR_MER
                   | FLASH_CR_SNB(0xf) | FLASH_CR_PSIZE(3));
}

/* Data read back after erase/program must not come from stale cache lines. */
static void flash_cache_flush(void)
{
    uint32_t acr = flash->acr;
    flash->acr = acr & ~(FLASH_ACR_ICEN | FLASH_ACR_DCEN);
    flash->acr = acr | FLASH_ACR_ICRST | FLASH_ACR_DCRST;
    flash->acr = acr;
}

/* Returns sector number if @addr is the first address of a sector, else -1. */
static int sector_start(uint32_t addr)
{
    uint32_t off = addr - 0x08000000u;
    if (off < 0x10000u)
        return (off & 0x3fffu) ? -1 : (int)(off >> 14);         /* 0-3 */
    if (off == 0x10000u)
        return 4;
    if ((off < 0x80000u) && !(off & 0x1ffffu))
        return 4 + (int)(off >> 17);                             /* 5-7 */
    return -1;
}

void fpec_init(void)
{
    /* Unlock the FPEC. */
    if (flash->cr & FLASH_CR_LOCK) {
        flash->keyr = 0x45670123;
        flash->keyr = 0xcdef89ab;
    }

    fpec_wait_and_clear();
}

void fpec_page_erase(uint32_t flash_address)
{
    int sector = sector_start(flash_address);

    if (sector < 0)
        return;

    fpec_wait_and_clear();
    flash->cr |= FLASH_CR_PSIZE(2) | FLASH_CR_SER | FLASH_CR_SNB(sector);
    flash->cr |= FLASH_CR_STRT;
    fpec_wait_and_clear();
    flash_cache_flush();
}

void fpec_write(const void *data, unsigned int size, uint32_t flash_address)
{
    volatile uint16_t *_f = (volatile uint16_t *)flash_address;
    const uint16_t *_d = data;

    fpec_wait_and_clear();
    flash->cr |= FLASH_CR_PSIZE(1) | FLASH_CR_PG;
    for (; size != 0; size -= 2) {
        *_f++ = *_d++;
        while (flash->sr & FLASH_SR_BSY)
            continue;
    }
    fpec_wait_and_clear();
    flash_cache_flush();
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
