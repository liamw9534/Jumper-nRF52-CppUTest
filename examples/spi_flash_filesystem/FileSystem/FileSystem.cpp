#include "FileSystem.h"
#include <algorithm>
#include <assert.h>

extern "C" {
#include <string.h>
}

#define FLASH(device) reinterpret_cast<SpiFlash *>(device)


static inline uint8_t get_user_flags(fs_priv_t *fs_priv, uint8_t sector)
{
    return fs_priv->alloc_unit_list[sector].file_info.file_flags.user_flags;
}

static inline uint8_t get_mode_flags(fs_priv_t *fs_priv, uint8_t sector)
{
    return fs_priv->alloc_unit_list[sector].file_info.file_flags.mode_flags;
}

static inline uint8_t get_file_protect(fs_priv_t *fs_priv, uint8_t sector)
{
    return fs_priv->alloc_unit_list[sector].file_info.file_protect;
}

static inline uint32_t get_alloc_counter(fs_priv_t *fs_priv, uint8_t sector)
{
    return fs_priv->alloc_unit_list[sector].alloc_counter;
}

static inline uint8_t get_file_id(fs_priv_t *fs_priv, uint8_t sector)
{
    return fs_priv->alloc_unit_list[sector].file_info.file_id;
}

static inline bool is_last_allocation_unit(fs_priv_t *fs_priv, uint8_t sector)
{
    return (fs_priv->alloc_unit_list[sector].file_info.next_allocation_unit ==
            (uint8_t)FS_PRIV_NOT_ALLOCATED);
}

static inline uint8_t next_allocation_unit(fs_priv_t *fs_priv, uint8_t sector)
{
    return fs_priv->alloc_unit_list[sector].file_info.next_allocation_unit;
}

static int init_fs_priv(fs_priv_t *fs_priv, void *device)
{
    fs_priv->device = device;  /* Keep a copy of the device index */

    /* Iterate through each sector and read the allocation unit header into
     * our file system device structure.
     */
    for (uint8_t sector = 0; sector < FS_PRIV_MAX_SECTORS; sector++)
    {
        if (FLASH(device)->read(FS_PRIV_SECTOR_ADDR(sector),
        		(uint8_t *)&fs_priv->alloc_unit_list[sector],
        		sizeof(fs_priv_alloc_unit_header_t)))
            return FS_ERROR_FLASH_MEDIA;
    }

    /* TODO: we should probably implement some kind of file system
     * validation check here to avoid using a corrupt file system.
     */
    return FS_NO_ERROR;
}

static inline uint16_t cached_bytes(fs_priv_handle_t *fs_priv_handle)
{
    return (fs_priv_handle->curr_data_offset - fs_priv_handle->last_data_offset);
}

static inline uint32_t remaining_bytes(fs_priv_handle_t *fs_priv_handle)
{
    return (FS_PRIV_USABLE_SIZE - fs_priv_handle->curr_data_offset);
}

static uint8_t find_free_allocation_unit(fs_priv_t *fs_priv)
{
    uint32_t min_allocation_counter = (uint32_t)FS_PRIV_NOT_ALLOCATED;
    uint8_t free_sector = (uint8_t)FS_PRIV_NOT_ALLOCATED;

    /* In the worst case, we have to check every sector on the disk to
     * find a free sector.
     */
    for (uint8_t sector = 0; sector < FS_PRIV_MAX_SECTORS; sector++)
    {
        /* Consider only unallocated sectors */
        if ((uint8_t)FS_PRIV_NOT_ALLOCATED == get_file_id(fs_priv, sector))
        {
            /* Special case for unformatted sector */
            if ((uint32_t)FS_PRIV_NOT_ALLOCATED == get_alloc_counter(fs_priv, sector))
            {
                /* Choose this sector since it has never been used */
                free_sector = sector;
                break;
            }
            else if (get_alloc_counter(fs_priv, sector) < min_allocation_counter)
            {
                /* This is now the least used sector */
                min_allocation_counter = fs_priv->alloc_unit_list[sector].alloc_counter;
                free_sector = sector;
            }
        }
    }

    return free_sector;
}

