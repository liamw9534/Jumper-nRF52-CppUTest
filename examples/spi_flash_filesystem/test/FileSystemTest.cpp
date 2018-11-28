#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"
#include "S25FL128.h"
#include "FileSystem.h"

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
static FileSystem *fs;
static uint8_t big_buffer[8*1024];
static uint8_t wr_buffer[1024];
static uint8_t rd_buffer[1024];

TEST_GROUP(FileSystem)
{
	void setup() {
		s25fl128 = new S25FL128(spi, spi_config);
		s25fl128->erase_all();
		fs = new FileSystem(*s25fl128);
		for (unsigned int i = 0; i < sizeof(wr_buffer); i++)
		{
			wr_buffer[i] = i;
			rd_buffer[i] = 0;
		}
	}

	void teardown() {
		delete fs;
		delete s25fl128;
	}
};

TEST(FileSystem, RemoveNoneExistentFile)
{
	CHECK_EQUAL(FS_ERROR_FILE_NOT_FOUND, fs->remove(0));
}

TEST(FileSystem, BasicApiChecks)
{
	FileHandle handle;
	unsigned int actual;

	CHECK_EQUAL(FS_NO_ERROR, fs->open(&handle, 0, FS_MODE_CREATE, NULL));
	CHECK_EQUAL(FS_ERROR_FILE_ALREADY_EXISTS, fs->open(&handle, 0, FS_MODE_CREATE, NULL));
	CHECK_EQUAL(FS_NO_ERROR, fs->write(handle, wr_buffer, sizeof(wr_buffer), &actual));
	CHECK_EQUAL(sizeof(wr_buffer), actual);
	CHECK_EQUAL(FS_NO_ERROR, fs->close(handle));
	CHECK_EQUAL(FS_NO_ERROR, fs->open(&handle, 0, FS_MODE_READONLY, NULL));
	CHECK_EQUAL(FS_NO_ERROR, fs->read(handle, rd_buffer, sizeof(rd_buffer), &actual));
	CHECK_EQUAL(sizeof(rd_buffer), actual);
	MEMCMP_EQUAL(wr_buffer, rd_buffer, sizeof(rd_buffer));
	CHECK_EQUAL(FS_ERROR_END_OF_FILE, fs->read(handle, rd_buffer, sizeof(rd_buffer), &actual));
	CHECK_EQUAL(0, actual);
	CHECK_EQUAL(FS_NO_ERROR, fs->close(handle));
	CHECK_EQUAL(FS_ERROR_INVALID_HANDLE, fs->close(handle));
}

TEST(FileSystem, FormatErasesExistingFiles)
{
	FileHandle handle;
	unsigned int actual;

	CHECK_EQUAL(FS_NO_ERROR, fs->open(&handle, 0, FS_MODE_CREATE, NULL));
	CHECK_EQUAL(FS_NO_ERROR, fs->write(handle, wr_buffer, sizeof(wr_buffer), &actual));
	CHECK_EQUAL(FS_NO_ERROR, fs->close(handle));
	CHECK_EQUAL(FS_NO_ERROR, fs->format());
	CHECK_EQUAL(FS_ERROR_FILE_NOT_FOUND, fs->open(&handle, 0, FS_MODE_READONLY, NULL));
}

IGNORE_TEST(FileSystem, SingleFileFillTheFlash)
{
	int ret;
	FileHandle handle;
	unsigned int actual, total = 0;
	const unsigned int max_blocks = s25fl128->get_capacity() / S25FL128_BLOCK_SIZE;
	const unsigned int max_capacity = max_blocks * (S25FL128_BLOCK_SIZE - S25FL128_PAGE_SIZE);

	CHECK_EQUAL(FS_NO_ERROR, fs->open(&handle, 0, FS_MODE_CREATE, NULL));

	do
	{
		ret = fs->write(handle, big_buffer, sizeof(big_buffer), &actual);
		total += actual;
	} while (ret == FS_NO_ERROR);

	CHECK_EQUAL(max_capacity, total);
	CHECK_EQUAL(FS_NO_ERROR, fs->close(handle));
	CHECK_EQUAL(FS_ERROR_FILESYSTEM_FULL, fs->open(&handle, 0, FS_MODE_CREATE, NULL));
}

TEST(FileSystem, ManyFilesExhaustAllSectors)
{
	FileHandle handle;
	const unsigned int max_blocks = s25fl128->get_capacity() / S25FL128_BLOCK_SIZE;

	for (unsigned int i = 0; i < max_blocks; i++)
	{
		CHECK_EQUAL(FS_NO_ERROR, fs->open(&handle, i, FS_MODE_CREATE, NULL));
		CHECK_EQUAL(FS_NO_ERROR, fs->close(handle));
	}

	CHECK_EQUAL(FS_ERROR_FILESYSTEM_FULL, fs->open(&handle, max_blocks, FS_MODE_CREATE, NULL));
}

TEST(FileSystem, OpenTooManyHandles)
{
	FileHandle handle;
	const unsigned int max_handles = FS_MAX_HANDLES;

	for (unsigned int i = 0; i < max_handles; i++)
		CHECK_EQUAL(FS_NO_ERROR, fs->open(&handle, i, FS_MODE_CREATE, NULL));

	CHECK_EQUAL(FS_ERROR_NO_FREE_HANDLE, fs->open(&handle, max_handles, FS_MODE_CREATE, NULL));
}

TEST(FileSystem, RemoveFileAndTryToOpenIt)
{
	FileHandle handle;

	CHECK_EQUAL(FS_NO_ERROR, fs->open(&handle, 0, FS_MODE_CREATE, NULL));
	CHECK_EQUAL(FS_NO_ERROR, fs->close(handle));
	CHECK_EQUAL(FS_NO_ERROR, fs->remove(0));
	CHECK_EQUAL(FS_ERROR_FILE_NOT_FOUND, fs->open(&handle, 0, FS_MODE_READONLY, NULL));
}

TEST(FileSystem, InvalidHandles)
{
	FileHandle handle;
	CHECK_EQUAL(FS_ERROR_INVALID_HANDLE, fs->close((FileHandle)0xdeadbeef));
	CHECK_EQUAL(FS_ERROR_INVALID_HANDLE, fs->flush((FileHandle)0xdeadbeef));
	CHECK_EQUAL(FS_ERROR_INVALID_HANDLE, fs->read((FileHandle)0xdeadbeef, NULL, 0, NULL));
	CHECK_EQUAL(FS_ERROR_INVALID_HANDLE, fs->write((FileHandle)0xdeadbeef, NULL, 0, NULL));
	CHECK_EQUAL(FS_NO_ERROR, fs->open(&handle, 0, FS_MODE_CREATE, NULL));
	CHECK_EQUAL(FS_ERROR_INVALID_HANDLE, fs->close((FileHandle)((intptr_t)handle + 1)));
}
