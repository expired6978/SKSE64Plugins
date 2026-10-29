"""Compile exact table source with narrow test stubs; never load a game/plugin."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_VCVARS = Path(r'C:/Program Files/Microsoft Visual Studio/18/Community/VC/Auxiliary/Build/vcvars64.bat')


def pin(path):
    return {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest(), 'bytes': path.stat().st_size}


def msvc_environment(vcvars, output):
    batch = output / 'msvc-environment.cmd'
    batch.write_text('@echo off\ncall "' + str(vcvars) + '" >nul\nif errorlevel 1 exit /b 1\nset\n', encoding='utf-8')
    result = subprocess.run(['cmd.exe', '/d', '/c', str(batch)],
                            capture_output=True, text=True, check=True)
    env = {key.upper(): value for key, value in os.environ.items()}
    for line in result.stdout.splitlines():
        if '=' in line and not line.startswith('='):
            key, value = line.split('=', 1)
            env[key.upper()] = value
    return env


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--vcvars', type=Path, default=DEFAULT_VCVARS)
    parser.add_argument('--baseline', default='eb39bdd185f0ee5b3154b2ae1910d2465b2627ed')
    args = parser.parse_args()
    output = args.output.resolve()
    if (output.exists() or output.is_relative_to(ROOT) or
            any((p / '.git').exists() for p in (output, *output.parents))):
        raise ValueError('Use a fresh output directory outside Git')
    output.mkdir(parents=True)
    env = msvc_environment(args.vcvars, output)
    env['ASAN_OPTIONS'] = 'halt_on_error=1:detect_leaks=0:alloc_dealloc_mismatch=0'
    results = []
    source_pins = []
    for variant in ('baseline', 'fixed'):
        folder = output / variant
        folder.mkdir()
        stubs = ROOT / 'tests/string-table-lifetime/stubs'
        shutil.copytree(stubs, folder, dirs_exist_ok=True)
        shutil.copyfile(ROOT / 'tests/string-table-lifetime/lifetime.cpp', folder / 'lifetime.cpp')
        for name in ('StringTable.cpp', 'StringTable.h'):
            target = folder / name
            if variant == 'baseline':
                raw = subprocess.check_output(['git', '-C', str(ROOT), 'show', args.baseline + ':skee64/' + name])
            else:
                raw = (ROOT / 'skee64' / name).read_bytes()
            target.write_bytes(raw)
            source_pins.append(dict(variant=variant, **pin(target)))
        compiler = shutil.which('cl.exe', path=env['PATH'])
        if not compiler:
            raise RuntimeError('MSVC setup did not provide cl.exe')
        command = [compiler, '/nologo', '/std:c++20', '/EHsc', '/MD', '/Od', '/Zi', '/fsanitize=address',
                   '/D_ITERATOR_DEBUG_LEVEL=0', '/I' + str(folder), 'StringTable.cpp', 'lifetime.cpp',
                   '/Fe:lifetime.exe', '/link', '/INCREMENTAL:NO']
        built = subprocess.run(command, cwd=folder, env=env, capture_output=True, text=True)
        (folder / 'build.log').write_text(built.stdout + built.stderr, encoding='utf-8')
        if built.returncode:
            raise RuntimeError(variant + ' compilation failed; inspect ' + str(folder / 'build.log'))
        for mode in (('teardown',) if variant == 'baseline' else ('teardown', 'operations', 'parallel')):
            run = subprocess.run([str(folder / 'lifetime.exe'), mode], cwd=folder, env=env,
                                 capture_output=True, text=True, timeout=60)
            log = folder / (mode + '.log')
            log.write_text(run.stdout + run.stderr, encoding='utf-8')
            if variant == 'baseline':
                accepted = (run.returncode != 0 and 'AddressSanitizer: use-after-poison' in run.stderr and
                            'StringTable::RemoveString' in run.stderr and 'DeleteStringEntry' in run.stderr)
            else:
                accepted = run.returncode == 0 and 'all tracked storage freed' in run.stdout
            results.append({'variant': variant, 'mode': mode, 'returnCode': run.returncode,
                            'expectedFailure': variant == 'baseline', 'passed': accepted, 'log': pin(log)})
    report = {'schemaVersion': 1, 'kind': 'offline-exact-source-string-table-lifetime',
              'baselineCommit': args.baseline, 'gameStarted': False, 'pluginLoaded': False,
              'liveInstallationChanged': False, 'method': 'byte-identical production table sources; narrow engine API stubs; real CRT teardown with ASan poison of already-destructed facade; tracked construction allocation reclamation',
              'compiler': pin(Path(compiler)), 'vcvars': pin(args.vcvars),
              'msvcToolsVersion': env.get('VCTOOLSVERSION'), 'windowsSdkVersion': env.get('WINDOWSSDKVERSION'),
              'sourcePins': source_pins, 'results': results, 'passed': all(row['passed'] for row in results)}
    (output / 'report.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(report, indent=2))
    return 0 if report['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