static int allocate_handle(fs_priv_handle_t *fs_priv_handle_list,
		fs_priv_t *fs_priv, fs_priv_handle_t **handle)
{
    for (uint8_t i = 0; i < FS_PRIV_MAX_HANDLES; i++)
    {
        if (NULL == fs_priv_handle_list[i].fs_priv)
        {
            *handle = &fs_priv_handle_list[i];
            fs_priv_handle_list[i].fs_priv = fs_priv;

            return FS_NO_ERROR;
        }
    }

    return FS_ERROR_NO_FREE_HANDLE;
}

static void free_handle(fs_priv_handle_t *handle)
{
    handle->fs_priv = NULL;
}

static bool is_protected(uint8_t protection_bits)
{
    uint8_t count_bits;
    for (count_bits = 0; protection_bits; count_bits++)
        protection_bits &= protection_bits - 1; /* Clear LSB */

    /* Odd number of bits means the file is protected */
    return (count_bits & 1) ? true : false;
}

static uint8_t set_protected(bool prot, uint8_t protected_bits)
{
    /* Only update protected bits if the current bits do not
     * match the required protection state
     */
    if (prot != is_protected(protected_bits))
        protected_bits &= protected_bits - 1; /* Clear LSB */

    /* Return new protected bits */
    return protected_bits;
}

static uint8_t find_file_root(fs_priv_t *fs_priv, uint8_t file_id)
{
    uint8_t root = (uint8_t)FS_PRIV_NOT_ALLOCATED;
    uint8_t parent[FS_PRIV_MAX_SECTORS];

    /* Do not allow FS_PRIV_NOT_ALLOCATED as file_id */
    if ((uint8_t)FS_PRIV_NOT_ALLOCATED == file_id)
        return FS_PRIV_NOT_ALLOCATED;

    /* Reset parent list to known values */
    memset(parent, (uint8_t)FS_PRIV_NOT_ALLOCATED, sizeof(parent));

    /* Scan all sectors and build a list of parent nodes for each
     * sector allocated against the specified file_id
     */
    for (uint8_t sector = 0; sector < FS_PRIV_MAX_SECTORS; sector++)
    {
        /* Filter by file_id */
        if (file_id == get_file_id(fs_priv, sector))
        {
            if (!is_last_allocation_unit(fs_priv, sector))
                parent[next_allocation_unit(fs_priv, sector)] = sector;
            /* Arbitrarily choose first found sector as the candidate root node */
            if (root == (uint8_t)FS_PRIV_NOT_ALLOCATED)
                root = sector;
        }
    }

    /* Start with candidate root sector and walk all the parent nodes until we terminate */
    while (root != (uint8_t)FS_PRIV_NOT_ALLOCATED)
    {
        /* Does this node have a parent?  If not then it is the root node */
        if (parent[root] == (uint8_t)FS_PRIV_NOT_ALLOCATED)
            break;
        else
            root = parent[root]; /* Try next one in chain */
    }

    /* Return the root node which may be FS_PRIV_NOT_ALLOCATED if no file was found */
    return root;
}

static int check_file_flags(fs_priv_t *fs_priv, uint8_t root, unsigned int mode)
{
    if ((uint8_t)FS_PRIV_NOT_ALLOCATED == root)
    {
        /* File does not exist so unless this is a create request then
         * return an error.
         */
        if ((mode & FS_FILE_CREATE) == 0)
            return FS_ERROR_FILE_NOT_FOUND;
    }
    else
    {
        /* Don't allow the file to be created since it already exists */
        if (mode & FS_FILE_CREATE)
            return FS_ERROR_FILE_ALREADY_EXISTS;

        /* If opened as writeable then make sure file is not protected */
        const uint8_t protection_bits = fs_priv->alloc_unit_list[root].file_info.file_protect;
        if ((mode & FS_FILE_WRITEABLE) && is_protected(protection_bits))
            return FS_ERROR_FILE_PROTECTED;
    }

    return FS_NO_ERROR;
}

