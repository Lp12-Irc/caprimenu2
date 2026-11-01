"""Prepare map textures and their coordinates from the installed game's VPK export."""
import json,re,shutil,hashlib,struct
from pathlib import Path
root=Path(__file__).parent.parent
export=root/'fontes/assets/radar-export'
target=root/'fontes/assets/radar'; target.mkdir(parents=True,exist_ok=True)
textures=export/'panorama/images/overheadmaps'
def parse_kv(text):
    tokens=re.findall(r'"([^"\r\n]*)"|([{}])',re.sub(r'//[^\r\n]*','',text))
    values=[string if not brace else brace for string,brace in tokens]; cursor=0
    def obj():
        nonlocal cursor
        result={}
        while cursor<len(values) and values[cursor]!='}':
            key=values[cursor]; cursor+=1
            value=values[cursor]; cursor+=1
            if value=='{': value=obj()
            result[key]=value
        if cursor<len(values): cursor+=1
        return result
    return next(iter(obj().values()))
def image_for(name):
    for suffix in ('_radar_psd.png','_radar_tga.png','_radar.png'):
        path=textures/(name+suffix)
        if path.exists():
            shutil.copy2(path,target/path.name)
            return path.name
    return None
prepared=[]
for overview in sorted((export/'resource/overviews').glob('*.txt')):
    data=parse_kv(overview.read_text(encoding='utf-8-sig'))
    if not all(key in data for key in ('pos_x','pos_y','scale')): continue
    name=overview.stem
    image=image_for(name)
    if not image:
        material=Path(data.get('material','')).name
        for alias in (material,material.removesuffix('_radar'),name.removesuffix('_v2')):
            image=image_for(alias)
            if image: break
    if not image: continue
    floors=[]
    for section,limits in data.get('verticalsections',{'default':{'AltitudeMin':-100000,'AltitudeMax':100000}}).items():
        floor_image=image if section=='default' else image_for(name+'_'+section)
        if not floor_image: continue
        floors.append({'name':section,'image':floor_image,'min_z':float(limits['AltitudeMin']),'max_z':float(limits['AltitudeMax'])})
    landmarks=[]
    for key,label in [('bombA','A'),('bombB','B')]:
        if key+'_x' in data and key+'_y' in data:
            landmarks.append({'label':label,'x':float(data[key+'_x']),'y':float(data[key+'_y'])})
    record={'name':name,'pos_x':float(data['pos_x']),'pos_y':float(data['pos_y']),'scale':float(data['scale']),
        'reference_size':1024,'image':image,'floors':floors,'landmarks':landmarks,
        'source':'Installed CS2 pak01_dir.vpk: resource/overviews/'+overview.name,
        'source_sha256':hashlib.sha256(overview.read_bytes()).hexdigest()}
    (target/(name+'.json')).write_text(json.dumps(record,indent=2))
    prepared.append(name)
(target/'PROVENANCE.txt').write_text('Extracted from the installed CS2 Steam build 26334290, engine build 14194.\n'
    'Valve overview coordinates and radar images; extracted with Source 2 Viewer CLI 20.1.\n'
    'Tool source: https://github.com/ValveResourceFormat/ValveResourceFormat\n'
    'Map images are Valve game assets used locally by this installation.\n')
print(json.dumps({'count':len(prepared),'maps':prepared}))
