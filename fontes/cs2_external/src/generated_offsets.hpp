// Generated from the running CS2 process; do not hand-edit.
// Source: ferramentas/cs2-local/{client_dll,offsets,info}.json
#pragma once
#include <cstdint>
namespace offsets {
inline constexpr uint32_t BUILD_NUMBER = 14194;
inline constexpr uint32_t CLIENT_TIMESTAMP = 0x6ae4fb84;
inline constexpr uint32_t CLIENT_IMAGE_SIZE = 0x29a2000;
inline constexpr uint32_t GLOBAL_CURTIME = 0x30; // clock_provenance.json
inline constexpr uint32_t m_iTeamNum = 0x3e7; // C_BaseEntity
inline constexpr uint32_t m_pGameSceneNode = 0x330; // C_BaseEntity
inline constexpr uint32_t m_iHealth = 0x34c; // C_BaseEntity
inline constexpr uint32_t m_iMaxHealth = 0x348; // C_BaseEntity
inline constexpr uint32_t m_hOwnerEntity = 0x520; // C_BaseEntity
inline constexpr uint32_t m_pCollision = 0x340; // C_BaseEntity
inline constexpr uint32_t m_vecMins = 0x40; // CCollisionProperty
inline constexpr uint32_t m_vecMaxs = 0x4c; // CCollisionProperty
inline constexpr uint32_t m_bPawnIsAlive = 0x934; // CCSPlayerController
inline constexpr uint32_t m_hPlayerPawn = 0x92c; // CCSPlayerController
inline constexpr uint32_t m_hObserverPawn = 0x930; // CCSPlayerController
inline constexpr uint32_t m_sSanitizedPlayerName = 0x878; // CCSPlayerController
inline constexpr uint32_t m_iPing = 0x838; // CCSPlayerController
inline constexpr uint32_t m_bPawnHasHelmet = 0x941; // CCSPlayerController
inline constexpr uint32_t m_ArmorValue = 0x1ed0; // C_CSPlayerPawn
inline constexpr uint32_t m_bIsScoped = 0x1ea0; // C_CSPlayerPawn
inline constexpr uint32_t m_bIsDefusing = 0x1ea2; // C_CSPlayerPawn
inline constexpr uint32_t m_angEyeAngles = 0x3600; // C_CSPlayerPawn
inline constexpr uint32_t m_iShotsFired = 0x1eb8; // C_CSPlayerPawn
inline constexpr uint32_t m_entitySpottedState = 0x1e88; // C_CSPlayerPawn
inline constexpr uint32_t m_pAimPunchServices = 0x1598; // C_CSPlayerPawn
inline constexpr uint32_t m_iIDEntIndex = 0x36dc; // C_CSPlayerPawn
inline constexpr uint32_t m_flFlashMaxAlpha = 0x150c; // C_CSPlayerPawnBase
inline constexpr uint32_t m_flFlashOverlayAlpha = 0x1504; // C_CSPlayerPawnBase
inline constexpr uint32_t m_flFlashScreenshotAlpha = 0x1500; // C_CSPlayerPawnBase
inline constexpr uint32_t m_predictableBaseAngle = 0x50; // CCSPlayer_AimPunchServices
inline constexpr uint32_t m_unpredictableBaseAngle = 0xa4; // CCSPlayer_AimPunchServices
inline constexpr uint32_t m_pObserverServices = 0x1308; // C_BasePlayerPawn
inline constexpr uint32_t m_vOldOrigin = 0x14a4; // C_BasePlayerPawn
inline constexpr uint32_t m_pWeaponServices = 0x12f0; // C_BasePlayerPawn
inline constexpr uint32_t m_vecViewOffset = 0xf60; // C_BaseModelEntity
inline constexpr uint32_t m_vecAbsOrigin = 0xc8; // CGameSceneNode
inline constexpr uint32_t m_bDormant = 0x103; // CGameSceneNode
inline constexpr uint32_t m_modelState = 0x140; // CSkeletonInstance
inline constexpr uint32_t m_hModel = 0xa0; // CModelState
inline constexpr uint32_t m_AttributeManager = 0x1290; // C_EconEntity
inline constexpr uint32_t m_Item = 0x50; // C_AttributeContainer
inline constexpr uint32_t m_iItemDefinitionIndex = 0x1ba; // C_EconItemView
inline constexpr uint32_t m_iClip1 = 0x1928; // C_BasePlayerWeapon
inline constexpr uint32_t m_pReserveAmmo = 0x1930; // C_BasePlayerWeapon
inline constexpr uint32_t m_hActiveWeapon = 0x60; // CPlayer_WeaponServices
inline constexpr uint32_t m_hMyWeapons = 0x48; // CPlayer_WeaponServices
inline constexpr uint32_t m_hObserverTarget = 0x4c; // CPlayer_ObserverServices
inline constexpr uint32_t m_bSpottedByMask = 0xc; // EntitySpottedState_t
inline constexpr uint32_t m_bBombTicking = 0x1288; // C_PlantedC4
inline constexpr uint32_t m_flC4Blow = 0x12b8; // C_PlantedC4
inline constexpr uint32_t m_flTimerLength = 0x12c0; // C_PlantedC4
inline constexpr uint32_t m_bBombDefused = 0x12dc; // C_PlantedC4
inline constexpr uint32_t m_flDefuseCountDown = 0x12d8; // C_PlantedC4
inline constexpr uint32_t m_bBeingDefused = 0x12c4; // C_PlantedC4
inline constexpr uint32_t m_nBombSite = 0x128c; // C_PlantedC4
inline constexpr uint32_t m_flDefuseLength = 0x12d4; // C_PlantedC4
inline constexpr uint32_t m_designerName = 0x20; // CEntityIdentity
inline constexpr uint32_t m_pEntity = 0x10; // CEntityInstance
// Entity layout verified against the local controller and pawn handles in build 14194.
inline constexpr uint32_t GLOBAL_MAP_NAME = 0x188; // map_camera_provenance.json, live map name
inline constexpr uint32_t MODEL_BONE_NAME_COUNT = 0x160; // rig_provenance.json, live CModel
inline constexpr uint32_t MODEL_BONE_NAME_DATA = 0x168; // rig_provenance.json, live CModel
inline constexpr uint32_t ENT_LIST_OFFSET = 0x10;
inline constexpr uint32_t ENT_IDENT_SIZE = 0x70;
inline constexpr uint32_t ENT_PAWN_PTR_OFFSET = 0;
inline constexpr uint32_t m_pBoneArray = 0x80; // CModelState; verified position/scale/quaternion records.
}
namespace game_offsets {
inline constexpr uintptr_t dwEntityList = 0x271eb28;
inline constexpr uintptr_t dwGameEntitySystem_highestEntityIndex = 0x2120;
inline constexpr uintptr_t dwLocalPlayerController = 0x25413f8;
inline constexpr uintptr_t dwViewMatrix = 0x256f1d0;
inline constexpr uintptr_t dwGlobalVars = 0x2234108;
inline constexpr uintptr_t dwPlantedC4 = 0x24cfb60;
inline constexpr uintptr_t dwViewAngles = 0x2580008;
inline constexpr uintptr_t dwSensitivity = 0x2566bc8;
inline constexpr uintptr_t dwSensitivity_sensitivity = 0x58;
inline constexpr uintptr_t dwBuildNumber = 0x6ba028;
}
