#!/usr/bin/env python3
"""Stage a CMake build with user-supplied game data in a persistent play directory."""
import argparse
from pathlib import Path
import shutil
import subprocess
import os

ROOT = Path(__file__).resolve().parents[2]


def copy_updated(source, destination):
    destination.mkdir(parents=True, exist_ok=True)
    for path in source.rglob('*'):
        if not path.is_file():
            continue
        target = destination / path.relative_to(source)
        if target.exists() and target.name in ('civpaths.txt', 'userprofile.txt', 'userkeymap.txt'):
            continue
        if target.exists() and (target.stat().st_size, target.stat().st_mtime_ns) == (path.stat().st_size, path.stat().st_mtime_ns):
            continue
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--data-dir', type=Path, required=True,
                        help='Directory containing ctp2_data and Scenarios from a legitimate game installation')
    parser.add_argument('--runtime-dir', type=Path,
                        help='Persistent writable directory outside build output (default: .native/play/<build-name>)')
    parser.add_argument('--stage-only', action='store_true')
    parser.add_argument('game_args', nargs='*')
    args = parser.parse_args()
    build = args.build_dir.resolve()
    runtime = (args.runtime_dir or ROOT / '.native/play' / build.name).resolve()
    if runtime == build or build in runtime.parents:
        parser.error('runtime-dir must be outside the build directory so cleaning cannot delete saves')
    binary_dir = build / 'runtime/ctp2_program/ctp'
    executable = 'ctp2.exe' if os.name == 'nt' else 'ctp2'
    if not (binary_dir / executable).is_file():
        parser.error('Build the game first; executable is missing in ' + str(binary_dir))
    data = args.data_dir.resolve()
    if not (data / 'ctp2_data').is_dir():
        parser.error('data-dir must contain ctp2_data')
    for directory in ('ctp2_data', 'Scenarios'):
        if (data / directory).is_dir():
            # Game data is copied initially; don't overwrite later user modifications.
            if not (runtime / directory).exists():
                shutil.copytree(data / directory, runtime / directory)
    program = runtime / 'ctp2_program/ctp'
    copy_updated(binary_dir, program)
    for name in ('civpaths.txt', 'userprofile.txt', 'userkeymap.txt'):
        source = ROOT / 'ctp2_code/ctp' / name
        if source.exists() and not (program / name).exists():
            shutil.copy2(source, program / name)
    # Supplied assets are overlaid with the versioned rules/UI of this checkout
    # by the caller's data-dir, as described in the upstream setup instructions.
    print('Runtime:', runtime, flush=True)
    if args.stage_only:
        return 0
    env = dict(os.environ, CTP2_USER_DIR=str(runtime / 'user'))
    if os.sys.platform == 'darwin':
        env.setdefault('DYLD_FALLBACK_LIBRARY_PATH', '/opt/homebrew/lib:/usr/local/lib:/usr/lib')
    os.chdir(program)
    os.execve(str(program / executable), [executable, 'nointromovie', *args.game_args], env)


if __name__ == '__main__':
    raise SystemExit(main())
