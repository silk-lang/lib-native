"""Build a package with the pinned Silk compiler and return its executable.

Manifests declare everything a build needs: native/gpu/webgpu/sys pins Dawn and compiles the callback
shim, and the frameworks and libraries reach dependent executables on their own. This module only
locates the pinned tools and, on Windows, puts the DLLs a test program loads beside it.
"""
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys

from _install import ROOT, REPO


def command_path(script):
    if sys.platform == "win32":
        if script in ("install-silk", "install-llvm"):
            name = "silkc" if script == "install-silk" else os.environ.get("SILK_CLANG", "clang")
            found = shutil.which(name)
            if not found:
                raise RuntimeError(f"Install {name} and put it on PATH before running this maintainer tool")
            path = Path(found).resolve()
            return path.parent if script == "install-silk" else path
        component = {"install-webgpu": "dawn", "install-glfw": "glfw", "install-dxc": "dxc"}.get(script)
        if component:
            return prepare_windows()[component]
    # Silk and LLVM installers are shared at the repository root; Dawn's lives here.
    directory = REPO / "scripts" if script in ("install-silk", "install-llvm", "install-glfw", "install-dxc") else ROOT / "scripts"
    return Path(subprocess.check_output([sys.executable, str(directory / script)], text=True).strip())


def executable(name):
    return name + (".exe" if os.name == "nt" else "")


_windows_prepared = {}


def prepare_windows(clang=None):
    """Use the same PowerShell preparation as direct Silk builds, once per toolchain."""
    clang = str(clang or command_path("install-llvm"))
    if clang not in _windows_prepared:
        powershell = shutil.which("pwsh") or shutil.which("powershell")
        if not powershell:
            raise RuntimeError("Windows preparation requires PowerShell")
        command = [powershell, "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                   str(REPO / "scripts/prepare-windows.ps1"), "-Clang", clang, "-Json"]
        result = subprocess.check_output(command, cwd=REPO, text=True)
        _windows_prepared[clang] = {name: Path(path) for name, path in json.loads(result).items()}
    return _windows_prepared[clang]


def silk_command(subcommand, manifest, release=False, target=None, clang=None):
    """The pinned compiler's command line for `subcommand` on `manifest`, and its environment.
    `clang` replaces the pinned LLVM's clang, for a wrapper that adds link flags."""
    compiler_bin = command_path("install-silk")
    clang = clang or command_path("install-llvm")
    command = [str(compiler_bin / executable("silkc")), subcommand, "--manifest-path", str(manifest),
               "--stdlib", str(compiler_bin.parent / "lib" / "silk" / "stdlib"), "--clang", str(clang)]
    if target:
        command += ["--target", target]
    if release:
        command.append("--release")
    return command, dict(os.environ, SILK_CLANG=str(clang))


def build(package_path, release=False, target=None, clang=None):
    """Build the package at `package_path` and return its executable."""
    package_path = Path(package_path).resolve()
    if package_path.is_file():
        package_path = package_path.parent
    name = re.search(r'^name = "([^"]+)"', (package_path / "silk.toml").read_text(encoding="utf-8"), re.M).group(1)
    command, env = silk_command("build", package_path / "silk.toml", release, target, clang)
    wasm = target == "wasm32-unknown-unknown"
    prepared = prepare_windows(clang) if sys.platform == "win32" and not wasm else None
    subprocess.run(command, cwd=REPO, env=env, check=True)
    profile = "release" if release else "debug"
    file = name if wasm else executable(name)
    # Workspace members build into the shared output directory; other packages into their own.
    candidates = [path for output in (REPO / "build", package_path / "build")
                  for path in (output / "llvm").glob(f"{target or '*'}/{profile}/{file}")]
    if len(candidates) != 1:
        raise RuntimeError(f"Expected one built executable named {file}, found {candidates}")
    program = candidates[0]
    if prepared:
        # DLL CRT startup initializes Dawn; DXC is required by its D3D12 backend.
        for source in (prepared["dawn"] / "bin/nonstd_dawn.dll",
                       prepared["dxc"] / "dxcompiler.dll", prepared["dxc"] / "dxil.dll",
                       prepared["glfw"] / "lib-vc2022/glfw3.dll"):
            shutil.copy2(source, program.parent / source.name)
    return program
