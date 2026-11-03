"""Windows CurrentUser DPAPI storage for server-only Cloudflare gateway credentials."""
import ctypes,json,os,secrets
from ctypes import wintypes
from pathlib import Path

ROOT=Path(__file__).resolve().parent.parent
FILE=ROOT/'servidor/cloudflare-gateway.dpapi'
class Blob(ctypes.Structure):
    _fields_=[('size',wintypes.DWORD),('data',ctypes.POINTER(ctypes.c_ubyte))]

def transform(data,protect):
    crypt=ctypes.WinDLL('crypt32',use_last_error=True)
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    kernel.LocalFree.argtypes=[ctypes.c_void_p]
    memory=ctypes.create_string_buffer(data)
    source=Blob(len(data),ctypes.cast(memory,ctypes.POINTER(ctypes.c_ubyte)));result=Blob()
    if protect:
        function=crypt.CryptProtectData
        function.argtypes=[ctypes.POINTER(Blob),wintypes.LPCWSTR,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,wintypes.DWORD,ctypes.POINTER(Blob)]
        success=function(ctypes.byref(source),'Capri Cloudflare gateway',None,None,None,1,ctypes.byref(result))
    else:
        function=crypt.CryptUnprotectData
        function.argtypes=[ctypes.POINTER(Blob),ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,ctypes.c_void_p,wintypes.DWORD,ctypes.POINTER(Blob)]
        success=function(ctypes.byref(source),None,None,None,None,1,ctypes.byref(result))
    if not success:raise ctypes.WinError(ctypes.get_last_error())
    try:return ctypes.string_at(result.data,result.size)
    finally:
        ctypes.memset(memory,0,len(data));kernel.LocalFree(result.data)

def load():
    return json.loads(transform(FILE.read_bytes(),False)) if FILE.exists() else None

def initialize():
    current=load()
    if current:return current
    value={'endpoint':'https://capri-conexao.leonardofernando06022013.workers.dev',
           'register_secret':secrets.token_urlsafe(48),'forward_secret':secrets.token_urlsafe(48)}
    temporary=FILE.with_suffix('.dpapi.tmp')
    temporary.write_bytes(transform(json.dumps(value).encode(),True));os.replace(temporary,FILE)
    return value
