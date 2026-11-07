import os
from pathlib import Path
import subprocess
import sys


def discover_compiler():
    if os.environ.get("CXX"):
        compiler = Path(os.environ["CXX"])
        if compiler.is_file():
            return compiler
        raise RuntimeError("CXX must be the full path to a Clang or GCC host compiler.")
    for name in ("clang++", "g++", "cl"):
        try:
            result = subprocess.run(
                ["where", name], capture_output=True, text=True, timeout=10
            )
        except (OSError, subprocess.TimeoutExpired):
            continue
        if result.returncode == 0:
            for candidate in result.stdout.splitlines():
                compiler = Path(candidate.strip())
                if compiler.is_file():
                    return compiler
    compiler = Path(r"C:\Program Files\LLVM\bin\clang++.exe")
    if compiler.is_file():
        return compiler
    raise RuntimeError("No host compiler found. Use a Visual Studio developer prompt or set CXX to a native clang++/g++/cl path.")


def main():
    root = Path(__file__).resolve().parent.parent
    build = root / "build_ptp_host_tests"
    try:
        build.mkdir(exist_ok=True)
        compiler = discover_compiler()
        executable = build / ("ptp_protocol_tests.exe" if os.name == "nt" else "ptp_protocol_tests")
        command = [
            str(compiler),
            "-std=c++17",
            "-Wall",
            "-Wextra",
            "-Werror",
            "-Wno-unused-function",
            str(root / "tests" / "ptp_protocol_tests.cpp"),
            "-o",
            str(executable),
        ]
        if compiler.name.lower() in ("cl", "cl.exe"):
            command = [
                str(compiler), "/nologo", "/std:c++17", "/EHsc", "/W4", "/WX", "/wd4505",
                str(root / "tests" / "ptp_protocol_tests.cpp"), f"/Fe:{executable}",
            ]
        print(f"Compiler: {compiler}", flush=True)
        compiled = subprocess.run(command, cwd=build, timeout=120)
        if compiled.returncode != 0:
            return compiled.returncode
        return subprocess.run([str(executable)], cwd=build, timeout=30).returncode
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        print(f"Host test runner failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
