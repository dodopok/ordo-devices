#!/usr/bin/env python3
"""Publishes a firmware for the compass to fetch over Wi-Fi.

    tools/release.py build --pt "O que mudou." [--en "..."] [--es "..."]
    tools/release.py keygen

`build` compiles both boards, copies the binaries to site/firmware/<version>/,
and writes site/firmware/manifest.json with each file's size, SHA-256 and,
when a signing key exists, its signature. Commit the result: Vercel serves
site/ at bussola.oficio.app, and a compass that looks there finds the new
version.

`keygen` makes the signing key (an ECDSA P-256 pair). The private half stays
on this computer, outside the repository; the public half is written to
compass/include/ota_public_key.h, to be committed. A firmware built with that
file refuses any update that is not signed with the private half, so whoever
takes over the site still cannot put a firmware on a compass.

The version is the one in platformio.ini (ORDO_FIRMWARE_VERSION): bump it
there first. Needs PlatformIO (`pio`) and, for signing, `openssl`.
"""
import argparse
import base64
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from datetime import date
from pathlib import Path

COMPASS = Path(__file__).resolve().parent.parent
SITE = COMPASS.parent / "site"
FIRMWARE = SITE / "firmware"
BOARDS = ["amoled175c", "amoled175"]
PUBLIC_HEADER = COMPASS / "include" / "ota_public_key.h"
NOTES_LIMIT = 400


def key_path() -> Path:
    return Path(os.environ.get("ORDO_OTA_KEY", Path.home() / ".config" / "ordo" / "firmware-signing.pem"))


def version() -> str:
    text = (COMPASS / "platformio.ini").read_text(encoding="utf-8")
    match = re.search(r'ORDO_FIRMWARE_VERSION=\\"(\d+\.\d+\.\d+)\\"', text)
    if not match:
        sys.exit("release: no ORDO_FIRMWARE_VERSION like 0.4.1 in platformio.ini")
    return match.group(1)


def pio() -> str:
    for candidate in (os.environ.get("PIO"), shutil.which("pio"), shutil.which("platformio")):
        if candidate:
            return candidate
    sys.exit("release: PlatformIO not found (set PIO=/path/to/pio)")


def sign(binary: Path) -> str:
    """ECDSA over the file's SHA-256, DER, base64: what net/ota.cpp verifies."""
    signature = subprocess.run(
        ["openssl", "dgst", "-sha256", "-sign", str(key_path()), str(binary)], check=True, capture_output=True
    ).stdout
    return base64.b64encode(signature).decode("ascii")


def build(args: argparse.Namespace) -> None:
    for language, text in (("pt", args.pt), ("en", args.en), ("es", args.es)):
        if text and len(text) > NOTES_LIMIT:
            sys.exit(f"release: the {language} notes are over {NOTES_LIMIT} characters")
    number = version()
    signing = key_path().exists()
    if PUBLIC_HEADER.exists() and not signing:
        sys.exit(f"release: this firmware demands signed updates, and the key is not at {key_path()}")

    if not args.no_compile:
        subprocess.run([pio(), "run", "-d", str(COMPASS)] + [flag for board in BOARDS for flag in ("-e", board)], check=True)

    # Only the release on offer is kept: the manifest points at nothing else,
    # and git remembers the rest.
    if FIRMWARE.exists():
        for old in FIRMWARE.iterdir():
            if old.is_dir():
                shutil.rmtree(old)
    target = FIRMWARE / number
    target.mkdir(parents=True)

    boards = {}
    for board in BOARDS:
        built = COMPASS / ".pio" / "build" / board / "firmware.bin"
        if not built.exists():
            sys.exit(f"release: {built} is missing; build it first")
        binary = target / f"{board}.bin"
        shutil.copyfile(built, binary)
        data = binary.read_bytes()
        boards[board] = {
            "path": f"/firmware/{number}/{board}.bin",
            "size": len(data),
            "sha256": hashlib.sha256(data).hexdigest(),
            "sig": sign(binary) if signing else "",
        }

    notes = {"pt": args.pt}
    if args.en:
        notes["en"] = args.en
    if args.es:
        notes["es"] = args.es
    manifest = {"v": 1, "version": number, "released": date.today().isoformat(), "notes": notes, "boards": boards}
    (FIRMWARE / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print(f"release {number}: {'signed' if signing else 'not signed'}")
    for board, entry in boards.items():
        print(f"  {board}: {entry['size']} bytes, sha256 {entry['sha256'][:16]}…")
    print(f"commit {FIRMWARE.relative_to(COMPASS.parent)}/ and push: Vercel publishes it.")


def keygen(_: argparse.Namespace) -> None:
    key = key_path()
    if key.exists():
        sys.exit(f"release: a key already exists at {key}; keeping it")
    key.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", str(key)], check=True)
    key.chmod(0o600)
    public = subprocess.run(["openssl", "ec", "-in", str(key), "-pubout"], check=True, capture_output=True).stdout.decode("ascii")
    lines = " \\\n".join(f'  "{line}\\n"' for line in public.strip().splitlines())
    PUBLIC_HEADER.write_text(
        "#pragma once\n\n"
        "// The public half of the key firmware releases are signed with\n"
        "// (tools/release.py keygen). A firmware built with this file installs only\n"
        "// updates carrying a valid signature. The private half is not in this\n"
        "// repository and must never be.\n"
        f"#define ORDO_OTA_PUBLIC_KEY_PEM \\\n{lines}\n",
        encoding="utf-8",
    )
    print(f"private key: {key}  (back it up; without it no update can be signed)")
    print(f"public key:  {PUBLIC_HEADER.relative_to(COMPASS.parent)}  (commit it)")
    print("From the next firmware on, a compass only takes updates signed with this key.")


def main() -> None:
    parser = argparse.ArgumentParser(description="Publishes a firmware for the compass to fetch over Wi-Fi.")
    commands = parser.add_subparsers(dest="command", required=True)
    b = commands.add_parser("build", help="compile, copy to site/firmware/ and write the manifest")
    b.add_argument("--pt", required=True, help="what this version brings, in Portuguese (shown on the compass)")
    b.add_argument("--en", help="the same in English")
    b.add_argument("--es", help="the same in Spanish")
    b.add_argument("--no-compile", action="store_true", help="use the binaries already in .pio/build")
    b.set_defaults(run=build)
    k = commands.add_parser("keygen", help="make the signing key and the public header")
    k.set_defaults(run=keygen)
    args = parser.parse_args()
    args.run(args)


if __name__ == "__main__":
    main()
