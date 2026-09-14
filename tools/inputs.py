"""Import unchanged compiler files from a qualified installation or updater.

The InstallShield layout and block decoder follow ISx and Unshield; see
docs/third-party.md. Only checksum-qualified updater inputs are parsed.
"""

from __future__ import annotations

import hashlib
from pathlib import Path
import struct
import zipfile
import zlib


class InputError(Exception):
    pass


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def string_at(data: bytes, offset: int) -> tuple[bytes, int]:
    end = data.index(b"\0", offset)
    return data[offset:end], end + 1


def installer_members(data: bytes) -> dict[str, bytes]:
    pe = u32(data, 0x3C)
    if data[:2] != b"MZ" or data[pe:pe + 4] != b"PE\0\0":
        raise InputError("Invalid updater executable header.")
    table = pe + 24 + u16(data, pe + 20)
    offset = max(u32(data, table + 40 * i + 16) + u32(data, table + 40 * i + 20)
                 for i in range(u16(data, pe + 6)))
    required = {"data1.cab", "data1.hdr", "data2.cab"}
    result = {}
    while offset < len(data):
        fields = []
        for _ in range(4):
            field, offset = string_at(data, offset)
            fields.append(field)
        size = int(fields[3])
        if size < 0 or offset + size > len(data):
            raise InputError("Updater member exceeds the input bounds.")
        name = fields[0].decode("ascii")
        if name in required:
            if name in result:
                raise InputError("Duplicate updater cabinet.")
            result[name] = data[offset:offset + size]
        offset += size
        if required <= result.keys():
            return result
    raise InputError("Updater is missing its cabinets.")


def decompress_blocks(data: bytes, expanded_size: int) -> bytes:
    output = bytearray()
    offset = 0
    while offset < len(data):
        if offset + 2 > len(data):
            raise InputError("Truncated compressed block header.")
        size = u16(data, offset)
        offset += 2
        if size == 0 or offset + size > len(data):
            raise InputError("Invalid compressed block bounds.")
        decoder = zlib.decompressobj(-15)
        block = decoder.decompress(data[offset:offset + size] + b"\0",
                                   expanded_size - len(output) + 1)
        if not decoder.eof or len(output) + len(block) > expanded_size:
            raise InputError("Compressed block exceeds its declared size.")
        output.extend(block)
        offset += size
    if len(output) != expanded_size:
        raise InputError("Decompressed file has the wrong size.")
    return bytes(output)


def compiler_members(data: bytes, expected: dict[str, str]) -> dict[str, bytes]:
    cabinets = installer_members(data)
    header = cabinets["data1.hdr"]
    if header[:4] != b"ISc(":
        raise InputError("Invalid InstallShield cabinet header.")
    base = u32(header, 12)
    strings = base + u32(header, base + 12)
    records = strings + u32(header, base + 44)
    canonical = {name.casefold(): name for name in expected}
    output = {}
    for index in range(u32(header, base + 40)):
        descriptor = records + index * 87
        flags = u16(header, descriptor)
        expanded, compressed, offset = struct.unpack_from("<QQQ", header, descriptor + 2)
        if flags & 8 or not offset:
            continue
        name, _ = string_at(header, strings + u32(header, descriptor + 58))
        directory_index = u16(header, descriptor + 62)
        directory, _ = string_at(header, strings + u32(header, strings + directory_index * 4))
        # Qualified tools have ASCII names. Other installer content is irrelevant.
        try:
            leaf = directory.decode("ascii").replace("\\", "/").rsplit("/", 1)[-1]
            candidate = canonical.get((leaf + "/" + name.decode("ascii")).casefold())
        except UnicodeDecodeError:
            continue
        if candidate is None or candidate in output:
            continue
        if flags & 3:
            raise InputError("A compiler file uses an unsupported cabinet encoding.")
        volume = u16(header, descriptor + 85)
        cabinet = cabinets[f"data{volume}.cab"]
        if offset + compressed > len(cabinet):
            raise InputError("Compiler file exceeds its cabinet bounds.")
        stored = cabinet[offset:offset + compressed]
        decoded = decompress_blocks(stored, expanded) if flags & 4 else stored
        if sha256(decoded) == expected[candidate]:
            output[candidate] = decoded
    missing = sorted(expected.keys() - output.keys())
    if missing:
        raise InputError("Updater lacks qualified files: " + ", ".join(missing))
    return output


def installation_files(directory: Path, expected: dict[str, str]) -> dict[str, Path]:
    """Accept the suite root, its bin folder, or an enclosing HEW installation."""
    roots = [directory, directory.parent] if directory.name.casefold() == "bin" else [directory]
    roots.extend(p.parent.parent for p in directory.rglob("*")
                 if p.is_file() and p.name.casefold() == "ch38.exe")
    best_missing = list(expected)
    for root in dict.fromkeys(roots):
        # Limit candidate matching to the two suite directories.
        candidates = {p.relative_to(root).as_posix().casefold(): p
                      for child in root.iterdir() if child.is_dir() and child.name.casefold() in {"bin", "include"}
                      for p in child.rglob("*") if p.is_file()}
        found = {}
        for name, checksum in expected.items():
            path = candidates.get(name.casefold())
            if path is not None and sha256(path.read_bytes()) == checksum:
                found[name] = path
        if len(found) == len(expected):
            return found
        missing = sorted(expected.keys() - found.keys())
        if len(missing) < len(best_missing):
            best_missing = missing
    raise InputError("Installation is incomplete or unqualified; missing or different: " +
                     ", ".join(best_missing[:6]))


def import_compiler(source: Path, inventories: dict, destination: Path) -> str:
    """Write to an isolated staging directory; the caller commits a valid setup."""
    if source.is_file() and source.name.casefold() == "ch38.exe":
        source = source.parent
    if source.is_dir():
        errors = []
        for version, suite in inventories.items():
            try:
                files = installation_files(source, suite["files"])
            except InputError as error:
                errors.append(f"{version}: {error}")
                continue
            contents = {name: path.read_bytes() for name, path in files.items()}
            break
        else:
            raise InputError("\n".join(errors))
    else:
        if source.stat().st_size > 256 * 1024 * 1024:
            raise InputError("Input is larger than any qualified updater package.")
        data = source.read_bytes()
        checksum = sha256(data)
        for version, suite in inventories.items():
            if checksum == suite["package_sha256"]:
                break
            archive = suite.get("archive")
            if archive and checksum == archive["sha256"]:
                with zipfile.ZipFile(source) as bundle:
                    data = bundle.read(archive["member"])
                if sha256(data) != suite["package_sha256"]:
                    raise InputError("Updater inside the ZIP has the wrong checksum.")
                break
        else:
            raise InputError("Unknown compiler package; see docs/build.md for qualified input checksums.")
        try:
            contents = compiler_members(data, suite["files"])
        except (struct.error, IndexError, KeyError, ValueError, zlib.error) as error:
            raise InputError("Malformed compiler updater content.") from error
    for name, data in contents.items():
        if sha256(data) != suite["files"][name]:
            raise InputError("Compiler input changed during setup; retry.")
        output = destination / version / name
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_bytes(data)
    return version
