"""Test native object identity/hash/gating without a disc, SDL or an account.

Requires an x86-64 Linux or Windows host and a C compiler (CC). All executable
bytes are deliberately synthetic fixtures; the real data check is stubbed.
"""
import hashlib
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
cc = shlex.split(os.environ.get("CC", "cc"), posix=os.name != "nt")
sources = [root / path for path in ("port/tests/ra_game.c", "port/src/gs/ra_game.c",
    "port/src/ra_memory.c", "port/third_party/rcheevos/src/rhash/md5.c")]
with tempfile.TemporaryDirectory(prefix="bt3-ra-game-", dir=os.environ.get("BT3_TEST_TMPDIR")) as tmp:
    work = Path(tmp)
    executable = work / ("test.exe" if os.name == "nt" else "test")
    subprocess.run(cc + ["-std=gnu99", "-Wall", "-Wextra", "-Werror"] + list(map(str, sources)) + ["-o", str(executable)], check=True)
    clean_env = {k: v for k, v in os.environ.items() if not k.startswith("BT3_")}
    for name, data in (("valid", b"\x7fELF" + bytes(60)), ("invalid", b"NOPE"), ("empty", b"")):
        data_root = work / name
        (data_root / "disc").mkdir(parents=True)
        (data_root / "disc/SLUS_216.78").write_bytes(data)
        expected = hashlib.md5(b"SLUS_216.78" + data).hexdigest() if name == "valid" else "invalid"
        subprocess.run([str(executable), str(data_root), expected], check=True, timeout=10, env=clean_env)