static uint8_t find_next_session_offset(fs_priv_t *fs_priv, uint8_t sector, uint32_t *data_offset)
{
    uint32_t write_offset = (uint8_t)FS_PRIV_NOT_ALLOCATED;
    uint32_t write_offsets[FS_PRIV_NUM_WRITE_SESSIONS];

    /* Read all the session offsets from flash */
    FLASH(fs_priv->device)->read(FS_PRIV_SECTOR_ADDR(sector) + FS_PRIV_SESSION_OFFSET,
    		(uint8_t *)write_offsets,
            sizeof(write_offsets));

    /* Scan session offsets to find first free entry.  If all entries
     * are already used then no further writes can be done and
     * FS_PRIV_NOT_ALLOCATED shall be returned.
     */
    for (uint8_t i = 0; i < FS_PRIV_NUM_WRITE_SESSIONS; i++)
    {
        if ((uint32_t)FS_PRIV_NOT_ALLOCATED == write_offsets[i])
        {
            if (i == 0)
                *data_offset = 0; /* None yet assigned */

            /* Next available write offset has been found */
            write_offset = i;
            break;
        }

        /* Set last known write offset */
        *data_offset = write_offsets[i];
    }

    return write_offset;
}

static uint8_t find_last_allocation_unit(fs_priv_t *fs_priv, uint8_t root)
{
    /* Loop through until we reach the end of the file chain */
    while (root != (uint8_t)FS_PRIV_NOT_ALLOCATED)
    {
        if (next_allocation_unit(fs_priv, root) != (uint8_t)FS_PRIV_NOT_ALLOCATED)
            root = next_allocation_unit(fs_priv, root);
        else
            break;
    }

    return root;
}

static uint8_t find_eof(fs_priv_t *fs_priv, uint8_t root, uint8_t *last_alloc_unit, uint32_t *data_offset)
{
    *last_alloc_unit = find_last_allocation_unit(fs_priv, root);
    return find_next_session_offset(fs_priv, *last_alloc_unit, data_offset);
}

static bool is_eof(fs_priv_handle_t *fs_priv_handle)
{
    fs_priv_t *fs_priv = fs_priv_handle->fs_priv;

    return ((fs_priv_handle->last_data_offset == fs_priv_handle->curr_data_offset) &&
            next_allocation_unit(fs_priv, fs_priv_handle->curr_allocation_unit) ==
                    (uint8_t)FS_PRIV_NOT_ALLOCATED);
}

static int erase_allocation_unit(fs_priv_t *fs_priv, uint8_t sector)
{
    /* Read existing allocation counter and increment for next allocation */
    uint32_t new_alloc_counter = fs_priv->alloc_unit_list[sector].alloc_counter + 1;

    /* Erase the entire sector (should be all FF) */
    if (FLASH(fs_priv->device)->erase_block(FS_PRIV_SECTOR_ADDR(sector)))
        return FS_ERROR_FLASH_MEDIA;

    /* Reset local copy of allocation unit header */
    memset(&fs_priv->alloc_unit_list[sector], 0xFF, sizeof(fs_priv->alloc_unit_list[sector]));

    /* Set allocation counter locally */
    fs_priv->alloc_unit_list[sector].alloc_counter = new_alloc_counter;

    /* Write only the allocation counter to flash */
    if (FLASH(fs_priv->device)->write(FS_PRIV_SECTOR_ADDR(sector) + FS_PRIV_ALLOC_COUNTER_OFFSET,
    		(const uint8_t *)&new_alloc_counter,
            sizeof(uint32_t)))
        return FS_ERROR_FLASH_MEDIA;

    return FS_NO_ERROR;
}

static int flush_page_cache(fs_priv_handle_t *fs_priv_handle)
{
    uint32_t size, address;

    /* Compute number of bytes in page cache; note that the cache fill policy
     * means that we can never exceed the next page boundary, so we don't need to
     * worry about crossing a page boundary.
     */
    size = cached_bytes(fs_priv_handle);

    if (size > 0)
    {
        /* Physical address calculation */
        address = FS_PRIV_SECTOR_ADDR(fs_priv_handle->curr_allocation_unit) +
                FS_PRIV_ALLOC_UNIT_SIZE + fs_priv_handle->last_data_offset;

        /* Write cached data to flash */
        if (FLASH(fs_priv_handle->fs_priv->device)->write(address,
        		fs_priv_handle->page_cache,
        		size))
            return FS_ERROR_FLASH_MEDIA;

        /* Mark the cache as empty and advance the pointer */
        fs_priv_handle->last_data_offset = fs_priv_handle->curr_data_offset;
    }

    return FS_NO_ERROR;
}

