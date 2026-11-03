"""Generate only the fields Capri uses from a local cs2-dumper extraction."""
import json
import hashlib, struct, re
from pathlib import Path
ROOT=Path(__file__).parent
dump=ROOT/'cs2-local'
schema=json.loads((dump/'client_dll.json').read_text())['client.dll']['classes']
globals_=json.loads((dump/'offsets.json').read_text())
info=json.loads((dump/'info.json').read_text())
client_path=Path(r'F:\SteamLibrary\steamapps\common\Counter-Strike Global Offensive\game\csgo\bin\win64\client.dll')
client_bytes=client_path.read_bytes()
pe=struct.unpack_from('<I',client_bytes,0x3c)[0]
timestamp=struct.unpack_from('<I',client_bytes,pe+8)[0]
image_size=struct.unpack_from('<I',client_bytes,pe+0x50)[0]
clock=json.loads((dump/'clock_provenance.json').read_text())
if clock['client_sha256'] != hashlib.sha256(client_bytes).hexdigest() or clock['build'] != info['build_number']:
    raise RuntimeError('Clock derivation belongs to a different client.dll')
rig=json.loads((dump/'rig_provenance.json').read_text())
if rig['client_sha256'] != hashlib.sha256(client_bytes).hexdigest() or rig['build'] != info['build_number']:
    raise RuntimeError('Rig derivation belongs to a different client.dll or build')
steam_manifest=(client_path.parents[6]/'appmanifest_730.acf').read_text()
steam_build=re.search(r'"buildid"\s+"(\d+)"',steam_manifest)
if not steam_build: raise RuntimeError('Installed Steam build ID not found')
extraction=json.loads((dump/'extraction.json').read_text())
if extraction['client_sha256']!=hashlib.sha256(client_bytes).hexdigest() or extraction['build_number']!=info['build_number']:
    raise RuntimeError('Schema extraction belongs to another binary or build')
for filename,digest in extraction['files'].items():
    if hashlib.sha256((dump/filename).read_bytes()).hexdigest()!=digest:
        raise RuntimeError(f'Extraction file changed: {filename}')
provenance={'build_number':info['build_number'],'dump_timestamp':info['timestamp'],
 'steam_build_id':steam_build[1],'client_sha256':hashlib.sha256(client_bytes).hexdigest(),
 'client_pe_timestamp':timestamp,'client_image_size':image_size,
 'source':'https://github.com/a2x/cs2-dumper','source_commit':'21ee85107074fdf0347dc8afd4a8c3dd8d2398e2',
 'dumper_sha256':extraction['dumper_sha256'],
 'method':'Local cs2-dumper extraction from running process using native read-only memory access; entity and bone layouts checked against live controller and pawn references.'}
(dump/'provenance.json').write_text(json.dumps(provenance,indent=2))
mapping={
 'C_BaseEntity':['m_iTeamNum','m_pGameSceneNode','m_iHealth','m_iMaxHealth','m_hOwnerEntity','m_pEntity','m_pCollision','m_vecAbsVelocity'],
 'CCollisionProperty':['m_vecMins','m_vecMaxs'],
 'CCSPlayerController':['m_bPawnIsAlive','m_hPlayerPawn','m_hObserverPawn','m_sSanitizedPlayerName','m_iPing','m_bPawnHasHelmet'],
 'C_CSPlayerPawn':['m_ArmorValue','m_bIsScoped','m_bIsDefusing','m_angEyeAngles','m_iShotsFired','m_entitySpottedState','m_pAimPunchServices','m_iIDEntIndex'],
 'C_CSPlayerPawnBase':['m_flFlashMaxAlpha','m_flFlashOverlayAlpha','m_flFlashScreenshotAlpha'],
 'CCSPlayer_AimPunchServices':['m_predictableBaseAngle','m_unpredictableBaseAngle'],
 'C_BasePlayerPawn':['m_pObserverServices','m_vOldOrigin','m_pWeaponServices'],
 'C_BaseModelEntity':['m_vecViewOffset'],
 'CGameSceneNode':['m_vecAbsOrigin','m_bDormant'],
 'CSkeletonInstance':['m_modelState'],
 'CModelState':['m_hModel','m_ModelName'],
 'C_EconEntity':['m_AttributeManager'],
 'C_AttributeContainer':['m_Item'],
 'C_EconItemView':['m_iItemDefinitionIndex'],
 'C_BasePlayerWeapon':['m_iClip1','m_pReserveAmmo'],
 'CPlayer_WeaponServices':['m_hActiveWeapon','m_hMyWeapons'],
 'CPlayer_ObserverServices':['m_hObserverTarget'],
 'EntitySpottedState_t':['m_bSpottedByMask'],
 'C_PlantedC4':['m_bBombTicking','m_flC4Blow','m_flTimerLength','m_bBombDefused','m_flDefuseCountDown','m_bBeingDefused','m_nBombSite','m_flDefuseLength'],
 'CEntityIdentity':['m_designerName'],
 'CEntityInstance':['m_pEntity'],
}
lines=['// Generated from the running CS2 process; do not hand-edit.',
       '// Source: ferramentas/cs2-local/{client_dll,offsets,info}.json',
       '#pragma once','#include <cstdint>','namespace offsets {',
       f'inline constexpr uint32_t BUILD_NUMBER = {info["build_number"]};',
       f'inline constexpr uint32_t CLIENT_TIMESTAMP = {hex(timestamp)};',
       f'inline constexpr uint32_t CLIENT_IMAGE_SIZE = {hex(image_size)};',
       f'inline constexpr uint32_t GLOBAL_CURTIME = {clock["CGlobalVars_curtime"]}; // clock_provenance.json']
for cls,names in mapping.items():
    fields=schema[cls]['fields']
    for name in names:
        if name=='m_pEntity' and cls=='C_BaseEntity': continue
        lines.append(f'inline constexpr uint32_t {name} = {hex(fields[name])}; // {cls}')
lines+=[f'// Entity layout verified against the local controller and pawn handles in build {info["build_number"]}.',
        'inline constexpr uint32_t GLOBAL_MAP_NAME = 0x188; // map_camera_provenance.json, live map name',
        f'inline constexpr uint32_t MODEL_BONE_NAME_COUNT = {rig["CModel_bone_name_count"]}; // rig_provenance.json, live CModel',
        f'inline constexpr uint32_t MODEL_BONE_NAME_DATA = {rig["CModel_bone_name_data"]}; // rig_provenance.json, live CModel',
        'inline constexpr uint32_t ENT_LIST_OFFSET = 0x10;',
        'inline constexpr uint32_t ENT_IDENT_SIZE = 0x70;',
        'inline constexpr uint32_t ENT_PAWN_PTR_OFFSET = 0;',
        'inline constexpr uint32_t m_pBoneArray = 0x80; // CModelState; verified position/scale/quaternion records.',
        '}', 'namespace game_offsets {']
for module,names in {'client.dll':['dwEntityList','dwGameEntitySystem_highestEntityIndex','dwLocalPlayerController','dwViewMatrix','dwGlobalVars','dwPlantedC4','dwViewAngles','dwSensitivity','dwSensitivity_sensitivity'], 'engine2.dll':['dwBuildNumber']}.items():
    for name in names: lines.append(f'inline constexpr uintptr_t {name} = {hex(globals_[module][name])};')
lines.append('}')
target=ROOT.parent/'fontes/cs2_external/src/generated_offsets.hpp'
target.write_text('\n'.join(lines)+'\n')
print(f'Generated {target} for engine build {info["build_number"]}')
