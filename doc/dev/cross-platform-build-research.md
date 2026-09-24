> Implementation update: after this research, the user authorized migrating the
> font renderer to FreeType 2. The CMake path now uses that API; references below
> to preserving FreeType 1 describe the original staged recommendation.

# Cross-platform build research

Research date: 2026-09-24. Scope: a common native Windows, macOS and Linux build for this existing C/C++ SDL2 game. This document distinguishes upstream documentation from project recommendations. It is a design proposal, not a claim that all three builds have passed.

## Recommendation

Introduce a target-based CMake build beside the existing Autotools and Visual Studio builds, then retire the old build definitions after equivalent behavior is demonstrated. Keep SDL2, the existing game architecture and compiler choices. Start with native builds on each operating system; cross-compilation is an additional capability with separate host-tool requirements.

CMake is a suitable common description because it expresses executable/library targets and their transitive requirements, while selecting native build tools separately. SDL2 itself documents concurrent migration from Autotools and provides CMake integration for system or vendored SDL2. This supports an incremental transition; it does not prove this game's platform code is portable. [CMake buildsystem](https://cmake.org/cmake/help/v3.30/manual/cmake-buildsystem.7.html), [SDL2 CMake guide](https://wiki.libsdl.org/SDL2/README-cmake)

The common build must own source lists, generated-code dependencies, platform selection, tests and installation. A CMake script that merely invokes today's Makefiles would retain their source-tree assumptions and fail to unify Windows with the Unix build.

## Target architecture

