# Historical device-layout capture decoding

Source: XboxRecomp `494beb888bca0f4487fe14838645046d19fb41d5`. Fixed device/channel/ring field offsets
are port layout observations, not a universal XDK ABI. Original decoding
blocks follow; current MM3 supplies its layout and decodes title fields.

## Original `src/kernel/xbox_memory_layout.c` boundary

```c
        /* XDK channel pointers are also contained in the raw device block.
         * Resolve the pointed-to registers while the snapshot is taken, as
         * they can name a software channel rather than the NV2A aperture. */
        if (capture_word(m->device_va, 0x17C0, &m->channel_va)) {
            m->channel_valid = capture_word(m->channel_va, 0x40, &m->channel_put);
            m->channel_valid &= capture_word(m->channel_va, 0x44, &m->channel_get);
        }
        if (capture_word(m->device_va, 0x4F8, &m->pfifo_va))
            m->pfifo_valid = capture_word(m->pfifo_va, NV2A_PFIFO_DMA_SUBROUTINE,
                                          &m->pfifo_subroutine);
```

## Original `src/kernel/nv2a_pb_scan.c` boundary

```c
        if (m->device_bytes < 0x2C) continue;
        memcpy(&low, m->device + 0x24, sizeof low);
        memcpy(&high, m->device + 0x28, sizeof high);
```

## Original `src/kernel/nv2a_pb_scan.c` boundary

```c
static int pb_capture_state_json(char *buffer, size_t capacity,
                                 const XBOX_NV2A_CAPTURE_STATE *state)
{
    int n, used = 0;
#define PB_CAPTURE_JSON(...) do { \
    n = snprintf(buffer + used, capacity - (size_t)used, __VA_ARGS__); \
    if (n < 0 || (size_t)n >= capacity - (size_t)used) return 0; \
    used += n; \
} while (0)
    PB_CAPTURE_JSON("{\"version\":%u,\"bytes\":%u,\"mmio_valid\":%u,"
        "\"user_put\":%u,\"user_get\":%u,\"pfifo_put\":%u,\"pfifo_get\":%u,"
        "\"pfifo_subroutine\":%u,\"get_held\":%u,\"get_held_value\":%u,"
        "\"scan_incomplete\":%u,\"notify_count\":%u,\"notify_param\":%u,"
        "\"notify_device_ptr\":%u,\"notify_event_offset\":%u,"
        "\"counter_mirrors\":%u,\"frame_counters\":%u,\"mirrors\":[",
        state->version, state->bytes, state->mmio_valid, state->user_put,
        state->user_get, state->pfifo_put, state->pfifo_get, state->pfifo_subroutine,
        state->get_held, state->get_held_value, state->scan_incomplete,
        state->notify_count, state->notify_param, state->notify_device_ptr,
        state->notify_event_offset, state->counter_mirrors, state->frame_counters);
    for (uint32_t i = 0; i < state->mirror_count && i < XBOX_NV2A_CAPTURE_MIRRORS; i++) {
        const XBOX_NV2A_CAPTURE_MIRROR *m = &state->mirrors[i];
        PB_CAPTURE_JSON("%s{\"device_ptr_va\":%u,\"device_va\":%u,"
            "\"device_valid\":%u,\"device_bytes\":%u,\"put_offset\":%u,"
            "\"get_ptr_offset\":%u,\"put_value\":%u,\"fence_va\":%u,"
            "\"fence_value\":%u,\"fence_valid\":%u,\"channel_va\":%u,"
            "\"channel_valid\":%u,\"channel_put\":%u,\"channel_get\":%u,"
            "\"pfifo_va\":%u,\"pfifo_valid\":%u,\"pfifo_subroutine\":%u,"
            "\"command_mode\":%u,\"command_dma\":%u,\"command_offset\":%u,"
            "\"command_pending\":%u,\"command_value\":%u,"
            "\"command_fence_va\":%u,\"command_queue_count\":%u,"
            "\"cursor\":%u,\"limit\":%u,\"flags\":%u,\"ring_low\":%u,"
            "\"ring_high\":%u,\"next_fence\":%u,\"fence_pointer\":%u,"
            "\"fence_index\":%u,\"fence_mask\":%u,\"ring_used\":%u,"
            "\"fence_table\":%u}", i ? "," : "",
            m->device_ptr_va, m->device_va, m->device_valid, m->device_bytes,
            m->put_offset, m->get_ptr_offset, m->put_value, m->fence_va,
            m->fence_value, m->fence_valid, m->channel_va, m->channel_valid,
            m->channel_put, m->channel_get, m->pfifo_va, m->pfifo_valid,
            m->pfifo_subroutine, m->command_mode, m->command_dma, m->command_offset,
            m->command_pending, m->command_value, m->command_fence_va,
            m->command_queue_count, pb_capture_device_word(m, 0),
            pb_capture_device_word(m, 4), pb_capture_device_word(m, 8),
            pb_capture_device_word(m, 0x24), pb_capture_device_word(m, 0x28),
            pb_capture_device_word(m, 0x2C), pb_capture_device_word(m, 0x30),
            pb_capture_device_word(m, 0x34), pb_capture_device_word(m, 0x38),
            pb_capture_device_word(m, 0x40), pb_capture_device_word(m, 0x48));
    }
    PB_CAPTURE_JSON("]}");
#undef PB_CAPTURE_JSON
    return used;
}
```
