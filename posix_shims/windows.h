// windows.h — SWELL compatibility shim for the Linux/macOS build.
//
// The Klinke source includes <windows.h> (Win32 convention). On non-Windows
// builds using SWELL there is no real windows.h; SWELL exposes the Win32 API
// surface (HWND, DWORD, GetTickCount, Sleep, OutputDebugString, ...) via
// swell.h. This shim is found through -Iposix_shims (only added on !WIN32)
// and redirects <windows.h> / "windows.h" to swell.h.
//
// On Windows, -Iposix_shims is NOT added, so the real Windows SDK windows.h
// is found — this shim never shadows it.
#ifndef MCU_KLINKE_WINDOWS_H_SHIM
#define MCU_KLINKE_WINDOWS_H_SHIM
#include "swell.h"
#endif
