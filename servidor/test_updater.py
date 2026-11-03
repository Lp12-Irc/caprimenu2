import hashlib,json,os,shutil,stat,subprocess,tempfile,unittest,zipfile,threading
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer
from pathlib import Path
ROOT=Path(__file__).resolve().parent.parent
WORK=ROOT/'verificacao/testes-atualizador';WORK.mkdir(parents=True,exist_ok=True)
PS=Path(os.environ['SystemRoot'])/'System32/WindowsPowerShell/v1.0/powershell.exe'
SCRIPT=ROOT/'Atualizar-Capri.ps1'
class InstallerTests(unittest.TestCase):
 def setUp(self):
  self.temp=tempfile.TemporaryDirectory(dir=WORK);self.root=Path(self.temp.name)
  shutil.copy2(SCRIPT,self.root/SCRIPT.name)
  (self.root/'configs').mkdir();(self.root/'assets').mkdir();(self.root/'atualizacao').mkdir()
  (self.root/'configs/active-profile.txt').write_bytes(b'Minha config')
  (self.root/'configs/Minha config.hbc').write_bytes(b'KEEP-NAMED-PROFILE')
  (self.root/'Capri.exe').write_bytes(b'OLD-EXE')
  (self.root/'client-version.json').write_text(json.dumps({'build':1,'version':'old'}))
  self.profile=bytes(x^0x4b for x in json.dumps({'saved_key':'KEEP-KEY','aim_fov':7,'radar_sz':120}).encode());(self.root/'configs/default.hbc').write_bytes(self.profile)
  self.connection=b'[auth]\nmode=online\nendpoint=https://example.com/validate\n';(self.root/'HillBlack.ini').write_bytes(self.connection)
  (self.root/'hillblack.db').write_bytes(b'KEEP-DATABASE')
  (self.root/'credentials.dat').write_bytes(b'KEEP-PROTECTED-CREDENTIALS')
  (self.root/'assets/changed.dat').write_bytes(b'OLD-ASSET')
  (self.root/'assets/unchanged.dat').write_bytes(b'SAME-ASSET')
  os.utime(self.root/'assets/unchanged.dat',(900000000,900000000))
 def tearDown(self):self.temp.cleanup()
 def package(self,extra=None,build=3,executable=None):
  binary=executable if executable is not None else b'MZ'+bytes(2046);manifest={'build':build,'version':'1.4.1','sha256':hashlib.sha256(binary).hexdigest()}
  files={'Capri.exe':binary,'client-version.json':json.dumps(manifest).encode(),'Atualizar-Capri.ps1':SCRIPT.read_bytes(),'configs/default.hbc':bytes(x^0x4b for x in json.dumps({'saved_key':''}).encode()),'HillBlack.ini':b'NEW-SERVER-URL','assets/changed.dat':b'NEW-ASSET','assets/unchanged.dat':b'SAME-ASSET','licenses/new.txt':b'NEW-LICENSE'}
  files.update(extra or {})
  path=self.root/'atualizacao/test.zip'
  with zipfile.ZipFile(path,'w',zipfile.ZIP_DEFLATED) as z:
   for name,data in files.items():z.writestr(name,data)
  return path,hashlib.sha256(path.read_bytes()).hexdigest(),binary
 def invoke(self,args,timeout=60):
  return subprocess.run([str(PS),'-NoProfile','-ExecutionPolicy','Bypass','-File',str(self.root/SCRIPT.name)]+args,text=True,capture_output=True,timeout=timeout)
 def run_update(self,path,digest,expected=3):
  result=self.invoke(['-ArchivePath',str(path),'-ExpectedSha256',digest,'-ExpectedBuild',str(expected),'-WaitProcessId','0','-NoRestart','-NoUi'],timeout=60)
  if result.returncode and (self.root/'atualizacao/atualizacao.log').exists():print((self.root/'atualizacao/atualizacao.log').read_text(errors='replace')[-1000:])
  return result
 def preserved(self):
  self.assertEqual((self.root/'configs/default.hbc').read_bytes(),self.profile)
  self.assertEqual((self.root/'HillBlack.ini').read_bytes(),self.connection)
  self.assertEqual((self.root/'hillblack.db').read_bytes(),b'KEEP-DATABASE')
  self.assertEqual((self.root/'credentials.dat').read_bytes(),b'KEEP-PROTECTED-CREDENTIALS')
  self.assertEqual((self.root/'configs/active-profile.txt').read_bytes(),b'Minha config')
  self.assertEqual((self.root/'configs/Minha config.hbc').read_bytes(),b'KEEP-NAMED-PROFILE')
 def test_apply_only_changed_and_preserve_settings(self):
  path,digest,binary=self.package();result=self.run_update(path,digest)
  self.assertEqual(result.returncode,0,result.stderr)
  self.assertEqual((self.root/'Capri.exe').read_bytes(),binary)
  self.assertEqual((self.root/'assets/changed.dat').read_bytes(),b'NEW-ASSET')
  self.assertTrue((self.root/'licenses/new.txt').exists())
  self.assertEqual(int((self.root/'assets/unchanged.dat').stat().st_mtime),900000000)
  self.preserved()
 def test_download_and_apply_from_loopback_server(self):
  path,digest,binary=self.package()
  data=path.read_bytes()
  class Download(BaseHTTPRequestHandler):
   def log_message(self,*args):pass
   def do_GET(self):
    self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers();self.wfile.write(data)
  server=ThreadingHTTPServer(('127.0.0.1',0),Download);thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
  try:
   result=self.invoke(['-DownloadUrl',f'http://127.0.0.1:{server.server_port}/package.zip','-ExpectedSha256',digest,'-ExpectedBuild','3','-PackageSize',str(len(data)),'-NoRestart','-NoUi'],timeout=60)
   self.assertEqual(result.returncode,0,result.stderr)
   self.assertEqual((self.root/'Capri.exe').read_bytes(),binary);self.preserved()
  finally:server.shutdown();server.server_close();thread.join()
 @unittest.skipUnless(os.environ.get('CAPRI_SLOW_DOWNLOAD_TEST')=='1','opt-in 125-second slow download regression')
 def test_download_can_take_more_than_two_minutes(self):
  import time
  path,digest,binary=self.package();data=path.read_bytes()
  class SlowDownload(BaseHTTPRequestHandler):
   def log_message(self,*args):pass
   def do_GET(self):
    self.send_response(200);self.send_header('Content-Length',str(len(data)));self.end_headers()
    start=time.monotonic()
    for step in range(126):
     begin=len(data)*step//126;end=len(data)*(step+1)//126
     self.wfile.write(data[begin:end]);self.wfile.flush()
     if step<125:time.sleep(max(0,start+step+1-time.monotonic()))
  server=ThreadingHTTPServer(('127.0.0.1',0),SlowDownload)
  thread=threading.Thread(target=server.serve_forever,daemon=True);thread.start()
  try:
   started=time.monotonic()
   result=self.invoke(['-DownloadUrl',f'http://127.0.0.1:{server.server_port}/package.zip','-ExpectedSha256',digest,'-ExpectedBuild','3','-PackageSize',str(len(data)),'-NoRestart','-NoUi'],timeout=190)
   self.assertEqual(result.returncode,0,result.stderr)
   self.assertGreater(time.monotonic()-started,120)
   self.assertEqual((self.root/'Capri.exe').read_bytes(),binary);self.preserved()
  finally:server.shutdown();server.server_close();thread.join()
 def test_reopens_updated_executable(self):
  import time
  fixture=ROOT/'fontes/build/Release/updater_restart_fixture.exe'
  self.assertTrue(fixture.is_file(),'build the restart fixture first')
  path,digest,_=self.package(executable=fixture.read_bytes())
  result=self.invoke(['-ArchivePath',str(path),'-ExpectedSha256',digest,'-ExpectedBuild','3','-NoUi'],timeout=60)
  self.assertEqual(result.returncode,0,result.stderr)
  for _ in range(30):
   if (self.root/'started.txt').exists():break
   time.sleep(.1)
  self.assertTrue((self.root/'started.txt').exists(),'updated executable was not started')
  time.sleep(.2);self.preserved()
 def test_hash_mismatch_applies_nothing(self):
  path,_,_=self.package();result=self.run_update(path,'0'*64);self.assertNotEqual(result.returncode,0);self.assertIn('integridade',result.stderr)
  self.assertEqual((self.root/'Capri.exe').read_bytes(),b'OLD-EXE');self.preserved()
 def test_wrong_build_applies_nothing(self):
  path,digest,_=self.package(build=4);result=self.run_update(path,digest);self.assertNotEqual(result.returncode,0);self.assertIn('versao exigida',result.stderr)
  self.assertEqual((self.root/'Capri.exe').read_bytes(),b'OLD-EXE');self.preserved()
 def test_traversal_and_database_payload_rejected(self):
  for name in ['../outside.txt','assets/../../outside.txt','assets/.. /hillblack.db','assets/CON','hillblack.db']:
   path,digest,_=self.package({name:b'INVALID'});self.assertNotEqual(self.run_update(path,digest).returncode,0)
   self.assertEqual((self.root/'Capri.exe').read_bytes(),b'OLD-EXE');self.preserved()
 def test_rollback_when_manifest_cannot_be_replaced(self):
  path,digest,_=self.package();target=self.root/'client-version.json';os.chmod(target,stat.S_IREAD)
  try:
   result=self.run_update(path,digest);self.assertNotEqual(result.returncode,0)
   self.assertEqual((self.root/'Capri.exe').read_bytes(),b'OLD-EXE')
   self.assertEqual((self.root/'assets/changed.dat').read_bytes(),b'OLD-ASSET')
   self.assertFalse((self.root/'licenses/new.txt').exists());self.preserved()
  finally:os.chmod(target,stat.S_IWRITE|stat.S_IREAD)
if __name__=='__main__':unittest.main()