static int update_session_offset(fs_priv_handle_t *fs_priv_handle)
{
    uint32_t address;

    /* Check to see if the session offset needs updating */
    if (fs_priv_handle->last_data_offset == fs_priv_handle->curr_session_value)
        return FS_NO_ERROR;

    /* Compute physical address for next write offset */
    address = FS_PRIV_SECTOR_ADDR(fs_priv_handle->curr_allocation_unit) +
            FS_PRIV_SESSION_OFFSET +
            (sizeof(uint32_t) * fs_priv_handle->curr_session_offset);

    /* Write the new offset into the allocation unit */
    if (FLASH(fs_priv_handle->fs_priv->device)->write(
            address,
            (const uint8_t *)&fs_priv_handle->last_data_offset,
            sizeof(uint32_t)))
        return FS_ERROR_FLASH_MEDIA;

    /* Update session write pointer */
    fs_priv_handle->curr_session_value = fs_priv_handle->last_data_offset;

    /* Set next available write offset */
    fs_priv_handle->curr_session_offset++;
    if (fs_priv_handle->curr_session_offset >= FS_PRIV_NUM_WRITE_SESSIONS)
    {
        /* No further sessions free */
        fs_priv_handle->curr_session_offset = (uint8_t)FS_PRIV_NOT_ALLOCATED;
    }

    return FS_NO_ERROR;
}

static int flush_handle(fs_priv_handle_t *fs_priv_handle)
{
    int ret;

    /* Don't allow flush if a session write offset is not available */
    if ((uint8_t)FS_PRIV_NOT_ALLOCATED == fs_priv_handle->curr_session_offset)
        return FS_ERROR_FILESYSTEM_FULL;

    /* Flush any bytes in the page cache */
    ret = flush_page_cache(fs_priv_handle);
    if (ret)
        return ret;

    /* Set new write offset */
    return update_session_offset(fs_priv_handle);
}

static int allocate_new_sector_to_file(fs_priv_handle_t *fs_priv_handle)
{
    uint8_t sector;
    fs_priv_t *fs_priv = fs_priv_handle->fs_priv;

    /* Find a free allocation unit */
    sector = find_free_allocation_unit(fs_priv);
    if ((uint8_t)FS_PRIV_NOT_ALLOCATED == sector)
    {
        /* File system is full but if the file type is circular
         * then we should erase the root sector and try to recycle it.
         */
        if ((fs_priv_handle->flags.mode_flags & FS_FILE_CIRCULAR) == 0 ||
            fs_priv_handle->root_allocation_unit == (uint8_t)FS_PRIV_NOT_ALLOCATED)
            return FS_ERROR_FILESYSTEM_FULL;

        /* Erase the current root sector so it can be recycled */
        uint8_t new_root =
            fs_priv->alloc_unit_list[fs_priv_handle->root_allocation_unit].file_info.next_allocation_unit;
        if (erase_allocation_unit(fs_priv, fs_priv_handle->root_allocation_unit))
            return FS_ERROR_FLASH_MEDIA;

        /* Set new root sector and also link the new sector's next pointer to
         * the new root sector.
         */
        sector = fs_priv_handle->root_allocation_unit;
        fs_priv_handle->root_allocation_unit = new_root;
    }

    /* Update file system allocation table information for this allocation unit */
    fs_priv->alloc_unit_list[sector].file_info.file_id = fs_priv_handle->file_id;
    fs_priv->alloc_unit_list[sector].file_info.next_allocation_unit = (uint8_t)FS_PRIV_NOT_ALLOCATED;
    fs_priv->alloc_unit_list[sector].file_info.file_flags.mode_flags =
            (fs_priv_handle->flags.mode_flags & FS_FILE_CIRCULAR);
    fs_priv->alloc_unit_list[sector].file_info.file_flags.user_flags =
            fs_priv_handle->flags.user_flags;

    /* Check if a root sector is already set for this handle */
    if ((uint8_t)FS_PRIV_NOT_ALLOCATED == fs_priv_handle->root_allocation_unit)
    {
        /* Assign this sector as the handle's root node */
        fs_priv_handle->root_allocation_unit = sector;

        /* Reset file protect bits */
        fs_priv->alloc_unit_list[sector].file_info.file_protect = 0xFF;
    }
    else
    {
        /* Set file protect bits for new sector using root sector file protect bits */
        fs_priv->alloc_unit_list[sector].file_info.file_protect =
                fs_priv->alloc_unit_list[fs_priv_handle->root_allocation_unit].file_info.file_protect;

        /* Chain newly allocated sector onto the end of the current sector */
        fs_priv->alloc_unit_list[fs_priv_handle->curr_allocation_unit].file_info.next_allocation_unit =
                sector;

        /* Write updated file information header contents to flash for the current sector */
        if (FLASH(fs_priv->device)->write(
                FS_PRIV_SECTOR_ADDR(fs_priv_handle->curr_allocation_unit),
                (const uint8_t *)&fs_priv->alloc_unit_list[fs_priv_handle->curr_allocation_unit],
                sizeof(fs_priv_file_info_t)))
        {
            return FS_ERROR_FLASH_MEDIA;
        }
    }

    /* Reset handle pointers to start of new sector */
    fs_priv_handle->curr_allocation_unit = sector;
    fs_priv_handle->last_data_offset = 0;
    fs_priv_handle->curr_data_offset = 0;
    fs_priv_handle->curr_session_offset = 0;
    fs_priv_handle->curr_session_value = 0;

    /* Write file information header contents to flash for new sector */
    if (FLASH(fs_priv->device)->write(
            FS_PRIV_SECTOR_ADDR(sector),
            (const uint8_t *)&fs_priv->alloc_unit_list[sector],
            sizeof(fs_priv_file_info_t)))
        return FS_ERROR_FLASH_MEDIA;

    return FS_NO_ERROR;
}

