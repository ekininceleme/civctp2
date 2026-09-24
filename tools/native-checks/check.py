#!/usr/bin/env python3
"""Native compiler/analyzer checks using the existing Autotools project."""
import argparse
import json
import os
import re
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / '.native/checks'
LLVM = ROOT / '.native/tools/scan-build-21.1.8'
WARNINGS = '-Wall -Wextra -Wformat=2 -Werror=non-pod-varargs -Wno-deprecated-declarations'
SANITIZERS = '-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all'


def run(args, cwd, log, env, timeout=None):
    print('Running:', ' '.join(map(str, args)), flush=True)
    with log.open('w') as stream:
        result = subprocess.run(args, cwd=cwd, env=env, stdout=stream, stderr=subprocess.STDOUT, timeout=timeout)
    print(f'Exit {result.returncode}; log: {log}', flush=True)
    return result.returncode


def environment():
    env = os.environ.copy()
    env['PATH'] = f'/Library/Developer/CommandLineTools/usr/bin:{ROOT}/.native/deps/bin:/opt/homebrew/opt/libtool/libexec/gnubin:' + env['PATH']
    env['lt_cv_sys_max_cmd_len'] = '262144'
    env['DEVELOPER_DIR'] = '/Library/Developer/CommandLineTools'
    env['SDKROOT'] = '/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk'
    # SDL2 compatibility dynamically loads SDL3 from Homebrew.
    env['DYLD_FALLBACK_LIBRARY_PATH'] = '/opt/homebrew/lib:/usr/local/lib:/usr/lib'
    return env


def prepare(mode, env, reports):
    # Autotools in this project has in-source build assumptions. Use a disposable
    # source snapshot inside this worktree, including uncommitted changes.
    work = OUT / 'work' / mode
    work.mkdir(parents=True, exist_ok=True)
    excludes = ['/.git', '/.native', '/build', '/CMakeUserPresets.json', '/ctp2_data', '/Scenarios', '/doc', '/tests',
                '*.o', '*.lo', '*.la', '*.a', '*.so', '.libs', '.deps', 'autom4te.cache',
                'config.status', 'config.log', 'config.cache', 'Makefile', 'GNUmakefile',
                '/ctp2_code/ctp/ctp2', '/ctp2_code/ctp/save', '/ctp2_code/ctp/dll',
                '*.log', '*.map', '*.tmp', '*~']
    subprocess.run(['rsync', '-a', '--delete', '--delete-excluded',
                    *['--exclude=' + p for p in excludes], str(ROOT) + '/', str(work) + '/'], check=True)
    flags = '-O1 -g -fms-extensions ' + WARNINGS
    extra = ' ' + SANITIZERS if mode == 'sanitize' else ''
    env = env.copy()
    env.update(CC='clang', CXX='clang++',
               CPPFLAGS=f'-I{ROOT}/.native/deps/include -I{ROOT}/.native/deps/include/SDL2 -I/opt/homebrew/include',
               CFLAGS=flags + ' -std=gnu11' + extra,
               CXXFLAGS=flags + extra,
               LDFLAGS=f'-L{ROOT}/.native/deps/lib -L/opt/homebrew/lib' + extra)
    # Instrumenting configure feature probes can change their results. Configure
    # with normal flags; pass sanitizer flags to the actual build explicitly.
    configure_env = env.copy()
    if mode == 'sanitize':
        for key in ('CFLAGS', 'CXXFLAGS', 'LDFLAGS'):
            configure_env[key] = configure_env[key].replace(extra, '')
    code = run(['./configure', '--without-x', '--enable-silent-rules',
                '--prefix=' + str(work / 'install')], work, reports / 'configure.log', configure_env)
    if code:
        raise RuntimeError(f'{mode} configuration failed; see {reports}/configure.log')
    return work, env


