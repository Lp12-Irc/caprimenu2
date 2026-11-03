"""Check generated offsets against the running game without writes or input."""
import contextlib
import hashlib
import io
import json
import math
from pathlib import Path
import re
import struct

with contextlib.redirect_stdout(io.StringIO()):
    import inspect_game as game

ROOT = Path(__file__).resolve().parent.parent
header = (ROOT / 'fontes/cs2_external/src/generated_offsets.hpp').read_text()
values = {name: int(value, 0) for name, value in re.findall(
    r'inline constexpr (?:uint32_t|uintptr_t) (\w+) = (0x[0-9a-fA-F]+|\d+);', header)}
proof = json.loads((ROOT / 'ferramentas/cs2-local/provenance.json').read_text())
client = Path(r'F:\SteamLibrary\steamapps\common\Counter-Strike Global Offensive\game\csgo\bin\win64\client.dll')
assert hashlib.sha256(client.read_bytes()).hexdigest() == proof['client_sha256']

game.handle = game.k.OpenProcess(0x410, False, game.pid)
if not game.handle:
    raise RuntimeError('Read-only game access unavailable')
try:
    base = game.base
    build = game.read(game.modules['engine2.dll'][0] + values['dwBuildNumber'], 'I')
    assert build == values['BUILD_NUMBER'] == proof['build_number']
    pe = game.read(base + 0x3c, 'I')
    assert game.read(base + pe + 8, 'I') == values['CLIENT_TIMESTAMP']
    assert game.read(base + pe + 0x50, 'I') == values['CLIENT_IMAGE_SIZE']
    system = game.read(base + values['dwEntityList'])
    controller = game.read(base + values['dwLocalPlayerController'])
    assert system and controller

    def entity(index):
        chunk = game.read(system + values['ENT_LIST_OFFSET'] + 8 * ((index & 0x7fff) >> 9))
        return game.read(chunk + values['ENT_IDENT_SIZE'] * (index & 0x1ff) + values['ENT_PAWN_PTR_OFFSET']) if chunk else 0

    controller_index = next((i for i in range(1, 65) if entity(i) == controller), None)
    assert controller_index is not None
    handle = game.read(controller + values['m_hPlayerPawn'], 'I')
    pawn = entity(handle & 0x7fff) if handle and handle != 0xffffffff else 0
    assert pawn, 'A player or main-menu agent must be loaded for rig validation'
    health = game.read(pawn + values['m_iHealth'], 'i')
    assert 0 <= health <= 1000
    scene = game.read(pawn + values['m_pGameSceneNode'])
    assert scene
    state = scene + values['m_modelState']
    name = game.raw(game.read(state + values['m_ModelName']), 200).split(b'\0')[0].decode('ascii')
    assert name.startswith('agents/models/')
    model = game.read(game.read(state + values['m_hModel']))
    count = game.read(model + values['MODEL_BONE_NAME_COUNT'], 'I')
    names = game.read(model + values['MODEL_BONE_NAME_DATA'])
    assert 16 <= count <= 128 and names
    bones = [game.raw(game.read(names + i * 8), 80).split(b'\0')[0].decode('ascii') for i in range(count)]
    assert {'pelvis', 'head_0', 'hand_L', 'hand_R'}.issubset(bones)
    transforms = game.raw(game.read(state + values['m_pBoneArray']), count * 32)
    assert len(transforms) == count * 32
    assert all(math.isfinite(x) for x in struct.unpack(f'{count * 8}f', transforms))
    matrix = struct.unpack('16f', game.raw(base + values['dwViewMatrix'], 64))
    assert all(math.isfinite(x) for x in matrix) and sum(abs(x) for x in matrix[12:]) > .001
    sensitivity = game.read(game.read(base + values['dwSensitivity']) + values['dwSensitivity_sensitivity'], 'f')
    assert .01 <= sensitivity <= 20
    current_time = game.read(game.read(base + values['dwGlobalVars']) + values['GLOBAL_CURTIME'], 'f')
    assert math.isfinite(current_time) and -10 <= current_time < 1e9
    report = {'build': build, 'steam_build_id': proof['steam_build_id'], 'client_sha256': proof['client_sha256'],
              'context': 'match' if game.read(controller + values['m_bPawnIsAlive'], 'B') else 'menu or observer',
              'entity_layout_verified': True, 'model': name, 'bone_count': count,
              'bone_transforms_readable': True, 'matrix_valid': True, 'sensitivity': sensitivity,
              'global_clock_finite': True, 'read_only': True}
    output = ROOT / 'verificacao' / f'live-offsets-{build}.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2), encoding='utf-8')
    print(json.dumps(report, indent=2))
finally:
    game.k.CloseHandle(game.handle)
