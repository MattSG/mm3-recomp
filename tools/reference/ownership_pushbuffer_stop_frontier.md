# Pushbuffer stop recovery ownership frontier

Status: policy migration implemented after fork 03cf828. Historical finding
and source excerpts below are retained as evidence.

The fork now exposes `nv2a_pb_set_stop_handler` in `nv2a_pb.h`. Its default
is NULL and executes no inferred semaphore release. The synchronous handler
runs only in execution mode on malformed work, before existing packet/call
cleanup and resume-at-PUT. Work-cap yields and partial packets use their
existing continuation path. Configuration is startup/quiesced; NULL removes
the callback. Positions are not a validated readable span.

MM3 registers `mm3_pb_recover_stop` from `src/mm3_pb_recovery.c` at startup.
It owns the inherited odd/128 heuristic and validates its own contiguous
span before reading it. This preserves the compatibility approximation;
it does not fix malformed submissions or prove hardware fence behavior.
Default no-release/caller isolation and separate MM3 policy tests replace
the shared test's implicit requirement for the heuristic.

`src/kernel/nv2a_pb_scan.c::pb_walk` routes invalid JUMP/CALL targets and
out-of-window reads through `stop`. That path discards packet/call state,
invokes `pb_stop_salvage`, and acknowledges the remaining span by setting PC
to PUT. The helper scans unparsed dwords for 0x00041D70 and accepts increasing
odd fence values within 128 of the previous value. It dispatches the selected
release through the executor even though those dwords were not decoded as a
command stream. Valid-looking payload data can therefore satisfy this rule.

The corresponding shared hitch test explicitly requires this heuristic.
A passing test protects the current compatibility policy; it does not prove
hardware semantics. Bounds checks and scheduler continuation are separate
mechanisms and should remain generic.

Original migration requirements: expose a generic stopped-submission notification/recovery
boundary, with the caller receiving the stopped position, submitted PUT and
reason. Shared defaults must not infer semaphore commands from undecoded
payload. MM3 must own any compatibility recovery it still needs, including
its odd/128 rule. Verify default no-release behavior and caller opt-in, preserve
valid/split/CALL parsing, then build both renderers and inspect bounded driving
before publishing a runtime behavior change. A direct helper deletion would
silently change MM3 fence progress and is not the migration.

## Current shared heuristic

```c
/* The words from a stop to PUT are never executed, and they may hold the
 * semaphore release the title's next fence wait spins on. Run the last such
 * release in [from, to): releasing it marks the earlier ones done too, and
 * none beyond PUT.
 *
 * The XDK writes a fence as (BACK_END_WRITE_SEMAPHORE_RELEASE, count 1,
 * subchannel 0) and its value, but that pair can also be data (index buffer
 * words). A value taken from data could push the fence far ahead, so a
 * candidate counts only if it looks like the next fence: odd, as the XDK's
 * fence values are, after the last release, and at most 128 past it. Returns
 * how the fence was handled, for the log. A span that wraps or can't be read
 * is left to the fence mirror. */
static const char *pb_stop_salvage(const uint8_t *mem, uint32_t from, uint32_t to,
                                   uint32_t *value)
{
    uint32_t va, last, rel;
    int found = 0, seen = 0;

    if (!s_exec_enabled)
        return "not executing";
    if (from >= to || !pb_in_window(from) || !pb_in_window(to - 4))
        return "span wraps or is unreadable; fence left to the mirror";
    last = nv2a_pb_exec_semaphore_last_value();
    for (va = from; va + 4 < to; va += 4) {
        if (*(const uint32_t *)(mem + va) == 0x00041D70u) {
            uint32_t v = *(const uint32_t *)(mem + va + 4);
            seen++;
            if ((v & 1u) && (int32_t)(v - last) > 0 && v - last <= 128u) {
                last = v;
                found = 1;
            }
        }
    }
    if (!found)
        return seen ? "release(s) in the rest but none plausible"
                    : "no fence release in the rest";
    rel = nv2a_pb_exec_semaphore_releases();
    nv2a_pb_exec_method(0, 0x1D70u, last);
    *value = last;
    if (nv2a_pb_exec_semaphore_releases() == rel)
        return "fence release found but refused by the executor";
    s_tot_salvaged++;
    return "fence release salvaged";
}

```

## Existing test expectation

```c
        /* After a stop, the next fence release between it and PUT still
         * runs (odd, after the last, within 128 of it), so the title's
         * fence wait completes. A release that doesn't look like the next
         * fence is left alone. */
        sema_last = 5;
        root[0] = (XBOX_CONTIG_SIZE + 0) | 1;
        root[1] = 0x00041D70; root[2] = 7;
        nv2a_pb_scan(XBOX_CONTIG_BASE + 0x900, XBOX_CONTIG_BASE + 0x90C);
        CHECK(command_count == 1 && commands[0].method == 0x1D70 && commands[0].param == 7);
        CHECK(s_tot_salvaged == 1);
        command_count = 0;
        root[2] = 0x9000;                  /* even and far ahead: data */
        nv2a_pb_scan(XBOX_CONTIG_BASE + 0x900, XBOX_CONTIG_BASE + 0x90C);
        CHECK(command_count == 0 && s_tot_salvaged == 1);
    }

```
