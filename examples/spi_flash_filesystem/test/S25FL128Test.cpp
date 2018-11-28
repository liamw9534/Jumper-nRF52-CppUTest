#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "S25FL128.h"

extern "C" {
	static const nrf_drv_spi_t spi = NRF_DRV_SPI_INSTANCE(0);

	static nrf_drv_spi_config_t spi_config =
	{
		    .sck_pin      = SPI_SCK_PIN,
		    .mosi_pin     = SPI_MOSI_PIN,
		    .miso_pin     = SPI_MISO_PIN,
		    .ss_pin       = SPI_SS_PIN,
		    .irq_priority = SPI_DEFAULT_CONFIG_IRQ_PRIORITY,
		    .orc          = 0xFF,
		    .frequency    = NRF_DRV_SPI_FREQ_4M,
		    .mode         = NRF_DRV_SPI_MODE_0,
		    .bit_order    = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST,
	};
}

static S25FL128 *s25fl128;
static uint8_t wr_buffer[S25FL128_PAGE_SIZE];
static uint8_t rd_buffer[S25FL128_PAGE_SIZE];

TEST_GROUP(SpiFlash)
{
	void setup() {
		s25fl128 = new S25FL128(spi, spi_config);
		s25fl128->erase_all();
		for (unsigned int i = 0; i < S25FL128_PAGE_SIZE; i++)
		{
			wr_buffer[i] = i;
			rd_buffer[i] = 0;
		}
	}

	void teardown() {
		delete s25fl128;
	}
};

TEST(SpiFlash, ReadErasedFlash)
{
	uint32_t data = 0;
	s25fl128->read(0, (uint8_t *)&data, 4);
	CHECK_EQUAL(0xFFFFFFFF, data);
}

TEST(SpiFlash, WriteFlashWithReadBack)
{
	uint32_t rd = 0;
	uint32_t wr = 0x12345678;
	s25fl128->write(0, (uint8_t *)&wr, 4);
	s25fl128->read(0, (uint8_t *)&rd, 4);
	CHECK_EQUAL(rd, wr);
}

TEST(SpiFlash, WriteEraseFlashWithReadBack)
{
	uint32_t rd = 0;
	uint32_t wr = 0x12345678;
	s25fl128->write(0, (uint8_t *)&wr, 4);
	s25fl128->erase_block(0);
	s25fl128->read(0, (uint8_t *)&rd, 4);
	CHECK_EQUAL(0xFFFFFFFF, rd);
}

TEST(SpiFlash, WriteFullPageWithReadBack)
{
	s25fl128->write(0, wr_buffer, S25FL128_PAGE_SIZE);
	s25fl128->read(0, rd_buffer, S25FL128_PAGE_SIZE);
	MEMCMP_EQUAL(wr_buffer, rd_buffer, S25FL128_PAGE_SIZE);
}

TEST(SpiFlash, WriteMultiPageWithReadBack)
{
	for (unsigned int i = 0; i < 4; i++)
	{
		s25fl128->write(i*S25FL128_PAGE_SIZE, wr_buffer, S25FL128_PAGE_SIZE);
		s25fl128->read(i*S25FL128_PAGE_SIZE, rd_buffer, S25FL128_PAGE_SIZE);
		MEMCMP_EQUAL(rd_buffer, wr_buffer, S25FL128_PAGE_SIZE);
	}
}

TEST(SpiFlash, WriteBlockWithReadBack)
{
	for (int i = 0; i < S25FL128_BLOCK_SIZE / S25FL128_PAGE_SIZE; i++)
		s25fl128->write(i*S25FL128_PAGE_SIZE, wr_buffer, S25FL128_PAGE_SIZE);
	s25fl128->erase_block(1*S25FL128_BLOCK_SIZE);
	for (int i = 0; i < S25FL128_BLOCK_SIZE / S25FL128_PAGE_SIZE; i++)
		s25fl128->read(i*S25FL128_PAGE_SIZE, rd_buffer, S25FL128_PAGE_SIZE);
		MEMCMP_EQUAL(wr_buffer, rd_buffer, S25FL128_PAGE_SIZE);
}

TEST(SpiFlash, WriteBlockEraseWithReadBack)
{
	for (int i = 0; i < S25FL128_BLOCK_SIZE / S25FL128_PAGE_SIZE; i++)
		s25fl128->write(i*S25FL128_PAGE_SIZE, wr_buffer, S25FL128_PAGE_SIZE);
	s25fl128->erase_block(0);
	for (int i = 0; i < S25FL128_BLOCK_SIZE / S25FL128_PAGE_SIZE; i++)
		s25fl128->read(i*S25FL128_PAGE_SIZE, rd_buffer, S25FL128_PAGE_SIZE);
		CHECK_EQUAL(0xFF, rd_buffer[0]);
}
