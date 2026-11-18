<h1 align="center">
  <img src="extras/livearea/pic0.png" alt="The Sims 3 Ambitions PS Vita Banner" width="700"><br>
  The Sims 3 Ambitions · PS Vita Port
</h1>

<p align="center">
  <a href="#-status--playability">Status & Playability</a> •
  <a href="#setup-instructions-for-players">How to install</a> •
  <a href="#controls">Controls</a> •
  <a href="#known-issues">Known Issues</a> •
  <a href="#build-instructions-for-developers">How to compile</a> •
  <a href="#credits--acknowledgments">Credits</a>
</p>

An unofficial, native **iOS-to-PS Vita wrapper port** of *The Sims 3 Ambitions* (original game by **Electronic Arts**). The 32-bit ARMv7 Mach-O executable from the iOS app is loaded by a custom Mach-O loader with a fixed slide (`0x98000000`). The Apple frameworks it imports are replaced by native Vita code: an Objective-C mini-runtime, UIKit, Foundation, CoreGraphics and CoreText emulation, rendering through vitaGL (`SOFTFP_ABI=1`), text through FreeType, and audio through OpenAL. The runtime is shared with the sibling port [The Sims 3 (iOS) for PS Vita](https://github.com/MetalSyntax/The-Sims-3-iOS-vita).

---

## 🎮 Status & Playability

**Status: WIP (version 01.00)**

The game boots to the main menu on a physical PS Vita, and menu text and touch input work. In-game play has not been verified yet, and menu performance is still below target.

- [x] **Loader & boot**: custom Mach-O loader with fixed slide `0x98000000`. The game reaches the main menu without crashing. *Verified on console.*
- [x] **Graphics**: fullscreen 16:9. The game renders at its iPhone 5 logical size (568x320). *Verified on console.*
- [x] **Text**: all menu text renders through FreeType, using `font.ttf` (NanumGothic) with `font_latin.ttf` as fallback. *Verified on console.*
- [x] **Touch**: touchscreen input works. *Verified on console.*
- [ ] **Performance**: the menu runs below the 30 FPS target. A glyph cache was just added and is not tested yet.
- [ ] **Audio**: the OpenAL backend is in place. Not yet verified on console.
- [ ] **In-game play**: not verified yet.

---

## Disclaimer

*The Sims 3 Ambitions* is copyright **Electronic Arts**. All rights reserved. The work presented in this repository is unofficial and is not produced, endorsed, or sanctioned by Electronic Arts or any affiliated entities.

This software does NOT contain original game code, binaries, copyrighted graphics, or game assets. The authors do not promote or condone piracy in any way. To play, you must provide your own legally obtained copy of the game, as a decrypted iOS IPA (`cryptid 0`).

---

## Setup Instructions (For Players)

1. **Custom firmware & plugins**: PS Vita on custom firmware with taiHEN and kubridge. Firmware 3.60 or 3.65 (Enso) is recommended. Add kubridge in `ur0:tai/config.txt`:
   ```ini
   *KERNEL
   ur0:tai/kubridge.skprx
   ```
2. **Shader compiler**: `libshacccg.suprx` must be in `ur0:data/` (extract it with **ShaRKBR33D**).
3. **Game files**: your own decrypted iOS IPA of *The Sims 3 Ambitions* (`cryptid 0`). The development build used the IPA named `The_Sims_3_Ambitions_v1.1.84_…`. Only this version has been tested. Extract the `Payload/<App>.app` bundle and stage it with `tools/stage_data.sh`.
4. **Copy to the Vita** (`ux0:data/sims3ambitions/`), for example with `tools/deploy_data.py` over FTP (VitaShell):
   ```
   ux0:data/sims3ambitions/
     game_app/        the extracted .app bundle (executable + resources)
     rebase.bin       rebase table generated from the Mach-O
     binds.bin        import bindings
     nib.txt          precompiled MainWindow.nib, if the game has one
     font.ttf         NanumGothic, used for menu text
     font_latin.ttf   Latin fallback font
     controls.txt     optional button-to-touch mapping (see Controls)
     Documents/       created automatically: your save games
     logs/            created automatically: one log per run
   ```
5. **Install VPK**: install `sims3ambitions.vpk` with VitaShell.

---

## Controls

Touch is the primary input. Physical buttons are optional: they are mapped to screen taps in `ux0:data/sims3ambitions/controls.txt`, which can be edited without rebuilding (format in [`tools/controls.example.txt`](tools/controls.example.txt)).

| Input | Action | Status |
| :---: | :--- | :---: |
| **Touchscreen** | Touch input, as on iOS | ✅ Verified on console |
| **CROSS, CIRCLE, SQUARE, TRIANGLE, L/R, START, SELECT, D-pad** | Tap at the screen position set in `controls.txt` (up to 6 bindings) | Not verified |
| **Left stick** | Tilt (accelerometer) by default. A `STICK x y r` line in `controls.txt` turns it into a virtual drag pad | Not verified |
| **SELECT + START** (hold 2 s) | Rotates the image; saved to `rotation.txt` | Not verified |

> [!TIP]
> Button coordinates are in the game's logical view space, with the origin at the top-left.

---

## Known Issues

- **Performance**: the menu does not reach the 30 FPS target yet. A glyph cache was added and is untested.
- **In-game play**: not verified on hardware yet.
- **Unverified input and audio**: the button mapping, tilt control, display rotation and audio have not been checked on the console (see the checklist above).
- **Crash dumps** (`.psp2dmp`) are written by the system to `ux0:data/`, not to the game folder. `tools/deploy_data.py --logs` downloads them together with the logs.

---

## Build Instructions (For Developers)

### Prerequisites

- [VitaSDK](https://vitasdk.org/) with `$VITASDK` set. The libraries linked by the build are listed in [`CMakeLists.txt`](CMakeLists.txt).
- CMake 3.14+, Python 3 (for the tools).
- vitaGL is vendored in `vendor/vitaGL` and built by CMake with `SOFTFP_ABI=1`. No separate install is needed.
- The original IPA and any extracted game files stay local. They are git-ignored (`*.ipa`, `*.app/`, `game_app`, `ux0_data/`, `rebase.bin`).

### Compilation Commands

```bash
git clone https://github.com/MetalSyntax/The-Sims-3-Ambitions-iOS-vita.git
cd The-Sims-3-Ambitions-iOS-vita
export VITASDK=$HOME/vitasdk
tools/stage_data.sh /path/to/Payload/<App>.app   # stages game data locally; never committed
cmake -B build
cmake --build build
```

The build produces `sims3ambitions.vpk` (Title ID `PSVSIMS3A`, version `01.00`) at `build/sims3ambitions.vpk`.

### Deploy & Debug

```bash
python3 -I tools/deploy_data.py --ip 192.168.x.y           # upload ux0_data/sims3ambitions/ over FTP (VitaShell)
python3 -I tools/deploy_data.py --ip 192.168.x.y --logs    # also download new logs and crash dumps
```

The IP and slug can also be stored in `.psvita-toolkit.json` (git-ignored). The loader lives in [`source/macho.c`](source/macho.c), and the libc/pthread compatibility layer in [`source/darwin.c`](source/darwin.c). Architecture and progress notes: [`PORTING_PLAN.md`](PORTING_PLAN.md) and [`port_progress.md`](port_progress.md).

---

## Credits & Acknowledgments

- ***The Sims 3 Ambitions*** is intellectual property of **Electronic Arts**. All rights reserved.
- **vitaGL** graphics library by [Rinnegatamante](https://github.com/Rinnegatamante/vitaGL), vendored in `vendor/vitaGL`. Its license texts are in `vendor/vitaGL/COPYING` (GPL-3.0) and `vendor/vitaGL/COPYING.LESSER` (LGPL-3.0).
- **soloader-boilerplate** by [Rinnegatamante](https://github.com/Rinnegatamante/soloader-boilerplate), the inspiration for the loader approach.
- **FreeType** font rendering library, [freetype.org](https://freetype.org/). See its own license.
- **NanumGothic** font (`font.ttf`). by Sandoll Communications / NAVER, SIL Open Font License 1.1.
- **kubridge** plugin by [Bythos](https://github.com/bythos14/kubridge).
- **VitaSDK** team, [vitasdk.org](https://vitasdk.org/).
- Sibling port [The Sims 3 (iOS) for PS Vita](https://github.com/MetalSyntax/The-Sims-3-iOS-vita), which shares the runtime.
- Special thanks to the PS Vita homebrew and porting community.
