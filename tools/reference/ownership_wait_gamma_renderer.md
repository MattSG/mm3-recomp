# Host wait, gamma and renderer interface ownership

Full source review at fork 8ffa9feef6ba84daaf12dde5b9e6ac6ba3a26522. All three file byte hashes match
the committed inventory; no source code changed in this checkpoint.

## nv2a_notify_wait.h

The helper waits for caller-supplied guest GET/PUT/notification/fence/stop
values. A host generation counter and WaitOnAddress wakeups reduce busy waiting;
completion still follows the guest predicate. There is no fixed title address,
callback layout or captured completion sequence. Existing notify/fence CTests
pass 2/2 on Windows and 2/2 on Linux after rebuilding their current targets.
This does not prove all guest memory ordering, older-host yield fallback or
live workload pacing behavior.

## d3d8_gamma.c

The caller supplies the ramp. Generic GPU shaders apply it to presentation,
with a source snapshot and restoration path preserving guest render-target
contents. There is no title-specific ramp, brightness preset or image payload.
The rebuilt WARP fixture reports zero failures across nine captured and one
real hidden DXGI presentation, including state/target preservation behavior.
This does not prove concurrent ramp mutation, arbitrary target dimensions/
formats or SDL GPU parity; lifecycle is single-device and serialized.

## nv2a_d3d11.h

The complete header exposes guest render state and generic callback seams for
placement, upload sanitization, depth/filter policy, idle presentation and
vertex alignment. Native draw resources have explicitly borrowed callback
lifetime. These declarations enable consuming-project policy rather than
embedding title payloads. The implementation bodies were not newly certified
by reading the interface; no fresh all-backend callback or ABI claim is made.

No new title gameplay, performance or full ownership completion is claimed.
