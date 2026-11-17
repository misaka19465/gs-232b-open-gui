"""Build caches change with compiled content, not packaging or timestamps."""

import importlib.util
import os
import pathlib
import tempfile
import unittest


spec = importlib.util.spec_from_file_location(
    "ci_build_cache", pathlib.Path(__file__).resolve().parents[2] / "scripts/ci-build-cache.py"
)
cache = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cache)


class BuildCacheTests(unittest.TestCase):
    def test_changed_object_or_library_changes_fingerprint(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            artifact = root / "app.obj"
            artifact.write_bytes(b"first compilation")
            initial = cache.build_fingerprint(root)
            artifact.write_bytes(b"second compilation")
            changed = cache.build_fingerprint(root)
            self.assertNotEqual(initial, changed)
            (root / "dependency.lib").write_bytes(b"compiled library")
            self.assertNotEqual(changed, cache.build_fingerprint(root))

    def test_repacking_and_timestamp_changes_do_not_trigger_upload(self):
        with tempfile.TemporaryDirectory() as folder:
            root = pathlib.Path(folder)
            artifact = root / "app.obj"
            artifact.write_bytes(b"unchanged object")
            initial = cache.build_fingerprint(root)
            os.utime(artifact, (1_700_000_000, 1_700_000_000))
            (root / "package").mkdir()
            (root / "package/Qt6Core.dll").write_bytes(b"repackaged SDK DLL")
            (root / "app.zip").write_bytes(b"new archive timestamp")
            (root / ".ninja_log").write_text("new build log", encoding="utf-8")
            self.assertEqual(initial, cache.build_fingerprint(root))


if __name__ == "__main__":
    unittest.main()
