# HillBlack — CS2 External

## Dependencies (cs2_external)
- Dear ImGui (master branch) — imgui.h, imgui_impl_win32.*, imgui_impl_dx11.*
  Clone: https://github.com/ocornut/imgui
- nlohmann/json — single header: https://github.com/nlohmann/json/releases
  Drop json.hpp into project root
- Windows SDK 10.0.22621+, MSVC v143, C++20

## Project Structure
Place all .hpp/.cpp files in cs2_external/src/
Add to MSVC project: src/*, imgui/*.cpp

## Build Settings (Release x64)
- Configuration: Release | x64
- C++ Standard: /std:c++20
- Optimization: /O2
- Runtime Library: /MT (static — no MSVCRT dependency)
- Linker: d3d11.lib dwmapi.lib winhttp.lib iphlpapi.lib
- Post-build: strip PDB (no /DEBUG), set /GS
- Strip exports: /EXPORT:none in linker

## Running
1. Start CS2 (windowed or borderless)
2. Run HillBlack.exe as Administrator
3. Enter key at prompt
4. INSERT to toggle menu

## Updating Offsets After CS2 Patch
When a pattern breaks (pattern_scan returns 0):

1. **Find the pattern in IDA/Ghidra:**
   Open new client.dll → Functions → search for function names or xrefs to known strings
   
   - EntitySystem: search string xref "CEntitySystem" or trace from
     `CEntitySystem::CreateEntityByName`
   - LocalPlayerController: find `CCSPlayerController::GetLocalPlayerController` vtable
   - ViewMatrix: find `CRenderGameSystem::FrameUpdate` — look for lea loading a 64-float array
   - PlantedC4: xref "planted_c4" string or `CCSGameRulesProxy::Think`

2. **Extract new pattern:**
   Copy 8–14 bytes surrounding the LEA/MOV, wildcard the 4-byte RIP offset:

Old: 48 8D 0D A1 B2 C3 D4 48 C1 E0 06
New: 48 8D 0D ? ? ? ? 48 C1 E0 06


3. **Verify resolve_rip parameters:**
   The int32 relative offset is at byte 3, instruction length is 7 for `lea reg,[rip+rel32]`
   Use `resolve_rip(sig, 3, 7)`

4. **Member offsets** (m_iHealth etc.) rarely move unless Valve restructures classes.
   Verify with cs2-dumper or offset-tracker repos after major updates.

## API Server Setup (VPS)
```bash
pip install fastapi uvicorn python-dateutil
# Generate self-signed cert or use Let's Encrypt
openssl req -x509 -newkey rsa:4096 -keyout key.pem -out cert.pem -days 365 -nodes
uvicorn main:app --host 0.0.0.0 --port 443 --ssl-keyfile key.pem --ssl-certfile cert.pem
```

Update auth.cpp with your VPS domain in the ENC() strings.

## Admin Panel
- .NET 8, WinForms
- NuGet: Microsoft.Data.Sqlite, BCrypt.Net-Next
- Default login: HillBlack / HillBlack2025! (change hash in Database.Init after first run)
- Keys generated as 32-char hex UUID, stored in hillblack.db
- HWID bound on first successful validation

## Detection Notes
| Feature       | Risk        | Mechanism                         |
|---------------|-------------|-----------------------------------|
| Read handle   | Low         | PROCESS_VM_READ — no write access |
| Silent aim    | Medium-High | WriteProcessMemory to CUserCmd    |
| No-flash      | Medium      | WriteProcessMemory to flash alpha |
| Overlay       | Low         | WDA_EXCLUDEFROMCAPTURE set        |
| SendInput aim | Low         | Mouse input — indistinguishable   |

For GC/ESEA survival: avoid write handle entirely.
Silent aim via write is the main risk vector.
Kernel driver approach for writes: map unsigned driver via vulnerable signed driver (BYOVD),
perform writes via MmCopyVirtualMemory from kernel — bypasses usermode RPM monitoring.
