# Native macOS checks

These scripts use the existing Autotools/Make project and Apple Clang.
Diagnostic builds explicitly use the installed Command Line Tools Clang 21 and
its SDK; Xcode Clang 17 remains the normal game build compiler. The older ASan
runtime hangs on this macOS version, confirmed with a minimal probe. The scripts
set a local SDL3 library search path for instrumented tests. Run from
any directory; source changes, including uncommitted changes, remain in this Git
worktree. No build-system migration or Homebrew installation is required.

Prerequisites: working native configuration (`.native/configure.sh`), local native
dependencies in `.native/deps`, Python 3 standard library, Xcode tools, rsync.
Install the official LLVM scan-build wrapper once (requires network and `gh`):

```sh
python3 tools/native-checks/install-scan-build.py
```

Run all checks:

```sh
python3 tools/native-checks/check.py all
```

Individual commands:

- `check.py test`: prove compiler/analyzer/sanitizer detection using deliberately
  broken examples, then run the native architecture/string-layout, archive buffer
  lifecycle, unaligned SLIC bytecode and SDL scaling regressions with ASan and UBSan.
- `check.py analyze`: clean build with `-Wall -Wextra -Wformat=2`, unsafe non-POD
  varargs treated as errors, and Clang Static Analyzer through `scan-build`.
  HTML reports include traces for suspected bugs. Findings return nonzero; they
  require review and may include false positives or existing upstream issues.
- `check.py sanitize`: clean build of the game and map plugins with
  AddressSanitizer and UndefinedBehaviorSanitizer. Build success does not mean
  gameplay has been checked; the instrumented game must be exercised.

Reports and logs: `.native/checks/reports/<timestamp>/`. Each full run writes
`summary.json`; nonzero status signals findings, build failures or test failures.
Compiler warnings are reported, not all treated as errors in this legacy project.
No suppression baseline is created automatically.

The upstream Makefiles assume in-source builds. To keep the playable binary and
save files intact, checks build disposable source snapshots under
`.native/checks/work/`, using current files from this worktree. These are generated
build directories, not separate Git repositories. Each run removes old build
products there, ensuring unchanged files are analyzed too. Do not edit them.
Downloaded scan-build scripts, reports, probes and build artifacts stay in
`.native/`; source scripts and test changes can be reviewed and committed in Git.

Static analysis explores potential paths without launching the game. Sanitizers
check only executed paths; they do not replace gameplay regressions. Existing
SDL tests cover scaling/rendering/input coordinates, not whole-game actions.
Third-party prebuilt SDL libraries are not sanitizer-instrumented by this setup.

References:
- https://clang.llvm.org/docs/analyzer/user-docs/CommandLineUsage.html
- https://clang.llvm.org/docs/AddressSanitizer.html
- https://clang.llvm.org/docs/UndefinedBehaviorSanitizer.html

To exercise the instrumented game after a successful sanitizer build:

```sh
tools/native-checks/run-game.sh
```

This copies the existing game assets into the diagnostic directory and keeps its
saves separate. Runtime errors are printed to the launching terminal and stop
the process at the first detected error. This command opens the game; the check
command itself does not launch interactive gameplay.

For a bounded startup observation after building the instrumented game:

```sh
python3 tools/native-checks/startup-check.py
```

This opens the diagnostic game for 20 seconds, records its output in a new report
folder, and closes that process. It fails on early exit, missing SDL initialization,
or a sanitizer report. Passing means only that the observed startup survived;
it does not prove the menu displayed correctly or that gameplay is safe.
