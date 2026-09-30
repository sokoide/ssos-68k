import importlib.util
from pathlib import Path
import os
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location('check', ROOT / 'tools/verify-check.py')
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)

class Verification(unittest.TestCase):
    def test_common_context_sequences(self):
        subprocess.run(['python3', str(ROOT / 'tools/check-context.py')], check=True,
                       stdout=subprocess.DEVNULL)

    def test_classifier_boundaries(self):
        self.assertEqual(module.classify('tools/makedisk/main.c')[1], ('xdf',))
        self.assertEqual(module.classify('ssos/standalone/trap14.s')[1], ('x',))
        self.assertIn('非同期', module.classify('ssos/os/kernel/preemptive/interrupts.s')[2])
        self.assertTrue(module.classify('ssos/os/kernel/scheduler.c')[0])
        self.assertFalse(module.classify('docs/example.md')[0])

    def test_git_error_untracked_and_deleted(self):
        with tempfile.TemporaryDirectory() as tmp:
            def git(*args):
                subprocess.run(['git', *args], cwd=tmp, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            git('init')
            p = Path(tmp) / 'ssos/os/kernel/preemptive'
            p.mkdir(parents=True)
            source = p / 'interrupts.s'
            source.write_text('old\n')
            git('add', '.')
            git('-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid', 'commit', '-m', 'fixture')
            source.unlink()
            (p / 'new.c').write_text('new\n')
            args = ['python3', str(ROOT / 'tools/verify-check.py')]
            result = subprocess.run(args, cwd=tmp, text=True, capture_output=True)
            self.assertEqual(result.returncode, 0)
            self.assertIn('new.c', result.stdout)
            self.assertIn('interrupts.s', result.stdout)
            self.assertIn('ssos_pre.xdf', result.stdout)
            self.assertNotEqual(subprocess.run(args + ['missing-ref'], cwd=tmp, capture_output=True).returncode, 0)

    def test_verdict_and_log_retention(self):
        runner = str(ROOT / 'tests/qemu/common/run_test.sh')
        with tempfile.TemporaryDirectory() as tmp:
            env = dict(os.environ, SS_TEST_LOG_DIR=tmp, SS_TEST_CASE='fixture', EXPECT='a[0]')
            def run(command):
                return subprocess.run(['sh', runner, '1', 'sh', '-c', command], env=env, capture_output=True)
            self.assertEqual(run("printf 'a[0] OK\\n'").returncode, 0)
            self.assertNotEqual(run("printf 'a0 OK\\n'").returncode, 0)
            self.assertNotEqual(run("printf 'a[0] FAIL OK\\n'").returncode, 0)
            self.assertNotEqual(run("printf 'a[0] OK\\n'; exit 7").returncode, 0)
            self.assertNotEqual(run('sleep 2').returncode, 0)
            self.assertEqual(run("printf 'a[0] OK\\n'; sleep 2").returncode, 0)
            self.assertEqual(len(list(Path(tmp).glob('fixture.*'))), 6)

    def test_config_stamp_changes_only_on_configuration(self):
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / 'stamp'
            cmd = ['python3', str(ROOT / 'tools/build-config.py'), str(p)]
            subprocess.run(cmd + ['cc', '-O2'], check=True)
            stamp = p.stat().st_mtime_ns
            subprocess.run(cmd + ['cc', '-O2'], check=True)
            self.assertEqual(p.stat().st_mtime_ns, stamp)
            subprocess.run(cmd + ['cc', '-O0'], check=True)
            self.assertIn('-O0', p.read_text())

if __name__ == '__main__':
    unittest.main()
