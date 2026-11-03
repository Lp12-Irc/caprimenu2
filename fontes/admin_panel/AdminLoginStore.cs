using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;

public sealed record SavedAdminLogin(string User, string Password);

// DPAPI, current Windows user only. No plaintext file and no authentication bypass.
public sealed class AdminLoginStore {
    private readonly string _path;
    private readonly byte[] _entropy;
    public AdminLoginStore(string databasePath) {
        string database=Path.GetFullPath(databasePath);
        _path=Path.Combine(Path.GetDirectoryName(database)!,"admin-login.dat");
        _entropy=SHA256.HashData(Encoding.UTF8.GetBytes("Capri.Admin.Login.v1\0"+database.ToUpperInvariant()));
    }
    private static bool Valid(SavedAdminLogin value) =>
        !string.IsNullOrWhiteSpace(value.User) && value.User.Length<=64 &&
        !string.IsNullOrEmpty(value.Password) && value.Password.Length<=256;

    public SavedAdminLogin? Load() {
        if(!File.Exists(_path))return null;
        using var stream=new FileStream(_path,FileMode.Open,FileAccess.Read,FileShare.Read);
        if(stream.Length<1 || stream.Length>8192)throw new CryptographicException("Arquivo de login inválido.");
        byte[] encrypted=new byte[(int)stream.Length];stream.ReadExactly(encrypted);
        byte[] plain=Transform(encrypted,false);
        try {
            var login=JsonSerializer.Deserialize<SavedAdminLogin>(plain);
            if(login is null || !Valid(login))throw new CryptographicException("Dados de login inválidos.");
            return login;
        } finally {CryptographicOperations.ZeroMemory(plain);}
    }
    public void Save(string user,string password) {
        var login=new SavedAdminLogin(user.Trim(),password);
        if(!Valid(login))throw new ArgumentException("Dados de login inválidos.");
        byte[] plain=JsonSerializer.SerializeToUtf8Bytes(login);
        byte[] encrypted;
        try {encrypted=Transform(plain,true);}finally{CryptographicOperations.ZeroMemory(plain);}
        string temporary=_path+"."+Guid.NewGuid().ToString("N")+".tmp";
        try {
            using(var stream=new FileStream(temporary,FileMode.CreateNew,FileAccess.Write,FileShare.None,4096,FileOptions.WriteThrough)) {
                stream.Write(encrypted);stream.Flush(true);
            }
            File.Move(temporary,_path,true);
        } finally {if(File.Exists(temporary))File.Delete(temporary);}
    }
    public void Clear() => File.Delete(_path);

    [StructLayout(LayoutKind.Sequential)]
    private struct Blob {public int Length;public IntPtr Data;}
    [DllImport("crypt32.dll",SetLastError=true,CharSet=CharSet.Unicode)]
    [return:MarshalAs(UnmanagedType.Bool)]
    private static extern bool CryptProtectData(ref Blob input,string? description,ref Blob entropy,IntPtr reserved,IntPtr prompt,uint flags,out Blob output);
    [DllImport("crypt32.dll",SetLastError=true)]
    [return:MarshalAs(UnmanagedType.Bool)]
    private static extern bool CryptUnprotectData(ref Blob input,IntPtr description,ref Blob entropy,IntPtr reserved,IntPtr prompt,uint flags,out Blob output);
    [DllImport("kernel32.dll")] private static extern IntPtr LocalFree(IntPtr value);
    private byte[] Transform(byte[] data,bool protect) {
        var inputHandle=GCHandle.Alloc(data,GCHandleType.Pinned);
        var entropyHandle=GCHandle.Alloc(_entropy,GCHandleType.Pinned);
        Blob output=default;
        try {
            var input=new Blob{Length=data.Length,Data=inputHandle.AddrOfPinnedObject()};
            var entropy=new Blob{Length=_entropy.Length,Data=entropyHandle.AddrOfPinnedObject()};
            const uint NoUi=1;
            bool success=protect?CryptProtectData(ref input,null,ref entropy,IntPtr.Zero,IntPtr.Zero,NoUi,out output):
                CryptUnprotectData(ref input,IntPtr.Zero,ref entropy,IntPtr.Zero,IntPtr.Zero,NoUi,out output);
            if(!success)throw new CryptographicException("Não foi possível proteger ou recuperar o login.",new Win32Exception(Marshal.GetLastWin32Error()));
            if(output.Length<1 || output.Length>8192)throw new CryptographicException("Dados protegidos inválidos.");
            byte[] result=new byte[output.Length];Marshal.Copy(output.Data,result,0,result.Length);return result;
        } finally {
            if(output.Data!=IntPtr.Zero){
                if(!protect && output.Length>0 && output.Length<=8192)Marshal.Copy(new byte[output.Length],0,output.Data,output.Length);
                LocalFree(output.Data);
            }
            entropyHandle.Free();inputHandle.Free();
        }
    }
}