def self_test(env, reports):
    """Prove the compiler, analyzer and runtime checks can catch known bad code."""
    source = reports / 'probe-address.cpp'
    source.write_text('#include <cstdlib>\nint main(int argc, char**) { int *p = (int*)malloc(sizeof(int)); p[argc] = 1; free(p); return 0; }\n')
    binary = reports / 'probe'
    if run(['clang++', '-O0', '-g', *SANITIZERS.split(), str(source), '-o', str(binary)], ROOT, reports / 'probe-build.log', env):
        raise RuntimeError('Sanitizer compiler probe failed')
    code = run([str(binary)], ROOT, reports / 'probe-runtime.log', env, timeout=30)
    if code == 0 or 'AddressSanitizer' not in (reports / 'probe-runtime.log').read_text():
        raise RuntimeError('Sanitizer did not detect the deliberate buffer overflow')
    source = reports / 'probe-null.cpp'
    source.write_text('int main() { int *p = 0; return *p; }\n')
    code = run(['clang++', '--analyze', '-Xanalyzer', '-analyzer-output=text', str(source)], ROOT, reports / 'probe-analyzer.log', env)
    if code or 'core.NullDereference' not in (reports / 'probe-analyzer.log').read_text():
        raise RuntimeError('Static analyzer did not detect the deliberate null dereference')
    source = reports / 'probe-varargs.cpp'
    source.write_text('struct X { X(); X(const X&); ~X(); }; void f(...); void g(X x) { f(x); }\n')
    code = run(['clang++', '-fsyntax-only', '-Werror=non-pod-varargs', str(source)], ROOT, reports / 'probe-varargs.log', env)
    if code == 0 or 'non-pod-varargs' not in (reports / 'probe-varargs.log').read_text():
        raise RuntimeError('Compiler did not reject the deliberate unsafe varargs call')
    source = reports / 'probe-undefined.cpp'
    source.write_text('#include <climits>\nint main(int argc, char**) { volatile int n = INT_MAX; return n + argc; }\n')
    if run(['clang++', '-O0', '-g', *SANITIZERS.split(), str(source), '-o', str(binary)], ROOT, reports / 'probe-undefined-build.log', env):
        raise RuntimeError('UBSan compiler probe failed')
    code = run([str(binary)], ROOT, reports / 'probe-undefined.log', env, timeout=30)
    if code == 0 or 'signed integer overflow' not in (reports / 'probe-undefined.log').read_text():
        raise RuntimeError('UBSan did not detect deliberate signed integer overflow')
    print('Detection probes passed (their failures are intentional).', flush=True)


