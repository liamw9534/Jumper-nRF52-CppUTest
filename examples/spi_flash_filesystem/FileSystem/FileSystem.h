#pragma once

#include "SpiFlash.h"

extern "C" {
#include "fs_priv.h"
}


#ifndef FS_MAX_HANDLES
#define FS_MAX_HANDLES		1
#endif


#define FS_FILE_ID_NONE    0xFF
#define FS_FILE_CREATE     0x08 /*!< File create flag */
#define FS_FILE_WRITEABLE  0x04 /*!< File is writeable flag */
#define FS_FILE_CIRCULAR   0x02 /*!< File is circular flag */

#define FS_NO_ERROR                    (  0)
#define FS_ERROR_FLASH_MEDIA           ( -1)
#define FS_ERROR_FILE_ALREADY_EXISTS   ( -2)
#define FS_ERROR_FILE_NOT_FOUND        ( -3)
#define FS_ERROR_FILE_PROTECTED        ( -4)
#define FS_ERROR_NO_FREE_HANDLE        ( -5)
#define FS_ERROR_INVALID_MODE          ( -6)
#define FS_ERROR_FILESYSTEM_FULL       ( -7)
#define FS_ERROR_END_OF_FILE           ( -8)
#define FS_ERROR_BAD_DEVICE            ( -9)
#define FS_ERROR_FILE_VERSION_MISMATCH (-10)
#define FS_ERROR_INVALID_HANDLE		   (-11)

#define FS_MODE_CREATE 					(FS_FILE_CREATE | FS_FILE_WRITEABLE)
#define FS_MODE_CREATE_CIRCULAR			(FS_FILE_CREATE | FS_FILE_WRITEABLE | FS_FILE_CIRCULAR)
#define FS_MODE_WRITEONLY				FS_FILE_WRITEABLE
#define FS_MODE_READONLY				0x00


typedef void *FileHandle;


class FileSystem
{
private:
	fs_priv_t  priv;
	fs_priv_handle_t fs_priv_handle_list[FS_MAX_HANDLES];
	bool is_valid_handle(FileHandle handle);

public:
	FileSystem(SpiFlash &flash_device);
	~FileSystem();
	int format();
	int remove(uint8_t file_id);
	int open(FileHandle *handle, uint8_t file_id, unsigned int mode, uint8_t *user);
	int close(FileHandle handle);
	int flush(FileHandle handle);
	int read(FileHandle handle, uint8_t *buf, unsigned int sz, unsigned int *actual);
	int write(FileHandle handle, const uint8_t *buf, unsigned int sz, unsigned int *actual);
	int protect(uint8_t file_id);
	int unprotect(uint8_t file_id);
};
