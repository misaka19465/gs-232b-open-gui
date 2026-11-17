"""Cache maintenance keeps two generations per family and ignores unrelated keys."""

import importlib.util
import pathlib
import unittest


spec = importlib.util.spec_from_file_location(
    "ci_cache", pathlib.Path(__file__).resolve().parents[2] / "scripts/cleanup-ci-cache.py"
)
cache = importlib.util.module_from_spec(spec)
spec.loader.exec_module(cache)


class CacheTests(unittest.TestCase):
    def test_third_and_older_generations_are_deleted_without_an_age_threshold(self):
        entries = [
            {"id": 1, "key": "gs232b-windows-build-v1-old", "created_at": "2026-11-17T03:00:00Z"},
            {"id": 2, "key": "gs232b-windows-build-v1-new", "created_at": "2026-11-17T03:02:00Z"},
            {"id": 3, "key": "gs232b-windows-build-v1-middle", "created_at": "2026-11-17T03:01:00Z"},
            {"id": 4, "key": "unrelated-old", "created_at": "2026-11-17T02:00:00Z"},
            {"id": 5, "key": "gs232b-windows-build-v1-oldest", "created_at": "2026-11-17T02:59:59Z"},
        ]
        self.assertEqual({entry["id"] for entry in cache.obsolete_caches(entries)}, {1, 5})

    def test_retention_is_separate_for_qt_build_and_tools(self):
        entries = []
        for family in ("qt", "build", "pe-tools", "venv"):
            for generation in range(3):
                entries.append({
                    "id": f"{family}-{generation}", "key": f"gs232b-windows-{family}-{generation}",
                    "created_at": f"2026-11-17T03:0{generation}:00Z",
                })
        self.assertEqual(
            {entry["id"] for entry in cache.obsolete_caches(entries)},
            {"qt-0", "build-0", "pe-tools-0", "venv-0"},
        )

    def test_two_old_generations_are_kept_regardless_of_age(self):
        entries = [
            {"id": 1, "key": "gs232b-windows-qt-a", "created_at": "2020-01-01T00:00:00Z"},
            {"id": 2, "key": "gs232b-windows-qt-b", "created_at": "2020-01-02T00:00:00Z"},
        ]
        self.assertEqual(cache.obsolete_caches(entries), [])


if __name__ == "__main__":
    unittest.main()
