# PS4 Market — Build 4 (PKG build verified, not yet run on a console)

This is an original C++/SDL2 native UI prototype for a standalone PS4 homebrew application.

## Included
- `src/main.cpp`: dashboard, details and confirm screens, keyboard/controller navigation
- `src/platform.h`, `src/platform_ps4.cpp`: network/hash/install boundary (stub)
- `CMakeLists.txt`: initial cross-build project definition
- `build-pkg.sh`, `Makefile.ps4`: the PKG build (see below); `build-linux.sh` / CMake are an unused starting point
- `pkg/README.md`: package layout notes
- `assets/`: reserved for original icons/art

## Firmware 14.02 compatibility status
The application source is separate from the console's homebrew-enabling method. The published GoldHEN release page lists support through firmware 11.00, not 14.02. Therefore this source is **not certified or claimed to run on 14.02**. A compatible, lawful homebrew runtime for the user's exact environment must be confirmed before console testing.

## Build the PKG
Needs the OpenOrbis toolchain release **v0.5.4** (`toolchain-llvm-18.tar.gz`; SDL2 headers and `libSDL2.a` ship inside it) and clang/lld 18.

```
export OO_PS4_TOOLCHAIN=/path/to/OpenOrbis/PS4Toolchain
./build-pkg.sh            # -> build/IV0000-PSMK00001_00-PS4MARKET0000001.pkg
```
- `PkgTool.Core` is .NET Core 3.0 and only understands OpenSSL 1.x. On a distro with only OpenSSL 3, set `SSL11_LIB` to a folder holding `libssl.so.1.1` + `libcrypto.so.1.1` (the CI workflow does this).
- Or push to GitHub: `.github/workflows/build-pkg.yml` does the same on `ubuntu-24.04` and uploads the PKG as an artifact.
- The CMake files are only a starting point and are not used for the PKG.

## NOVA GitHub catalog integration

- `tools/sync_nova_catalog.py` fetches the configured GitHub `GAMES.json` over HTTPS whenever `make -f Makefile.ps4 pkg` is run.
- The script converts the upstream `DATA` object into a metadata-only `catalog.json`, discarding the source object's URL keys. It retains game name, CUSA/title ID, region, version, release date, file size, minimum firmware and cover URL.
- The PKG build embeds that metadata at `sce_sys/catalog.json`. At launch, the app first checks `/data/ps4market/catalog.json`, then falls back to `/app0/sce_sys/catalog.json`.
- This is a **build-time refresh**, not an in-app live refresh: rebuilding the PKG fetches the latest catalog. The app does not yet make HTTPS requests while running.
- The upstream source contains package URLs but no SHA-256 values. Those URLs are deliberately not imported, so the GAMES.json entries display as catalog metadata and cannot be downloaded from this app. The existing download path remains available for separate catalogs containing packages you are authorized to distribute and their trusted SHA-256 hashes.
- The existing tile artwork remains generated placeholders; `cover_url` is preserved in the catalog metadata but remote cover-image rendering is not implemented.

