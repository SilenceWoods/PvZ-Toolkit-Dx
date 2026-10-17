#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
下载 PvZ Toolkit 的 32 位构建依赖。

- WinLibs MinGW-w64 i686 (msvcrt) 便携工具链
- FLTK 1.4.5 源码

下载前先比对 sha256，已存在且校验通过的会跳过。
"""

import hashlib
import os
import sys
import urllib.request

ENV_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DOWNLOAD_DIR = os.path.join(ENV_DIR, "downloads")

DEPS = [
    {
        "name": "winlibs-i686-posix-dwarf-gcc-16.2.0-mingw-w64msvcrt-14.0.0-r2.zip",
        "url": "https://github.com/brechtsanders/winlibs_mingw/releases/download/"
               "16.2.0posix-14.0.0-msvcrt-r2/"
               "winlibs-i686-posix-dwarf-gcc-16.2.0-mingw-w64msvcrt-14.0.0-r2.zip",
        "sha256": "f1fab67fbe6ed1b0a390d06db6af8b4f0ca25fceab4f51bac2a88af8b81a93ec",
    },
    {
        "name": "fltk-1.4.5-source.tar.gz",
        "url": "https://github.com/fltk/fltk/releases/download/release-1.4.5/"
               "fltk-1.4.5-source.tar.gz",
        "sha256": "eede1fb2b8e9c2e581e77082e15252145855c79aad30070ee3b24aabe2f926f1",
    },
    # 新前端 (Dear ImGui + GLFW)
    {
        "name": "imgui-1.91.8.tar.gz",
        "url": "https://github.com/ocornut/imgui/archive/refs/tags/v1.91.8.tar.gz",
        "sha256": "db3a2e02bfd6c269adf0968950573053d002f40bdfb9ef2e4a90bce804b0f286",
    },
    {
        "name": "glfw-3.4.tar.gz",
        "url": "https://github.com/glfw/glfw/archive/refs/tags/3.4.tar.gz",
        "sha256": "c038d34200234d071fae9345bc455e4a8f2f544ab60150765d7704e08f3dac01",
    },
]


def sha256_of(path, chunk=1 << 20):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while True:
            b = f.read(chunk)
            if not b:
                break
            h.update(b)
    return h.hexdigest()


def human(n):
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024:
            return "%.1f %s" % (n, unit)
        n /= 1024.0
    return "%.1f TB" % n


def download(dep):
    dest = os.path.join(DOWNLOAD_DIR, dep["name"])

    if os.path.exists(dest) and sha256_of(dest) == dep["sha256"]:
        print("[skip] %s 已存在且校验通过" % dep["name"], flush=True)
        return True

    tmp = dest + ".part"
    print("[get ] %s" % dep["name"], flush=True)
    req = urllib.request.Request(dep["url"], headers={"User-Agent": "python-urllib"})
    with urllib.request.urlopen(req, timeout=60) as r, open(tmp, "wb") as f:
        total = int(r.headers.get("Content-Length") or 0)
        got = 0
        last = -1
        while True:
            b = r.read(1 << 20)
            if not b:
                break
            f.write(b)
            got += len(b)
            pct = int(got * 100 / total) if total else 0
            if pct != last and pct % 5 == 0:
                last = pct
                print("       %3d%%  %s / %s" % (pct, human(got), human(total)), flush=True)

    actual = sha256_of(tmp)
    if actual != dep["sha256"]:
        os.remove(tmp)
        print("[FAIL] %s sha256 不匹配\n  期望 %s\n  实际 %s"
              % (dep["name"], dep["sha256"], actual), flush=True)
        return False

    os.replace(tmp, dest)
    print("[ ok ] %s  %s  sha256 校验通过" % (dep["name"], human(os.path.getsize(dest))), flush=True)
    return True


def main():
    os.makedirs(DOWNLOAD_DIR, exist_ok=True)
    ok = True
    for dep in DEPS:
        try:
            ok = download(dep) and ok
        except Exception as e:  # noqa: BLE001
            print("[FAIL] %s: %s: %s" % (dep["name"], type(e).__name__, e), flush=True)
            ok = False
    print("RESULT: %s" % ("ALL OK" if ok else "FAILED"), flush=True)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
