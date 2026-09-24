#!/usr/bin/env python3
"""Observe instrumented startup for 20 seconds, then close the test process.

This detects sanitizer failures on executed startup paths; it does not verify
that the menu rendered or exercise gameplay. Run after check.py sanitize.
"""
from datetime import datetime
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]


def main():
    report = ROOT / '.native/checks/reports' / datetime.now().strftime('startup-%Y%m%d-%H%M%S-%f')
    report.mkdir(parents=True)
    log_path = report / 'startup.log'
    survived = False
    with log_path.open('w') as log:
        process = subprocess.Popen([str(ROOT / 'tools/native-checks/run-game.sh')],
                                   cwd=ROOT, stdout=log, stderr=subprocess.STDOUT)
        try:
            process.wait(timeout=20)
        except subprocess.TimeoutExpired:
            survived = True
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait()
    output = log_path.read_text(errors='replace')
    sanitizer_error = any(marker in output for marker in
                          ('runtime error:', 'ERROR: AddressSanitizer', 'ERROR: LeakSanitizer',
                           'AddressSanitizer:DEADLYSIGNAL'))
    passed = survived and 'SDL display:' in output and not sanitizer_error
    print('Startup observation:', 'PASS' if passed else 'FAIL')
    print('Log:', log_path)
    if passed:
        print('Survived 20 seconds and initialized SDL without a sanitizer report. Gameplay is untested.')
    else:
        print(output[-10000:])
    return 0 if passed else 1


if __name__ == '__main__':
    sys.exit(main())
