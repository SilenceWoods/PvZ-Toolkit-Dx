# ---------------------------------------------------------------------------
# MARKER FILE -- this tree does NOT build with CMake.
#
# Both targets are built by plain g++ command lines under env\scripts\:
#     build\pztk_imgui.exe  <- env\scripts\build_imgui_demo.cmd
#     build\pztk_hook.dll   <- env\scripts\build_hook_dll.cmd
#     (all at once: env\scripts\build_all.cmd)
#
# Upstream's FLTK target "pvztoolkit" - and everything that only fed it - is
# gone from this tree on purpose (scheme A: only the ImGui frontend plus the
# injected DLL are kept).  Removed with it: src\window.cpp, src\toolkit.cpp,
# src\main.cpp, inc\window.h, inc\toolkit.h, res\version.rc, tools\*.makefile,
# deps\*, .vscode\*, README.English.md, docs\pvztoolkit_*.gif.
#
# The FLTK source tree itself is still there, but ONLY as the zlib source
# (env\src\fltk-1.4.5\zlib -> env\scripts\build_zlib.cmd -> libfltk_z.a);
# no FLTK interface library is built or linked any more.
#
# So: do NOT add find_package(FLTK), do NOT restore the MSVC/MinGW branches,
# and do NOT "sync" this file back from a tree that still carries the old
# full CMake project - that would silently re-add the FLTK target.
#
# DO NOT DELETE THIS FILE: env\scripts\provision.cmd and build_all.cmd use it
# as the ASCII-alias sentinel ("workspace alias is not usable") - keep it.
