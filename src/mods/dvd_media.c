/* The DVD media check. MM3's emulated drive answers the game's SCSI
 * MODE SENSE(10) for page 0x3E with the layout the game's own validation
 * reads: byte 11 must be exactly 1, bytes 10 and 12 must both be non-zero.
 * Anything else reads as "not the expected disc" and the game exits through
 * HalReturnToFirmware. Required: the game does not boot without it. */
#include <stdint.h>
#include "recomp_types.h"
#include "mods.h"

extern void xbox_kernel_set_dvd_scsi_hook(
    int (*hook)(uint32_t scsi_va, uint32_t scsi_len, uint32_t *information));

/* SCSI_PASS_THROUGH_DIRECT, 32-bit layout, 44 bytes:
 *   0 Length(USHORT)  2 ScsiStatus  3 PathId  4 TargetId  5 Lun
 *   6 CdbLength  7 SenseInfoLength  8 DataIn
 *   12 DataTransferLength  16 TimeOutValue  20 DataBuffer
 *   24 SenseInfoOffset  28 Cdb[16] */
static int dvd_scsi(uint32_t scsi_va, uint32_t scsi_len, uint32_t *information)
{
    uint32_t data_va, data_len, i;

    if (scsi_len < 44 || MEM8(scsi_va + 28) != 0x5A)   /* not MODE SENSE(10) */
        return 0;

    data_va  = MEM32(scsi_va + 20);
    data_len = MEM32(scsi_va + 12);
    if (data_va && data_len) {
        for (i = 0; i < data_len; i++)
            MEM8(data_va + i) = 0;

        /* An 8-byte MODE SENSE(10) parameter header, then the page. */
        if (data_len >= 2) {
            MEM8(data_va + 0) = 0;
            MEM8(data_va + 1) = 26;   /* mode data length */
        }
        if (data_len >= 10) {
            MEM8(data_va + 8) = 0x3E; /* page code */
            MEM8(data_va + 9) = 18;   /* page length */
        }
        if (data_len >= 13) {
            MEM8(data_va + 10) = 1;   /* non-zero */
            MEM8(data_va + 11) = 1;   /* exactly 1 */
            MEM8(data_va + 12) = 1;   /* non-zero */
        }
    }

    MEM8(scsi_va + 2) = 0;            /* ScsiStatus = GOOD */
    *information = scsi_len;
    return 1;
}

void mm3_dvd_media_init(void)
{
    xbox_kernel_set_dvd_scsi_hook(dvd_scsi);
}
