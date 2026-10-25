/* XDK write-back-cache kick for the NV2A command stream. */
#ifndef XBOX_NV2A_WBC_FLUSH_H
#define XBOX_NV2A_WBC_FLUSH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Fence guest writes, set NV_PFB_WBC_FLUSH in the device's flush word and wait
 * until the executor clears it. device_ptr_va is the guest address that holds
 * the device object; the object's register block is read from +0x17C8.
 * object_out and value_out receive the register block and the flush word
 * value written, so a guest-state wrapper can publish them. */
void xbox_Nv2aWbcFlush(uint32_t device_ptr_va, uint32_t *object_out, uint32_t *value_out);

#ifdef __cplusplus
}
#endif

#endif
