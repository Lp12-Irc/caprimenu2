from pathlib import Path
from contextlib import closing
import importlib.util,json,sqlite3,subprocess,tempfile,threading,argparse,re
parser=argparse.ArgumentParser();parser.add_argument('probe',type=Path);parser.add_argument('--public',action='store_true');args=parser.parse_args()
root=Path(__file__).resolve().parent.parent
current_build=int(re.search(r'CAPRI_CLIENT_BUILD=(\d+)',(root/'fontes/cs2_external/src/version.hpp').read_text()).group(1))
spec=importlib.util.spec_from_file_location('licenses',root/'servidor/license_server.py');api=importlib.util.module_from_spec(spec);spec.loader.exec_module(api)
def probe(endpoint,key,expected):
 result=subprocess.run([str(args.probe),endpoint,expected],input=key+'\n',text=True,capture_output=True,timeout=25)
 print(result.stdout.strip())
 if result.returncode:raise RuntimeError('Falha na integracao WinHTTP: '+expected)
work=root/'verificacao/testes-integracao';work.mkdir(parents=True,exist_ok=True)
with tempfile.TemporaryDirectory(dir=work) as folder:
 db=Path(folder)/'fixture.db'
 with closing(sqlite3.connect(db)) as c:
  c.executescript("CREATE TABLE keys(key_value TEXT PRIMARY KEY,status TEXT,expires_at TEXT,hwid_bound TEXT); INSERT INTO keys VALUES('active','active','never','different-pc'),('revoked','revoked','never',''),('expired','active','2000-01-01','');");c.commit()
 server=api.LicenseServer(('127.0.0.1',0),db);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
 try:
  endpoint=f'http://127.0.0.1:{server.server_port}/validate'
  for key,expected in [('active','OK'),('missing','BAD_KEY'),('revoked','BAD_KEY'),('expired','EXPIRED')]:probe(endpoint,key,expected)
  probe('http://example.com/validate','active','NET_ERROR')
  with closing(sqlite3.connect(db)) as c:
   c.executescript("CREATE TABLE client_release(id INTEGER PRIMARY KEY,build INTEGER,version TEXT,url TEXT,notes TEXT); INSERT INTO client_release VALUES(1,2,'1.4.0','https://example.com/client.zip','Changes');");c.commit()
  with closing(sqlite3.connect(db)) as c:c.execute('UPDATE client_release SET build=?',(current_build+1,));c.commit()
  probe(endpoint,'active','UPDATE_REQUIRED')
  with closing(sqlite3.connect(db)) as c:c.execute('UPDATE client_release SET build=?',(current_build,));c.commit()
  probe(endpoint,'active','OK')
 finally:server.shutdown();server.server_close();thread.join()
if args.public:
 state=json.loads((root/'servidor/server-state.json').read_text(encoding='utf-8-sig'))
 endpoint=state['url']+'/validate'
 probe(endpoint,'TESTE-INEXISTENTE-PARA-VALIDACAO','BAD_KEY')
 with closing(sqlite3.connect((root/'hillblack.db').as_uri()+'?mode=ro',uri=True)) as c:
  row=c.execute("SELECT key_value FROM keys WHERE status='active' AND (expires_at='never' OR julianday(expires_at)>julianday('now')) LIMIT 1").fetchone()
 if row:probe(endpoint,row[0],'OK');print('Chave ativa do banco central validada por HTTPS; valor nao exibido.')
 else:print('Nenhuma chave ativa no banco central para testar o caminho positivo publico.')
