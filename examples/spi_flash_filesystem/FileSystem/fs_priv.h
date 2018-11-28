/* Copyright 2018 Arribada
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _FS_PRIV_H_
#define _FS_PRIV_H_

#include <stdint.h>

/* Constants */

#define FS_PRIV_NOT_ALLOCATED          -1

#ifndef FS_PRIV_MAX_DEVICES
#define FS_PRIV_MAX_DEVICES             1
#endif

#ifndef FS_PRIV_MAX_HANDLES
#define FS_PRIV_MAX_HANDLES             1
#endif

/* This defines the maximum number of sectors supported
 * by the implementation.
 */
#ifndef FS_PRIV_MAX_SECTORS
#define FS_PRIV_MAX_SECTORS             64
#endif

#ifndef FS_PRIV_SECTOR_SIZE
#define FS_PRIV_SECTOR_SIZE             (256 * 1024)
#endif

#define FS_PRIV_USABLE_SIZE             (FS_PRIV_SECTOR_SIZE - FS_PRIV_ALLOC_UNIT_SIZE)

#ifndef FS_PRIV_PAGE_SIZE
#define FS_PRIV_PAGE_SIZE               512
#endif

#define FS_PRIV_SECTOR_ADDR(s)          (s * FS_PRIV_SECTOR_SIZE)

/* Relative addresses to sector boundary for data structures */
#define FS_PRIV_ALLOC_UNIT_HEADER_REL_ADDRESS      0x00000000
#define FS_PRIV_ALLOC_UNIT_SIZE                    FS_PRIV_PAGE_SIZE
#define FS_PRIV_FILE_DATA_REL_ADDRESS \
    (FS_PRIV_ALLOC_UNIT_HEADER_REL_ADDRESS + FS_PRIV_ALLOC_UNIT_SIZE)

#define FS_PRIV_NUM_WRITE_SESSIONS      126

/* Address offsets in allocation unit */
#define FS_PRIV_FILE_ID_OFFSET          0
#define FS_PRIV_FILE_PROTECT_OFFSET     1
#define FS_PRIV_NEXT_ALLOC_UNIT_OFFSET  2
#define FS_PRIV_FLAGS_OFFSET            3
#define FS_PRIV_ALLOC_COUNTER_OFFSET    4
#define FS_PRIV_SESSION_OFFSET          8

/* Macros */

/* Types */

typedef union
{
    uint8_t flags;
    struct
    {
        uint8_t mode_flags:4;
        uint8_t user_flags:4;
    };
} fs_priv_flags_t;


typedef struct
{
    uint8_t file_id;
    uint8_t file_protect;
    uint8_t next_allocation_unit;
    fs_priv_flags_t file_flags;
} fs_priv_file_info_t;

typedef struct
{
    fs_priv_file_info_t file_info;
    uint32_t            alloc_counter;
} fs_priv_alloc_unit_header_t;

typedef struct
{
    fs_priv_alloc_unit_header_t header;
    uint32_t                    write_offset[FS_PRIV_NUM_WRITE_SESSIONS];
} fs_priv_alloc_unit_t;

typedef struct
{
    void						*device;
    fs_priv_alloc_unit_header_t alloc_unit_list[FS_PRIV_MAX_SECTORS];
} fs_priv_t;

typedef struct
{
	fs_priv_t      *fs_priv;              /*!< File system pointer */
    fs_priv_flags_t flags;                /*!< File open mode flags */
    uint8_t         file_id;              /*!< File identifier for this file */
    uint8_t         root_allocation_unit; /*!< Root sector of file */
    uint8_t         curr_allocation_unit; /*!< Current accessed sector of file */
    uint8_t         curr_session_offset;  /*!< Session offset to use */
    uint32_t        curr_session_value;   /*!< Session offset value */
    uint32_t        last_data_offset;     /*!< Read: last readable offset, Write: last flash write position */
    uint32_t        curr_data_offset;     /*!< Current read/write data offset in sector */
    uint8_t         page_cache[FS_PRIV_PAGE_SIZE]; /*!< Page align cache */
} fs_priv_handle_t;

#endif /* _FS_PRIV_H_ */
