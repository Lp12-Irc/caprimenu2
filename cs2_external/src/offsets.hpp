// language: C++20, file: offsets.hpp, target: CS2 Source 2, Windows 11 x64
// Patterns resolve at runtime — survive updates as long as surrounding code path is stable
// Re-derive workflow (after update that breaks a pattern):
//   1. Load new client.dll in IDA/Ghidra
//   2. Search string xrefs for known identifiers (e.g. "local_player_controller")
//   3. Find the MOV/LEA loading the global pointer in that context
//   4. Select 8-12 bytes around the RIP-relative instruction, wildcarding the 4-byte offset
//   5. Replace the pattern constant below
#pragma once
#include <cstdint>

namespace patterns {

// --- client.dll ---

// CEntitySystem* — lea rcx,[rip+?] near CEntitySystem ctor xref
// RIP-resolve: read int32 at +3, add to (addr+7)
constexpr const char* ENTITY_SYSTEM =
    "48 8B 0D ? ? ? ? 48 89 7C 24 ? 8B FA";

// CCSPlayerController** (local player controller slot)
// RIP-resolve: read int32 at +3, add to (addr+7)
constexpr const char* LOCAL_PLAYER_CTRL =
    "48 8B 05 ? ? ? ? 48 85 C0 74 ? 48 8B 88 ? ? ? ? 48 85 C9 74";

// ViewMatrix[4][4] — lea rax,[rip+?] near CRenderGameSystem view setup
// RIP-resolve: read int32 at +3, add to (addr+7)
constexpr const char* VIEW_MATRIX =
    "48 8D 0D ? ? ? ? 48 C1 E0 06 48 03 C1 C3";

// PlantedC4 list — 48 8B 15 ? ? ? ? near "planted_c4_list" xref
constexpr const char* PLANTED_C4 =
    "48 8B 15 ? ? ? ? 41 8B 80 ? ? ? ?";

// m_angEyeAngles direct — search EyeAngles function, grab MOV loading angles
// Fallback: derive from LocalPlayerController → pawn → +0x1398
constexpr const char* EYE_ANGLES =
    "F3 0F 11 89 ? ? ? ? F3 0F 11 81 ? ? ? ? F3 0F 11 91 ? ? ? ?";
// offset extracted at match+4 (int32, added directly to struct base)

} // namespace patterns

namespace offsets {

// ── CBaseEntity ────────────────────────────────────────────────────────────
constexpr uint32_t m_iTeamNum             = 0x3CB;
constexpr uint32_t m_pGameSceneNode       = 0x318; // → CGameSceneNode*

// ── CCSPlayerController ────────────────────────────────────────────────────
constexpr uint32_t m_bPawnIsAlive         = 0x6B8;
constexpr uint32_t m_hPawn                = 0x6C8; // CHandle<C_CSPlayerPawn>
constexpr uint32_t m_sSanitizedPlayerName = 0x710; // char* (heap)
constexpr uint32_t m_iPing                = 0xB08;

// ── C_CSPlayerPawn / CCSPlayerPawnBase ────────────────────────────────────
constexpr uint32_t m_iHealth              = 0x334;
constexpr uint32_t m_iMaxHealth           = 0x338;
constexpr uint32_t m_ArmorValue           = 0xE10;
constexpr uint32_t m_bHasHelmet           = 0xE14;
constexpr uint32_t m_bHasHeavyArmor       = 0xE15;
constexpr uint32_t m_flFlashAlpha         = 0xCC0; // write requires VM_WRITE
constexpr uint32_t m_bIsScoped            = 0xCD1;
constexpr uint32_t m_bIsDefusing          = 0x1940;
constexpr uint32_t m_aimPunchAngle        = 0x1664; // QAngle (pitch, yaw, roll)
constexpr uint32_t m_aimPunchAngleVel     = 0x1670;
constexpr uint32_t m_angEyeAngles         = 0x1398; // QAngle — verify per build
constexpr uint32_t m_iIDShotsFired        = 0x1470; // shot count for RCS sync
constexpr uint32_t m_pClippingWeapon      = 0x130C; // CHandle<C_CSWeaponBase>
constexpr uint32_t m_hObserverTarget      = 0x111C; // who this pawn is spectating
constexpr uint32_t m_vOldOrigin           = 0x1274; // Vector (lerp position)
constexpr uint32_t m_vecLastClipCameraPos = 0x128C;

// ── CGameSceneNode ─────────────────────────────────────────────────────────
constexpr uint32_t m_vecAbsOrigin         = 0xC8;  // Vector — world position
constexpr uint32_t m_modelState           = 0x170; // CModelState

// ── CModelState ────────────────────────────────────────────────────────────
constexpr uint32_t m_pBoneArray           = 0x80;  // bone_matrix_t* (external heap)
// Each bone: 32 bytes = matrix3x4 (float[3][4])
// Position: bone[i].m[0][3], [1][3], [2][3]

// ── C_CSWeaponBase ─────────────────────────────────────────────────────────
constexpr uint32_t m_AttributeManager     = 0xE28; // CEconItemView
constexpr uint32_t m_iItemDefinitionIndex = 0xD8;  // relative to AttributeManager base
constexpr uint32_t m_iClip1               = 0x1698;
constexpr uint32_t m_pReserveAmmoCount    = 0x16AC;
constexpr uint32_t m_flNextPrimaryAttack  = 0x16C8;

// ── CPlantedC4 ─────────────────────────────────────────────────────────────
constexpr uint32_t m_bBombTicking         = 0xE00;
constexpr uint32_t m_flC4Blow             = 0xE08; // time of detonation (GlobalVars->curtime)
constexpr uint32_t m_flTimerLength        = 0xE18;
constexpr uint32_t m_bBombDefused         = 0xE28;
constexpr uint32_t m_bBombDefuseEnded     = 0xE29;
constexpr uint32_t m_flDefuseCountDown    = 0xE30;
constexpr uint32_t m_nBombSite            = 0xE3C; // 0=A, 1=B

// ── CEntitySystem internal list ───────────────────────────────────────────
constexpr uint32_t ENT_LIST_OFFSET        = 0x10;  // CEntityList within CEntitySystem
constexpr uint32_t ENT_IDENT_SIZE         = 0x78;  // sizeof(CEntityIdentity)
constexpr uint32_t ENT_PAWN_PTR_OFFSET    = 0x10;  // IClientEntity* within identity

// ── GlobalVars ─────────────────────────────────────────────────────────────
// For curtime (bomb timer math): engine2.dll pattern near "gpGlobals"
// curtime offset within CGlobalVarsBase: 0x34 (float)

} // namespace offsets

