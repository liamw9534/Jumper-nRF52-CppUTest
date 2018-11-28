#include <algorithm>
#include "SpiFlash.h"

extern "C" {

#include <string.h>

static void spi_event_handler(nrf_drv_spi_evt_t const * p_event, void * p_context)
{
	struct SpiFlash *p = (struct SpiFlash *)p_context;
	p->_spi_event_handler(p_event);
}

}

#define WREN        			0x06
#define RDSR        			0x05
#define PP          			0x02
#define SE          			0xD8
#define BE          			0xC7
#define READ        			0x03


/* SPI flash status bits */
#define RDSR_BUSY   (1 << 0)
#define RDSR_WEL    (1 << 1)
#define RDSR_BP0    (1 << 2)
#define RDSR_BP1    (1 << 3)
#define RDSR_BP2    (1 << 4)
#define RDSR_SRWD   (1 << 7)


void SpiFlash::_spi_event_handler(nrf_drv_spi_evt_t const * p_event)
{
	xfer_busy = false;
}

int SpiFlash::status(uint8_t &value)
{
	int ret;
	spi_buffer[0] = RDSR;
	spi_buffer[1] = 0;
	ret = xfer(2);
	value = spi_buffer[1];
	return ret;
}

int SpiFlash::wren()
{
	spi_buffer[0] = WREN;
	return xfer(1);
}

int SpiFlash::xfer(unsigned int sz)
{
	xfer_busy = true;
	nrf_drv_spi_transfer(spi_instance, spi_buffer, sz, spi_buffer, sz);
	while (xfer_busy);
	return 0;
}

int SpiFlash::busy_wait()
{
	int ret;
	uint8_t s;

	do
	{
		ret = status(s);
	} while ((s & RDSR_BUSY) && !ret);

	return ret;
}

SpiFlash::SpiFlash(const nrf_drv_spi_t &spi, const nrf_drv_spi_config_t &spi_config)
{
	spi_instance = &spi;
	nrf_drv_spi_init(spi_instance, &spi_config, spi_event_handler, static_cast<void*>(this));
}

SpiFlash::~SpiFlash()
{
	nrf_drv_spi_uninit(spi_instance);
}

unsigned int SpiFlash::get_capacity()
{
	return num_pages * page_size;
}

int SpiFlash::write(unsigned int addr, const uint8_t *data, unsigned int sz)
{
    while (sz > 0)
    {
    	wren();

        spi_buffer[0] = PP;
        spi_buffer[1] = (uint8_t)(addr >> 16);
        spi_buffer[2] = (uint8_t)(addr >> 8);
        spi_buffer[3] = (uint8_t)(addr);

        uint8_t wr_size = std::min(sz, sizeof(spi_buffer) - 4);
        memcpy(&spi_buffer[4], data, wr_size);

        xfer(wr_size + 4);
        busy_wait();

        sz -= wr_size;
        addr += wr_size;
        data += wr_size;
    }

    return 0;
}

int SpiFlash::read(unsigned int addr, uint8_t *data, unsigned int sz)
{
	while (sz > 0)
	{
		spi_buffer[0] = READ;
		spi_buffer[1] = (uint8_t)(addr >> 16);
		spi_buffer[2] = (uint8_t)(addr >> 8);
		spi_buffer[3] = (uint8_t)(addr);

		uint8_t rd_size = std::min(sz, sizeof(spi_buffer) - 4);

		xfer(rd_size + 4);

		sz -= rd_size;
		addr += rd_size;
		memcpy(data, &spi_buffer[4], rd_size);
		data += rd_size;
	}

	return 0;
}

int SpiFlash::erase_block(unsigned int addr)
{
	wren();
    spi_buffer[0] = SE;
    spi_buffer[1] = (uint8_t) (addr >> 16);
    spi_buffer[2] = (uint8_t) (addr >> 8);
    spi_buffer[3] = (uint8_t) (addr);
    xfer(4);
    return busy_wait();
}

int SpiFlash::erase_all()
{
	wren();
    spi_buffer[0] = BE;
    xfer(1);
    return busy_wait();
}
