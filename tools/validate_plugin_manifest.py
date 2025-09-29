#!/usr/bin/env python3
"""Validate Notepad++ macOS plugin manifest files.

Usage:
    python tools/validate_plugin_manifest.py manifest.json

Checks performed:
    - JSON is well-formed UTF-8
    - Required fields are present (identifier, displayName, version, download, architectures)
    - identifier matches reverse-DNS pattern (letters, digits, dot, hyphen)
    - version is semantic version (major.minor.patch)
    - architectures only contains supported values ("universal2", "x86_64", "arm64")
    - download.url uses HTTPS
    - download.sha256 is 64 hex chars
    - optional signature fields (signature) must be base64-like
"""

from __future__ import annotations

import argparse
import base64
import json
import re
import sys
from pathlib import Path

RE_IDENTIFIER = re.compile(r"^[A-Za-z][A-Za-z0-9\-]*(\.[A-Za-z0-9\-]+)+$")
RE_SEMVER = re.compile(r"^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:[-+].*)?$")
SUPPORTED_ARCHS = {"universal2", "x86_64", "arm64"}


def fail(message: str) -> None:
    print(f"ERROR: {message}")
    sys.exit(1)


def validate_identifier(identifier: str) -> None:
    if not RE_IDENTIFIER.match(identifier):
        fail(f"Invalid identifier '{identifier}'. Use reverse-DNS notation (e.g. org.example.plugin)")


def validate_version(version: str) -> None:
    if not RE_SEMVER.match(version):
        fail(f"Version '{version}' does not match semantic versioning (major.minor.patch)")


def validate_architectures(architectures) -> None:
    if not isinstance(architectures, list) or not architectures:
        fail("architectures must be a non-empty array")
    invalid = [arch for arch in architectures if arch not in SUPPORTED_ARCHS]
    if invalid:
        fail(f"Unsupported architectures {invalid}; supported: {sorted(SUPPORTED_ARCHS)}")


def validate_download(download: dict) -> None:
    if not isinstance(download, dict):
        fail("download must be an object")
    url = download.get("url")
    if not isinstance(url, str) or not url.startswith("https://"):
        fail("download.url must be an HTTPS URL")
    sha256 = download.get("sha256")
    if not isinstance(sha256, str) or len(sha256) != 64 or not all(c in "0123456789abcdef" for c in sha256.lower()):
        fail("download.sha256 must be a 64-character hexadecimal string")


def validate_signature(signature: str) -> None:
    try:
        base64.b64decode(signature, validate=True)
    except Exception as exc:  # noqa: BLE001
        fail(f"Invalid signature encoding: {exc}")


def validate_manifest(manifest: dict) -> None:
    required = {"identifier", "displayName", "version", "architectures", "download"}
    missing = required - manifest.keys()
    if missing:
        fail(f"Missing required fields: {sorted(missing)}")

    validate_identifier(manifest["identifier"])
    if not isinstance(manifest["displayName"], str) or not manifest["displayName"].strip():
        fail("displayName must be a non-empty string")
    validate_version(manifest["version"])
    validate_architectures(manifest["architectures"])
    validate_download(manifest["download"])

    signature = manifest.get("signature")
    if signature is not None:
        if not isinstance(signature, str):
            fail("signature must be a string")
        validate_signature(signature)



def main() -> None:
    parser = argparse.ArgumentParser(description="Validate Notepad++ macOS plugin manifest")
    parser.add_argument("manifest", type=Path, help="Path to manifest JSON file")
    args = parser.parse_args()

    try:
        data = args.manifest.read_text(encoding="utf-8")
    except FileNotFoundError:
        fail(f"Manifest file '{args.manifest}' not found")
    except UnicodeDecodeError as exc:
        fail(f"Manifest not valid UTF-8: {exc}")

    try:
        manifest = json.loads(data)
    except json.JSONDecodeError as exc:
        fail(f"Invalid JSON: {exc}")

    if not isinstance(manifest, dict):
        fail("Manifest root must be an object")

    validate_manifest(manifest)
    print(f"Manifest '{args.manifest}' is valid.")


if __name__ == "__main__":
    main()
