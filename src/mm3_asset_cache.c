#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include "kernel.h"
#include "mm3_asset_cache.h"

#define MM3_CACHE_BLOCK_SIZE (64u * 1024u)
#define MM3_CACHE_CREATION_BUCKET_TICKS 20000000ULL

/* Retail sub_000224EA divides then multiplies the unsigned FILETIME by
 * 0x01312D00. Compare that quotient so the game's truncated cache timestamp
 * stays current, including source/cache differences below two seconds. */
static ULONGLONG creation_bucket(const FILETIME *time)
{
    ULONGLONG ticks = ((ULONGLONG)time->dwHighDateTime << 32) |
                      time->dwLowDateTime;
    return ticks / MM3_CACHE_CREATION_BUCKET_TICKS;
}

static void cache_error(const char *operation)
{
    fprintf(stderr, "[MM3_ASSET_CACHE] %s; cache unchanged (error=%lu)\n",
            operation, GetLastError());
}

/* Do not follow a reparse point in any component of the cache/source path.
 * Missing cache files are normal; other path errors must not cause deletion. */
static int ordinary_path(const wchar_t *path, int *missing)
{
    wchar_t component[MAX_PATH];
    size_t root_length = 3;
    int leaf = 1;
    *missing = 0;
    wcscpy_s(component, MAX_PATH, path);
    if (component[0] == L'\\' && component[1] == L'\\') {
        const wchar_t *share = wcschr(component + 2, L'\\');
        const wchar_t *end = share ? wcschr(share + 1, L'\\') : NULL;
        root_length = end ? (size_t)(end - component) + 1 : wcslen(component);
    }
    for (;;) {
        DWORD attributes = GetFileAttributesW(component);
        if (attributes == INVALID_FILE_ATTRIBUTES) {
            DWORD error = GetLastError();
            if (leaf && (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)) {
                *missing = 1;
                return 0;
            }
            cache_error("cannot inspect archive path");
            return 0;
        }
        if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) ||
            (leaf && (attributes & FILE_ATTRIBUTE_DIRECTORY))) {
            SetLastError(ERROR_INVALID_NAME);
            cache_error("archive path is not an ordinary file");
            return 0;
        }
        if (wcslen(component) <= root_length) return 1;
        wchar_t *separator = wcsrchr(component, L'\\');
        if (!separator) {
            SetLastError(ERROR_INVALID_NAME);
            cache_error("archive path has no absolute root");
            return 0;
        }
        if ((size_t)(separator - component) < root_length)
            component[root_length] = L'\0';
        else
            *separator = L'\0';
        leaf = 0;
    }
}

static int read_block(HANDLE file, unsigned char *data, DWORD *length)
{
    *length = 0;
    while (*length < MM3_CACHE_BLOCK_SIZE) {
        DWORD count = 0;
        if (!ReadFile(file, data + *length, MM3_CACHE_BLOCK_SIZE - *length,
                      &count, NULL)) return 0;
        if (!count) break;
        *length += count;
    }
    return 1;
}

/* Check the whole stream even after a mismatch: a later I/O failure leaves
 * the cache alone instead of treating an incomplete read as proof. */
static int compare_archives(HANDLE source, HANDLE cache)
{
    unsigned char source_data[MM3_CACHE_BLOCK_SIZE];
    unsigned char cache_data[MM3_CACHE_BLOCK_SIZE];
    int different = 0;
    for (;;) {
        DWORD source_length, cache_length;
        if (!read_block(source, source_data, &source_length) ||
            !read_block(cache, cache_data, &cache_length)) {
            cache_error("cannot read archives");
            return -1;
        }
        if (source_length != cache_length ||
            memcmp(source_data, cache_data, source_length < cache_length ?
                   source_length : cache_length)) different = 1;
        if (!source_length && !cache_length) return different;
    }
}

