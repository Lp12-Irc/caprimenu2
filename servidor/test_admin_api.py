import unittest,tempfile,sqlite3,threading,json,urllib.request,urllib.error,bcrypt
from pathlib import Path
from contextlib import closing
from license_server import LicenseServer

class AdminTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(dir=Path(__file__).resolve().parent.parent/'verificacao');self.path=Path(self.temp.name)/'test.db'
  with closing(sqlite3.connect(self.path)) as db:
   db.executescript("CREATE TABLE admins(username TEXT PRIMARY KEY,password_hash TEXT NOT NULL);CREATE TABLE keys(id INTEGER PRIMARY KEY,key_value TEXT UNIQUE,created_at TEXT,expires_at TEXT,hwid_bound TEXT DEFAULT '',status TEXT DEFAULT 'active',note TEXT DEFAULT '');")
   db.execute('INSERT INTO admins VALUES(?,?)',('owner',bcrypt.hashpw(b'Fixture-Secret-2026!',bcrypt.gensalt(4)).decode()));db.commit()
  self.server=LicenseServer(('127.0.0.1',0),self.path);self.thread=threading.Thread(target=self.server.serve_forever);self.thread.start();self.url='http://127.0.0.1:'+str(self.server.server_port)
 def tearDown(self):self.server.shutdown();self.server.server_close();self.thread.join();self.temp.cleanup()
 def post(self,route,payload,token=None,ip='198.51.100.1'):
  headers={'Content-Type':'application/json','CF-Connecting-IP':ip}
  if token:headers['Authorization']='Bearer '+token
  try:
   with urllib.request.urlopen(urllib.request.Request(self.url+route,json.dumps(payload).encode(),headers),timeout=8) as response:return response.status,json.load(response)
  except urllib.error.HTTPError as e:return e.code,json.load(e)
 def login(self):return self.post('/admin/login',{'username':'owner','password':'Fixture-Secret-2026!'})[1]['result']['token']
 def action(self,token,action,**values):return self.post('/admin/action',{'action':action,**values},token)
 def test_unauthenticated_and_wrong_password(self):
  self.assertEqual(self.action(None,'keys_list')[0],401)
  self.assertEqual(self.post('/admin/login',{'username':'owner','password':'wrong'})[0],401)
  self.assertEqual(self.post('/admin/login',{'username':"' OR 1=1 --",'password':'wrong'})[0],401)
 def test_shared_crud_ip_and_no_admin_registration(self):
  token=self.login();status,result=self.action(token,'keys_create',note='remote owner',expires=None);self.assertEqual(status,200);key=result['result']
  rows=self.action(token,'keys_list')[1]['result'];self.assertEqual(rows[0]['KeyValue'],key)
  self.assertTrue(self.server.validate(key,18,'198.51.100.2')['valid'])
  self.assertEqual(self.action(token,'ips_list',key=key)[1]['result'][0]['Ip'],'198.51.100.2')
  self.action(token,'ips_limit',key=key,limit=2);self.assertTrue(self.server.validate(key,18,'198.51.100.3')['valid'])
  self.action(token,'ips_limit',key=key,limit=1);self.assertEqual(len(self.action(token,'ips_list',key=key)[1]['result']),1)
  self.action(token,'keys_revoke',key=key);self.assertFalse(self.server.validate(key,18,'198.51.100.2')['valid'])
  self.action(token,'keys_activate',key=key);self.assertTrue(self.server.validate(key,18,'198.51.100.2')['valid'])
  self.action(token,'ips_reset',key=key);self.assertEqual(self.action(token,'ips_list',key=key)[1]['result'],[])
  self.assertEqual(self.action(token,'admin_create',key=key)[0],400)
 def test_delete_key_cleans_only_selected_license(self):
  token=self.login();key=self.action(token,'keys_create',note='delete fixture',expires=None)[1]['result']
  other=self.action(token,'keys_create',note='keep fixture',expires=None)[1]['result']
  self.assertTrue(self.server.validate(key,25,'198.51.100.2')['valid'])
  self.action(token,'ips_limit',key=key,limit=2)
  self.assertEqual(self.action(None,'keys_delete',key=key)[0],401)
  self.assertEqual(self.action(token,'keys_delete',key=key)[0],200)
  self.assertFalse(self.server.validate(key,25,'198.51.100.2')['valid'])
  self.assertTrue(self.server.validate(other,25,'198.51.100.2')['valid'])
  self.assertEqual(self.action(token,'keys_delete',key=key)[0],404)
  with closing(sqlite3.connect(self.path)) as db:
   for table in ['keys','key_ip_bindings','key_ip_policy']:
    self.assertEqual(db.execute('SELECT COUNT(*) FROM '+table+' WHERE key_value=?',(key,)).fetchone()[0],0)
 def test_session_ip_expiry_logout_and_password_change(self):
  token=self.login();self.assertEqual(self.post('/admin/action',{'action':'keys_list'},token,'198.51.100.2')[0],401)
  self.action(token,'logout');self.assertEqual(self.action(token,'keys_list')[0],401)
  token=self.login();self.assertFalse(self.action(token,'password_change',current='wrong',password='New-Fixture-2026!')[1]['result'])
  self.assertTrue(self.action(token,'password_change',current='Fixture-Secret-2026!',password='New-Fixture-2026!')[1]['result'])
  with closing(sqlite3.connect(self.path)) as db:self.assertTrue(db.execute("SELECT password_hash FROM admins WHERE username='owner'").fetchone()[0].startswith('$2b$12$'))
  self.assertEqual(self.action(token,'keys_list')[0],401)
  self.assertEqual(self.post('/admin/login',{'username':'owner','password':'New-Fixture-2026!'})[0],200)
 def test_invalid_limit_and_login_rate(self):
  token=self.login();key=self.action(token,'keys_create',expires=None,note='')[1]['result']
  for limit in [0,33,True,'2']:self.assertEqual(self.action(token,'ips_limit',key=key,limit=limit)[0],400)
  with self.server.admin.lock:self.server.admin.failures['198.51.100.1']=(__import__('time').monotonic(),30)
  self.assertEqual(self.post('/admin/login',{'username':'owner','password':'wrong'})[0],429)
 def test_no_private_files_or_secrets_in_errors(self):
  for path in ['/hillblack.db','/admin-login.dat','/credentials.dat','/servidor/server-state.json','/servidor/admin_api.py','/.git/config','/packages/../hillblack.db']:
   with self.assertRaises(urllib.error.HTTPError) as caught:urllib.request.urlopen(self.url+path)
   self.assertEqual(caught.exception.code,404)
  status,data=self.action(None,'keys_list');self.assertEqual(status,401);self.assertNotIn('result',data)
  serialized=json.dumps(data);self.assertNotIn('password_hash',serialized);self.assertNotIn('Fixture-Secret',serialized)

if __name__=='__main__':unittest.main()