static inline bool is_full(fs_priv_handle_t *fs_priv_handle)
{
    return (fs_priv_handle->curr_session_offset == (uint8_t)FS_PRIV_NOT_ALLOCATED ||
            fs_priv_handle->curr_data_offset >= FS_PRIV_USABLE_SIZE);
}

static int write_through_cache(fs_priv_handle_t *fs_priv_handle, const uint8_t *src, uint16_t size, uint16_t *written)
{
    uint16_t cached, page_boundary, sz;

    /* Cache operation
     *
     * Rule 1: Write the cache only when there are at least page_boundary bytes in it.
     * Rule 2: Keep caching data until Rule 1 applies.
     *
     * cache = { 0 ... 512 } => number of bytes in the cache
     * page_boundary = { 0 ... 512 } => number of bytes until the next flash page boundary
     * 0 <= (page_boundary - cache) <= 512 => cache may never exceed page_boundary
     *
     * Example:
     *
     * start => page_boundary = 512, cache = 0
     * write(4) => page_boundary = 512, cache = 4
     * flush() => page_boundary = 508, cache = 0
     * write(56) => page_boundary = 508, cache = 56
     * write(3) => page_boundary = 508, cache = 59
     * write(512) => page_boundary = 512, cache = 63
     * flush() => page_boundary = 449, cache = 0
     * write(512) => page_boundary = 512, cache =  63
     * etc
     */
    cached = cached_bytes(fs_priv_handle);
    page_boundary = FS_PRIV_PAGE_SIZE - (fs_priv_handle->last_data_offset & (FS_PRIV_PAGE_SIZE - 1));
    *written = 0;

    assert(cached <= page_boundary);

    /* Append to the cache up to the limit of the next page boundary */
    sz = std::min((unsigned int)(page_boundary - cached), (unsigned int)size);
    if (sz > 0)
    {
        /* The size is guaranteed to be non-zero */
        memcpy(&fs_priv_handle->page_cache[cached], src, sz);
        src += sz;
        size -= sz;
        cached += sz;
        *written += sz;
        fs_priv_handle->curr_data_offset += sz;
    }

    /* Cache can never exceed page boundary but we should
     * remove data from the cache once there is sufficient
     * data to write to the next page boundary.
     */
    if (cached == page_boundary)
    {
        /* Write through to page boundary */
        uint32_t address = FS_PRIV_SECTOR_ADDR(fs_priv_handle->curr_allocation_unit) +
                FS_PRIV_ALLOC_UNIT_SIZE + fs_priv_handle->last_data_offset;
        if (FLASH(fs_priv_handle->fs_priv->device)->write(
                address,
                fs_priv_handle->page_cache,
                page_boundary))
            return FS_ERROR_FLASH_MEDIA;

        /* Advance last write position to the next page boundary */
        fs_priv_handle->last_data_offset += page_boundary;
    }

    if (size > 0)
    {
        /* Cache is guaranteed to be empty */
        memcpy(&fs_priv_handle->page_cache, src, size);
        *written += size;
        fs_priv_handle->curr_data_offset += size;
    }

    return FS_NO_ERROR;
}

