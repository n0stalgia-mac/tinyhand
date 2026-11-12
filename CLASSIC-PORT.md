# TinyHand classic port

Classic Toolbox application for a 68000 Mac Plus running System 6.0.8 (512×342, 1-bit). The Swift macOS app stays in this tree. The classic app is `classic/`. Creator `tHnd`, type `APPL`.

`./build.sh` configures `build-68k/` with the Retro68 toolchain and copies `dist/TinyHand-68k.bin` and `dist/TinyHand-68k.dsk`. `build-68k/` is not part of the source. `-m68000 -msoft-float` are applied only when `CMAKE_SYSTEM_NAME` is `Retro68`.

The window is `screenBits.bounds` with the top inset by the menu bar (`GetMBarHeight`). The hand is drawn on the first paint, fingertip at the window centre, before the mouse moves. After the mouse moves inside the content, the fingertip sits on the pointer and the arrow cursor is hidden. The arrow comes back over the menu bar.

| Upstream (macOS Swift) | This port |
| --- | --- |
| Menu-bar status item and ctrl-option-H global toggle. AppKit plus a global hotkey. | This port is the front application. The File and Hand menus replace that. There is no global hotkey and no CGEvent tap. |
| Clicks are swallowed so the app underneath is not clicked. | This port's window eats the clicks. |
| 36 colour PNG hands, a laser dot, and custom hands made from user images (`HandMaker.swift`, `assets/hand_*.png`, about 2 MB each). | Four 1-bit QuickDraw styles, drawn with lines, ovals, rects, and patterns: Outline, Solid, Robot, Laser. The PNGs are not embedded. |
| Colour, shadow, and 3D toy rendering. | 1-bit QuickDraw, so a Mac Plus can show it. |
| Hold-drag ink and a command-key arrowhead. | Same gestures. Ink is a fixed ring of 256 segments of short coordinates. The oldest segment is dropped when the ring is full. Command on mouse-up of a drag draws a two-stroke arrowhead on the last segment. |
| ctrl-option-X clears. Esc clears and turns the pointer off. | The Clear menu item and Esc clear the ink. Esc on an empty canvas quits. Command-Q quits. Command-period quits. Command-C and Command-X clear. There is no separate overlay "off" mode. |
| Recorded tap sound and a mute control. | `SysBeep(30)` for a tap that barely moves, and a Sound menu that toggles the beep. |
| `tinyhand://` links, `~/Library/Application Support/TinyHand/state.json`, and the pid file. | Not implemented. |
| Sparkle updater, Accessibility permission, welcome window, settings window (`SettingsView.swift` is about 58 KB). | Not implemented. |
| Bottom toolbar of hands, Raycast, and Alfred. | The Hand menu replaces the toolbar. No URL automation. |

## PowerPC

Not built in this run. The same `classic/CMakeLists.txt` omits the 68000 flags when `CMAKE_SYSTEM_NAME` is not `Retro68`, so a later RetroPPC configure can reuse it. Toolchain, when that task runs: `/home/grok/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake`.
