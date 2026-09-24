# Shared native build: repository findings and migration plan

2026-09-24. Baseline: fork branch `native-macos`, commit `efea4a237`.
This is the implementation plan following [primary-source research](cross-platform-build-research.md).
The shared CMake build has compiled on macOS arm64 and Linux arm64; see the
current evidence below for validation limits. Windows/MSVC remains unvalidated.
Per user preference, no GitHub Actions workflow is included: builds and checks
are invoked locally.
The user authorized a FreeType 2 API migration; CMake uses that implementation,
while the old build entry points retain their FreeType 1 path during migration.

New Game font loading is covered by a real-data regression run:

```sh
python3 tools/cmake/smoke.py --build-dir build/local-sanitize --data-dir . \
  --runtime-dir .native/play/ui-regression
```

Use a dedicated runtime: the test sets a 1024x768 window and 100% scale. It
constructs the actual New Game screen, renders frames and shuts down normally.
This covers screen construction, not an entire gameplay session. A desktop
screenshot also verified the leader-name field rendering. Desktop mouse input
currently reaches SDL with inconsistent coordinates, so it is not yet a reliable
automated gameplay driver.

## Decision

Use one target-based CMake description for native SDL2 builds on Windows x64/MSVC,
Linux x86_64/GCC, and macOS arm64/Apple Clang. Treat these as the initial acceptance
matrix, not a decision to remove existing platforms. Keep the existing Autotools
and Visual Studio entry points while proving parity. Preserve the current playable
Mac build as the comparison build.

CMake describes what to build; the compiler still produces machine code. Ninja,
Make or the IDE executes the generated build graph. CTest runs regression tests.
A dependency provider supplies libraries. These are separate responsibilities.

CMake is the best fit here because it can serve existing Visual Studio users and
Unix native builds with a common target graph, and SDL2 documents its integration.
Meson is also technically viable and supports IDE integration and dependency
fallbacks; choosing it would not remove this project's generator, legacy-library
or runtime-layout problems. This preference is an engineering judgment, not a
claim that Meson lacks multiplatform support. Keeping separate Autotools/VC files
would preserve today's duplicated ownership. A CMake wrapper around `make` would
not resolve that duplication.