/* FileSystem Class Methods */

int FileSystem::format()
{
    int ret;
    fs_priv_t *fs_priv = &priv;

    for (uint8_t sector = 0; sector < FS_PRIV_MAX_SECTORS; sector++)
    {
        ret = erase_allocation_unit(fs_priv, sector);
        if (ret) break;
    }

    return ret;
}

int FileSystem::open(FileHandle *handle, uint8_t file_id, unsigned int mode, uint8_t *user_flags)
{
	int ret;
    fs_priv_t *fs_priv = &priv;
    fs_priv_handle_t *fs_priv_handle;

    /* Find the root allocation unit for this file (if file exists) */
    uint8_t root = find_file_root(fs_priv, file_id);

    /* Check file identifier versus requested open mode */
    ret = check_file_flags(fs_priv, root, mode);
    if (ret)
        return ret;

    /* Allocate a free handle */
    ret = allocate_handle(fs_priv_handle_list, fs_priv, &fs_priv_handle);
    if (ret)
        return ret;

    /* Reset file handle */
    *handle = fs_priv_handle;
    fs_priv_handle->file_id = file_id;
    fs_priv_handle->root_allocation_unit = (uint8_t)FS_PRIV_NOT_ALLOCATED;

    if (root != (uint8_t)FS_PRIV_NOT_ALLOCATED)
    {
        /* Existing file: populate file handle */
        fs_priv_handle->root_allocation_unit = root;
        fs_priv_handle->flags.user_flags = get_user_flags(fs_priv, root);
        fs_priv_handle->flags.mode_flags = get_mode_flags(fs_priv, root) | mode;

        /* Retrieve existing user flags if requested */
        if (user_flags)
            *user_flags = fs_priv_handle->flags.user_flags;

        if ((mode & FS_FILE_WRITEABLE) == 0)
        {
            /* Read only - reset to beginning of root sector */
            fs_priv_handle->curr_data_offset = 0;
            fs_priv_handle->curr_allocation_unit = root;

            /* Find the last known write position in this sector so we
             * can check for when to advance to next sector or catch EOF
             */
            find_next_session_offset(fs_priv, root,
                    &fs_priv_handle->last_data_offset);
        }
        else
        {
            /* Write only: find end of file for appending new data */
            fs_priv_handle->curr_session_offset = find_eof(fs_priv, root,
                    &fs_priv_handle->curr_allocation_unit,
                    &fs_priv_handle->curr_data_offset);

            /* Reset session pointer value */
            fs_priv_handle->curr_session_value = fs_priv_handle->curr_data_offset;

            /* Page write cache should be marked empty */
            fs_priv_handle->last_data_offset = fs_priv_handle->curr_data_offset;
        }
    }
    else
    {
        /* Set file flags since we are creating a new file */
        fs_priv_handle->flags.mode_flags = mode;
        fs_priv_handle->flags.user_flags = user_flags ? *user_flags : 0;

        /* Allocate new sector to file handle */
        ret = allocate_new_sector_to_file(fs_priv_handle);
        if (ret)
            free_handle(fs_priv_handle);
    }

    return ret;
}

int FileSystem::close(FileHandle handle)
{
	if (!is_valid_handle(handle))
		return FS_ERROR_INVALID_HANDLE;

    flush(handle);

	fs_priv_handle_t *fs_priv_handle = (fs_priv_handle_t *)handle;
    free_handle(fs_priv_handle);

    return FS_NO_ERROR;
}