namespace bones {
    constexpr int HEAD      = 6;
    constexpr int NECK      = 5;
    constexpr int CHEST_UP  = 4;
    constexpr int CHEST_LO  = 2;
    constexpr int LSHOULDER = 8;
    constexpr int RSHOULDER = 13;
    constexpr int LELBOW    = 9;
    constexpr int RELBOW    = 14;
    constexpr int LWRIST    = 10;
    constexpr int RWRIST    = 15;
    constexpr int LHIP      = 22;
    constexpr int RHIP      = 25;
    constexpr int LKNEE     = 23;
    constexpr int RKNEE     = 26;
    constexpr int LANKLE    = 24;
    constexpr int RANKLE    = 27;
    constexpr int ROOT      = 0;

    // Skeleton draw pairs (from → to)
    constexpr int PAIRS[][2] = {
        {HEAD, NECK}, {NECK, CHEST_UP}, {CHEST_UP, CHEST_LO},
        {CHEST_UP, LSHOULDER}, {LSHOULDER, LELBOW}, {LELBOW, LWRIST},
        {CHEST_UP, RSHOULDER}, {RSHOULDER, RELBOW}, {RELBOW, RWRIST},
        {CHEST_LO, LHIP}, {LHIP, LKNEE}, {LKNEE, LANKLE},
        {CHEST_LO, RHIP}, {RHIP, RKNEE}, {RKNEE, RANKLE}
    };
    constexpr int PAIR_COUNT = 15;
}

namespace weapons {
    inline const char* name(uint16_t id) {
        switch (id) {
            case 1:  return "DEagle";   case 2:  return "Duals";
            case 3:  return "FiveSeveN";case 4:  return "Glock";
            case 7:  return "AK-47";    case 8:  return "AUG";
            case 9:  return "AWP";      case 10: return "FAMAS";
            case 11: return "G3SG1";    case 13: return "Galil";
            case 14: return "M249";     case 16: return "M4A4";
            case 17: return "MAC-10";   case 19: return "P90";
            case 23: return "MP5-SD";   case 24: return "UMP-45";
            case 25: return "XM1014";   case 26: return "Bizon";
            case 27: return "MAG-7";    case 28: return "Negev";
            case 29: return "Sawed-Off";case 30: return "Tec-9";
            case 31: return "Zeus";     case 32: return "P2000";
            case 33: return "MP7";      case 34: return "MP9";
            case 35: return "Nova";     case 36: return "P250";
            case 38: return "SCAR-20";  case 39: return "SG553";
            case 40: return "SSG08";    case 60: return "M4A1-S";
            case 61: return "USP-S";    case 63: return "CZ75";
            case 64: return "R8";
            // Utilities
            case 43: return "Flash";    case 44: return "HE";
            case 45: return "Smoke";    case 46: return "Molotov";
            case 47: return "Decoy";    case 48: return "Incend.";
            case 49: return "C4";
            default: return "???";
        }
    }

    inline const char* util_tag(uint16_t id) {
        switch (id) {
            case 43: return "FL"; case 44: return "HE";
            case 45: return "SM"; case 46: return "MOL";
            case 47: return "DEC"; case 48: return "INC";
            default: return nullptr;
        }
    }

    inline bool is_utility(uint16_t id) {
        return id == 43 || id == 44 || id == 45 ||
               id == 46 || id == 47 || id == 48;
    }
}
