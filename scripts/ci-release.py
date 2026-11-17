#!/usr/bin/env python3
"""Prepare a versioned copy of a CI artifact; publish only with --publish."""

import argparse
import hashlib
import json
import pathlib
import re
import shutil
import subprocess


def read_metadata(project: pathlib.Path) -> tuple[str, str]:
    cmake = (project / "CMakeLists.txt").read_text(encoding="utf-8")
    match = re.search(r"project\(\s*gs_232b_open_gui\s+VERSION\s+(\d+\.\d+\.\d+)\b", cmake)
    if not match:
        raise ValueError("Cannot read the application version from CMakeLists.txt")
    version = match.group(1)
    changelog = (project / "CHANGELOG.md").read_text(encoding="utf-8")
    sections = re.findall(
        rf"^## \[{re.escape(version)}\][^\n]*\n(.*?)(?=^## |\Z)",
        changelog, flags=re.MULTILINE | re.DOTALL,
    )
    if len(sections) != 1 or not sections[0].strip():
        raise ValueError(f"CHANGELOG.md must have one nonempty section for [{version}]")
    return version, sections[0].strip()


def prepare_release(
    project: pathlib.Path, archive: pathlib.Path, output: pathlib.Path,
    asset_suffix: str = "windows-x86_64",
) -> dict:
    if not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", asset_suffix):
        raise ValueError("Asset suffix must contain lowercase letters, digits, and hyphens")
    version, notes = read_metadata(project)
    if not archive.is_file():
        raise ValueError(f"Missing successful build artifact: {archive}")
    output.mkdir(parents=True, exist_ok=True)
    target = output / f"gs-232b-open-gui-{version}-{asset_suffix}.zip"
    shutil.copy2(archive, target)
    checksum = target.with_suffix(".zip.sha256")
    with target.open("rb") as artifact:
        digest = hashlib.file_digest(artifact, "sha256").hexdigest()
    checksum.write_text(f"{digest}  {target.name}\n", encoding="utf-8", newline="\n")
    notes_file = output / "release-notes.md"
    notes_file.write_text(notes + "\n", encoding="utf-8", newline="\n")
    return {"tag": f"v{version}", "archive": target, "checksum": checksum, "notes": notes_file}


def github_resource(repository: str, resource: str):
    result = subprocess.run(
        ["gh", "api", f"repos/{repository}/{resource}"],
        capture_output=True, text=True, encoding="utf-8",
    )
    if result.returncode:
        if "HTTP 404" in result.stderr:
            return None
        raise RuntimeError(result.stderr.strip())
    return json.loads(result.stdout)


def publish_release(
    repository: str, commit: str, files: dict, allow_existing_release_assets: bool = False,
) -> None:
    if not repository or not re.fullmatch(r"[0-9a-f]{40}", commit):
        raise ValueError("Publication requires --repository and a full --commit SHA")
    tag = files["tag"]
    existing_release = github_resource(repository, f"releases/tags/{tag}")
    if existing_release is not None and not allow_existing_release_assets:
        raise ValueError(f"Release {tag} already exists; increment the version instead of overwriting it")
    existing_tag = github_resource(repository, f"git/ref/tags/{tag}")
    tag_commit = None
    if existing_tag:
        target = existing_tag["object"]
        while target["type"] == "tag":
            target = github_resource(repository, f"git/tags/{target['sha']}")["object"]
        if target["type"] == "commit":
            tag_commit = target["sha"]
    if existing_release:
        if tag_commit != commit:
            raise ValueError(f"Release {tag} does not point to the built commit")
        subprocess.run([
            "gh", "release", "upload", tag, str(files["archive"]), str(files["checksum"]),
            "--repo", repository,
        ], check=True)
    else:
        if existing_tag and tag_commit != commit:
            raise ValueError(f"Tag {tag} points to another commit")
        subprocess.run([
            "gh", "release", "create", tag, str(files["archive"]), str(files["checksum"]),
            "--repo", repository, "--target", commit, "--title", tag,
            "--notes-file", str(files["notes"]),
        ], check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--project", type=pathlib.Path, default=pathlib.Path(__file__).resolve().parents[1])
    parser.add_argument("--archive", type=pathlib.Path, required=True)
    parser.add_argument("--output-dir", type=pathlib.Path, required=True)
    parser.add_argument("--asset-suffix", default="windows-x86_64")
    parser.add_argument("--publish", action="store_true")
    parser.add_argument("--allow-existing-release-assets", action="store_true")
    parser.add_argument("--repository", default="")
    parser.add_argument("--commit", default="")
    args = parser.parse_args()
    files = prepare_release(args.project, args.archive, args.output_dir, args.asset_suffix)
    print(f"Prepared {files['tag']}: {files['archive']}")
    if args.publish:
        publish_release(args.repository, args.commit, files, args.allow_existing_release_assets)


if __name__ == "__main__":
    main()
