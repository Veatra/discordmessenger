"""Negative regression cases for the Windows package's PE checks."""
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec = importlib.util.spec_from_file_location(
    'package_check', Path(__file__).resolve().parents[1] / 'tools/check_windows_package.py')
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)


class PackageTests(unittest.TestCase):
    def test_headers(self):
        image = bytearray(512)
        image[:2] = b'MZ'
        struct.pack_into('<I', image, 60, 128)
        image[128:132] = b'PE\0\0'
        struct.pack_into('<H', image, 132, 0x14c)
        struct.pack_into('<H', image, 152, 0x10b)
        struct.pack_into('<HH', image, 192, 6, 0)
        struct.pack_into('<HH', image, 200, 6, 0)
        struct.pack_into('<H', image, 220, 2)
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / 'example.exe'
            path.write_bytes(image)
            checker.check_pe(path, executable=True)
            # Accidentally pairing x64 DLLs with the x86 executable must fail.
            struct.pack_into('<H', image, 132, 0x8664)
            path.write_bytes(image)
            with self.assertRaisesRegex(ValueError, 'expected x86'):
                checker.check_pe(path)
            struct.pack_into('<H', image, 132, 0x14c)
            struct.pack_into('<HH', image, 200, 6, 3)
            path.write_bytes(image)
            with self.assertRaisesRegex(ValueError, '6.0 loader'):
                checker.check_pe(path, executable=True)
            path.write_bytes(b'bad download')
            with self.assertRaises(ValueError):
                checker.check_pe(path)


if __name__ == '__main__':
    unittest.main()