**Documented mechanism.** CMake targets carry source files, include paths, definitions and link dependencies. `PRIVATE`, `PUBLIC` and `INTERFACE` govern which requirements propagate to consumers. Requirements should describe what consumers actually need, rather than broadcasting convenient settings globally. [Target usage requirements](https://cmake.org/cmake/help/v3.30/manual/cmake-buildsystem.7.html#target-usage-requirements)

**Proposed initial targets**, subject to the source audit:

| Target | Responsibility |
| --- | --- |
| `ctpdb` | Build-time database generator, executable on the build host |
| `ctp2` | Game executable and existing tightly coupled game sources |
| One target per map plugin | Loadable modules, with filenames and exported symbols matching the current loader |
| Bundled FreeType 1 / ANet targets | Preserve existing legacy APIs, source lists and required definitions |
| Component regression executables | Existing ABI, archive, bytecode and scaling tests |

Do not split every source directory into a library solely to make the build look modular. That would introduce speculative boundaries and static-library link ordering problems. Extract reusable libraries only when actual consumers justify them. Use target-local source selection for operating-system implementations; compiler-specific options depend on compiler identity, not merely the OS.

CMake distinguishes ordinary shared libraries from `MODULE` libraries, which are intended for runtime loading. An object library compiles sources without producing a normal archive; use it only when shared compilation is needed, not as a substitute for a coherent interface. [Library target types](https://cmake.org/cmake/help/latest/command/add_library.html)

Keep compiler-provided CPU macros intact. Express game capabilities through project-owned configuration names and compile checks. A generated configuration header can expose detected features without pretending one architecture is another. CMake's source checks compile and normally link a test, so they can establish capabilities without executing target code. [CheckCXXSourceCompiles](https://cmake.org/cmake/help/latest/module/CheckCXXSourceCompiles.html)

## Generated code is the critical migration boundary

**Documented mechanism.** `add_custom_command(OUTPUT ...)` describes generated files and their dependencies. List secondary outputs in `BYPRODUCTS`, and use `VERBATIM` for argument escaping. A generator target in `DEPENDS` makes changes to its executable regenerate outputs. Multiple independent targets must not race to generate the same files. `PRE_BUILD` has Visual-Studio-specific semantics, so it is unsuitable for a portable parser/database generation rule. [Custom command documentation](https://cmake.org/cmake/help/latest/command/add_custom_command.html)

**Recommendation for this project.** Run the database generator once per schema, with every generated record source/header listed under that one rule. Avoid one rule per generated record if those rules all run the same schema and write overlapping files. Make parser generation a preceding dependency of the generator executable. Write all generated files into the build tree and add only those directories to the consumers.

Pass input filenames explicitly using the generator's supported input option; do not make shell redirection part of the portable command. Audit generated output enumeration and schema inclusions before declaring the dependency list complete. Record the parser generator implementation/version: Bison and byacc are not interchangeable merely because both accept yacc-like grammars. CMake's `FindBISON` is specifically an integration for Bison, not a guarantee that switching a byacc grammar is compatible. [FindBISON](https://cmake.org/cmake/help/latest/module/FindBISON.html)

Validation should include a clean parallel build; deletion of one generated output; editing a schema; editing the generator; a no-change rebuild; and two independent build directories. These checks exercise dependency correctness instead of just proving one warmed-up checkout builds.

## Native builds first; host and target must stay distinct

CMake toolchain files describe compilers, SDKs and cross-compilation search behavior. In cross builds, programs needed during the build generally belong to the host, while libraries/headers belong to the target. Configure checks that execute a compiled program require extra care when that executable cannot run on the host. [Toolchain documentation](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)

A target executable named in a custom command is run directly for native builds. Cross-compiling requires a configured emulator or another way to supply a runnable generator. Therefore, a Mac-to-Windows build cannot compile `ctpdb.exe` for Windows and casually execute it during the Mac build. [Executable custom commands](https://cmake.org/cmake/help/latest/command/add_custom_command.html#command)

**Recommendation.** Initially build on Mac for Mac, Windows for Windows, and Linux for Linux. Provide a future explicit path to a host-built `ctpdb` when cross-compiling, with a clear configure-time error if unavailable. Do not promise arbitrary cross builds in the first migration. A universal macOS binary also requires compatible dependencies for both architectures; setting an architecture list alone is not sufficient evidence.

## Dependencies: discovery first, acquisition policy separate

SDL2 documents `SDL2::SDL2` and optional `SDL2::SDL2main` targets, including linking SDL2main first when present. It supports using an installed SDL or adding its source tree. Preserve the SDL2 API and avoid coupling this build migration to SDL3 adoption. [SDL2 integration](https://wiki.libsdl.org/SDL2/README-cmake#including-sdl-in-your-project)

| Approach | Benefit | Cost and appropriate role |
| --- | --- | --- |
| Installed packages through `find_package` | Respects existing developer/distribution environments | Versions and optional codec features can differ; document and test the accepted configuration |
| vcpkg manifest + pinned baseline | Explicit dependencies, platform triplets and version policy | Another bootstrap/cache/toolchain to maintain; verify legacy libraries and optional codecs before choosing it |
| Pinned source via `FetchContent` | Same selected source revision can build with the chosen compiler | Configure-time downloads/build integration; dependency options and transitive libraries remain the project's responsibility |
| Existing bundled source | Preserves legacy APIs that modern packages cannot replace | Project must maintain its build integration and acknowledge the old component's maintenance status |

vcpkg manifest mode declares project dependencies. Its versioning uses baselines and constraints; a baseline is not a byte-for-byte lock of every resulting binary, since toolchain, triplet and build inputs still matter. Integration occurs through a CMake toolchain and can cooperate with another toolchain. [Manifest mode](https://learn.microsoft.com/en-us/vcpkg/consume/manifest-mode), [Versioning](https://learn.microsoft.com/en-us/vcpkg/users/versioning), [CMake integration](https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration), [Triplets](https://learn.microsoft.com/en-us/vcpkg/users/triplets)

`FetchContent` makes sources available at configure time and can integrate with `find_package`. Pin immutable revisions or archive hashes, and offer a local-source override for offline/reproducible use. Do not silently download a different version when an explicit dependency selection fails. [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html)

**Recommendation.** Keep dependency discovery behind imported targets. Preserve bundled FreeType 1's `TT_*` API and bundled ANet until independently migrated. Prototype package availability on all three systems before selecting a default acquisition tool. Do not make a modern FreeType package a drop-in replacement, and do not assume a directory containing platform binaries is portable source. Record static/shared linkage and enabled image/audio codecs as part of the tested configuration.

## Presets and diagnostics

CMake separates shared `CMakePresets.json` from local `CMakeUserPresets.json`; presets cover configure/build/test workflows and can inherit settings. Keep checked-in presets free of developer-specific absolute paths. Toolchains select compilers/SDKs; presets select repeatable developer workflows. [Presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)

**Recommendation.** Start with ordinary debug/release and diagnostic presets, with separate build directories for incompatible compiler/sanitizer configurations. Preserve Clang on macOS. Keep existing Windows compiler support until CI demonstrates the intended replacement. Select the minimum CMake version from features actually used and verify that baseline, rather than requiring the newest release by habit.

CMake can export `compile_commands.json` with Makefile/Ninja generators. Its target clang-tidy integration also works with Makefile/Ninja generators; it is not universally available for every IDE generator. A Ninja diagnostic preset can coexist with Visual Studio/Xcode development workflows. [Compilation database](https://cmake.org/cmake/help/latest/variable/CMAKE_EXPORT_COMPILE_COMMANDS.html), [clang-tidy target property](https://cmake.org/cmake/help/latest/prop_tgt/LANG_CLANG_TIDY.html)

ASan must participate in compilation and final executable linking; its runtime is a diagnostic dependency, not a production packaging choice. It detects memory faults in exercised paths and stops on a detected error. UBSan adds checks such as alignment and signed overflow; fail-fast configuration prevents a test from continuing after known undefined behavior. [ASan usage and limitations](https://clang.llvm.org/docs/AddressSanitizer.html), [UBSan](https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html)

**Recommendation.** Keep warning, analyzer and sanitizer options target-scoped and compiler-aware. Test the sanitizer toolchain with deliberate negative fixtures, as the existing tooling does. Run component tests without proprietary assets. Label startup tests requiring game data separately, and never equate a successful startup timeout with completed gameplay validation. Retain actionable checks such as non-POD varargs errors; do not turn the entire legacy warning inventory into a blocking migration prerequisite.

## CI, installation and acceptance

Implementation decision: the user does not want GitHub Actions. The following
research describes an available option, not an enabled or planned workflow.
Validation for this branch uses locally invoked builds and tests.

GitHub Actions matrix jobs can vary operating systems and compilers independently, including explicit inclusions/exclusions. Native jobs provide evidence about each actual platform rather than just whether the common build script configures. [GitHub matrix documentation](https://docs.github.com/en/actions/how-tos/write-workflows/choose-what-workflows-do/run-job-variations)

**Proposed initial coverage:** macOS arm64/Clang, Linux x86_64/GCC, Windows x64 with the existing supported compiler, plus one Clang ASan/UBSan job. Add Linux Clang and macOS x86_64 as capacity permits. Record compiler/SDK/dependency versions in job output. Publish diagnostic logs, but never commercial game assets, saves or credentials.

CMake installation rules distinguish executables, Windows runtime DLLs, libraries and macOS bundles. Relative installation destinations support relocating the staging prefix. Runtime dependency collection exists, but does not decide licensing or replace verification of the installed app. [Installation rules](https://cmake.org/cmake/help/latest/command/install.html)

**Recommendation.** Define an install/staging layout separately from the source tree. Verify launch from the staged directory with no accidental dependency on the developer checkout or Homebrew library paths. Preserve user-owned game data and saves outside build outputs. Keep signing/notarization and store distribution outside the first build migration.

Migration completion requires: all platform jobs compile/link; component tests pass; generated-file and incremental-build checks pass; plugins load with the expected names; the native Mac test build retains current scaling/gameplay behavior; and staged artifacts locate their dependencies/data correctly. Only then remove duplicate legacy build definitions. Until that evidence exists, report platform support as pending verification rather than inferred from CMake support.
