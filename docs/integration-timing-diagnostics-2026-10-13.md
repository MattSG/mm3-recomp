# Superseded integration timing diagnostics — October 13, 2026

These observations predate the final actual-loop scheduler correction. They are
retained to explain the call-routing/clock-lifecycle failures and their regression
coverage; they are not current timing or handling acceptance. Current verified
results are in the [integration record](integration-2026-10-13.md).

### Preliminary actual-game smoke and measured frame rate

The initial combined MSVC build passed. A Paris Courier smoke run configured for
60 FPS reached the mission with a normal screenshot; the harness reported four
AI cars moving forward without backward travel and the player moving 293.7 world
units forward/1.6 backward. Its sample-JSON export failed, so that trajectory summary
has not been independently reconstructed from JSON.

An independent CSV analysis used the last 20 seconds of each initial run:

| Requested FPS | Frame samples | Median frame time | FPS from median | FPS over elapsed window |
|---|---:|---:|---:|---:|
| 60 | 1,169 | 16.6790 ms | 59.96 | 58.41 |
| 90 | 1,179 | 16.5638 ms | 60.37 | 58.93 |

The 90-request stderr recorded `profile=Balanced ... fps=90` and
`[MOD] framerate on`. During its measured driving window no frame was shorter
than 12 ms, and all pacing waits were below 1 ms. In contrast, its earlier
10–20-second frontend window had a median 11.1113 ms frame time (90.00 FPS).
The requested limit therefore worked, while the measured driving window was
limited by throughput. This run does not validate driving at an observed 90 FPS.

The 90-request JSON contained one AI car and the player, with 78 samples each.
Independent displacement projected onto each car's preceding heading measured
609.66 world units forward/0 backward for the AI and 508.32 forward/0 backward for
the player, over roughly 19.84 seconds. All 77 sampled AI intervals moved
forward. The AI's -89.27 world-unit displacement along its *initial* heading reflects
turning during the route; it is not evidence of backwards movement. The initial
harness incorrectly used that net displacement to label the AI as reversing;
its verdict logic now uses travel along the car's current heading. The cadence
report now selects the last elapsed seconds using actual `qpc_ms`, rather than
assuming 60 frames per second; it rejects malformed/nonfinite timestamps and
nonfinite/nonpositive frame durations. Five extracted-helper checks passed,
including both real CSVs, a 30 FPS elapsed window, invalid durations and
French-culture parsing/even median. These initial smoke results predate the
latest production call-routing
and launcher fixes and must not be treated as final acceptance of those changes.

### Superseded post-routing 60/90-request diagnostics

After the production call-routing correction and fresh build,
`integration-fixed-60-20261013` passed the direction/acceleration harness. It still
had the subsequently discovered clock-lifecycle error. Independent analysis
parsed both exported JSON files and reconstructed all five cars' travel. Each
car had 78 samples over approximately 19.84 seconds. The four AI cars moved
969.97, 994.05, 988.48 and 1,001.37 world units forward, with no backwards sampled
intervals. The player moved 725.38 forward/0.48 backward; its measured one-second
peak was 66.41 world units per second. Game coordinates are reported in world
units, without assuming a metre conversion.

The last 20 seconds of frame CSV contained 1,139 samples over 19.993565 seconds:
median 16.7127 ms (59.83 FPS), elapsed average 56.92 FPS and 95th-percentile frame
time 21.1211 ms. Stderr confirmed `fps=60` and the framerate mod enabled, without
crash/fatal/exception/assertion messages. Both telemetry JSON files parsed
completely, and the temporary isolated save directory was removed by the
harness's cleanup. These verify the observed direction, acceleration, telemetry
and rendered cadence, without establishing the world rate or handling parity.

`integration-fixed-90-20261013` likewise passed direction/acceleration checks,
with the same clock-lifecycle error. One AI and the player each had 78 samples:
842.72 world units forward/0 backward for the AI, and 1,026.98 forward/0 backward
for the player; the player's measured one-second peak was 100.19 world units/s.
Its last 20 seconds contained 1,214 frames over 19.997418 seconds, median
15.9841 ms (62.56 FPS), elapsed average 60.66 FPS and 95th percentile 21.1139 ms.
All pacing waits were below 1 ms. Stderr confirmed requested `fps=90` and the
framerate mod enabled. Both JSON files parsed, no crash-related messages were
found, and its isolated save directory was removed.

The timed menu harness reached different missions: four AI/11 deliveries for
the 60-request run and one AI/five deliveries for the 90-request run. Their
travel and peak speeds cannot support a matched comparison. Both runs are
superseded by the final real-loop scheduler correction; they
do not establish final world-rate, speed or handling acceptance, or actual
90 FPS driving.
