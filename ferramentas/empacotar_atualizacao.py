"""Build an updater payload for existing installations from the verified full ZIP.

Builds 18 and 19 already contain the same radar/collision assets. Keep those
installed files and include every neon agent mesh, so either build can upgrade.
Fresh installations should continue using Capri-Cliente.zip.
"""
from pathlib import Path
import hashlib,json,zipfile

ROOT=Path(__file__).resolve().parent.parent

def main():
    source=ROOT/'entregas/Capri-Cliente.zip'
    output=ROOT/'entregas/Capri-Atualizacao.zip'
    with zipfile.ZipFile(source) as original,zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED,compresslevel=9) as package:
        assert original.testzip() is None
        version=json.loads(original.read('client-version.json'))
        assert version['build']>=20
        for entry in original.infolist():
            if entry.filename.startswith(('assets/collision/','assets/radar/')):continue
            package.writestr(entry,original.read(entry.filename))
    with zipfile.ZipFile(output) as package:
        assert package.testzip() is None
        assert hashlib.sha256(package.read('Capri.exe')).hexdigest()==version['sha256']
        models=json.loads(package.read('assets/models/manifest.json'))
        for model in models['models'].values():
            assert hashlib.sha256(package.read('assets/models/'+model['file'])).hexdigest()==model['sha256']
    print(f'Atualizacao de instalacoes existentes: {output} ({output.stat().st_size/1024**2:.2f} MiB); mapas existentes preservados.')

if __name__=='__main__':main()
