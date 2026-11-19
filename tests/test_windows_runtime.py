# GS-232B Open GUI - rotator controller GUI.
# Copyright (C) 2026 BG5GEB <bg5geb@misakanet.team>
#
# SPDX-License-Identifier: GPL-3.0-only
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, version 3 of the License.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

"""Windows 发布包运行库约束的回归检查，无需安装编译器。"""

import contextlib
import importlib.util
import io
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_writebytecode = True
spec = importlib.util.spec_from_file_location(
    "windows_runtime", pathlib.Path(__file__).resolve().parents[1] / "scripts/verify-windows-runtime.py"
)
runtime = importlib.util.module_from_spec(spec)
spec.loader.exec_module(runtime)

MSVC_UCRT = {"vcruntime140.dll", "msvcp140.dll", "api-ms-win-crt-heap-l1-1-0.dll"}


class WindowsRuntimeTests(unittest.TestCase):
    def verify(self, name, imports):
        with patch.object(runtime, "inspect_imports", return_value=imports), contextlib.redirect_stdout(io.StringIO()):
            runtime.verify(pathlib.Path(name))

    def test_msvc_ucrt_application_and_qt_pass(self):
        for name in ("app.exe", "Qt5Core.dll", "platforms/qwindows.dll"):
            with self.subTest(name=name):
                self.verify(name, MSVC_UCRT)

    def test_original_msvcrt_qt_is_rejected(self):
        with self.assertRaises(ValueError):
            self.verify("Qt5Core.dll", {"msvcrt.dll", "libstdc++-6.dll"})

    def test_ucrt_mingw_application_is_rejected(self):
        with self.assertRaises(ValueError):
            self.verify("app.exe", {"ucrtbase.dll", "libstdc++-6.dll", "libgcc_s_seh-1.dll"})

    def test_mixed_crt_is_rejected_even_with_msvc_imports(self):
        with self.assertRaises(ValueError):
            self.verify("app.exe", MSVC_UCRT | {"msvcrt.dll"})

    def test_debug_and_legacy_runtime_are_rejected(self):
        for name in ("msvcp140d.dll", "vcruntime140d.dll", "ucrtbased.dll", "msvcr120.dll", "msvcp120.dll"):
            with self.subTest(name=name), self.assertRaises(ValueError):
                self.verify("helper.dll", MSVC_UCRT | {name})

    def test_unused_mingw_runtime_is_rejected(self):
        with self.assertRaises(ValueError):
            self.verify("libstdc++-6.dll", {"ucrtbase.dll"})

    def test_static_or_missing_runtime_is_rejected(self):
        for name in ("app.exe", "Qt5Core.dll", "Qt5Pdf.dll", "platforms/qwindows.dll"):
            for imports in ({"kernel32.dll"}, {"ucrtbase.dll"}, {"vcruntime140.dll"}):
                with self.subTest(name=name, imports=imports), self.assertRaises(ValueError):
                    self.verify(name, imports)

    def test_neutral_helper_is_allowed(self):
        self.verify("helper.dll", {"kernel32.dll"})

    def test_qt_network_package_requires_openssl(self):
        with tempfile.TemporaryDirectory() as folder:
            package = pathlib.Path(folder)
            (package / "Qt5Network.dll").touch()
            with patch.object(runtime, "inspect_imports", return_value=MSVC_UCRT), \
                    self.assertRaises(ValueError):
                runtime.verify_directory(package)

    def test_qt_network_package_accepts_openssl(self):
        with tempfile.TemporaryDirectory() as folder:
            package = pathlib.Path(folder)
            (package / "Qt5Network.dll").touch()
            for name in runtime.OPENSSL_RUNTIME_DLLS:
                (package / name).touch()
            with patch.object(runtime, "inspect_imports", return_value=MSVC_UCRT), \
                    contextlib.redirect_stdout(io.StringIO()):
                runtime.verify_directory(package)

    def test_import_names_are_normalized(self):
        output = "file format pei-x86-64\n DLL Name: VCRUNTIME140.dll\n DLL Name: UCRTBASE.DLL\n"
        with patch.object(runtime.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, stdout=output)):
            self.assertEqual(runtime.inspect_imports(pathlib.Path("app.exe")), {"vcruntime140.dll", "ucrtbase.dll"})

    def test_wrong_architecture_and_unreadable_pe_are_rejected(self):
        with patch.object(runtime.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, stdout="file format pei-i386")):
            with self.assertRaises(ValueError):
                runtime.inspect_imports(pathlib.Path("app.exe"))
        with patch.object(runtime.subprocess, "run", side_effect=subprocess.CalledProcessError(1, "objdump")):
            with self.assertRaises(subprocess.CalledProcessError):
                runtime.inspect_imports(pathlib.Path("app.exe"))

    def test_nested_plugin_failure_reaches_exit_status(self):
        with tempfile.TemporaryDirectory() as folder:
            plugin = pathlib.Path(folder) / "platforms" / "qwindows.dll"
            plugin.parent.mkdir()
            plugin.touch()
            with patch.object(sys, "argv", ["verify", folder]), patch.object(runtime, "inspect_imports", return_value={"msvcrt.dll"}):
                with contextlib.redirect_stderr(io.StringIO()):
                    self.assertEqual(runtime.main(), 1)

    def test_empty_directory_is_rejected(self):
        with tempfile.TemporaryDirectory() as folder, patch.object(sys, "argv", ["verify", folder]):
            with contextlib.redirect_stderr(io.StringIO()):
                self.assertEqual(runtime.main(), 1)


if __name__ == "__main__":
    unittest.main()
