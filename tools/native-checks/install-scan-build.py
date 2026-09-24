#!/usr/bin/env python3
"""Install the small official LLVM scan-build wrapper locally; no package manager."""
import base64
import json
from pathlib import Path
import subprocess

TAG = 'llvmorg-21.1.8'
DEST = Path(__file__).resolve().parents[2] / '.native/tools/scan-build-21.1.8'


def api(path):
    return json.loads(subprocess.check_output(['gh', 'api', path]))


def download(remote, local):
    local.mkdir(parents=True, exist_ok=True)
    for item in api(f'repos/llvm/llvm-project/contents/{remote}?ref={TAG}'):
        target = local / item['name']
        if item['type'] == 'dir':
            download(item['path'], target)
        else:
            target.write_bytes(base64.b64decode(api(item['url'])['content']))
            if local.name in ('bin', 'libexec'):
                target.chmod(0o755)


for name in ('bin', 'libexec', 'share'):
    download('clang/tools/scan-build/' + name, DEST / name)
license_data = api(f'repos/llvm/llvm-project/contents/llvm/LICENSE.TXT?ref={TAG}')
(DEST / 'LICENSE.TXT').write_bytes(base64.b64decode(license_data['content']))
print(DEST)
