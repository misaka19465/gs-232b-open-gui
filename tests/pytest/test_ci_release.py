"""Release preparation and publication guards, without publishing to GitHub."""

import hashlib
import contextlib
import importlib.util
import io
import pathlib
import tempfile
import unittest
import zipfile
from unittest.mock import patch


spec = importlib.util.spec_from_file_location(
    "ci_release", pathlib.Path(__file__).resolve().parents[2] / "scripts/ci-release.py"
)
release = importlib.util.module_from_spec(spec)
spec.loader.exec_module(release)


class ReleaseTests(unittest.TestCase):
    def test_current_version_selects_its_notes_not_unreleased_or_older_notes(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            (root / "CMakeLists.txt").write_text(
                "project(gs_232b_open_gui VERSION 0.2.1 LANGUAGES C CXX)\n", encoding="utf-8"
            )
            (root / "CHANGELOG.md").write_text(
                "# Changelog\n\n## [Unreleased]\n\nFuture\n\n"
                "## [0.2.1] - 2026-11-17\n\n- 中文更新。\n\n## [0.2.0]\n\nOlder\n",
                encoding="utf-8",
            )
            version, notes = release.read_metadata(root)
            self.assertEqual(version, "0.2.1")
            self.assertEqual(notes, "- 中文更新。")

    def test_missing_or_empty_version_notes_reject_release(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            (root / "CMakeLists.txt").write_text(
                "project(gs_232b_open_gui VERSION 0.2.1 LANGUAGES C CXX)\n", encoding="utf-8"
            )
            for changelog in ("## [0.2.0]\nOld\n", "## [0.2.1]\n\n## [0.2.0]\nOld\n"):
                with self.subTest(changelog=changelog):
                    (root / "CHANGELOG.md").write_text(changelog, encoding="utf-8")
                    with self.assertRaises(ValueError):
                        release.read_metadata(root)

    def test_preparation_keeps_tested_zip_bytes_and_writes_checksum_and_notes(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            (root / "CMakeLists.txt").write_text(
                "project(gs_232b_open_gui VERSION 0.2.1 LANGUAGES C CXX)\n", encoding="utf-8"
            )
            (root / "CHANGELOG.md").write_text("## [0.2.1]\n\n- 发布说明。\n", encoding="utf-8")
            archive = root / "tested.zip"
            with zipfile.ZipFile(archive, "w") as package:
                package.writestr("app/CHANGELOG.md", "## [0.2.1]\n\n- 发布说明。\n")
            original = archive.read_bytes()
            files = release.prepare_release(root, archive, root / "release")
            self.assertEqual(files["tag"], "v0.2.1")
            self.assertEqual(files["archive"].name, "gs-232b-open-gui-0.2.1-windows-x86_64.zip")
            self.assertEqual(files["archive"].read_bytes(), original)
            self.assertEqual(
                files["checksum"].read_text(encoding="utf-8"),
                hashlib.sha256(original).hexdigest() + "  gs-232b-open-gui-0.2.1-windows-x86_64.zip\n",
            )
            self.assertEqual(files["notes"].read_text(encoding="utf-8"), "- 发布说明。\n")

    def test_existing_release_is_rejected_without_mutating_github(self):
        with patch.object(release, "github_resource", return_value={"id": 1}), \
                patch.object(release.subprocess, "run") as command:
            with self.assertRaisesRegex(ValueError, "already exists"):
                release.publish_release("owner/repo", "a" * 40, {"tag": "v0.2.1"})
            command.assert_not_called()

    def test_tag_for_another_commit_is_rejected_without_mutating_github(self):
        tag = {"object": {"type": "commit", "sha": "b" * 40}}
        with patch.object(release, "github_resource", side_effect=[None, tag]), \
                patch.object(release.subprocess, "run") as command:
            with self.assertRaisesRegex(ValueError, "another commit"):
                release.publish_release("owner/repo", "a" * 40, {"tag": "v0.2.1"})
            command.assert_not_called()

    def test_matching_annotated_tag_publishes_the_built_commit_and_changelog(self):
        annotated = {"object": {"type": "tag", "sha": "b" * 40}}
        resolved = {"object": {"type": "commit", "sha": "a" * 40}}
        files = {"tag": "v0.2.1", "archive": "app.zip", "checksum": "app.zip.sha256", "notes": "notes.md"}
        with patch.object(release, "github_resource", side_effect=[None, annotated, resolved]), \
                patch.object(release.subprocess, "run") as command:
            release.publish_release("owner/repo", "a" * 40, files)
        arguments = command.call_args.args[0]
        self.assertEqual(arguments[:4], ["gh", "release", "create", "v0.2.1"])
        self.assertEqual(arguments[arguments.index("--target") + 1], "a" * 40)
        self.assertEqual(arguments[arguments.index("--notes-file") + 1], "notes.md")
        self.assertIn("app.zip.sha256", arguments)

    def test_cli_prepares_locally_without_publishing_by_default(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            (root / "CMakeLists.txt").write_text(
                "project(gs_232b_open_gui VERSION 0.2.1 LANGUAGES C CXX)\n", encoding="utf-8"
            )
            (root / "CHANGELOG.md").write_text("## [0.2.1]\n\n- Test\n", encoding="utf-8")
            archive = root / "tested.zip"
            archive.write_bytes(b"tested artifact")
            argv = ["ci-release.py", "--project", str(root), "--archive", str(archive),
                    "--output-dir", str(root / "release")]
            with patch("sys.argv", argv), patch.object(release.subprocess, "run") as command, \
                    contextlib.redirect_stdout(io.StringIO()):
                release.main()
            command.assert_not_called()
            self.assertEqual((root / "release/gs-232b-open-gui-0.2.1-windows-x86_64.zip").read_bytes(), b"tested artifact")


if __name__ == "__main__":
    unittest.main()
