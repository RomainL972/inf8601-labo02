#!/usr/bin/env python3
"""Package a self-contained, student-downloadable source tree of the lab.

Nothing from charge/ ships (solutions included), nor build directories or generated images.
The spec (`charge/spec.md`) is folded into the end of the packaged README.md instead of
shipping as its own file.

Usage:
    charge/create-student-archive.py [--source DIR] [--output DIR]
"""

import argparse
import shutil
import sys
import tarfile
import tempfile
from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
DEFAULT_SOURCE = SCRIPT_DIR.parent
DEFAULT_OUTPUT = SCRIPT_DIR / "dist"

ARCHIVE_ROOT_NAME = "inf8601-labo02"

EXCLUDE_TOP_LEVEL_DIRS = {".git", ".plans", ".cache", "charge", "build"}
EXCLUDE_TOP_LEVEL_PREFIXES = ("build-",)

# Files the students must complete: they have to still be the stubs when we package.
STUDENT_FILES = (
    "source/sinoscope-openmp.c",
    "source/sinoscope-opencl.c",
    "source/kernel/sinoscope.cl",
)
STUB_MARKER = "return -1;"


def parse_args():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--source",
        type=Path,
        default=DEFAULT_SOURCE,
        help=f"path to the lab source tree (default: {DEFAULT_SOURCE})",
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=DEFAULT_OUTPUT,
        help=f"directory to write the .tar.gz into (default: {DEFAULT_OUTPUT})",
    )
    return parser.parse_args()


def check_source(source: Path):
    if not source.is_dir():
        sys.exit(f"error: source tree not found: {source}")

    if not (source / "charge" / "spec.md").is_file():
        sys.exit(f"error: missing spec: {source / 'charge' / 'spec.md'}")

    # The solutions live in charge/solution, the files in source/ are the students' stubs. Make
    # sure a solution was not copied there by mistake before shipping it to a whole class.
    for name in STUDENT_FILES[:2]:
        text = (source / name).read_text()
        if STUB_MARKER not in text:
            sys.exit(f"error: {name} does not look like a stub (no `{STUB_MARKER}`)")

    kernel = (source / STUDENT_FILES[2]).read_text()
    if "__kernel" in kernel:
        sys.exit(f"error: {STUDENT_FILES[2]} contains a kernel, it must only be the stub")


def copy_tree(source: Path, staging: Path):
    def ignore(dir_path, names):
        rel = Path(dir_path).relative_to(source)
        ignored = set()

        for name in names:
            if (
                rel == Path(".")
                and (
                    name in EXCLUDE_TOP_LEVEL_DIRS
                    or name.startswith(EXCLUDE_TOP_LEVEL_PREFIXES)
                )
                or name.endswith(".png")
                or name == "__pycache__"
                or name == ".DS_Store"
            ):
                ignored.add(name)

        return ignored

    shutil.copytree(source, staging, ignore=ignore)


def replace_once(text: str, old: str, new: str, what: str) -> str:
    if text.count(old) != 1:
        sys.exit(
            f"error: expected exactly one occurrence of {what} to patch; "
            "source file has changed upstream, update this script"
        )
    return text.replace(old, new, 1)


def patch_readme(staging: Path, source: Path):
    path = staging / "README.md"
    text = path.read_text()

    spec_text = (source / "charge" / "spec.md").read_text()
    _, _, spec = spec_text.partition("\n\n")  # drop the "# ..." heading of spec.md

    text = replace_once(
        text,
        "<!-- SPEC -->",
        spec.strip(),
        "the <!-- SPEC --> placeholder",
    )

    path.write_text(text)


def make_archive(staging: Path, output: Path) -> Path:
    output.mkdir(parents=True, exist_ok=True)
    archive_path = output / f"{ARCHIVE_ROOT_NAME}.tar.gz"

    with tarfile.open(archive_path, "w:gz") as tar:
        tar.add(staging, arcname=ARCHIVE_ROOT_NAME)

    return archive_path


def main():
    args = parse_args()
    check_source(args.source)

    with tempfile.TemporaryDirectory(prefix="student-archive-") as tmp:
        staging = Path(tmp) / ARCHIVE_ROOT_NAME
        copy_tree(args.source, staging)
        patch_readme(staging, args.source)

        archive_path = make_archive(staging, args.output)

    size_kb = archive_path.stat().st_size / 1024
    print(f"wrote {archive_path} ({size_kb:.0f} KiB)")


if __name__ == "__main__":
    main()
