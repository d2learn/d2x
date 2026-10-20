"""Portable checker startup/transport failure injection; no course/toolchain needed."""
import json
import os
from pathlib import Path
import shlex
import signal
import subprocess
import sys
import tempfile
import time
import unittest


def emit(**event):
    print(json.dumps(event), flush=True)


def provider():
    mode, verb = os.environ.get('TEST_MODE', 'ok'), sys.argv[2]
    if verb == 'describe':
        if mode == 'slow':
            time.sleep(2)
        if mode in ('hang', 'chatty', 'tree'):
            if mode == 'tree':
                child = subprocess.Popen([sys.executable, '-c', 'import time; time.sleep(120)'])
                Path('child.pid').write_text(str(child.pid))
            while True:
                if mode == 'chatty':
                    print('still waiting', file=sys.stderr, flush=True)
                time.sleep(.1)
        if mode == 'empty':
            return
        if mode == 'bad-version':
            emit(event='describe', name='test', protocol=99)
            return
        if mode == 'stderr-json':
            print('{"event":"describe","name":"test","protocol":1}', file=sys.stderr)
            return
        if mode == 'flood':
            sys.stdout.write('x' * 1500000 + '\n')
            sys.stderr.write('diagnostic ' * 20000 + '\n')
        emit(event='describe', name='test', protocol=1)
        if mode == 'exit':
            print('compiler failed here', file=sys.stderr, flush=True)
            sys.exit(7)
    elif verb == 'exercises':
        if mode == 'enumerate-empty':
            return
        emit(event='exercise', id='one', order=1, files=[str(Path('one.txt').resolve())])
        if mode == 'enumerate-exit':
            sys.exit(8)
    elif verb == 'check':
        if mode == 'check-hang':
            time.sleep(120)
        emit(event='verdict', outcome='pass', stage='run')
        if mode == 'check-exit':
            sys.exit(9)


