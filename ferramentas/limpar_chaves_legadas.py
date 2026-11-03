"""Remove legacy saved keys from appearance profiles; preserve an encrypted DPAPI backup."""
from pathlib import Path
import ctypes,json,os
from ctypes import wintypes
root=Path(__file__).resolve().parent.parent
class Blob(ctypes.Structure):_fields_=[('size',wintypes.DWORD),('data',ctypes.POINTER(ctypes.c_ubyte))]
crypt=ctypes.WinDLL('crypt32',use_last_error=True);kernel=ctypes.WinDLL('kernel32',use_last_error=True)
crypt.CryptProtectData.argtypes=[ctypes.POINTER(Blob),wintypes.LPCWSTR,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,wintypes.DWORD,ctypes.POINTER(Blob)]
crypt.CryptProtectData.restype=wintypes.BOOL;kernel.LocalFree.argtypes=[ctypes.c_void_p]
for path in (root/'configs').glob('*.hbc'):
    original=path.read_bytes()
    try:profile=json.loads(bytes(value^0x4b for value in original))
    except (ValueError,UnicodeError):continue
    if not profile.get('saved_key'):continue
    memory=ctypes.create_string_buffer(original);source=Blob(len(original),ctypes.cast(memory,ctypes.POINTER(ctypes.c_ubyte)));protected=Blob()
    if not crypt.CryptProtectData(ctypes.byref(source),'Capri legacy profile backup',None,None,None,1,ctypes.byref(protected)):raise ctypes.WinError(ctypes.get_last_error())
    try:
        folder=root/'backups/auditoria-seguranca';folder.mkdir(parents=True,exist_ok=True)
        (folder/(path.name+'.dpapi')).write_bytes(ctypes.string_at(protected.data,protected.size))
    finally:
        ctypes.memset(memory,0,len(original));kernel.LocalFree(protected.data)
    profile['saved_key']='';sanitized=bytes(value^0x4b for value in json.dumps(profile,ensure_ascii=False,indent=2).encode())
    temporary=path.with_suffix('.hbc.sanitizing');temporary.write_bytes(sanitized);os.replace(temporary,path)
    print('Campo legado removido; backup protegido pelo Windows:',path.name)
