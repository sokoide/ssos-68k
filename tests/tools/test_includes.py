import importlib.util
import subprocess
from pathlib import Path
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('includes', ROOT / 'tools/check-includes.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class IncludeBoundaries(unittest.TestCase):
    def fixture(self, files, public=(), application=(), core=()):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name, text in files.items():
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text)
            return module.check(root, public, application, core)

    def test_repository_boundaries(self):
        self.assertEqual(module.check(ROOT), [])

    def test_public_headers_compile_independently(self):
        for name in module.PUBLIC:
            with self.subTest(header=name):
                result = subprocess.run(
                    ['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsyntax-only', '-x', 'c', '-'],
                    input=f'#include "{ROOT / name}"\n', text=True, capture_output=True)
                self.assertEqual(result.returncode, 0, result.stderr)

    def test_private_include_is_rejected_transitively(self):
        files = {'ssos/os/app/scene.c': '#include "scene.h"\n',
                 'ssos/os/app/scene.h': '#include "../kernel/scheduler.h"\n',
                 'ssos/os/kernel/scheduler.h': ''}
        errors = self.fixture(files, application=('ssos/os/app/scene.c',))
        self.assertEqual(errors, [
            'ssos/os/app/scene.c -> ssos/os/app/scene.h -> ssos/os/kernel/scheduler.h'])

    def test_hardware_is_rejected_from_core_and_public_api(self):
        files = {'ssos/os/kernel/task.h': '#include "x68k_mfp.h"\n',
                 'ssos/os/kernel/scheduler.c': '#include <x68k/iocs.h>\n',
                 'ssos/os/kernel/x68k_mfp.h': ''}
        errors = self.fixture(files, public=('ssos/os/kernel/task.h',),
                              core=('ssos/os/kernel/scheduler.c',))
        self.assertEqual(len(errors), 2)

    def test_angle_include_cannot_bypass_private_boundary(self):
        files = {'ssos/os/kernel/task.h': '#include <scheduler.h>\n',
                 'ssos/os/kernel/scheduler.h': ''}
        self.assertTrue(self.fixture(files, public=('ssos/os/kernel/task.h',)))

    def test_bootstrap_may_use_private_headers(self):
        files = {'ssos/os/app/scene.c': '#include "scene.h"\n',
                 'ssos/os/app/scene.h': '',
                 'ssos/os/app/main.c': '#include "../kernel/scheduler.h"\n',
                 'ssos/os/kernel/scheduler.h': ''}
        self.assertEqual(self.fixture(files, application=('ssos/os/app/scene.c',)), [])

    def test_cycles_and_comments_do_not_create_false_violations(self):
        files = {'ssos/os/kernel/task.h': '#include "status.h"\n'
                 '/* #include "scheduler.h" */\n// #include <x68k/iocs.h>\n',
                 'ssos/os/kernel/status.h': '#include "task.h"\n#include <stdint.h>\n'}
        self.assertEqual(self.fixture(files, public=('ssos/os/kernel/task.h',)), [])

    def test_missing_local_include_fails_closed(self):
        files = {'ssos/os/kernel/task.h': '#include "kernel.h"\n'}
        self.assertTrue(self.fixture(files, public=('ssos/os/kernel/task.h',)))


if __name__ == '__main__':
    unittest.main()
