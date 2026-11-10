#!/usr/bin/env python3
"""Reject mixed-architecture packages and regressions to the failing PE header.

This checks packaging, not full Windows loader/API compatibility. Requires the
MinGW objdump used by the build to inspect imported DLLs.
"""
from pathlib import Path
import argparse
import re
import struct
import subprocess


def check_pe(path, executable=False):
    data = Path(path).read_bytes()
    if len(data) < 64 or data[:2] != b'MZ':
        raise ValueError(f'{path}: missing DOS/PE header')
    pe = struct.unpack_from('<I', data, 60)[0]
    if pe + 96 > len(data) or data[pe:pe + 4] != b'PE\0\0':
        raise ValueError(f'{path}: invalid/truncated PE header')
    if struct.unpack_from('<H', data, pe + 4)[0] != 0x14c:
        raise ValueError(f'{path}: expected x86 (0x014c), not x64/another architecture')
    if struct.unpack_from('<H', data, pe + 24)[0] != 0x10b:
        raise ValueError(f'{path}: expected PE32 optional header')
    if executable:
        subsystem_version = struct.unpack_from('<HH', data, pe + 72)
        os_version = struct.unpack_from('<HH', data, pe + 64)
        if subsystem_version != (6, 0) or os_version != (6, 0):
            raise ValueError(f'{path}: retain the working 6.0 loader header; API target stays 8.1')
        if struct.unpack_from('<H', data, pe + 92)[0] != 2:
            raise ValueError(f'{path}: expected a Windows GUI executable')


def check_package(folder, objdump):
    folder = Path(folder)
    required = ['DiscordMessenger.exe', 'libcrypto-4.dll', 'libssl-4.dll', 'libwinpthread-1.dll']
    for name in required:
        if not (folder / name).is_file():
            raise ValueError(f'{folder}: missing {name}')
    images = [folder / required[0]] + sorted(folder.glob('*.dll'))
    local_names = {p.name.lower() for p in images}
    for image in images:
        check_pe(image, image.suffix.lower() == '.exe')
        result = subprocess.run([objdump, '-p', str(image)], check=True,
                                capture_output=True, text=True)
        imports = re.findall(r'DLL Name:\s*(\S+)', result.stdout)
        for name in imports:
            # Compiler/OpenSSL/WebP DLLs must be bundled. System DLLs and API
            # sets are resolved by Windows and are never copied from the builder.
            if name.lower().startswith('lib') and name.lower() not in local_names:
                raise ValueError(f'{image}: missing imported runtime DLL {name}')
    print(f'PASS: {len(images)} x86 images, required runtime DLLs, conservative loader header')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('folder', type=Path)
    parser.add_argument('--objdump', default='i686-w64-mingw32-objdump')
    args = parser.parse_args()
    try:
        check_package(args.folder, args.objdump)
    except (ValueError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f'Package validation failed: {error}\n')
