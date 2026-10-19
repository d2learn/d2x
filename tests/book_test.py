#!/usr/bin/env python3
"""Book CLI integration: real socket conflicts, isolated config, fake tools.

Usage: D2X=/absolute/path/to/d2x python3 tests/book_test.py
No installation, browser launch, or existing service termination occurs.
"""
import errno
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import unittest

D2X = str(Path(os.environ["D2X"]).resolve())


class BookTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="d2x-book-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.home = self.root / "home"
        self.home.mkdir()
        self.project = self.root / "课程 space's book"
        self.project.mkdir()
        (self.project / "book").mkdir()
        self.bin = self.root / "bin"
        self.bin.mkdir()
        self.calls = self.root / "calls.jsonl"
        fake = self.bin / "fake.py"
        fake.write_text('''import json, os, sys
tool, args = sys.argv[1], sys.argv[2:]
with open(os.environ["BOOK_TEST_CALLS"], "a", encoding="utf-8") as f:
    f.write(json.dumps([tool, args]) + "\\n")
if tool == "xlings" and args == ["--version"]:
    print("xlings 2026.10.19.2")
    sys.exit(0)
print(tool + " visible output", flush=True)
sys.exit(int(os.environ.get("BOOK_TEST_" + tool.upper() + "_EXIT", "0")))
''', encoding="utf-8")
        for tool in ("xlings", "mdbook"):
            if os.name == "nt":
                wrapper = self.bin / (tool + ".cmd")
                wrapper.write_text(f'@"{sys.executable}" "{fake}" {tool} %*\n', encoding="utf-8")
            else:
                import shlex
                wrapper = self.bin / tool
                wrapper.write_text("#!/bin/sh\nexec " + shlex.quote(sys.executable)
                                   + " " + shlex.quote(str(fake)) + " " + tool + ' "$@"\n')
                wrapper.chmod(0o755)
        self.env = {k: v for k, v in os.environ.items() if not k.startswith("D2X_")}
        self.env.update(HOME=str(self.home), USERPROFILE=str(self.home),
                        PATH=str(self.bin) + os.pathsep + os.environ["PATH"],
                        BOOK_TEST_CALLS=str(self.calls))

    def run_book(self, *args, ok=True, **env):
        result = subprocess.run([D2X, "book", *args], cwd=self.project,
                                env=dict(self.env, **env), capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=20)
        output = result.stdout + result.stderr
        self.assertEqual(result.returncode == 0, ok, f"exit={result.returncode}: {output}")
        return result, output

    def recorded(self, tool):
        rows = self.calls.read_text(encoding="utf-8").splitlines() if self.calls.exists() else []
        return [args for name, args in map(json.loads, rows) if name == tool]

    def served_port(self):
        args, = self.recorded("mdbook")
        self.assertEqual(args[:3], ["serve", "--open", "--port"])
        return int(args[3]), Path(args[4])

    def config(self, data, global_=False):
        parent = self.home if global_ else self.project
        (parent / ".d2x.json").write_text(json.dumps(data), encoding="utf-8")

    def hold(self, port, family=None, host=None):
        addresses = socket.getaddrinfo("localhost", port, type=socket.SOCK_STREAM,
                                       proto=socket.IPPROTO_TCP)
        held = []
        seen = set()
        for af, kind, proto, _, address in addresses:
            if family is not None and family != af:
                continue
            address = (host, port) if host is not None else address
            key = (af, address)
            if key in seen:
                continue
            seen.add(key)
            sock = socket.socket(af, kind, proto)
            self.addCleanup(sock.close)
            if os.name == "nt":
                sock.setsockopt(socket.SOL_SOCKET, socket.SO_EXCLUSIVEADDRUSE, 1)
            try:
                sock.bind(address)
                sock.listen()
            except OSError as error:
                sock.close()
                if error.errno not in (errno.EADDRINUSE, errno.EAFNOSUPPORT):
                    raise
            else:
                held.append(sock)
        return held

    def free_port(self):
        # Keep the candidate away from the automatic scan range and check both families.
        for _ in range(50):
            with socket.socket() as sock:
                sock.bind(("127.0.0.1", 0))
                port = sock.getsockname()[1]
            held = self.hold(port)
            if held:
                for sock in held:
                    sock.close()
                return port
        self.fail("No test port available")

    def test_help(self):
        _, output = self.run_book("--help")
        self.assertIn("--port", output)
        self.assertIn("-p", output)
        self.assertFalse(self.recorded("mdbook"))

    def test_default_and_quoted_path(self):
        _, output = self.run_book()
        port, path = self.served_port()
        self.assertTrue(3000 <= port <= 3099)
        self.assertEqual(path, self.project / "book")
        self.assertIn("xlings visible output", output)
        self.assertIn("mdbook visible output", output)
        self.assertIn(["install", "mdbook", "-y"], self.recorded("xlings"))

    def test_auto_skips_contiguous_conflicts(self):
        self.hold(3000)
        self.hold(3001)
        _, output = self.run_book()
        self.assertGreaterEqual(self.served_port()[0], 3002)
        self.assertEqual(output.count("are in use; trying"), 1)

    def test_scan_exhausted(self):
        for port in range(3000, 3100):
            self.hold(port)
        _, output = self.run_book(ok=False)
        self.assertIn("3000-3099 are in use", output)
        self.assertFalse(self.recorded("mdbook"))

    def test_root_command_registration(self):
        result = subprocess.run([D2X, "--help"], env=self.env, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=10)
        self.assertEqual(result.returncode, 0)
        self.assertIn("d2x command line tool", result.stdout)
        for command in ("book", "checker", "status", "install", "config"):
            self.assertIn(command, result.stdout)

    def test_port_boundaries_are_valid_syntax(self):
        for port in (1, 65535):
            result = subprocess.run([D2X, "book", "-p", str(port)], cwd=self.project,
                                    env=self.env, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=20)
            self.assertNotIn("Invalid book port", result.stdout + result.stderr)
        self.assertIn(["install", "mdbook", "-y"], self.recorded("xlings"))

    def test_explicit_port(self):
        port = self.free_port()
        self.run_book("--port", str(port))
        self.assertEqual(self.served_port()[0], port)

    def test_short_port_and_cli_priority(self):
        port = self.free_port()
        self.config({"book_port": "bad"})
        self.run_book("-p", str(port), D2X_BOOK_PORT="bad")
        self.assertEqual(self.served_port()[0], port)

    def test_env_priority(self):
        port = self.free_port()
        self.config({"book_port": "bad"})
        self.config({"book_port": False}, global_=True)
        self.run_book(D2X_BOOK_PORT=str(port))
        self.assertEqual(self.served_port()[0], port)

    def test_local_priority(self):
        port = self.free_port()
        self.config({"book_port": port})
        self.config({"book_port": "bad"}, global_=True)
        self.run_book()
        self.assertEqual(self.served_port()[0], port)

    def test_global_with_complete_local_ui_llm(self):
        port = self.free_port()
        self.config({"ui_backend": "print", "llm": {"api_key": "test"}})
        self.config({"book_port": port}, global_=True)
        self.run_book()
        self.assertEqual(self.served_port()[0], port)

    def test_invalid_ports(self):
        for value in ("0", "65536", "-1", "3001abc", "", "999999999999999999999", "+3001", " 3001"):
            with self.subTest(value=value):
                _, output = self.run_book("--port", value, ok=False)
                self.assertIn("--port", output)
        self.assertFalse(self.recorded("xlings"))
        self.assertFalse(self.recorded("mdbook"))

    def test_invalid_json_types_and_env(self):
        for value in ("3001", 3001.0, True, None, 0, -1, 65536, 10**30):
            with self.subTest(value=value):
                self.config({"book_port": value})
                _, output = self.run_book(ok=False)
                self.assertIn("book_port", output)
        _, output = self.run_book(ok=False, D2X_BOOK_PORT="")
        self.assertIn("D2X_BOOK_PORT", output)
        self.assertFalse(self.recorded("xlings"))

    def test_invalid_config_object(self):
        (self.project / ".d2x.json").write_text("{broken", encoding="utf-8")
        _, output = self.run_book(ok=False)
        self.assertIn("Invalid config", output)
        self.assertFalse(self.recorded("mdbook"))

    def test_explicit_conflict_sources(self):
        port = self.free_port()
        self.hold(port)
        for source in ("cli", "env", "local", "global"):
            with self.subTest(source=source):
                (self.project / ".d2x.json").unlink(missing_ok=True)
                if source == "cli":
                    _, output = self.run_book("-p", str(port), ok=False)
                elif source == "env":
                    _, output = self.run_book(ok=False, D2X_BOOK_PORT=str(port))
                else:
                    self.config({"book_port": port}, global_=source == "global")
                    _, output = self.run_book(ok=False)
                self.assertIn("is in use", output)
        self.assertFalse(self.recorded("mdbook"))

    def test_ipv6_conflict(self):
        if not socket.has_ipv6:
            self.skipTest("IPv6 unavailable")
        port = self.free_port()
        held = self.hold(port, socket.AF_INET6)
        if not held:
            self.skipTest("localhost has no usable IPv6 address")
        _, output = self.run_book("-p", str(port), ok=False)
        self.assertIn("is in use", output)

    def test_ipv4_wildcard_conflict(self):
        port = self.free_port()
        self.hold(port, socket.AF_INET, "0.0.0.0")
        _, output = self.run_book("-p", str(port), ok=False)
        self.assertIn("is in use", output)

    def test_install_failure_stops_serve(self):
        result, output = self.run_book(ok=False, BOOK_TEST_XLINGS_EXIT="7")
        self.assertEqual(result.returncode, 7)
        self.assertIn("installation failed", output)
        self.assertFalse(self.recorded("mdbook"))

    def test_serve_exit_status(self):
        result, output = self.run_book(ok=False, BOOK_TEST_MDBOOK_EXIT="9")
        self.assertEqual(result.returncode, 9)
        self.assertIn("mdBook exited with 9", output)

    def test_language_resolution(self):
        (self.project / "book" / "ja").mkdir()
        self.run_book("--lang", "ja")
        self.assertEqual(self.served_port()[1], self.project / "book" / "ja")

    def test_missing_language_falls_back(self):
        self.run_book("--lang", "missing")
        self.assertEqual(self.served_port()[1], self.project / "book")

    def test_missing_book(self):
        (self.project / "book").rmdir()
        _, output = self.run_book(ok=False)
        self.assertIn("No book found", output)
        self.assertFalse(self.recorded("mdbook"))

    def test_config_round_trip(self):
        self.config({"book_port": 8080})
        # Choose local, modify, accept seven existing field defaults.
        result = subprocess.run([D2X, "config"], cwd=self.project, env=self.env,
                                input="2\ny\n" + "\n" * 7, capture_output=True,
                                text=True, encoding="utf-8", errors="replace", timeout=10)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        data = json.loads((self.project / ".d2x.json").read_text(encoding="utf-8"))
        self.assertEqual(data["book_port"], 8080)

    @unittest.skipUnless(os.name == "nt", "Windows shell-specific rejection")
    def test_windows_variable_path_rejected(self):
        project = self.root / "unsafe%PATH%"
        project.mkdir()
        (project / "book").mkdir()
        self.project = project
        _, output = self.run_book(ok=False)
        self.assertIn("unsupported shell characters", output)
        self.assertFalse(self.recorded("mdbook"))


if __name__ == "__main__":
    unittest.main(verbosity=2)
