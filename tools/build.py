"""Ninja's H8 producer steps; configure.py is the user-facing setup interface."""

from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time

from . import host
from .common import (ROOT, artwork, compare, digest, read_json, source_inventory,
                     validate_rom, validate_toolchain, windows_path, write_bytes, write_json)
from .inputs import InputError


def invoke(stage: Path, tool: str, arguments: list[str], cwd: Path,
           launcher: dict, output: Path, stem: str) -> dict:
    temp = stage / "tmp"
    temp.mkdir()
    env = dict(os.environ)
    env["PATH"] = str(stage / "compiler/bin") + os.pathsep + env.get("PATH", "")
    # A trailing separator is required by this driver's include-path parser.
    env["CH38"] = windows_path(stage / "compiler/include") + ";"
    env["CH38TMP"] = (os.path.relpath(temp, cwd).replace("/", "\\")
                      if tool == "ch38.exe" else windows_path(temp))
    for name in ("TMP", "TEMP", "TMPDIR"):
        env[name] = str(temp)
    command = [*host.command(launcher), str(stage / "compiler/bin" / tool), *arguments]
    start = time.monotonic()
    result = subprocess.run(command, cwd=cwd, env=env, capture_output=True,
                            timeout=600, text=True, errors="replace")
    text = result.stdout + result.stderr
    write_bytes(output / f"{stem}.log", text.encode("utf-8"))
    call = {"tool": tool, "arguments": arguments, "exit_code": result.returncode,
            "seconds": round(time.monotonic() - start, 3), "log": f"{stem}.log"}
    if result.returncode or re.search(r"\b[ACFL]\d{4}\s*\((?:E|F)\)", text):
        raise InputError(f"{tool} failed; see {output / (stem + '.log')}\n{text[-4000:]}")
    return call


def step(version: str, action: str, index: int | None) -> None:
    settings = read_json(ROOT / ".local/config.json")
    if settings["toolchain"] != version:
        raise InputError("Build graph and selected compiler disagree; rerun configure.py.")
    config = read_json(ROOT / "config/build.json")
    output = ROOT / "build" / version
    if action in {"artwork", "verify"}:
        rom = validate_rom(ROOT / ".local/retail.bin", config["target"])
        if action == "artwork":
            write_bytes(output / "rom_assets.h", artwork(rom, read_json(ROOT / "config/artwork.json")))
        else:
            (output / "verified.json").unlink(missing_ok=True)
            image = (output / "pw.bin").read_bytes()
            compare(image, rom)
            write_json(output / "verified.json", {"bytes": len(rom), "sha256": hashlib.sha256(image).hexdigest()})
            print(f"IDENTICAL: {len(rom):,} / {len(rom):,} bytes")
        return

    inventory = read_json(ROOT / "config/toolchains.json")[version]["files"]
    suite = ROOT / ".local/toolchains" / version
    validate_toolchain(suite, inventory)
    before = source_inventory()
    with tempfile.TemporaryDirectory(prefix="pw-", dir=settings["work_dir"]) as temporary:
        stage = Path(temporary)
        shutil.copytree(suite, stage / "compiler")
        (stage / "out").mkdir()
        if action == "runtime":
            stem, tool = "runtime", "lbg38.exe"
            arguments = [f"-output={windows_path(stage / 'out/runtime.lib')}",
                         "-head=Runtime", "-cpu=300HN"]
            cwd, results = stage / "compiler/bin", ["runtime.lib"]
        elif action == "compile":
            source = config["sources"][index]
            stem, tool = f"{index:02d}", "ch38.exe"
            for name in ("src", "include"):
                shutil.copytree(ROOT / name, stage / name)
            if source["file"] == "graphics.c":
                (stage / "generated").mkdir()
                shutil.copyfile(output / "rom_assets.h", stage / "generated/rom_assets.h")
            includes = [*config["include_dirs"], "compiler/include"]
            arguments = [*config["compiler_flags"], "-include=" + ",".join(includes),
                         *source["flags"], f"-object={windows_path(stage / 'out' / (stem + '.obj'))}",
                         windows_path(stage / "src" / source["file"])]
            cwd, results = stage, [stem + ".obj"]
        else:
            stem, tool = "link", "optlnk.exe"
            objects = [f"{i:02d}.obj" for i in range(len(config["sources"]))]
            for name in [*objects, "runtime.lib"]:
                shutil.copyfile(output / name, stage / "out" / name)
            lines = [*(f"input out\\{name}" for name in objects), "library out\\runtime.lib",
                     "form binary", *config["linker_options"], "output out\\pw.bin=0-BFFF",
                     "space FF", "show symbol", "list out\\pw.map", "exit", ""]
            (stage / "out/link.sub").write_bytes("\r\n".join(lines).encode("ascii"))
            arguments = ["-subcommand=out\\link.sub"]
            cwd, results = stage, ["pw.bin", "pw.map", "link.sub"]
        call = invoke(stage, tool, arguments, cwd, settings["launcher"], output, stem)
        if source_inventory() != before:
            raise InputError("Source files changed while the producer ran; repeat the build.")
        host.command(settings["launcher"])
        for name in results:
            path = stage / "out" / name
            if not path.is_file():
                raise InputError(f"{tool} did not create {name}. See {output / (stem + '.log')}.")
            write_bytes(output / name, path.read_bytes())
        write_json(output / f"{stem}.json", call)
        if action == "link":
            calls = [read_json(output / "runtime.json")]
            calls.extend(read_json(output / f"{i:02d}.json") for i in range(len(config["sources"])))
            calls.append(call)
            write_json(output / "build.json", {
                "toolchain": version, "host": platform.platform(), "architecture": platform.machine(),
                "launcher": settings["launcher"], "source_files": before,
                "tool_files": inventory, "calls": calls, "image_sha256": digest(output / "pw.bin"),
            })


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("version", choices=["6.02.01", "6.02.02"])
    parser.add_argument("action", choices=["artwork", "runtime", "compile", "link", "verify"])
    parser.add_argument("index", type=int, nargs="?")
    args = parser.parse_args()
    try:
        step(args.version, args.action, args.index)
    except (InputError, OSError, ValueError, subprocess.SubprocessError) as error:
        print(f"Error: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
