"""Publish an uninstalled CI binary with exact source and content identity."""
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

binary = next(p for p in Path('target').rglob('*') if p.is_file() and p.name in ('d2x', 'd2x.exe'))
dest = Path('candidate')
dest.mkdir()
shutil.copy2(binary, dest / binary.name)
version = subprocess.check_output([str(binary.resolve()), '--version'], text=True).strip()
(dest / 'provenance.json').write_text(json.dumps({
    'repository': 'd2learn/d2x',
    'sha': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(),
    'version': version,
    'file': binary.name,
    'sha256': hashlib.sha256(binary.read_bytes()).hexdigest(),
}, indent=2) + '\n')
