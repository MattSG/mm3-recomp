#!/usr/bin/env python3
"""Download and unpack the pinned Tracy 0.14.1 client sources into tools/bin.

Usage:
  python tools/scripts/install_tracy_client.py

Does nothing when
tools/bin/tracy-0.14.1/public/TracyClient.cpp already exists. Works on any OS;
the archive is verified against a pinned SHA-256 before extraction.
"""
import hashlib
import argparse
import os
import tarfile
import urllib.request

VERSION = "0.14.1"
EXPECTED_SHA256 = "bf4af567e9c7524d07f3caa745fad02fb33bd5694f11910750382d1efbb251c1"
URL = f"https://codeload.github.com/wolfpld/tracy/tar.gz/refs/tags/v{VERSION}"


def main():
    argparse.ArgumentParser(description=__doc__.splitlines()[0]).parse_args()
    repo =os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    install = os.path.join(repo, "tools", "bin", f"tracy-{VERSION}")
    if os.path.isfile(os.path.join(install, "public", "TracyClient.cpp")):
        return
    cache = os.path.join(repo, "tools", "cache", "tracy")
    bin_dir = os.path.join(repo, "tools", "bin")
    os.makedirs(cache, exist_ok=True)
    os.makedirs(bin_dir, exist_ok=True)
    archive = os.path.join(cache, f"tracy-source-{VERSION}.tar.gz")
    urllib.request.urlretrieve(URL, archive)
    with open(archive, "rb") as f:
        digest = hashlib.sha256(f.read()).hexdigest()
    if digest != EXPECTED_SHA256:
        raise SystemExit("Tracy client archive hash mismatch")
    with tarfile.open(archive, "r:gz") as tar:
        if hasattr(tarfile, "data_filter"):
            tar.extractall(bin_dir, filter="data")
        else:
            tar.extractall(bin_dir)
    if not os.path.isfile(os.path.join(install, "public", "TracyClient.cpp")):
        raise SystemExit("Tracy client extraction failed")


if __name__ == "__main__":
    main()
