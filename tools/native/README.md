# Native macOS build

Apple Silicon port based on upstream master `9d9677e4d` (2026-09-07).
Native source, SDL scaling, crash fixes and check scripts are maintained on the
single local `native-macos` branch.

From the repository root:

```sh
tools/native/configure.sh   # regenerate/configure when needed
tools/native/build.sh
tools/native/run.sh
```

The existing `.native/*.sh` launch/build paths forward to these tracked scripts.
The executable is `ctp2_code/ctp/ctp2`; map plugins are native arm64 bundles.

This checkout already has the locally built dependencies in `.native/deps`:
byacc 20260126, SDL2_image 2.8.8 and SDL2_mixer 2.8.1. It also uses the existing
Homebrew SDL2 compatibility library, SDL3, libtiff and GNU libtool. These scripts
assume those dependencies are present; they are not a clean-machine installer.
FFmpeg movies are disabled. Original GOG game assets must be supplied separately;
local copied assets, binaries, dependencies and saves must not be committed.

The normal build uses Apple Clang and rejects non-POD variadic arguments. That
check prevents the compiler-generated traps previously encountered when founding
a city and identified in cargo/fortify paths. Static checks do not establish that
all gameplay paths work.

Options > Graphics > Game scale changes the SDL logical canvas in 25% increments,
keeping the output resolution high. It applies after restart; the minimum logical
canvas is 800x600. The user verified 200% controls, clicks and the settings button.

The launcher sets `CTP2_USER_DIR` to `.native/user`. Other game files and saves
also live under `ctp2_code/ctp` according to `civpaths.txt`.

For repeatable compiler, static-analysis and sanitizer checks, see
[the check instructions](../native-checks/README.md).
