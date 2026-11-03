"""Run the same installation/rollback/download contract against the native helper."""
import os,shutil,subprocess,unittest
from pathlib import Path
import test_updater as legacy

class NativeInstallerTests(legacy.InstallerTests):
 def setUp(self):
  super().setUp()
  self.runner=self.root/'atualizacao/Capri.Updater.exe'
  shutil.copy2(legacy.ROOT/'fontes/assets/updater/Capri.Updater.exe',self.runner)
 def tearDown(self):
  import time
  self.assertTrue(self.root.resolve().is_relative_to(legacy.WORK.resolve()))
  for attempt in range(40):
   try:self.temp.cleanup();return
   except PermissionError:
    if attempt==39:raise
    time.sleep(.1)
 def invoke(self,args,timeout=60):
  mapping={'-ArchivePath':'--archive','-DownloadUrl':'--url','-ExpectedSha256':'--sha',
   '-ExpectedBuild':'--build','-PackageSize':'--size','-WaitProcessId':'--wait-pid',
   '-NoRestart':'--no-restart','-NoUi':'--no-ui'}
  translated=[mapping.get(str(arg),str(arg)) for arg in args]
  return subprocess.run([str(self.runner),'--root',str(self.root)]+translated,text=True,capture_output=True,timeout=timeout)
 def test_installed_helper_can_be_replaced_while_cached_runner_is_active(self):
  helper=b'MZ-NEXT-UPDATER';path,digest,_=self.package({'assets/updater/Capri.Updater.exe':helper})
  result=self.run_update(path,digest)
  self.assertEqual(result.returncode,0,result.stderr)
  self.assertEqual((self.root/'assets/updater/Capri.Updater.exe').read_bytes(),helper)
  self.assertTrue(self.runner.is_file());self.preserved()

if __name__=='__main__':unittest.main()