int FileSystem::write(FileHandle handle, const uint8_t *src, unsigned int size, unsigned int *written)
{
	if (!is_valid_handle(handle))
		return FS_ERROR_INVALID_HANDLE;

	int ret = FS_NO_ERROR;
    fs_priv_handle_t *fs_priv_handle = (fs_priv_handle_t *)handle;
    uint16_t write_size, actual_write;

    /* Reset counter */
    *written = 0;

    /* Check the file is writable */
    if ((fs_priv_handle->flags.mode_flags & FS_FILE_WRITEABLE) == 0)
        return FS_ERROR_INVALID_MODE;

    while (size > 0 && !ret)
    {
        /* Check if the current sector is full */
        if (is_full(fs_priv_handle))
        {
            /* Flush file to clear cache and update session write offset */
            flush_handle(fs_priv_handle);

            /* Allocate new sector to file chain */
            ret = allocate_new_sector_to_file(fs_priv_handle);
            if (ret) return ret;
        }

        /* The permitted write size is limited by the page size and also the
         * number of free bytes remaining in this sector i.e., we don't
         * permit the cache to fill above the sector size since we might not
         * be able to allocate a new sector for the file if no sectors
         * are free i.e., no hidden data loss allowed.
         */
        write_size = std::min((unsigned int)FS_PRIV_PAGE_SIZE, (unsigned int)size);
        write_size = std::min((unsigned int)remaining_bytes(fs_priv_handle), (unsigned int)write_size);

        /* Write data through the cache.  Note that the actual write size
         * could be less than requested if a flash media error occurred i.e.,
         * we won't try to fill the cache on a flash media error to prevent
         * hidden data loss.
         */
        ret = write_through_cache(fs_priv_handle, src, write_size, &actual_write);
        src += actual_write;
        size -= actual_write;
        *written += actual_write;
    }

    return ret;
}

int FileSystem::read(FileHandle handle, uint8_t *dest, unsigned int size, unsigned int *read)
{
	if (!is_valid_handle(handle))
		return FS_ERROR_INVALID_HANDLE;

	fs_priv_handle_t *fs_priv_handle = (fs_priv_handle_t *)handle;
    fs_priv_t *fs_priv = &priv;

    /* Reset counter */
    *read = 0;

    /* Check the file is read only */
    if (fs_priv_handle->flags.mode_flags & FS_FILE_WRITEABLE)
        return FS_ERROR_INVALID_MODE;

    /* Check for end of file */
    if (is_eof(fs_priv_handle))
        return FS_ERROR_END_OF_FILE;

    while (size > 0)
    {
        /* Check to see if we need to move to the next sector in the file chain */
        if (fs_priv_handle->last_data_offset == fs_priv_handle->curr_data_offset)
        {
            /* Check if we reached the end of the file chain */
            if (is_last_allocation_unit(fs_priv, fs_priv_handle->curr_allocation_unit))
                break;

            /* Not the end of the file chain */
            uint8_t sector = next_allocation_unit(fs_priv, fs_priv_handle->curr_allocation_unit);

            /* Find the last known write position in this sector so we
             * can check for when to advance to next sector or catch EOF
             */
            find_next_session_offset(fs_priv, sector, &fs_priv_handle->last_data_offset);

            /* Reset data offset pointer */
            fs_priv_handle->curr_allocation_unit = sector;
            fs_priv_handle->curr_data_offset = 0;
        }

        /* Read as many bytes as possible from this sector */
        uint32_t read_size = std::min((unsigned int)size, (unsigned int)(fs_priv_handle->last_data_offset - fs_priv_handle->curr_data_offset));
        uint32_t address = FS_PRIV_SECTOR_ADDR(fs_priv_handle->curr_allocation_unit) +
                FS_PRIV_FILE_DATA_REL_ADDRESS +
                fs_priv_handle->curr_data_offset;
        if (FLASH(fs_priv->device)->read(
                address,
                dest,
                read_size))
            return FS_ERROR_FLASH_MEDIA;
        *read += read_size;
        size -= read_size;
        fs_priv_handle->curr_data_offset += read_size;
    }

    return FS_NO_ERROR;
}

int FileSystem::flush(FileHandle handle)
{
	if (!is_valid_handle(handle))
		return FS_ERROR_INVALID_HANDLE;

    fs_priv_handle_t *fs_priv_handle = (fs_priv_handle_t *)handle;

    /* Make sure the file is writeable */
    if ((fs_priv_handle->flags.mode_flags & FS_FILE_WRITEABLE) == 0)
        return FS_ERROR_INVALID_MODE;

    /* Flush the handle */
    return flush_handle(fs_priv_handle);
}

