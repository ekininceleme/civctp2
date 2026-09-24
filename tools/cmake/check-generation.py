#!/usr/bin/env python3
"""Exercise the real CMake generation graph in an isolated source/build fixture."""
from pathlib import Path
import argparse
import hashlib
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def digest_tree(root):
    return {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
            for p in root.rglob('*') if p.is_file()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--byacc', required=True)
    parser.add_argument('--flex', required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix='ctp2 generation ') as tmp:
        base = Path(tmp)
        source = base / 'source with spaces'
        source.mkdir()
        for name in ('CMakeLists.txt', 'CMakePresets.json'):
            shutil.copy2(ROOT / name, source / name)
        shutil.copytree(ROOT / 'cmake', source / 'cmake')
        for name in ('os/include', 'os/nowin32', 'ctp/ctp2_utils', 'gs/dbgen', 'gs/newdb'):
            src = ROOT / 'ctp2_code' / name
            dst = source / 'ctp2_code' / name
            dst.mkdir(parents=True)
            for path in src.iterdir():
                if path.suffix in ('.h', '.cpp', '.cdb', '.l', '.y'):
                    # Do not allow old generated record/parser headers to mask dependencies.
                    if path.name.endswith('Record.h') or path.name in ('y.tab.h', 'sc.tab.h', 'config.h'):
                        continue
                    shutil.copy2(path, dst / path.name)
        (source / 'tests').mkdir()
        for name in ('native-abi.cpp', 'slic-bytecode.cpp'):
            shutil.copy2(ROOT / 'tests' / name, source / 'tests' / name)
        target = source / 'ctp2_code/gs/slic'
        target.mkdir(parents=True)
        shutil.copy2(ROOT / 'ctp2_code/gs/slic/SlicBytecode.h', target)
        original = digest_tree(source)
        build = base / 'first build'

        def configure(directory):
            subprocess.run(['cmake', '-S', str(source), '-B', str(directory), '-G', 'Ninja',
                            '-DCTP2_BUILD_GAME=OFF', '-DCMAKE_BUILD_TYPE=RelWithDebInfo',
                            '-DCTP2_BYACC=' + str(Path(args.byacc).resolve()),
                            '-DCTP2_FLEX=' + str(Path(args.flex).resolve())], check=True)

        def generate(directory):
            subprocess.run(['cmake', '--build', str(directory), '--target', 'ctp2_database', '--parallel', '4'], check=True)

        def stamps():
            return {p.name: p.stat().st_mtime_ns for p in (build / 'generated/newdb').glob('*.complete')}

        configure(build)
        generate(build)
        generated = build / 'generated/newdb'
        assert len(list(generated.glob('*Record.h'))) == 49
        before = stamps()
        generate(build)
        assert before == stamps(), 'unchanged build regenerated schemas'
        (generated / 'AdvanceRecord.h').unlink()
        generate(build)
        assert (generated / 'AdvanceRecord.h').is_file(), 'missing output was not recovered'
        assert before['unit.cdb.complete'] == stamps()['unit.cdb.complete']
        before = stamps()
        schema = source / 'ctp2_code/gs/newdb/advance.cdb'
        schema.write_bytes(schema.read_bytes() + b'\n')
        generate(build)
        assert before['advance.cdb.complete'] != stamps()['advance.cdb.complete']
        assert before['unit.cdb.complete'] == stamps()['unit.cdb.complete']
        before = stamps()
        generate(build)
        assert before == stamps(), 'unchanged generated contents caused a rebuild loop'
        generator = source / 'ctp2_code/gs/dbgen/Datum.cpp'
        generator.write_bytes(generator.read_bytes() + b'\n')
        generate(build)
        assert all(before[k] != stamps()[k] for k in before), 'generator changes did not rebuild every schema'
        second = base / 'second build'
        configure(second)
        generate(second)
        for path in generated.glob('*Record.*'):
            if path.suffix in ('.h', '.cpp'):
                assert path.read_bytes() == (second / 'generated/newdb' / path.name).read_bytes()
        final = digest_tree(source)
        changed = {name for name in set(original) | set(final) if original.get(name) != final.get(name)}
        assert changed == {'ctp2_code/gs/dbgen/Datum.cpp', 'ctp2_code/gs/newdb/advance.cdb'}, changed
    print('Generation checks passed: parallel clean build, deletion, schema/generator edits, no-op, independent directories, clean source.')


if __name__ == '__main__':
    main()
