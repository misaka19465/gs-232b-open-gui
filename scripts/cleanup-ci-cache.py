#!/usr/bin/env python3
"""Keep the latest two generations of each managed Windows/macOS CI cache family."""

import argparse
import collections
import datetime
import json
import subprocess


CACHE_FAMILIES = (
    "gs232b-windows-qt-", "gs232b-windows-build-",
    "gs232b-windows-pe-tools-", "gs232b-windows-venv-",
    "gs232b-macos-qt-", "gs232b-macos-build-", "gs232b-macos-venv-",
)


def obsolete_caches(entries: list[dict], keep: int = 2) -> list[dict]:
    if keep < 1:
        raise ValueError("At least one cache generation must be retained")
    families = collections.defaultdict(list)
    for entry in entries:
        for prefix in CACHE_FAMILIES:
            if entry["key"].startswith(prefix):
                families[prefix].append(entry)
                break
    obsolete = []
    for family in families.values():
        family.sort(key=lambda entry: (
            datetime.datetime.fromisoformat(entry["created_at"].replace("Z", "+00:00")), str(entry["id"])
        ), reverse=True)
        obsolete.extend(family[keep:])
    return obsolete


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repository", required=True)
    parser.add_argument("--keep", type=int, default=2)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()
    result = subprocess.run([
        "gh", "api", "--paginate", "--slurp", f"repos/{args.repository}/actions/caches?per_page=100",
    ], check=True, capture_output=True, text=True, encoding="utf-8")
    entries = [entry for page in json.loads(result.stdout) for entry in page["actions_caches"]]
    obsolete = obsolete_caches(entries, args.keep)
    for entry in obsolete:
        print(f"{'Would delete' if args.dry_run else 'Deleting'} cache {entry['id']}: {entry['key']}")
        if not args.dry_run:
            deletion = subprocess.run([
                "gh", "api", "--method", "DELETE", f"repos/{args.repository}/actions/caches/{entry['id']}",
            ], capture_output=True, text=True, encoding="utf-8")
            # Another workflow or GitHub's own eviction may have removed it in the meantime.
            if deletion.returncode and "HTTP 404" not in deletion.stderr:
                raise RuntimeError(deletion.stderr.strip())
    print(f"Retaining {args.keep} generations per managed family; {len(obsolete)} obsolete cache(s).")


if __name__ == "__main__":
    main()