void mm3_asset_cache_refresh(void)
{
    wchar_t translated_source[MAX_PATH], translated_cache[MAX_PATH];
    wchar_t source_path[MAX_PATH], cache_path[MAX_PATH];
    HANDLE source = INVALID_HANDLE_VALUE, cache = INVALID_HANDLE_VALUE;
    BY_HANDLE_FILE_INFORMATION source_info, cache_info;
    int missing, different = -1, metadata_different = 0;
    DWORD length;

    if (!xbox_translate_path("D:\\Data\\Data_hd.zip", translated_source, MAX_PATH) ||
        !xbox_translate_path("Z:\\Data\\Data_hd.zip", translated_cache, MAX_PATH)) {
        SetLastError(ERROR_INVALID_NAME);
        cache_error("cannot resolve archive paths");
        return;
    }
    length = GetFullPathNameW(translated_source, MAX_PATH, source_path, NULL);
    if (!length || length >= MAX_PATH) {
        cache_error("cannot resolve source archive path");
        return;
    }
    length = GetFullPathNameW(translated_cache, MAX_PATH, cache_path, NULL);
    if (!length || length >= MAX_PATH) {
        cache_error("cannot resolve cache archive path");
        return;
    }
    if (!_wcsicmp(source_path, cache_path)) {
        SetLastError(ERROR_INVALID_NAME);
        cache_error("source and cache resolve to the same path");
        return;
    }
    if (!ordinary_path(cache_path, &missing)) return;
    if (!ordinary_path(source_path, &missing)) {
        if (missing) cache_error("source archive is missing");
        return;
    }
    source = CreateFileW(source_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                         OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (source == INVALID_HANDLE_VALUE) {
        cache_error("cannot open source archive");
        return;
    }
    cache = CreateFileW(cache_path, GENERIC_READ, FILE_SHARE_READ, NULL,
                        OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, NULL);
    if (cache == INVALID_HANDLE_VALUE) {
        cache_error("cannot open cached archive");
        goto close_files;
    }
    if (!GetFileInformationByHandle(source, &source_info) ||
        !GetFileInformationByHandle(cache, &cache_info)) {
        cache_error("cannot inspect open archives");
        goto close_files;
    }
    if ((source_info.dwFileAttributes | cache_info.dwFileAttributes) &
        (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)) {
        SetLastError(ERROR_INVALID_NAME);
        cache_error("open archive is not an ordinary file");
        goto close_files;
    }
    if (source_info.dwVolumeSerialNumber == cache_info.dwVolumeSerialNumber &&
        source_info.nFileIndexHigh == cache_info.nFileIndexHigh &&
        source_info.nFileIndexLow == cache_info.nFileIndexLow) {
        SetLastError(ERROR_INVALID_NAME);
        cache_error("source and cache refer to the same file");
        goto close_files;
    }
    different = compare_archives(source, cache);
    if (different == 0 &&
        creation_bucket(&source_info.ftCreationTime) !=
        creation_bucket(&cache_info.ftCreationTime)) {
        /* A copied archive can have equal bytes but different retail cache
         * metadata. Let retail rebuild it after both read-only handles close;
         * changing the timestamp here triggered immediate sharing failures. */
        different = 1;
        metadata_different = 1;
    }

close_files:
    if (cache != INVALID_HANDLE_VALUE && !CloseHandle(cache)) {
        cache_error("cannot close cached archive");
        different = -1;
    }
    if (!CloseHandle(source)) {
        cache_error("cannot close source archive");
        different = -1;
    }
    if (different == 1) {
        /* Full stream verification and successful closes are required before
         * deleting either stale bytes or a retail-significant timestamp. */
        if (DeleteFileW(cache_path))
            fprintf(stderr, "[MM3_ASSET_CACHE] stale Data_hd.zip invalidated (%s); retail startup will rebuild it\n",
                    metadata_different ? "creation-time metadata" : "archive contents");
        else
            cache_error("cannot invalidate stale cached archive");
    }
}
