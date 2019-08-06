#include "storage.h"
#include "spi.h"

STATIC const spi_proto_cfg_t spiflash_bus_cfg = {
    .spi = &spi_obj[0], /* Is this right? */
    .baudrate = 80000000,
    .polarity = 0,
    .phase = 0,
    .bits = 8,
    .firstbit = SPI_FIRSTBIT_MSB,
};

STATIC mp_spiflash_cache_t spi_bdev_cache;

const mp_spiflash_config_t spiflash_config = {
    .bus_kind = MP_SPIFLASH_BUS_SPI,
    .bus.u_spi.cs = MICROPY_HW_SPIFLASH_CS,
    .bus.u_spi.data = (void*)&spiflash_bus_cfg,
    .bus.u_spi.proto = &spi_proto,
    .cache = &spi_bdev_cache,
};

spi_bdev_t spi_bdev;
