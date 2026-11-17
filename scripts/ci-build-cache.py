#!/usr/bin/env python3
"""Fingerprint compiled intermediates and outputs, ignoring repackaging and mtimes."""

import argparse
import hashlib
import pathlib


COMPILED_SUFFIXES = {
    ".obj", ".o", ".lib", ".a", ".pch", ".gch", ".res", ".pdb", ".ilk",
    ".exe", ".dll", ".dylib", ".so", ".bundle",
}
COMPILED_PATHS = {"gs_232b_open_gui.app/Contents/MacOS/gs_232b_open_gui"}


def build_fingerprint(root: pathlib.Path) -> str:
    digest = hashlib.sha256()
    files = sorted(
        path for path in root.rglob("*") if path.is_file()
        and path.relative_to(root).parts[0] != "package"
        and (path.suffix.lower() in COMPILED_SUFFIXES
             or path.relative_to(root).as_posix() in COMPILED_PATHS)
    )
    for path in files:
        with path.open("rb") as artifact:
            content = hashlib.file_digest(artifact, "sha256").hexdigest()
        digest.update(f"{path.relative_to(root).as_posix()}\0{content}\n".encode("utf-8"))
    return digest.hexdigest()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=pathlib.Path)
    print(build_fingerprint(parser.parse_args().directory))
