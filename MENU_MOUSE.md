# Native menu mouse

Enable Menu mouse and Menu keys under Controls. Direct launches also need
`RECOMP_KEYBOARD=1` and `RECOMP_MENU_KEYS=1`.

Movement focuses the native control under the pointer at the next menu update.
Button hit areas use the game's actual pill geometry rather than the wider
controller-navigation row, so blank space beside a control stays inactive.
A click focuses that item, then activates after native layout has reconciled
the changed focus (one additional menu update). Disabled controls,
empty space and letterbox bars do not activate the previously selected item.
Keyboard/controller focus stays put within an unchanged layout until the mouse moves. A new native layout refreshes picking under a stationary pointer.

The cursor is a custom orange and gold arrow matching MM3's menus.
Windows composites it independently of guest rendering; it hides while driving.
The cursor and menu input do not use ImGui.

Car cards use geometry captured inside the native car widget. Clicking a side
card selects it through the original directional actions; clicking the selected
card confirms. The mod never writes the vehicle binding or selection index.

Blitz track tiles use geometry from the native race-list painter. Clicking a
different unlocked tile selects it through the original directional actions;
clicking the selected tile confirms. Locked tiles and gaps do not activate the
previously selected race. Native paint supplies the current binding value, so
the mod waits for each original action before issuing the next one.

The hooks collect native tab-stop bounds during `001C5935`, then apply focus
before `00246894` dispatches queued input. Guest registers, x87/XMM state and
stack are preserved around calls. `MM3_MENU_NATIVE_TRACE=1` enables bounded
layout diagnostics and focus/event timestamps.

Focused checks (Visual Studio CMake, Windows):

```powershell
cmake -S tests/menu_mouse -B build-menu-mouse -DRECOMP_GEN_DIR=<generated-dir>
cmake --build build-menu-mouse --config Release
ctest --test-dir build-menu-mouse -C Release --output-on-failure
cmake -S tests/input_contract -B build-input-contract
cmake --build build-input-contract --config Release
ctest --test-dir build-input-contract -C Release --output-on-failure
```

These checks exercise native focus/click contracts and the actual window/input
handlers. They do not replace checking real main/options/pause/car menus.

Validated on Windows, 2026-10-15: RelWithDebInfo build and all seven contracts
passed. An isolated game run with private saves exercised profile selection,
main menu, options, city selection, car selection (one and two places), selected
car confirmation, transmission, loading, gameplay, pause Options and mouse
resume. Input was posted only to that process's framebuffer window.
Follow-up checks verified the themed cursor bitmap, native pill bounds, exact
Blitz tile selection (Arc de Triomphe and Wild French West), and rejection of
locked tiles and blank space horizontally beside a menu button.

Native focus/event traces showed 0–32 ms from posted input in the sampled
frontend operations; this measures guest dispatch, not physical mouse latency
or the duration of the game's page-transition animations. The native Windows
cursor is composited separately and is not present in renderer BMP captures.

Visible A Select, B Back, X and Y help prompts are clickable within their native painted pill bounds. They enqueue the corresponding original button action without moving menu focus.

Footer validation: native A Select opens Work Undercover and B Back returns to the main menu. A/B/X/Y dispatch contracts pass; X/Y use the game IDs 21/22.

A/B/X/Y help prompts keep their original gold skin and lighten only their text to warm ivory on hover; the native orange cursor becomes a pointing hand over them. Track path icons turn pale gold and car carousel arrows turn orange on hover; locked tiles do not react. Native geometry, click bounds and labels stay stable. No extra outlines or overlay draws are added.


## Control audit (2026-10-15)

| Control | Result |
| --- | --- |
| Main/Single Race/Options buttons | Native silver focus responds to hover; clicks activate the chosen row. |
| Paris/Washington city cards | Native bright/dim focus and click work. |
| Blitz track tiles | Pale-gold icon hover; clicking selects Arc de Triomphe; locked fourth tile has no cue or activation. |
| Car thumbnails | Thumbnails retain their original colours; side-card click selects Renault 1985 R5 without confirming. Left/right arrows turn orange on hover. |
| A Select/B Back/X Edit License Plate | Original button dispatch works; hover uses an orange pointing-hand cursor. Y uses the same tested dispatch path. |
| Game Setup toggles/Sound Setup values | Direct native left/right arrow clicks; volume 90 to 95 and back verified, with the native 100 limit preserved. |
| License-plate keyboard cells | Individual key hit areas set the native caret; original activation handles letters, Shift/Caps Lock, Backspace, Space, caret arrows and Done. Modal input blocks the underlying car menu. |
| Car paint swatches/large carousel arrows | Direct swatch clicks select through native up/down actions, including multi-step changes; locked swatches are rejected. Native car and colour arrows dispatch their original directions and turn orange on hover. Unselected, unlocked swatches gain an orange native ring on hover; the selected swatch keeps its white ring. |
| Stationary pointer after page changes | Native layout changes refresh picking without requiring mouse motion; unchanged layout preserves controller focus. |

Follow-up on 2026-10-16: individual keyboard letter and Backspace clicks,
keyboard caret movement, Space and Done, R5 yellow to orange to red swatch
selection, car/colour arrows, locked Beetle swatches and Sound Setup arrow adjustments
were checked in isolated runs with private saves. The seven contracts cover
modal input, disabled/stale keys, native arrow routing and locked swatches.
Swatches use elliptical hit areas within their native paint bounds. The mod
never writes a colour binding; it waits for each native action to reach the
requested swatch. Option arrows are associated with their native context and
row because the style node differs from its parent tab-stop node.
