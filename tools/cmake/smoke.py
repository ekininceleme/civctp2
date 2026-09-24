#!/usr/bin/env python3
"""Exercise New Game screen construction with real data and capture diagnostics."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--launch-game', action='store_true', help='Also generate a world and start gameplay')
    parser.add_argument('--build-dir', type=Path, required=True)
    parser.add_argument('--data-dir', type=Path, required=True)
    parser.add_argument('--runtime-dir', type=Path, required=True,
                        help='Dedicated test runtime; do not use your regular play directory')
    args = parser.parse_args()
    command = [sys.executable, str(Path(__file__).with_name('run.py')),
               '--build-dir', str(args.build_dir), '--data-dir', str(args.data_dir),
               '--runtime-dir', str(args.runtime_dir)]
    subprocess.run([*command, '--stage-only'], check=True)
    profile = args.runtime_dir / 'ctp2_program/ctp/userprofile.txt'
    if not profile.exists():
        parser.error('Test runtime needs an initial userprofile.txt')
    lines = profile.read_text().splitlines()
    settings = {'WindowedMode': 'Yes', 'ScreenResWidth': '1024',
                'ScreenResHeight': '768', 'ScreenScalePercent': '100'}
    lines = [line for line in lines if line.split('=', 1)[0] not in settings]
    profile.write_text('\n'.join(lines + [f'{k}={v}' for k, v in settings.items()]) + '\n')
    env = dict(os.environ, CTP2_TEST_NEW_GAME='launch' if args.launch_game else '1', UBSAN_OPTIONS='print_stacktrace=1')
    with tempfile.TemporaryFile(mode='w+') as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, env=env)
        completed = False
        try:
            completed = process.wait(timeout=90) == 0
        except subprocess.TimeoutExpired:
            print("Test timed out before normal shutdown")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
        log.seek(0)
        output = log.read()
    print(output)
    failed = any(marker in output for marker in
                 ('runtime error:', 'ERROR: AddressSanitizer', 'AddressSanitizer:DEADLYSIGNAL'))
    marker = 'CTP2 test: Game launched and processed 60 frames' if args.launch_game else 'CTP2 test: New Game screen initialized'
    passed = completed and not failed and marker in output
    print('Game launch smoke test:' if args.launch_game else 'New Game screen smoke test:', 'PASS' if passed else 'FAIL')
    return 0 if passed else 1


if __name__ == '__main__':
    raise SystemExit(main())
