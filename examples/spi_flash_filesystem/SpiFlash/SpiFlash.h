#pragma once

extern "C" {
#include <stdint.h>
#include "sdk_config.h"
#include "nrf.h"
#include "nordic_common.h"
#include "nrf_drv_spi.h"

}

class SpiFlash
{
private:
	const nrf_drv_spi_t *spi_instance;
	bool xfer_busy;
	uint8_t spi_buffer[255];

	int status(uint8_t &value);
	int wren();
	int busy_wait();
	int xfer(unsigned int sz);

protected:
	unsigned int num_pages;
	unsigned int block_size;
	unsigned int page_size;

public:
	~SpiFlash();
	SpiFlash(const nrf_drv_spi_t &spi, const nrf_drv_spi_config_t &spi_config);
	unsigned int get_capacity();
	int write(unsigned int addr, const uint8_t *data, unsigned int sz);
	int read(unsigned int addr, uint8_t *data, unsigned int sz);
	int erase_block(unsigned int addr);
	int erase_all();
	void _spi_event_handler(nrf_drv_spi_evt_t const * p_event);
};