Sources: [CMake targets](https://cmake.org/cmake/help/v3.30/manual/cmake-buildsystem.7.html),
[SDL2 integration](https://wiki.libsdl.org/SDL2/README-cmake),
[Meson IDE integration](https://mesonbuild.com/IDE-integration.html),
[Meson dependency fallback](https://mesonbuild.com/Subprojects.html).

## Findings in this checkout

| Evidence | Consequence for the new build |
| --- | --- |
| Root `configure.ac`, recursive `Makefile.am` files and independent `ctp2_code/ctp/civctp.vcxproj` | Inventory both source lists and platform definitions; neither alone represents every platform. Windows project contains 747 compile entries, including generated/platform-specific sources. |
| `ctp2_code/gs/newdb/Makefile.am:145–176` runs `ctpdb` for different record stamps from the same input schema | Give each schema exactly one generation rule and declare all its outputs. Existing repeated writers are a plausible race, not a runtime-reproduced diagnosis. |
| `ctp2_code/gs/dbgen/ctpdb.cpp:94–144` already supports `-i inputfile` and writes to the working directory | Use a portable argument-based command in the binary directory; no shell redirection or generator CLI rewrite is needed. |
| `ctp2_code/os/include/ctp2_config.h:8–13` includes adjacent `config.h` for Autotools, or `config_win32.h` for MSVC | A CMake config header needs a distinct name/explicit selection branch so an old adjacent Autotools header cannot silently win include lookup. |
| `ctp2_code/os/autoconf/os_defs.m4` uses `LINUX` for the legacy non-Windows path, including macOS | Keep this compatibility definition narrowly scoped initially; move real platform behavior behind explicit platform/capability decisions incrementally. Do not mass-rename every conditional during build migration. Never invent compiler CPU macros. |
| `ctp2_code/os/autoconf/Makefile.common` broadcasts include paths and Unix link flags | Assign options and dependencies to their owning targets. Use platform-aware thread/dynamic-loader dependencies instead of broadcasting `-ldl -lpthread` to every OS. |
| `ctp2_code/ui/aui_common/aui_bitmapfont.cpp:165` calls `TT_Init_FreeType` | Preserve bundled FreeType 1.3.1 as a legacy target; modern FreeType 2 is not a compatible replacement. |
| Bundled ANet has its own source lists and compile variants in `ctp2_code/libs/anet/src/linux/dp/GNUmakefile.am` | Build the required networking implementation explicitly; do not replace it with a generic modern package based on name. Audit Windows selection separately. |
| Tracked `ctp2_code/libs/SDL2*` distributions contain no `src/` files | They are not source trees suitable for `add_subdirectory`. Discover installed packages or acquire real pinned source. |
| `ctp2_code/mapgen/Makefile.am` defines four plugins; `gs/world/wldgen.cpp:2569` changes `.dll` to `.so` on the Unix path | Define four module targets and preserve exact loader filenames, exported entry points and locations on Mac as well as Linux. CMake's default suffix alone is not the runtime contract. |
| `tools/native/*.sh` assumes `.native/deps` and Homebrew; diagnostics select an installed CLT/SDK | Put machine-local prefixes and SDK selection in user presets/toolchains. Shared presets must not encode this developer's machine. |
| `.travis.yml` covers an old Linux toolchain; `.gitlab-ci.yml` includes asset-dependent GUI tests; no tracked GitHub Actions workflows | Run native OS checks locally. Builds/component tests must work without additional proprietary assets; gameplay tests require separately supplied data. |

## Generator experiment (performed locally)

Ran the existing native `ctpdb -i <absolute-schema-path>` in a fresh isolated
output directory for each of the 33 distinct `.cdb` inputs referenced by the
current database Makefile. All invocations exited successfully. Output inventory:

- 49 record types, each with a `.cpp` and `.h`: 98 files total.
- Exact set equality with the 49 `newdb_CDBTYPES` declared in the Makefile.
- No output filename is owned by more than one schema.
- Multiple-record inputs: `advance.cdb` (3), `citystyle.cdb` (2),
  `strategy.cdb` (7), `terrain.cdb` (2), `unit.cdb` (6), `wonder.cdb` (2).

This validates a one-rule-per-schema design. It does not yet prove the new build
has correct dependencies, that generators produce byte-identical output on each
OS, or that concurrent execution succeeds. Preserve the existing byacc/flex
grammars initially; switching to Bison is a separate compatibility change.

`ctpdb` preserves existing source/header timestamps when contents are unchanged.
Use a command completion stamp with explicit byproducts, or an equivalent design
verified against no-change rebuilds and deleted outputs; otherwise a schema edit
can cause repeated regeneration. Include the generator executable in file-level
dependencies, not just target build ordering. This distinction is documented in
[CMake custom commands](https://cmake.org/cmake/help/v3.30/command/add_custom_command.html).

## Intended build ownership

- **Game executable:** explicit game source manifest, initially keeping tightly
  coupled gameplay/UI code together. Do not invent a reusable `engine` library
  before establishing a real boundary or copy source lists at configure time
  from the old Makefiles as a permanent implementation.
- **Build-time tools and generated code:** `ctpdb`, parser/scanner rules and
  schema outputs in the binary tree. Generator must run on the build host.
- **Legacy dependencies:** dedicated FreeType 1 and ANet targets with private
  compatibility options. Keep third-party warnings distinct from game warnings.
- **Map plugins:** independent module targets with their existing ABI/layout.
- **External dependencies:** SDL2, SDL2_image, SDL2_mixer, TIFF and zlib exposed
  through imported targets. FFmpeg remains an explicit optional feature; fail
  clearly if requested but unavailable. Record codec features and linkage mode.
- **Diagnostics/tests:** CTest targets, compiler-specific warnings, optional
  ASan/UBSan and analyzer workflows. Keep release/diagnostic directories separate.
- **Runtime staging:** executable, plugins, runtime libraries and distributable
  data staged outside the source tree. User-supplied assets and saves remain
  outside cleanable build output. Verify launch from a different working directory.

Use installed packages for the first Mac parity build to avoid changing SDL
implementation and build system at once. Evaluate pinned vcpkg manifests for a
repeatable three-platform dependency setup, while retaining a system-package
route for Linux packaging. Do not adopt both FetchContent and vcpkg as competing
default providers. Bundled FreeType 1/ANet need their own integration either way.

## Ordered implementation and exit criteria

1. **Common foundation and generator slice.** Add root CMake, target-scoped
   configuration, explicit parser/schema output ownership, presets and the
   dependency-free component tests. Build `ctpdb` from source. Verify clean
   parallel generation, no-change rebuild, schema edit, generator edit, deleted
   output recovery, paths with spaces and two independent binary directories.
   Confirm the source tree remains unchanged. This slice is not the game port.
2. **Native Mac parity.** Integrate preserved legacy dependencies, SDL2 imported
   targets, the game sources and four plugins. Build and run the staged native
   game with the same scaling/settings behavior. Check a fresh source checkout,
   not just this populated workspace. Run component tests and sanitizer startup.
3. **Linux and Windows parity.** Select actual OS sources/resources/libraries;
   use native Linux and Windows environments. Build/link/test with the shared CMake
   graph and validate plugin loading. Preserve MSVC rather than assuming GCC-like
   options work on Windows. Debug/Release must map deliberately to existing
   `_DEBUG`, `_PLAYTEST`, `_BFR_` and logging behavior; debug configuration has
   gameplay/tooling effects in this legacy code.
4. **Reproducibility and delivery.** Pin dependency acquisition inputs, add native
   build/test presets and compiler/SDK version reporting, verify staged library/data
   lookup, then test gameplay on each platform where assets are available.
   Port current sanitizer regression checks into the shared CTest workflow.
5. **Retire duplicate entry points only after parity.** Make launch/build helpers
   thin conveniences over the shared configuration. Keep unsupported legacy
   DirectX/32-bit configurations explicitly on their existing path until migrated
   or deliberately retired; a three-platform SDL build does not establish parity
   for all historical configurations.

Universal Mac binaries, Mac-to-Windows cross-compilation, SDL3,
rewriting networking, signing/notarization and redesigning gameplay modules are
separate follow-up work. They are not prerequisites for one sound native build
architecture. Compiling on Mac alone cannot establish Windows/Linux support.

## Current evidence and remaining uncertainty

The shared CMake graph now builds the full native macOS game, its four map
plugins, generated database/parser code, and eight CTest checks. FreeType 2 is
implemented with legacy spacing and line metrics plus smooth grayscale rendering;
see [the migration review](freetype-migration-review.md). Homebrew provides
CMake, Ninja and CMake docs on the development Mac. The earlier Linux ARM64 build
and five checks passed before the subsequent font and launch fixes; those results
do not validate the latest changes. Windows remains unvalidated; hosted CI is deliberately excluded.

The automated launch in `tools/cmake/smoke.py --launch-game` uses real game data
and the New Game/Launch callbacks. With Apple Clang 21 ASan and UBSan enabled,
it now creates a world, renders the map, processes 60 frames and exits normally.
All eight component tests pass in regular and sanitizer builds. The regular build
also passed two consecutive full launch tests after one initial 90-second
timeout; the timeout has not been reproduced or assigned a confirmed cause.
The screen-only test previously missed this path. This is startup coverage, not
proof of an entire campaign, multiplayer, save/load or all graphics modes.

Failures found and repaired while extending coverage:

- Bare font resource names now receive a valid point size before FreeType loads.
- macOS map modules remain resident while their handles are balanced, avoiding
  sanitizer metadata failures on repeated unload/reload; generator objects still
  follow their normal lifetime.
- Packed tile file scalars use alignment-safe copies, retaining their file format.
- All eight tile run encoders stop before looking beyond a scanline. Regression
  checks exercise exact-sized input buffers and encoded output for both depths.
- Paired sprite pixels and shadows use alignment-safe copies because a 16-bit
  surface does not guarantee 32-bit alignment at every pixel.
- AI border permission checks handle unowned land before selecting a player bit.

Run component checks with `ctest --preset <preset>`. The full launch check requires
legitimately supplied assets and an isolated persistent runtime, for example:

```sh
python3 tools/cmake/smoke.py --launch-game --build-dir build/sanitize \
  --data-dir /path/to/game --runtime-dir .native/play/smoke-sanitize
```

The test runtime's display profile is modified for windowed testing; use a
separate directory from normal play. No proprietary game assets belong in CI or
source control. Native Windows/Linux parity and clean-machine packaging remain
to be verified before replacing their existing supported build paths.
