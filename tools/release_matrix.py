#!/usr/bin/env python3
"""Build a GitHub Actions matrix from release-branches.txt."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path


LABEL_RE = re.compile(r"^[a-z0-9][a-z0-9-]*$")


def resolve_origin(branch: str) -> str:
    result = subprocess.run(
        ["git", "ls-remote", "--exit-code", "origin", f"refs/heads/{branch}"],
        check=True,
        capture_output=True,
        text=True,
    )
    return result.stdout.split()[0]


def load_entries(path: Path, resolve: bool) -> list[dict[str, str]]:
    entries: list[dict[str, str]] = []
    labels: set[str] = set()
    branches: set[str] = set()

    for line_number, raw_line in enumerate(path.read_text().splitlines(), 1):
        line = raw_line.strip()
        if not line or line.startswith("#"):
            continue

        fields = line.split()
        if len(fields) != 2:
            raise ValueError(f"{path}:{line_number}: expected LABEL BRANCH")
        label, branch = fields
        if not LABEL_RE.fullmatch(label):
            raise ValueError(
                f"{path}:{line_number}: invalid asset label {label!r}; "
                "use lowercase letters, digits, and hyphens"
            )
        if label in labels:
            raise ValueError(f"{path}:{line_number}: duplicate label {label!r}")
        if branch in branches:
            raise ValueError(f"{path}:{line_number}: duplicate branch {branch!r}")

        entry = {"label": label, "branch": branch}
        if resolve:
            entry["sha"] = resolve_origin(branch)
        entries.append(entry)
        labels.add(label)
        branches.add(branch)

    if not entries:
        raise ValueError(f"{path}: no release branches configured")
    return entries


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("path", type=Path)
    parser.add_argument(
        "--resolve-origin",
        action="store_true",
        help="resolve each configured origin branch to an immutable commit SHA",
    )
    args = parser.parse_args()
    print(
        json.dumps(
            {"include": load_entries(args.path, args.resolve_origin)},
            separators=(",", ":"),
        )
    )


if __name__ == "__main__":
    main()