## What build 4 changes
- `Makefile.ps4` rewritten to match the toolchain's own `samples/SDL2/Makefile`; it now really builds: eboot, `param.sfo` (via `PkgTool sfo_*`), `gp4` and the `.pkg`.
  - `-std=gnu++17` (strict `c++17` hides `nanosleep()` in the toolchain's libc headers), `-DPS4_NATIVE`, and the libs the SDL2 sample uses plus `-lSceNet`.
  - `sce_module/libc.prx`, `libSceFios2.prx` and `sce_sys/about/right.sprx` are copied from the toolchain's SDL2 sample, as that sample does.
- PS4 code path in `main.cpp` (`#ifdef PS4_NATIVE`, desktop path unchanged), copied from how the working SDL2 sample does it: 1920x1080 window surface + software renderer (UI scaled from 1280x720), `SDL_UpdateWindowSurface` per frame, and the raw joystick API, because the PS4 SDL port has no game-controller mapping (the old `SDL_IsGameController` check would have left the pad unopened). Buttons: Cross=0, Circle=1, D-pad=13..16.
- CI workflow pinned to toolchain v0.5.4 (SHA-256 checked) and fetches libssl 1.1 for `PkgTool.Core`.

- NOVA-style UI laid out like the mockup (placeholder content only, no retail titles or artwork): icon tab bar GAMES / APPS / TOOLS / FAVORITES / DOWNLOADS / LIBRARY with clock, a wide hero card with original synthwave-city artwork (varies per item), wave background, carousel dots, TRENDING / NEW RELEASES rows of 6 tiles (just your catalog order, first 6 then next 6), and Store Info / Quick Links panels, all driven by your own `catalog.json`. Download progress shows in a bar along the bottom. Quick Links: My Favorites and My Downloads jump to those tabs; Updates and Settings are visual only.
  - **L1 / R1 switch tabs.** Square toggles a favorite (kept in memory only; not saved between launches). Items are sorted into tabs by their catalog `type`: GAME, APP, everything else under TOOLS. Finished downloads show up in LIBRARY for the session.
  - **Circle on the home screen no longer quits the app** (that was the "crash"). Circle still goes back from details/dialogs.
  - Desktop keys: Q / E = L1 / R1, F = favorite. `tools/render-test.sh` now replays a tab-switching script.

## Verified vs not verified
- Verified: all sources compile and link against the real PS4 SDK/SDL2; the PKG builds from a clean checkout; its entries list correctly and the extracted `eboot.bin` is byte-identical to the one that went in; the desktop test scripts still pass.
- **Not verified: running on a console.** Nothing here has been executed on a PS4. Expect to debug on the device (video init, pad input, `/data` write access, network).

## What build 3 changes
- Fixed: the bitmap font lookup was mismatched (table is 0-9 then A-Z), so all text rendered as garbage.
- Fixed: the period glyph branch was unreachable (dangling else); punctuation is now a proper table.
- Fixed: controllers were never opened, so pad input never arrived. D-pad, Cross (open/confirm), Circle (back) now work.
- New: Up/Down navigation, details screen, confirm dialog, result dialog, translucent overlay.
- New: the catalog loads from `/data/ps4market/catalog.json`, then the bundled `/app0/sce_sys/catalog.json`, then the built-in sample. The parser supports both the app's `items` format and the GitHub `DATA` object. GAMES.json package URL keys are discarded; metadata-only entries cannot be downloaded. 6 tiles per page, with paging.
- New: downloader (`src/http_download.*`) and streaming SHA-256 (`src/sha256.*`). Runs one step per frame, no threads, shows live progress. The file is only given its final name if the hash matches; otherwise it is deleted.
- New: `Makefile.ps4`, `build-pkg.sh`, `pkg/sce_sys/icon0.png` for the OpenOrbis path (untested, see pkg/README.md).

## Testing
- `tools/test-downloader.sh`: SHA-256 against Python hashlib on 13 sizes, plus the downloader against a local server (good file, wrong hash, 404, redirect, truncated body, chunked reply, refused connection, https, missing hash, unwritable disk). All pass on desktop Linux.
- `tools/test-catalog.sh`: catalog parser (valid, malformed and hostile inputs), `make_catalog.py`, `serve_catalog.py`, and a download driven by a generated catalog. All pass.
- `tools/render-test.sh`: renders the screens with a stand-in SDL (needs only g++).

## Running a marketplace (host side)
1. Put `.pkg` files you are authorized to distribute in a folder (optional `<name>.json` beside each for name, type, version, publisher, description).
2. `python3 tools/make_catalog.py FOLDER --base-url http://<your-computer-LAN-IP>:8080 --out FOLDER/catalog.json` (computes each SHA-256 and size).
3. `python3 tools/serve_catalog.py FOLDER 8080` on a network you trust.
4. Copy `catalog.json` to `/data/ps4market/` on the console by whatever method your homebrew environment provides.

## Limits you should know about
- http:// only. There is no TLS in this build, so the catalog and hash must come from a source you trust; a hash served next to the file over plain HTTP does not protect against tampering.
- No redirects, no chunked encoding, no resume, one download at a time.
- There is no installer step. After a verified download the app tells you where the .pkg was saved; installing it needs the console's package-install API, which is not implemented here.
- Socket calls use POSIX APIs (see the `net` section of `http_download.cpp`). If your PS4 SDK needs sceNet*, swap only that section.
- Logic was tested on desktop Linux; the PS4 build compiles and packages against the real toolchain, but has never been run on a console.

Only distribute or install packages you are authorized to use. A real store should verify publisher-provided hashes and require user confirmation before any install.
