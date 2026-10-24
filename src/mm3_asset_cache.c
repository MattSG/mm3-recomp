#include "mm3_asset_cache.h"
#include "derived_cache.h"

/* Retail sub_000224EA divides then multiplies the unsigned FILETIME by
 * 0x01312D00. Compare that quotient so the game's truncated cache timestamp
 * stays current, including source/cache differences below two seconds. */
#define MM3_CACHE_CREATION_BUCKET_TICKS 20000000ULL

void mm3_asset_cache_refresh(void)
{
    static const XboxDerivedCachePolicy policy = {
        "D:\\Data\\Data_hd.zip",
        "Z:\\Data\\Data_hd.zip",
        "MM3_ASSET_CACHE",
        "Data_hd.zip",
        "retail startup will rebuild it",
        MM3_CACHE_CREATION_BUCKET_TICKS,
    };
    xbox_derived_cache_refresh(&policy);
}
