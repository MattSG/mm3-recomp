/* Exercise the production startup helper against disposable files only.
 * Deletion requires full reads and successful closes of read-only handles. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>

static wchar_t source_name[MAX_PATH], cache_name[MAX_PATH];
static int deletes, read_calls, fail_read_at, fail_delete, reparse_cache;
static int same_path, bad_translation;
static int open_calls, info_calls, fail_info_at, close_calls, fail_close_at;
static int source_eof, cache_eof;
static HANDLE helper_source = INVALID_HANDLE_VALUE, helper_cache = INVALID_HANDLE_VALUE;

static HANDLE test_create(LPCWSTR path, DWORD access, DWORD share,
                          LPSECURITY_ATTRIBUTES security, DWORD disposition,
                          DWORD flags, HANDLE template_file)
{
    HANDLE file;
    ++open_calls;
    assert(access == GENERIC_READ && share == FILE_SHARE_READ);
    assert(disposition == OPEN_EXISTING && flags == FILE_FLAG_OPEN_REPARSE_POINT);
    file = CreateFileW(path, access, share, security, disposition, flags, template_file);
    if (!_wcsicmp(path, source_name)) helper_source = file;
    else {
        assert(!_wcsicmp(path, cache_name));
        helper_cache = file;
    }
    return file;
}

static BOOL test_info(HANDLE file, LPBY_HANDLE_FILE_INFORMATION info)
{
    if (++info_calls == fail_info_at) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    return GetFileInformationByHandle(file, info);
}

static BOOL test_close(HANDLE file)
{
    BOOL result;
    assert(file == helper_source || file == helper_cache);
    if (++close_calls == fail_close_at) {
        SetLastError(ERROR_INVALID_HANDLE);
        return FALSE;
    }
    result = CloseHandle(file);
    if (result) {
        if (file == helper_source) helper_source = INVALID_HANDLE_VALUE;
        else helper_cache = INVALID_HANDLE_VALUE;
    }
    return result;
}

static BOOL test_read(HANDLE file, LPVOID buffer, DWORD count,
                      LPDWORD read, LPOVERLAPPED overlapped)
{
    BOOL result;
    if (++read_calls == fail_read_at) {
        SetLastError(ERROR_READ_FAULT);
        return FALSE;
    }
    result = ReadFile(file, buffer, count, read, overlapped);
    if (result && !*read) {
        if (file == helper_source) source_eof = 1;
        else {
            assert(file == helper_cache);
            cache_eof = 1;
        }
    }
    return result;
}

static BOOL test_delete(LPCWSTR path)
{
    ++deletes;
    assert(!_wcsicmp(path, cache_name));
    assert(source_eof && cache_eof && close_calls == 2);
    assert(helper_source == INVALID_HANDLE_VALUE && helper_cache == INVALID_HANDLE_VALUE);
    if (fail_delete) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    return DeleteFileW(path);
}

static DWORD test_attributes(LPCWSTR path)
{
    DWORD result = GetFileAttributesW(path);
    if (reparse_cache && !_wcsicmp(path, cache_name) &&
        result != INVALID_FILE_ATTRIBUTES) result |= FILE_ATTRIBUTE_REPARSE_POINT;
    return result;
}

#define ReadFile test_read
#define DeleteFileW test_delete
#define GetFileAttributesW test_attributes
#define CreateFileW test_create
#define GetFileInformationByHandle test_info
#define CloseHandle test_close
#include "../src/derived_cache.c"
#include "../src/mm3_asset_cache.c"
#undef ReadFile
#undef DeleteFileW
#undef GetFileAttributesW
#undef CreateFileW
#undef GetFileInformationByHandle
#undef CloseHandle

BOOL xbox_translate_path(const char *path, xbox_host_char *out, DWORD length)
{
    if (bad_translation) return FALSE;
    if (!strcmp(path, "D:\\Data\\Data_hd.zip"))
        wcscpy_s(out, length, source_name);
    else {
        assert(!strcmp(path, "Z:\\Data\\Data_hd.zip"));
        wcscpy_s(out, length, same_path ? source_name : cache_name);
    }
    return TRUE;
}

static void write_file(const wchar_t *path, int changed, int shorter)
{
    static unsigned char data[2 * DERIVED_CACHE_BLOCK_SIZE + 19];
    HANDLE file = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD written;
    assert(file != INVALID_HANDLE_VALUE);
    memset(data, 0x37, sizeof(data));
    if (changed == 1) data[0] = 0x99;
    if (changed == 2) data[sizeof(data) - 1] = 0x99;
    assert(WriteFile(file, data, (DWORD)sizeof(data) - (DWORD)shorter, &written, NULL));
    assert(written == sizeof(data) - (unsigned)shorter);
    assert(CloseHandle(file));
}

static int exists(const wchar_t *path)
{
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static void set_creation(const wchar_t *path, ULONGLONG ticks)
{
    HANDLE file = CreateFileW(path, FILE_WRITE_ATTRIBUTES, 0, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    FILETIME creation = { (DWORD)ticks, (DWORD)(ticks >> 32) };
    assert(file != INVALID_HANDLE_VALUE);
    assert(SetFileTime(file, &creation, NULL, NULL));
    assert(CloseHandle(file));
}

static BY_HANDLE_FILE_INFORMATION file_info(const wchar_t *path)
{
    BY_HANDLE_FILE_INFORMATION info;
    /* An exclusive open also proves the helper closed its inspection handle. */
    HANDLE file = CreateFileW(path, GENERIC_READ | FILE_WRITE_ATTRIBUTES, 0, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    assert(file != INVALID_HANDLE_VALUE);
    assert(GetFileInformationByHandle(file, &info));
    assert(CloseHandle(file));
    return info;
}