def regression_tests(env, reports):
    binary = reports / 'native-abi-test'
    code = run(['clang++', '-std=c++11', '-g', *SANITIZERS.split(), '-DHAVE_CONFIG_H',
                '-I' + str(ROOT / 'ctp2_code/os/include'), str(ROOT / 'tests/native-abi.cpp'),
                '-o', str(binary)], ROOT, reports / 'native-abi-build.log', env)
    if code:
        return code
    code = run([str(binary)], ROOT, reports / 'native-abi-test.log', env, timeout=30)
    if code:
        return code
    # Link the real archive implementation with a small buffer-lifecycle test.
    # Dead stripping excludes unrelated game entry points and dependencies.
    include_dirs = [ROOT, ROOT / 'ctp2_code/os/include', ROOT / 'ctp2_code/os/nowin32',
                    ROOT / 'ctp2_code', ROOT / 'ctp2_code/libs/anet/h',
                    ROOT / '.native/deps/include', Path('/opt/homebrew/include'),
                    Path('/opt/homebrew/include/SDL2')]
    common = (ROOT / 'ctp2_code/os/autoconf/Makefile.common').read_text()
    include_dirs += [ROOT / 'ctp2_code' / path for path in
                     re.findall(r'-I\$\(ctp2_code\)/([^\s\\]+)', common)]
    binary = reports / 'archive-buffer-test'
    code = run(['clang++', '-std=c++11', '-DHAVE_CONFIG_H', '-fms-extensions', '-g',
                *SANITIZERS.split(), *['-I' + str(path) for path in dict.fromkeys(include_dirs)],
                str(ROOT / 'tests/archive-buffer.cpp'),
                str(ROOT / 'ctp2_code/robot/aibackdoor/civarchive.cpp'),
                '-Wl,-dead_strip', '-o', str(binary)], ROOT, reports / 'archive-build.log', env)
    if code:
        return code
    code = run([str(binary)], ROOT, reports / 'archive-test.log', env, timeout=30)
    if code:
        return code
    binary = reports / 'slic-bytecode-test'
    code = run(['clang++', '-std=c++11', '-g', *SANITIZERS.split(),
                str(ROOT / 'tests/slic-bytecode.cpp'), '-o', str(binary)],
               ROOT, reports / 'slic-bytecode-build.log', env)
    if code:
        return code
    code = run([str(binary)], ROOT, reports / 'slic-bytecode-test.log', env, timeout=30)
    if code:
        return code
    binary = reports / 'display-scaling-test'
    code = run(['clang++', '-std=c++11', '-g', *SANITIZERS.split(), '-I/opt/homebrew/include', '-I/opt/homebrew/include/SDL2',
                str(ROOT / 'tests/display-scaling.cpp'), '-L/opt/homebrew/lib', '-lSDL2', '-o', str(binary)],
               ROOT, reports / 'scaling-build.log', env)
    if code:
        return code
    return run([str(binary)], ROOT, reports / 'scaling-test.log', dict(env, SDL_VIDEODRIVER='dummy'), timeout=60)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('mode', choices=['test', 'analyze', 'sanitize', 'all'], nargs='?', default='all')
    parser.add_argument('-j', '--jobs', type=int, default=6)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error('jobs must be positive')
    OUT.mkdir(parents=True, exist_ok=True)
    # Avoid overlapping rsync/builds in the shared disposable build directories.
    import fcntl
    with (OUT / 'lock').open('w') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            parser.error('another native check is running')
        reports = OUT / 'reports' / time.strftime('%Y%m%d-%H%M%S')
        reports.mkdir(parents=True)
        env = environment()
        run(['clang', '--version'], ROOT, reports / 'toolchain.txt', env)
        results = {}
        if args.mode in ('test', 'all'):
            self_test(env, reports)
            results['tests'] = regression_tests(env, reports)
        for mode in ('analyze', 'sanitize'):
            if args.mode not in (mode, 'all'):
                continue
            if mode == 'analyze' and not (LLVM / 'bin/scan-build').exists():
                raise RuntimeError('Run tools/native-checks/install-scan-build.py first')
            stage = reports / mode
            stage.mkdir()
            work, build_env = prepare(mode, env, stage)
            command = ['make', f'-j{args.jobs}', 'CC=clang', 'CXX=clang++']
            command += [f'{key}={build_env[key]}' for key in ('CPPFLAGS', 'CFLAGS', 'CXXFLAGS', 'LDFLAGS')]
            if mode == 'analyze':
                # Explicit CC/CXX are needed because configure writes them into Makefiles.
                command[2:4] = ['CC=' + str(LLVM / 'libexec/ccc-analyzer'),
                                'CXX=' + str(LLVM / 'libexec/c++-analyzer')]
                command = [str(LLVM / 'bin/scan-build'), '--use-analyzer', shutil.which('clang', path=env['PATH']),
                           '--use-cc', shutil.which('clang', path=env['PATH']),
                           '--use-c++', shutil.which('clang++', path=env['PATH']),
                           '--status-bugs', '-o', str(stage / 'html'), *command]
            results[mode] = run(command, work, stage / 'build.log', build_env)
            if mode == 'sanitize' and results[mode] == 0:
                print('Sanitized game built:', work / 'ctp2_code/ctp/ctp2', flush=True)
                print('Full gameplay has not been exercised by this build.', flush=True)
        (reports / 'summary.json').write_text(json.dumps(results, indent=2) + '\n')
        print('Results:', results, '\nReports:', reports, flush=True)
        return int(any(results.values()))


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (RuntimeError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
        sys.exit(str(exc))
