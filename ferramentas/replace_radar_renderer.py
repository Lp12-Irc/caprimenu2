from pathlib import Path
p=Path(__file__).parent.parent/'fontes/cs2_external/src/esp.cpp'
s=p.read_text(encoding='utf-8'); start=s.index('void Esp::draw_radar('); end=s.index('// ─── Crosshair',start)
s=s[:start]+'void Esp::draw_radar(ImDrawList* dl,const EntityCache& cache,const Config& cfg,int sw,int sh) {\n    radar_.draw(dl,cache,cfg,sw,sh);\n}\n\n'+s[end:]
p.write_text(s,encoding='utf-8')