static void reset(void)
{
    assert(helper_source == INVALID_HANDLE_VALUE && helper_cache == INVALID_HANDLE_VALUE);
    if (exists(cache_name)) assert(DeleteFileW(cache_name));
    write_file(source_name, 0, 0);
    write_file(cache_name, 0, 0);
    deletes = read_calls = fail_read_at = fail_delete = reparse_cache = 0;
    same_path = bad_translation = 0;
    open_calls = info_calls = fail_info_at = close_calls = fail_close_at = 0;
    source_eof = cache_eof = 0;
    set_creation(source_name, 132000000000000000ULL);
    set_creation(cache_name, 132000000000000000ULL);
}

int main(void)
{
    wchar_t directory[MAX_PATH];
    HANDLE locked;
    DWORD length = GetCurrentDirectoryW(MAX_PATH, directory);
    BY_HANDLE_FILE_INFORMATION source_before, cache_before, after;
    static const ULONGLONG within_bucket[] = { 1ULL, 4510000ULL, 19999999ULL };
    assert(length && length < MAX_PATH);
    assert(swprintf_s(source_name, MAX_PATH, L"%s\\source.zip", directory) > 0);
    assert(swprintf_s(cache_name, MAX_PATH, L"%s\\cache.zip", directory) > 0);

    reset();
    mm3_asset_cache_refresh();
    assert(!deletes && exists(cache_name));
    assert(read_calls > 4); /* Covers the boundary and final partial block. */
    assert(source_eof && cache_eof && open_calls == 2 && close_calls == 2);
    file_info(source_name);
    file_info(cache_name); /* Both inspection handles closed. */

    for (size_t i = 0; i < sizeof within_bucket / sizeof *within_bucket; ++i) {
        reset();
        set_creation(source_name, 132000000000000000ULL + within_bucket[i]);
        source_before = file_info(source_name);
        cache_before = file_info(cache_name); /* Retail may truncate to seconds. */
        mm3_asset_cache_refresh();
        assert(!deletes && exists(cache_name) && source_eof && cache_eof);
        after = file_info(cache_name);
        assert(CompareFileTime(&after.ftCreationTime, &cache_before.ftCreationTime) == 0);
        assert(CompareFileTime(&after.ftLastWriteTime, &cache_before.ftLastWriteTime) == 0);
        after = file_info(source_name);
        assert(CompareFileTime(&after.ftCreationTime, &source_before.ftCreationTime) == 0);
        assert(CompareFileTime(&after.ftLastWriteTime, &source_before.ftLastWriteTime) == 0);
        mm3_asset_cache_refresh();
        assert(!deletes && exists(cache_name)); /* Repeated refresh is current. */
    }

    reset();
    set_creation(cache_name, 132000030000000000ULL); /* Same bytes, copied later. */
    source_before = file_info(source_name);
    mm3_asset_cache_refresh();
    assert(deletes == 1 && !exists(cache_name));
    after = file_info(source_name);
    assert(CompareFileTime(&after.ftCreationTime, &source_before.ftCreationTime) == 0);
    assert(CompareFileTime(&after.ftLastWriteTime, &source_before.ftLastWriteTime) == 0);
    assert(after.nFileSizeLow == source_before.nFileSizeLow &&
           after.nFileSizeHigh == source_before.nFileSizeHigh);
    mm3_asset_cache_refresh();
    assert(deletes == 1); /* Missing cache does not cause another deletion. */

    reset();
    set_creation(source_name, 132000000019999999ULL);
    set_creation(cache_name, 132000000020000000ULL); /* One tick crosses the boundary. */
    mm3_asset_cache_refresh();
    assert(deletes == 1 && !exists(cache_name));

    reset();
    set_creation(cache_name, 132000030000000000ULL);
    cache_before = file_info(cache_name);
    fail_read_at = 5; /* Full equality was not established. */
    mm3_asset_cache_refresh();
    after = file_info(cache_name);
    assert(!deletes && read_calls == 5);
    assert(CompareFileTime(&after.ftCreationTime, &cache_before.ftCreationTime) == 0);

    for (int i = 1; i <= 2; ++i) {
        reset();
        set_creation(cache_name, 132000030000000000ULL);
        fail_info_at = i;
        mm3_asset_cache_refresh();
        assert(!deletes && !read_calls && exists(cache_name) && close_calls == 2);
        file_info(source_name);
        file_info(cache_name);

        reset();
        set_creation(cache_name, 132000030000000000ULL);
        cache_before = file_info(cache_name);
        fail_close_at = i;
        mm3_asset_cache_refresh();
        assert(!deletes && source_eof && cache_eof && close_calls == 2 && exists(cache_name));
        /* Release the deliberately unclosed test handle, never the helper. */
        if (helper_source != INVALID_HANDLE_VALUE) {
            assert(CloseHandle(helper_source));
            helper_source = INVALID_HANDLE_VALUE;
        }
        if (helper_cache != INVALID_HANDLE_VALUE) {
            assert(CloseHandle(helper_cache));
            helper_cache = INVALID_HANDLE_VALUE;
        }
        after = file_info(cache_name);
        assert(CompareFileTime(&after.ftCreationTime, &cache_before.ftCreationTime) == 0);
    }

    reset();
    write_file(cache_name, 1, 0); /* Different bytes, identical length. */
    mm3_asset_cache_refresh();
    assert(deletes == 1 && !exists(cache_name));

    reset();
    write_file(cache_name, 2, 0); /* Final partial block differs. */
    mm3_asset_cache_refresh();
    assert(deletes == 1 && !exists(cache_name));

    reset();
    write_file(cache_name, 0, 1); /* Different length. */
    mm3_asset_cache_refresh();
    assert(deletes == 1 && !exists(cache_name));

    reset();
    assert(DeleteFileW(cache_name));
    mm3_asset_cache_refresh();
    assert(!deletes && !read_calls);

    reset();
    assert(DeleteFileW(source_name));
    mm3_asset_cache_refresh();
    assert(!deletes && exists(cache_name));

    reset();
    write_file(cache_name, 1, 0);
    fail_read_at = 5; /* Failure after the first block proved different. */
    mm3_asset_cache_refresh();
    assert(!deletes && exists(cache_name) && read_calls == 5);

    reset();
    write_file(cache_name, 1, 0);
    locked = CreateFileW(cache_name, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    assert(locked != INVALID_HANDLE_VALUE);
    mm3_asset_cache_refresh();
    assert(!deletes && exists(cache_name));
    assert(CloseHandle(locked));

    reset();
    set_creation(cache_name, 132000030000000000ULL);
    locked = CreateFileW(source_name, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    assert(locked != INVALID_HANDLE_VALUE);
    mm3_asset_cache_refresh();
    assert(!deletes && !read_calls && exists(cache_name));
    assert(CloseHandle(locked));

    reset();
    same_path = 1;
    mm3_asset_cache_refresh();
    assert(!deletes && !read_calls && exists(source_name));

    reset();
    assert(DeleteFileW(cache_name));
    assert(CreateHardLinkW(cache_name, source_name, NULL));
    mm3_asset_cache_refresh();
    assert(!deletes && !read_calls && exists(source_name));

    reset();
    write_file(cache_name, 1, 0);
    reparse_cache = 1;
    mm3_asset_cache_refresh();
    assert(!deletes && !read_calls && exists(cache_name));

    reset();
    bad_translation = 1;
    mm3_asset_cache_refresh();
    assert(!deletes && !read_calls && exists(cache_name));

    reset();
    write_file(cache_name, 1, 0);
    fail_delete = 1;
    mm3_asset_cache_refresh();
    assert(deletes == 1 && exists(cache_name));

    reset();
    set_creation(cache_name, 132000030000000000ULL);
    cache_before = file_info(cache_name);
    fail_delete = 1;
    mm3_asset_cache_refresh();
    after = file_info(cache_name);
    assert(deletes == 1 && exists(cache_name));
    assert(CompareFileTime(&after.ftCreationTime, &cache_before.ftCreationTime) == 0);

    puts("PASS: read-only cache verification; retail two-second creation buckets; mismatched metadata invalidated only after full reads and successful closes; matching metadata retained; read/info/close/open/delete failures preserve cache; source unchanged; byte/path guards");
    return 0;
}
