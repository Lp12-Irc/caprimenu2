"""Publica um ZIP ja compilado, depois de verificar seu download HTTPS."""
from pathlib import Path, PurePosixPath
from contextlib import closing
import argparse, configparser, hashlib, json, re, shutil, sqlite3, urllib.request
import sys
if sys.platform=='win32':
    import truststore
    truststore.inject_into_ssl()

ROOT = Path(__file__).resolve().parent.parent

def inspect_package(path):
    import zipfile
    size = path.stat().st_size
    if not 0 < size <= 100 * 1024 * 1024:
        raise ValueError('Pacote deve ter ate 100 MB.')
    with zipfile.ZipFile(path) as archive:
        names = set(); expanded = 0
        if len(archive.infolist()) > 1024:
            raise ValueError('Arquivos demais no pacote.')
        for entry in archive.infolist():
            name = entry.filename; normalized = name.rstrip('/').casefold()
            parts = name.rstrip('/').split('/')
            if normalized in names or not normalized or '\\' in name or ':' in name or name.startswith('/') or any(p in ('', '.', '..') for p in parts):
                raise ValueError('Caminho invalido ou repetido no ZIP.')
            names.add(normalized)
            allowed = name in ('Capri.exe', 'client-version.json', 'Atualizar-Capri.ps1', 'HillBlack.ini', 'COMO-ABRIR.txt', 'configs/default.hbc') or name.startswith(('assets/', 'licenses/'))
            if not allowed or PurePosixPath(name).suffix.lower() in ('.db', '.sqlite', '.sqlite3'):
                raise ValueError('O pacote deve conter somente o cliente.')
            expanded += entry.file_size
            if expanded > 150 * 1024 * 1024:
                raise ValueError('Pacote expandido muito grande.')
        for required in ('Capri.exe', 'client-version.json', 'Atualizar-Capri.ps1', 'HillBlack.ini'):
            if required not in archive.namelist():
                raise ValueError('Arquivo obrigatorio ausente: ' + required)
        if archive.getinfo('client-version.json').file_size > 4096:
            raise ValueError('Manifesto invalido.')
        manifest = json.loads(archive.read('client-version.json'))
        if type(manifest.get('build')) is not int or not 1 <= manifest['build'] <= 2147483647 or not isinstance(manifest.get('version'), str) or not 1 <= len(manifest['version']) <= 64:
            raise ValueError('Versao invalida.')
        binary = archive.read('Capri.exe')
        if not binary.startswith(b'MZ') or hashlib.sha256(binary).hexdigest() != manifest.get('sha256'):
            raise ValueError('EXE nao corresponde ao manifesto.')
        if 'configs/default.hbc' in archive.namelist():
            profile = archive.read('configs/default.hbc')
            if len(profile) > 65536 or json.loads(bytes(b ^ 0x4b for b in profile)).get('saved_key', ''):
                raise ValueError('Perfil contem chave salva ou e invalido.')
        config = configparser.ConfigParser(); config.read_string(archive.read('HillBlack.ini').decode('utf-8-sig'))
        return manifest, hashlib.sha256(path.read_bytes()).hexdigest(), size, config.get('auth', 'endpoint')

def main():
    from urllib.parse import urlsplit, urljoin
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--pacote', type=Path, default=ROOT / 'entregas/Capri-Cliente.zip')
    parser.add_argument('--notas', default='Otimizacoes e correcoes do cliente Capri.')
    parser.add_argument('--publicar', action='store_true', help='Exigir este build; sem esta opcao apenas verifica e prepara o download.')
    args = parser.parse_args()
    if len(args.notas) > 2000:
        raise ValueError('Notas muito longas.')
    manifest, digest, size, package_endpoint = inspect_package(args.pacote)
    config = configparser.ConfigParser(); config.read(ROOT / 'HillBlack.ini', encoding='utf-8-sig')
    endpoint = config.get('auth', 'endpoint'); origin = urlsplit(endpoint)
    if origin.scheme != 'https' or not origin.hostname or origin.username or origin.password or endpoint != package_endpoint:
        raise ValueError('O pacote e o servidor precisam usar o mesmo endpoint HTTPS.')
    database = ROOT / 'hillblack.db'
    with closing(sqlite3.connect(database.as_uri() + '?mode=ro', uri=True)) as connection:
        previous = connection.execute('SELECT build,version,url,notes,package_sha256,package_size FROM client_release WHERE id=1').fetchone()
    if previous and (manifest['build'] < previous[0] or manifest['build'] == previous[0] and previous[4] and previous[4] != digest):
        raise ValueError('Arquivos diferentes exigem build maior que o publicado.')
    folder = ROOT / 'servidor/releases'; folder.mkdir(parents=True, exist_ok=True)
    target = folder / (digest + '.zip')
    if not target.exists():
        temporary = target.with_suffix('.part')
        try:
            shutil.copyfile(args.pacote, temporary)
            if hashlib.sha256(temporary.read_bytes()).hexdigest() != digest:
                raise ValueError('Pacote mudou durante a copia.')
            temporary.replace(target)
        finally:
            temporary.unlink(missing_ok=True)
    if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
        raise ValueError('Pacote hospedado corrompido.')
    url = urljoin(endpoint, '/packages/' + digest + '.zip')
    with urllib.request.urlopen(urllib.request.Request(url,headers={'User-Agent':'Capri-Updater/1.0'}), timeout=30) as response:
        downloaded = response.read(size + 1)
        if response.geturl() != url or len(downloaded) != size or hashlib.sha256(downloaded).hexdigest() != digest:
            raise ValueError('Download publico nao corresponde ao pacote.')
    if not args.publicar:
        print(f'HTTPS e pacote verificados: {manifest["version"]}, build {manifest["build"]}. Ainda nao publicado. Use --publicar.')
        return
    backup = ROOT / 'backups/politicas-atualizacao'; backup.mkdir(parents=True, exist_ok=True)
    (backup / f'antes-build-{manifest["build"]}-{digest[:12]}.json').write_text(json.dumps(previous, ensure_ascii=False, indent=2), encoding='utf-8')
    with closing(sqlite3.connect(database)) as connection:
        with connection:
            connection.execute('BEGIN IMMEDIATE')
            current = connection.execute('SELECT build,package_sha256 FROM client_release WHERE id=1').fetchone()
            if current and (manifest['build'] < current[0] or manifest['build'] == current[0] and current[1] and current[1] != digest):
                raise ValueError('Outra publicacao alterou a versao. Revalide o pacote.')
            connection.execute('INSERT INTO client_release(id,build,version,url,notes,package_sha256,package_size) VALUES(1,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET build=excluded.build,version=excluded.version,url=excluded.url,notes=excluded.notes,package_sha256=excluded.package_sha256,package_size=excluded.package_size', (manifest['build'], manifest['version'], url, args.notas, digest, size))
    print(f'Publicado e obrigatorio: {manifest["version"]}, build {manifest["build"]}. Download HTTPS verificado.')

if __name__ == '__main__':
    main()
