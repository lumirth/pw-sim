"""Utility contracts, using authored data rather than firmware or compiler fixtures."""

import hashlib
import io
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch
import zlib

from tools.common import artwork, compare, digest, validate_rom, write_bytes
from tools.host import download
from tools.inputs import InputError, decompress_blocks, import_compiler


class UtilityTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def test_rom_identity_includes_length_and_content(self):
        data = b"authored firmware fixture"
        path = self.root / "rom.bin"
        path.write_bytes(data)
        target = {"size": len(data), "sha256": hashlib.sha256(data).hexdigest()}
        self.assertEqual(validate_rom(path, target), data)
        for wrong in (data[:-1], data + b"\0", b"x" + data[1:]):
            path.write_bytes(wrong)
            with self.assertRaises(InputError):
                validate_rom(path, target)

    def test_comparison_rejects_truncation_extension_and_moved_bytes(self):
        data = bytes(range(32))
        compare(data, data)
        for wrong in (data[:-1], data + b"\0", data[1:] + data[:1]):
            with self.assertRaisesRegex(InputError, "DIFFERS"):
                compare(wrong, data)

    def test_artwork_extracts_only_the_selected_range(self):
        data = bytes(range(16))
        output = artwork(data, [{"offset": 12, "size": 4, "name": "g_fixture"}])
        self.assertIn(b"const u8 g_fixture[4]", output)
        self.assertIn(b"0x0C, 0x0D, 0x0E, 0x0F", output)
        self.assertNotIn(b"0x0B", output)
        for offset, size in ((-1, 2), (12, 5), (16, 1), (0, 0)):
            with self.assertRaises(InputError):
                artwork(data, [{"offset": offset, "size": size, "name": "g_fixture"}])

    def test_import_accepts_bin_and_enclosing_installation_paths(self):
        suite = self.root / "HEW/H8/BIN"
        suite.mkdir(parents=True)
        (suite / "CH38.EXE").write_bytes(b"authored compiler fixture")
        (suite.parent / "INCLUDE").mkdir()
        (suite.parent / "INCLUDE/TYPES.H").write_bytes(b"authored header fixture")
        files = {"bin/ch38.exe": digest(suite / "CH38.EXE"),
                 "include/types.h": digest(suite.parent / "INCLUDE/TYPES.H")}
        inventories = {"fixture": {"files": files}}
        for index, source in enumerate((suite, suite.parent, self.root / "HEW", suite / "CH38.EXE")):
            destination = self.root / f"import-{index}"
            self.assertEqual(import_compiler(source, inventories, destination), "fixture")
            self.assertEqual(digest(destination / "fixture/bin/ch38.exe"), files["bin/ch38.exe"])
        (suite / "CH38.EXE").write_bytes(b"different compiler")
        rejected = self.root / "rejected"
        with self.assertRaises(InputError):
            import_compiler(suite, inventories, rejected)
        self.assertFalse(rejected.exists())

    def test_unknown_installer_is_rejected_before_extraction(self):
        source = self.root / "unknown.exe"
        source.write_bytes(b"not an installer")
        with self.assertRaisesRegex(InputError, "Unknown compiler package"):
            import_compiler(source, {"fixture": {"package_sha256": "0" * 64}}, self.root / "out")
        self.assertFalse((self.root / "out").exists())

    def test_installshield_blocks_require_exact_expanded_size(self):
        data = b"authored compressed fixture" * 30
        chunks = []
        for block in (data[:100], data[100:]):
            encoder = zlib.compressobj(wbits=-15)
            compressed = encoder.compress(block) + encoder.flush()
            chunks.append(len(compressed).to_bytes(2, "little") + compressed)
        stored = b"".join(chunks)
        self.assertEqual(decompress_blocks(stored, len(data)), data)
        for bad, size in ((stored, len(data) - 1), (stored, len(data) + 1),
                          (stored[:-2], len(data)), (b"\0\0", 1), (b"\1", 1)):
            with self.assertRaises(InputError):
                decompress_blocks(bad, size)

    def test_atomic_file_replacement(self):
        path = self.root / "with spaces/data.bin"
        write_bytes(path, b"first")
        write_bytes(path, b"second")
        self.assertEqual(path.read_bytes(), b"second")
        self.assertEqual(list(path.parent.iterdir()), [path])

    def test_download_hash_failure_preserves_existing_tool(self):
        path = self.root / "wibo"
        path.write_bytes(b"existing")
        with patch("tools.host.urllib.request.urlopen", return_value=io.BytesIO(b"wrong")):
            with self.assertRaises(InputError):
                download("https://example.invalid/tool", "0" * 64, path)
        self.assertEqual(path.read_bytes(), b"existing")
        self.assertEqual(list(self.root.iterdir()), [path])
        data = b"authored tool fixture"
        with patch("tools.host.urllib.request.urlopen", return_value=io.BytesIO(data)):
            download("https://example.invalid/tool", hashlib.sha256(data).hexdigest(), path)
        self.assertEqual(path.read_bytes(), data)


if __name__ == "__main__":
    unittest.main()
