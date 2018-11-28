#include "S25FL128.h"

S25FL128::S25FL128(const nrf_drv_spi_t &spi, const nrf_drv_spi_config_t &spi_config) :
	SpiFlash(spi, spi_config)
{
	page_size = S25FL128_PAGE_SIZE;
	block_size = S25FL128_BLOCK_SIZE;
	num_pages = S25FL128_NUM_PAGES;
}
