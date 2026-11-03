import importlib.util,json,sqlite3,tempfile,threading,unittest,urllib.request,urllib.error,hashlib
from pathlib import Path
from contextlib import contextmanager
@contextmanager
def connection(path):
 db=sqlite3.connect(path)
 try:
  with db:yield db
 finally:db.close()

spec=importlib.util.spec_from_file_location('licenses',Path(__file__).parent/'license_server.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
class ServerTests(unittest.TestCase):
 def setUp(self):
  work=Path(__file__).resolve().parent.parent/'verificacao/testes-servidor';work.mkdir(parents=True,exist_ok=True)
  self.temp=tempfile.TemporaryDirectory(dir=work);self.db=Path(self.temp.name)/'test.db'
  with connection(self.db) as c:
   c.executescript("CREATE TABLE keys(key_value TEXT PRIMARY KEY,status TEXT,expires_at TEXT,hwid_bound TEXT); INSERT INTO keys VALUES('active','active','never','owner-pc'),('revoked','revoked','never',''),('expired','active','2000-01-01',''),('invalid-date','active','invalid',''),('future','active','2099-01-01','');")
  self.server=module.LicenseServer(('127.0.0.1',0),self.db);self.thread=threading.Thread(target=self.server.serve_forever,daemon=True);self.thread.start();self.url='http://127.0.0.1:'+str(self.server.server_port)
 def tearDown(self):self.server.shutdown();self.server.server_close();self.thread.join();self.temp.cleanup()
 def post(self,payload,ip=None):
  headers={'Content-Type':'application/json'}
  if ip is not None:headers['CF-Connecting-IP']=ip
  req=urllib.request.Request(self.url+'/validate',json.dumps(payload).encode(),headers)
  try:
   with urllib.request.urlopen(req) as r:return r.status,json.load(r)
  except urllib.error.HTTPError as e:return e.code,json.load(e)
 def test_key_only_and_no_binding_change(self):
  status,data=self.post({'key':'active'});self.assertEqual(status,200);self.assertTrue(data['valid'])
  with connection(self.db) as c:self.assertEqual(c.execute("SELECT hwid_bound FROM keys WHERE key_value='active'").fetchone()[0],'owner-pc')
 def test_invalid_revoked_expired_and_sql(self):
  for key in ['missing','revoked','expired','invalid-date',"' OR 1=1 --"]:self.assertFalse(self.post({'key':key})[1]['valid'])
  self.assertTrue(self.post({'key':'future'})[1]['valid'])
 def test_revocation_is_read_from_same_database(self):
  self.assertTrue(self.post({'key':'active'})[1]['valid'])
  with connection(self.db) as c:c.execute("UPDATE keys SET status='revoked' WHERE key_value='active'")
  self.assertFalse(self.post({'key':'active'})[1]['valid'])
  with connection(self.db) as c:c.execute("UPDATE keys SET status='active' WHERE key_value='active'")
  self.assertTrue(self.post({'key':'active'})[1]['valid'])
 def test_two_second_polling_shared_ip(self):
  for i in range(65):self.assertEqual(self.post({'key':'active','client_build':17})[0],200)
 def test_first_ip_is_bound_and_other_ip_denied(self):
  first=self.post({'key':'active'},'198.51.100.1')[1];self.assertTrue(first['valid']);self.assertEqual(first['ip_count'],1)
  self.assertTrue(self.post({'key':'active'},'198.51.100.1')[1]['valid'])
  rejected=self.post({'key':'active'},'198.51.100.2')[1];self.assertFalse(rejected['valid']);self.assertEqual(rejected['reason'],'ip_limit')
  with connection(self.db) as c:self.assertEqual(c.execute('SELECT ip FROM key_ip_bindings').fetchall(),[('198.51.100.1',)])
 def test_ip_limit_change_reset_and_key_isolation(self):
  self.assertTrue(self.post({'key':'active'},'198.51.100.1')[1]['valid'])
  self.assertTrue(self.post({'key':'future'},'198.51.100.2')[1]['valid'])
  with connection(self.db) as c:c.execute("INSERT INTO key_ip_policy VALUES('active',2)")
  self.assertTrue(self.post({'key':'active'},'198.51.100.2')[1]['valid'])
  self.assertFalse(self.post({'key':'active'},'198.51.100.3')[1]['valid'])
  with connection(self.db) as c:c.execute("DELETE FROM key_ip_bindings WHERE key_value='active'")
  self.assertTrue(self.post({'key':'active'},'198.51.100.3')[1]['valid'])
 def test_ip_canonicalization_and_untrusted_payload(self):
  self.assertTrue(self.post({'key':'active','ip':'1.1.1.1'},'2001:db8::1')[1]['valid'])
  self.assertTrue(self.post({'key':'active'},'2001:0db8:0:0:0:0:0:1')[1]['valid'])
  self.assertEqual(self.post({'key':'active'},'not-an-ip')[0],400)
  from email.message import Message
  headers=Message();headers['CF-Connecting-IP']='203.0.113.1'
  self.assertEqual(module.request_ip(headers,'198.51.100.1'),'198.51.100.1')
 def test_first_ip_race_is_atomic(self):
  from concurrent.futures import ThreadPoolExecutor
  with ThreadPoolExecutor(max_workers=8) as pool:
   results=list(pool.map(lambda i:self.post({'key':'active'},'198.51.100.'+str(i))[1],range(1,9)))
  self.assertEqual(sum(bool(r['valid']) for r in results),1)
  with connection(self.db) as c:self.assertEqual(c.execute('SELECT COUNT(*) FROM key_ip_bindings').fetchone()[0],1)
 def test_rejected_license_never_claims_ip(self):
  for key in ['revoked','expired','missing']:self.assertFalse(self.post({'key':key},'198.51.100.1')[1]['valid'])
  with connection(self.db) as c:self.assertEqual(c.execute('SELECT COUNT(*) FROM key_ip_bindings').fetchone()[0],0)
 def test_bad_payload(self):
  for payload in [None,[],{'key':9},{'key':''},{'key':'x'*4097}]:self.assertEqual(self.post(payload)[0],400)
 def test_expiry_during_session_and_timezone(self):
  import datetime,time
  end=datetime.datetime.now(datetime.timezone.utc)+datetime.timedelta(seconds=2)
  stamp=end.astimezone(datetime.timezone(datetime.timedelta(hours=-3))).isoformat()
  with connection(self.db) as c:c.execute("UPDATE keys SET expires_at=? WHERE key_value='active'",(stamp,))
  self.assertTrue(self.post({'key':'active'})[1]['valid'])
  time.sleep(max(0,(end-datetime.datetime.now(datetime.timezone.utc)).total_seconds())+.05)
  result=self.post({'key':'active'})[1]
  self.assertFalse(result['valid']);self.assertEqual(result['reason'],'expired')
 def test_no_database_download(self):
  with self.assertRaises(urllib.error.HTTPError) as caught:urllib.request.urlopen(self.url+'/hillblack.db')
  self.assertEqual(caught.exception.code,404)
 def test_fail_closed(self):
  self.db.rename(self.db.with_suffix('.gone'));status,data=self.post({'key':'active'});self.assertEqual(status,503);self.assertFalse(data['valid'])
 def test_rate_limit(self):
  self.server.limits['127.0.0.1']=(module.time.monotonic(),module.VALIDATION_LIMIT);self.assertEqual(self.post({'key':'active'})[0],429)
 def test_required_update(self):
  with connection(self.db) as c:
   c.executescript("CREATE TABLE client_release(id INTEGER PRIMARY KEY,build INTEGER,version TEXT,url TEXT,notes TEXT); INSERT INTO client_release VALUES(1,2,'1.4.0','https://example.com/client.zip','Changes');")
  for build in [0,1]:
   data=self.post({'key':'active','client_build':build})[1];self.assertFalse(data['valid']);self.assertEqual(data['reason'],'update_required');self.assertEqual(data['min_build'],2)
  self.assertTrue(self.post({'key':'active','client_build':2})[1]['valid'])
  self.assertTrue(self.post({'key':'active','client_build':3})[1]['valid'])
 def test_bad_build(self):
  for build in [-1,True,'2',2147483648]:self.assertEqual(self.post({'key':'active','client_build':build})[0],400)
 def test_download_package_and_block_private_paths(self):
  folder=self.db.parent/'servidor/releases';folder.mkdir(parents=True)
  payload=b'PK-test-package';digest=hashlib.sha256(payload).hexdigest();(folder/(digest+'.zip')).write_bytes(payload)
  with urllib.request.urlopen(self.url+'/packages/'+digest+'.zip') as response:self.assertEqual(response.read(),payload)
  for path in ['/packages/../test.db','/packages/test.db','/packages/'+('0'*64)+'.zip']:
   with self.assertRaises(urllib.error.HTTPError) as caught:urllib.request.urlopen(self.url+path)
   self.assertEqual(caught.exception.code,404)
if __name__=='__main__':unittest.main()