class Startup(unittest.TestCase):
    def run_case(self, mode='ok', config=None, env=None, ui='events'):
        with tempfile.TemporaryDirectory(prefix='d2x startup ') as folder:
            root = Path(folder)
            quote = (lambda s: '"' + s + '"') if os.name == 'nt' else shlex.quote
            command = ' '.join(quote(s) for s in [sys.executable, str(Path(__file__).resolve()), '--provider'])
            data = {'buildtools': command, 'provider_startup_timeout': 5, 'editor': ''}
            data.update(config or {})
            (root / '.d2x.json').write_text(json.dumps(data))
            (root / 'one.txt').write_text('one')
            settings = {**os.environ, 'TEST_MODE': mode, 'D2X_PROVIDER_IDLE_TIMEOUT': '1', **(env or {})}
            start = time.monotonic()
            p = subprocess.run([os.environ['D2X'], 'checker', '--ui', 'print', *(['--emit-events'] if ui == 'events' else [])],
                               cwd=root, env=settings, capture_output=True, encoding='utf-8', errors='replace', timeout=15)
            self.assertLess(time.monotonic() - start, 12)
            events = [json.loads(line) for line in p.stdout.splitlines()] if ui == 'events' else []
            logs = {path.name: path.read_text(errors='replace') for path in (root / '.d2x').glob('provider.*.log')}
            return p, events, logs

    def test_old_config_and_slow_start(self):
        for mode in ['ok', 'slow', 'flood']:
            p, events, logs = self.run_case(mode)
            self.assertEqual(p.returncode, 0, p.stderr)
            self.assertTrue(any(e['event'] == 'preparing' for e in events))
            self.assertEqual(events[-1]['event'], 'done')
            if mode == 'flood':
                self.assertIn('diagnostic', logs['provider.stderr.log'])
                self.assertIn('x' * 100, logs['provider.stdout.log'])

    def test_startup_deadline_cannot_be_fed_by_stderr(self):
        for mode in ['hang', 'chatty', 'tree']:
            p, events, _ = self.run_case(mode, {'provider_startup_timeout': 1})
            self.assertNotEqual(p.returncode, 0)
            self.assertEqual(events[-1]['code'], 'startup_timeout')
            self.assertEqual(events[-1]['phase'], 'describe')

    def test_protocol_and_child_failures(self):
        for mode, code, phase in [('empty', 'provider_protocol', 'describe'),
                                  ('stderr-json', 'provider_protocol', 'describe'),
                                  ('bad-version', 'provider_protocol', 'describe'),
                                  ('exit', 'provider_exit', 'describe'),
                                  ('enumerate-exit', 'provider_exit', 'exercises'),
                                  ('enumerate-empty', 'empty_course', 'exercises'),
                                  ('check-exit', 'provider_exit', 'check'),
                                  ('check-hang', 'idle_timeout', 'check')]:
            p, events, _ = self.run_case(mode)
            self.assertNotEqual(p.returncode, 0, mode)
            self.assertEqual(events[-1]['event'], 'error', mode)
            self.assertEqual(events[-1]['code'], code, mode)
            self.assertEqual(events[-1]['phase'], phase, mode)
            if mode == 'exit':
                self.assertIn('compiler failed here', events[-1]['message'])

    def test_timeout_validation_and_env_precedence(self):
        for value in [0, -1, '5', True, 1.5]:
            p, events, _ = self.run_case(config={'provider_startup_timeout': value})
            self.assertNotEqual(p.returncode, 0)
            self.assertEqual(events[-1]['code'], 'configuration')
        p, _, _ = self.run_case(config={'provider_startup_timeout': 0}, env={'D2X_PROVIDER_STARTUP_TIMEOUT': '5'})
        self.assertEqual(p.returncode, 0, p.stderr)
        p, _, _ = self.run_case(env={'D2X_PROVIDER_STARTUP_TIMEOUT': '5garbage'})
        self.assertNotEqual(p.returncode, 0)

    @unittest.skipIf(os.name == 'nt', 'POSIX signal delivery; Windows Job timeout covered above')
    def test_cancel_reaps_process_tree(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            command = ' '.join(shlex.quote(s) for s in [sys.executable, str(Path(__file__).resolve()), '--provider'])
            (root / '.d2x.json').write_text(json.dumps({'buildtools': command}))
            with subprocess.Popen([os.environ['D2X'], 'checker', '--emit-events'], cwd=root,
                                  env={**os.environ, 'TEST_MODE': 'tree'}, stdout=subprocess.PIPE,
                                  stderr=subprocess.PIPE, text=True) as process:
                limit = time.monotonic() + 5
                while not (root / 'child.pid').exists() and time.monotonic() < limit:
                    time.sleep(.05)
                self.assertTrue((root / 'child.pid').exists())
                child = int((root / 'child.pid').read_text())
                process.send_signal(signal.SIGINT)
                out, err = process.communicate(timeout=5)
                self.assertEqual(process.returncode, 130, err)
                self.assertEqual(json.loads(out.splitlines()[-1])['code'], 'cancelled')
                # A killed orphan may be a zombie until init reaps it; it must
                # no longer execute, even when the direct shell already died.
                state = Path(f'/proc/{child}/stat')
                if state.exists():
                    self.assertIn(state.read_text().split()[2], ['Z', 'X'])
                else:
                    with self.assertRaises(ProcessLookupError):
                        os.kill(child, 0)
                self.assertFalse((root / '.d2x/checker.lock').exists())

    def test_print_preparing_and_error(self):
        p, _, _ = self.run_case('empty', ui='print')
        self.assertNotEqual(p.returncode, 0)
        self.assertIn('Preparing course environment', p.stdout)
        self.assertIn('provider_protocol', p.stdout)


if __name__ == '__main__':
    if '--provider' in sys.argv:
        provider()
    else:
        unittest.main()
