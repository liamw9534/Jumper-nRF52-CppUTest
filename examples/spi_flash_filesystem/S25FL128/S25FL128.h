#include "SpiFlash.h"

#define S25FL128_PAGE_SIZE		0x200
#define S25FL128_BLOCK_SIZE		0x40000
#define S25FL128_NUM_PAGES		0x8000

class S25FL128 : public SpiFlash
{
public:
	S25FL128(const nrf_drv_spi_t &spi, const nrf_drv_spi_config_t &spi_config);
};
