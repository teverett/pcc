"""Runs whichever hosted hello binary was just built for this OS and checks
its output - a real hosted executable needs no simulator, just a direct run.
"""
import pathlib
import subprocess
import sys

build = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path("build")

for exe in (build / "hello-macos", build / "hello-linux", build / "hello-windows.exe"):
    if exe.exists():
        break
else:
    sys.exit(f"no hosted-hello binary found in {build}")

result = subprocess.run([str(exe)], capture_output=True)
assert result.returncode == 0, result.returncode
assert result.stdout in (b"hello rcc\n", b"hello rcc\r\n"), result.stdout
print(f"PASS ({exe.name})")