int FileSystem::protect(uint8_t file_id)
{
    fs_priv_t *fs_priv = &priv;

    /* Find the root allocation unit for this file */
    uint8_t root = find_file_root(fs_priv, file_id);
    if ((uint8_t)FS_PRIV_NOT_ALLOCATED == root)
        return FS_ERROR_FILE_NOT_FOUND;

    /* No action needed if already protected */
    if (is_protected(get_file_protect(fs_priv, root)))
        return FS_NO_ERROR;

    uint8_t file_protect = get_file_protect(fs_priv, root);
    file_protect = set_protected(true, file_protect);

    /* Write updated file protect bits to flash */
    if (FLASH(fs_priv->device)->write(
            FS_PRIV_SECTOR_ADDR(root) + FS_PRIV_FILE_PROTECT_OFFSET,
            &file_protect,
            sizeof(uint8_t)))
        return FS_ERROR_FLASH_MEDIA;

    fs_priv->alloc_unit_list[root].file_info.file_protect = file_protect;

    return FS_NO_ERROR;
}

int FileSystem::unprotect(uint8_t file_id)
{
    fs_priv_t *fs_priv = &priv;

    /* Find the root allocation unit for this file */
    uint8_t root = find_file_root(fs_priv, file_id);
    if ((uint8_t)FS_PRIV_NOT_ALLOCATED == root)
        return FS_ERROR_FILE_NOT_FOUND;

    /* No action needed if already unprotected */
    if (!is_protected(get_file_protect(fs_priv, root)))
        return FS_NO_ERROR;

    uint8_t file_protect = get_file_protect(fs_priv, root);
    file_protect = set_protected(false, file_protect);

    /* Write updated file protect bits to flash */
    if (FLASH(fs_priv->device)->write(
            FS_PRIV_SECTOR_ADDR(root) + FS_PRIV_FILE_PROTECT_OFFSET,
            &file_protect,
            sizeof(uint8_t)))
        return FS_ERROR_FLASH_MEDIA;

    fs_priv->alloc_unit_list[root].file_info.file_protect = file_protect;

    return FS_NO_ERROR;
}

int FileSystem::remove(uint8_t file_id)
{
    int ret;
    fs_priv_t *fs_priv = &priv;

    /* Find the root allocation unit for this file */
    uint8_t root = find_file_root(fs_priv, file_id);
    if ((uint8_t)FS_PRIV_NOT_ALLOCATED == root)
        return FS_ERROR_FILE_NOT_FOUND;

    /* Make sure the file is not protected */
    if (is_protected(get_file_protect(fs_priv, root)))
        return FS_ERROR_FILE_PROTECTED;

    /* Erase each allocation unit associated with the file */
    while ((uint8_t)FS_PRIV_NOT_ALLOCATED != root)
    {
        uint8_t temp = root;

        /* Grab next sector in file chain before erasing this one */
        root = next_allocation_unit(fs_priv, root);

        /* This will erase both the flash sector and the local copy
         * of the allocation unit's header.
         */
        ret = erase_allocation_unit(fs_priv, temp);
        if (ret)
            return ret;
    }

    return FS_NO_ERROR;
}

bool FileSystem::is_valid_handle(FileHandle handle)
{
	intptr_t base_ptr = (intptr_t)fs_priv_handle_list;
	intptr_t end_ptr = (size_t)base_ptr + sizeof(fs_priv_handle_list);
	intptr_t offset = (intptr_t)handle - base_ptr;

	return ((intptr_t)handle >= base_ptr && (intptr_t)handle < end_ptr &&
			(offset % sizeof(fs_priv_handle_t)) == 0 &&
			(((fs_priv_handle_t *)handle)->fs_priv == &priv));
}

FileSystem::FileSystem(SpiFlash &flash_device)
{
	/* Initialize private data */
    init_fs_priv(&priv, &flash_device);

    /* Mark all handles as free */
    for (unsigned int i = 0; i < FS_MAX_HANDLES; i++)
    	free_handle(&fs_priv_handle_list[i]);
}

FileSystem::~FileSystem()
{
}
