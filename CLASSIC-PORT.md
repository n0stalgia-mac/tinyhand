# TinyHand classic port

Classic Toolbox application. The minimum system is System 7.1 on both 68000 and PowerPC. It draws 1-bit QuickDraw, including on a Mac Plus at 512×342. The Swift macOS app stays in this tree. The classic app is `classic/`. Creator `tHnd`, type `APPL`.

## System 7.1

At launch, after `InitDialogs`, the app calls `Gestalt(gestaltSystemVersion)`. That selector returns a BCD long: System 7.0.1 is `0x0701`, System 7.1 is `0x0710`, System 7.1.2 is `0x0712`. A numeric compare matches that order. If Gestalt returns an error, or the value is below `0x0710`, the app shows alert 129 ("TinyHand requires System 7.1 or later.") and quits. It does not open the window. Gestalt itself dates to System 6.0.4, so a System 6 Mac reaches the alert instead of an unimplemented trap.

The Toolbox calls in `classic/src/tinyhand.c` are ones System 7.1 already has. The 68k build emits the classic A-line traps (Gestalt is `0xA1AD`, `GetMenuItemText` is `0xA946`, the original `GetItem`). `GetMBarHeight` is not a trap: on 68k it is the inline `MOVE.W $0BAA,D0` that reads the `MBarHeight` low-memory global, and on PowerPC it is the InterfaceLib 7.1 export of that same read. PowerPC `Gestalt` is the InterfaceLib 7.1 import.

Managers added after System 7.1 are not used. That includes the Drag Manager (System 7.5), the Thread Manager as an app API, Appearance Manager (Mac OS 8), Navigation Services (Mac OS 8.5), Contextual Menus, Control Strip, and Apple Guide. The hand is not drawn in a `GWorld`. A `NewPtr` bitmap holds the pixels under the hand so they can be put back.

`'SIZE' (-1)` is `is32BitCompatible` on both builds. The source does not mask addresses or poke 24-bit master pointers. System 7.1 on a 68020 machine may be in 32-bit mode, and Finder will not launch the PowerPC PEF unless the flag is set. The 68000 preferred partition stays 128K, minimum 64K.

Snow on a Mac IIx (System 7.1, 640×480) reaches the hand: `Gestalt` returns at least `0x0710`, so the alert is not shown. The framebuffer is `dist/tinyhand-system71.png`. On a Mac Plus running System 6.0.8 the same build stops on alert 129 (`dist/tinyhand-below-71.png`).

`./build.sh` configures `build-68k/` with the Retro68 toolchain and `build-ppc/` with the RetroPPC toolchain, then copies `dist/TinyHand-68k.bin`, `dist/TinyHand-68k.dsk`, `dist/TinyHand-ppc.bin`, and `dist/TinyHand-ppc.dsk`. The build directories are not part of the source. `-m68000 -msoft-float` are applied only when `CMAKE_SYSTEM_NAME` is `Retro68`.

There is no full-screen window and no white background. The hand, the stick, and the ink are drawn in the Window Manager port, in global coordinates, over the desktop and over other applications. Before each paint the pixels under that drawing are copied aside with `CopyBits` and put back when the drawing moves or the app suspends, so Finder and other windows stay visible. The drawing is clipped below the menu bar (`GetMBarHeight`). The hand is drawn on the first paint, fingertip at the centre of the screen below the menu bar, before the mouse moves. The outline and the robot put that hand on the top of the stick: the index finger is on the left, and the shaft hangs from the wrist to the right of the finger. At launch the drawing is scaled from the main screen bounds (`qd.screenBits.bounds`). The palm, from the fingertip to the wrist, is 13 percent of that screen's height, the same default as the macOS pointer size. The stick, the robot blocks, and the laser dot use that same scale, so a 512×342 screen and a larger screen show the pointer in the same proportion. After the mouse moves inside that area, the fingertip sits on the pointer and the arrow cursor is hidden. The arrow comes back over the menu bar.

| Upstream (macOS Swift) | This port |
| --- | --- |
| Menu-bar status item and ctrl-option-H global toggle. AppKit plus a global hotkey. | This port is the front application. The File and Hand menus replace that. There is no global hotkey and no CGEvent tap. |
| Clicks are swallowed so the app underneath is not clicked. | While TinyHand is in front, a click below the menu bar draws ink here. The desktop and other applications stay on screen. |
| 36 colour PNG hands, a laser dot, and custom hands made from user images (`HandMaker.swift`, `assets/hand_*.png`, about 2 MB each). | Four 1-bit QuickDraw styles, drawn with lines, ovals, rects, and patterns: Outline, Solid, Robot, Laser. The PNGs are not embedded. |
| Colour, shadow, and 3D toy rendering. | 1-bit QuickDraw, so a Mac Plus can show it. |
| Hold-drag ink and a command-key arrowhead. | Same gestures. Ink is a fixed ring of 256 segments of short coordinates. The oldest segment is dropped when the ring is full. Command on mouse-up of a drag draws a two-stroke arrowhead on the last segment. |
| ctrl-option-X clears. Esc clears and turns the pointer off. | The Clear menu item and Esc clear the ink. Esc on an empty canvas quits. Command-Q quits. Command-period quits. Command-C and Command-X clear. There is no separate overlay "off" mode. |
| Recorded tap sound and a mute control. | `SysBeep(30)` for a tap that barely moves, and a Sound menu that toggles the beep. |
| `tinyhand://` links, `~/Library/Application Support/TinyHand/state.json`, and the pid file. | Not implemented. |
| Sparkle updater, Accessibility permission, welcome window, settings window (`SettingsView.swift` is about 58 KB). | Not implemented. |
| Bottom toolbar of hands, Raycast, and Alfred. | The Hand menu replaces the toolbar. No URL automation. |

## PowerPC

`./build.sh` configures `build-ppc/` with `/home/grok/Retro68-build/toolchain/powerpc-apple-macos/cmake/retroppc.toolchain.cmake`. The same `classic/src/tinyhand.c` is compiled with `-std=gnu99 -Os`. `int` is 32-bit on that compiler. The source already uses `short` and `long` for coordinates and counts, and the PowerPC link did not need a trap substitution.

`TinyHand.r` is shared. Its `'SIZE' (-1)` overrides the one in `RetroPPCAPPL.r`, so the PowerPC target appends `classic/src/TinyHandPPC.r`.

| Item | 68000 | PowerPC |
| --- | --- | --- |
| `'SIZE' (-1)` | `is32BitCompatible`, preferred 128K, minimum 64K | `is32BitCompatible`, preferred 1024K, minimum 1024K. Finder will not launch a PEF that is not 32-bit clean, and the RetroPPC partition is 1 MB. |
