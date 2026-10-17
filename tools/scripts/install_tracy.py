#!/usr/bin/env python3
"""Install the pinned Tracy 0.14.1 profiler (tracy-profiler.exe) into tools/bin.

Usage:
  python tools/scripts/install_tracy.py

The upstream release only ships a
Windows binary, so on other systems this exits with "not supported on this OS";
the profiler itself must come from your package manager or a source build of
tools/bin/tracy-0.14.1/profiler. The client sources for building are installed
by install_tracy_client.py on every OS.
"""
import hashlib
import argparse
import os
import urllib.request
import zipfile

VERSION = "0.14.1"
SHA256 = "f7499d74914aa3ba94a2c1ce72f36477d7b61d9d0f7c9790e05274c258c97fb5"
URL = f"https://github.com/wolfpld/tracy/releases/download/v{VERSION}/windows-{VERSION}.zip"


def main():
    argparse.ArgumentParser(description=__doc__.splitlines()[0]).parse_args()
    if os.name != "nt":
        raise SystemExit(
            f"not supported on this OS: the pinned tracy-profiler {VERSION} is only published "
            "as a Windows binary; install it from your package manager or build it from "
            f"tools/bin/tracy-{VERSION}/profiler")
    repo = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    cache = os.path.join(repo, "tools", "cache", "tracy")
    archive = os.path.join(cache, f"windows-{VERSION}.zip")
    install = os.path.join(repo, "tools", "bin", f"tracy-{VERSION}")
    profiler = os.path.join(install, "tracy-profiler.exe")

    if os.path.exists(profiler):
        print(f"Tracy {VERSION} already installed: {profiler}")
        return

    os.makedirs(cache, exist_ok=True)
    urllib.request.urlretrieve(URL, archive)
    with open(archive, "rb") as f:
        actual = hashlib.sha256(f.read()).hexdigest()
    if actual != SHA256:
        os.remove(archive)
        raise SystemExit(f"Tracy archive SHA-256 mismatch: {actual}")

    os.makedirs(install, exist_ok=True)
    with zipfile.ZipFile(archive) as zf:
        zf.extractall(install)
    if not os.path.exists(profiler):
        raise SystemExit(f"Tracy archive did not contain tracy-profiler.exe: {install}")
    print(f"Installed Tracy {VERSION}: {profiler}")


if __name__ == "__main__":
    main()
