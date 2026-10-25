"""Build small harnesses around production C, using the available host compiler."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
COMPILER = shutil.which('cc') or shutil.which('cl')
COMPATIBILITY = r'''
#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#define __attribute__(x)
#include <stdint.h>
#include <intrin.h>
#include <stdlib.h>
static unsigned host_clz(unsigned value) {
    unsigned long bit;
    _BitScanReverse(&bit, value);
    return 31u - bit;
}
#define __builtin_clz host_clz
#define __builtin_bswap16 _byteswap_ushort
#endif
'''


def compile_run(source, defines=()):
    with tempfile.TemporaryDirectory() as directory:
        tmp = Path(directory)
        src = tmp / 'regression.c'
        src.write_text(COMPATIBILITY + '#define main regression_main\n' + source + r'''
#undef main
int main(void) {
#ifdef _MSC_VER
    _set_error_mode(_OUT_TO_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    return regression_main();
}
''', encoding='utf-8')
        exe = tmp / ('regression.exe' if os.name == 'nt' else 'regression')
        includes = [ROOT / 'main/common', ROOT / 'main/boards']
        if Path(COMPILER).name.lower() == 'cl.exe':
            command = [COMPILER, '/nologo', '/std:c11', '/O2',
                       *('/D' + d for d in defines),
                       *('/I' + str(p) for p in includes), str(src), '/Fe:' + str(exe)]
        else:
            command = [COMPILER, '-std=gnu11', '-O2', '-Wall', '-Wextra',
                       '-Wno-unused-parameter', '-Wno-unused-function',
                       *(['-fsanitize=undefined', '-fno-sanitize-recover=undefined']
                         if os.name != 'nt' else []),
                       *('-D' + d for d in defines),
                       *('-I' + str(p) for p in includes), str(src), '-lm', '-o', str(exe)]
        subprocess.run(command, cwd=tmp, check=True, timeout=60)
        subprocess.run([str(exe)], cwd=tmp, check=True, timeout=15)
