// language: C#, file: Database.cs
// NuGet: Microsoft.Data.Sqlite, BCrypt.Net-Next
using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using Microsoft.Data.Sqlite;

public class KeyRecord {
    public int    Id         { get; set; }
    public string KeyValue   { get; set; } = "";
    public string CreatedAt  { get; set; } = "";
    public string ExpiresAt  { get; set; } = "";  // "never" for lifetime
    public string HwidBound  { get; set; } = "";
    public string Status     { get; set; } = "active"; // active/revoked/expired
    public string Note       { get; set; } = "";
    public int IpCount { get; set; }
    public int IpLimit { get; set; }=1;
    public string FirstIp { get; set; }="";
}
public sealed record KeyIp(string Ip,string FirstSeen,string LastSeen);

public sealed record ClientRelease(int Build,string Version,string Url,string Notes,string PackageHash="",long PackageSize=0);
public class Database {
    private readonly string _conn;
    private readonly string _folder;
    private readonly RemoteAdminClient? _remote;
    public bool IsRemote=>_remote!=null;
    public string RemoteEndpoint=>_remote?.Endpoint??"";
    public void Logout()=>_remote?.Logout();

    public Database(string path,string? remoteEndpoint=null) {
        _conn = new SqliteConnectionStringBuilder { DataSource=Path.GetFullPath(path) }.ToString();
        _folder=Path.GetDirectoryName(Path.GetFullPath(path))!;
        if(remoteEndpoint!=null)_remote=new RemoteAdminClient(remoteEndpoint);
    }

    public void Init() {
        if(IsRemote)return;
        using var c = new SqliteConnection(_conn);
        c.Open();
        using var cmd = c.CreateCommand();
        cmd.CommandText = @"
            CREATE TABLE IF NOT EXISTS admins (
                id            INTEGER PRIMARY KEY AUTOINCREMENT,
                username      TEXT    NOT NULL UNIQUE,
                password_hash TEXT    NOT NULL
            );
            CREATE TABLE IF NOT EXISTS keys (
                id          INTEGER PRIMARY KEY AUTOINCREMENT,
                key_value   TEXT    NOT NULL UNIQUE,
                created_at  TEXT    NOT NULL,
                expires_at  TEXT    NOT NULL,
                hwid_bound  TEXT    DEFAULT '',
                status      TEXT    NOT NULL DEFAULT 'active',
                note        TEXT    DEFAULT ''
            );";
        cmd.ExecuteNonQuery();

        cmd.CommandText=@"CREATE TABLE IF NOT EXISTS key_ip_policy(key_value TEXT PRIMARY KEY,ip_limit INTEGER NOT NULL DEFAULT 1 CHECK(ip_limit BETWEEN 1 AND 32));
            CREATE TABLE IF NOT EXISTS key_ip_bindings(key_value TEXT NOT NULL,ip TEXT NOT NULL,first_seen TEXT NOT NULL DEFAULT(strftime('%Y-%m-%dT%H:%M:%fZ','now')),last_seen TEXT NOT NULL DEFAULT(strftime('%Y-%m-%dT%H:%M:%fZ','now')),PRIMARY KEY(key_value,ip));";
        cmd.ExecuteNonQuery();
        cmd.CommandText="CREATE TABLE IF NOT EXISTS client_release(id INTEGER PRIMARY KEY CHECK(id=1),build INTEGER NOT NULL,version TEXT NOT NULL,url TEXT NOT NULL,notes TEXT NOT NULL)";
        cmd.ExecuteNonQuery();
        cmd.CommandText="CREATE TABLE IF NOT EXISTS admin_release(id INTEGER PRIMARY KEY CHECK(id=1),build INTEGER NOT NULL,version TEXT NOT NULL,url TEXT NOT NULL,notes TEXT NOT NULL,package_sha256 TEXT NOT NULL,package_size INTEGER NOT NULL)";cmd.ExecuteNonQuery();
        var columns=new HashSet<string>();cmd.CommandText="PRAGMA table_info(client_release)";
        using(var reader=cmd.ExecuteReader())while(reader.Read())columns.Add(reader.GetString(1));
        if(!columns.Contains("package_sha256")){cmd.CommandText="ALTER TABLE client_release ADD COLUMN package_sha256 TEXT NOT NULL DEFAULT ''";cmd.ExecuteNonQuery();}
        if(!columns.Contains("package_size")){cmd.CommandText="ALTER TABLE client_release ADD COLUMN package_size INTEGER NOT NULL DEFAULT 0";cmd.ExecuteNonQuery();}
    }

    public bool HasAdmins() {
        if(IsRemote)return true;
        using var c=new SqliteConnection(_conn);c.Open();using var cmd=c.CreateCommand();
        cmd.CommandText="SELECT COUNT(*) FROM admins";return (long)(cmd.ExecuteScalar()??0L)>0;
    }
    public void CreateFirstAdmin(string user,string password) {
        if(IsRemote)throw new InvalidOperationException("Crie a conta no PC do servidor.");
        user=user.Trim();if(user.Length<3 || user.Length>64 || password.Length<12 || Encoding.UTF8.GetByteCount(password)>72)throw new ArgumentException("Use um usuário de 3 a 64 caracteres e senha de pelo menos 12 caracteres e até 72 bytes UTF-8.");
        var hash=BCrypt.Net.BCrypt.HashPassword(password,12);
        using var c=new SqliteConnection(_conn);c.Open();using var tx=c.BeginTransaction();using var cmd=c.CreateCommand();cmd.Transaction=tx;
        cmd.CommandText="SELECT COUNT(*) FROM admins";if((long)(cmd.ExecuteScalar()??0L)!=0)throw new InvalidOperationException("Já existe uma conta. Entre com suas credenciais.");
        cmd.CommandText="INSERT INTO admins(username,password_hash) VALUES(@u,@h)";cmd.Parameters.AddWithValue("@u",user);cmd.Parameters.AddWithValue("@h",hash);cmd.ExecuteNonQuery();tx.Commit();
    }
    public bool ChangePassword(string user,string current,string password) {
        if(_remote!=null)return _remote.ChangePassword(user,current,password);
        if(password.Length<12 || Encoding.UTF8.GetByteCount(password)>72)throw new ArgumentException("Use uma senha de pelo menos 12 caracteres e até 72 bytes UTF-8.");
        using var c=new SqliteConnection(_conn);c.Open();using var cmd=c.CreateCommand();
        cmd.CommandText="SELECT password_hash FROM admins WHERE username=@u";cmd.Parameters.AddWithValue("@u",user);
        var previous=cmd.ExecuteScalar() as string;if(previous==null || !BCrypt.Net.BCrypt.Verify(current,previous))return false;
        cmd.CommandText="UPDATE admins SET password_hash=@h WHERE username=@u AND password_hash=@old";
        cmd.Parameters.AddWithValue("@h",BCrypt.Net.BCrypt.HashPassword(password,12));cmd.Parameters.AddWithValue("@old",previous);return cmd.ExecuteNonQuery()==1;
    }
    public void AddAdmin(string owner,string currentPassword,string user,string password){
        if(IsRemote)throw new InvalidOperationException("Crie administradores no PC do servidor.");
        if(!CheckLogin(owner,currentPassword))throw new ArgumentException("Senha atual incorreta.");
        user=user.Trim();if(user.Length<3 || user.Length>64 || password.Length<12 || Encoding.UTF8.GetByteCount(password)>72)throw new ArgumentException("Use usuario de 3 a 64 caracteres e senha de 12 caracteres ou mais (ate 72 bytes).");
        using var c=new SqliteConnection(_conn);c.Open();using var cmd=c.CreateCommand();
        cmd.CommandText="INSERT INTO admins(username,password_hash) VALUES(@u,@h)";cmd.Parameters.AddWithValue("@u",user);cmd.Parameters.AddWithValue("@h",BCrypt.Net.BCrypt.HashPassword(password,12));cmd.ExecuteNonQuery();
    }

    public bool CheckLogin(string user, string password) {
        if(_remote!=null)return _remote.Login(user,password);
        using var c = new SqliteConnection(_conn);
        c.Open();
        using var cmd = c.CreateCommand();
        cmd.CommandText = "SELECT password_hash FROM admins WHERE username=@u";
        cmd.Parameters.AddWithValue("@u", user);
        var hash = cmd.ExecuteScalar() as string;
        return hash != null && BCrypt.Net.BCrypt.Verify(password, hash);
    }

    public string GenerateKey(DateTime? expires, string note) {
        if(_remote!=null)return _remote.Create(expires,note);
        string key = Guid.NewGuid().ToString("N").ToUpper();
        string exp = expires.HasValue ? expires.Value.ToString("O") : "never";
        using var c = new SqliteConnection(_conn);
        c.Open();
        using var cmd = c.CreateCommand();
        cmd.CommandText = @"INSERT INTO keys(key_value,created_at,expires_at,note)
                            VALUES(@k,@ca,@ex,@n)";
        cmd.Parameters.AddWithValue("@k",  key);
        cmd.Parameters.AddWithValue("@ca", DateTime.UtcNow.ToString("O"));
        cmd.Parameters.AddWithValue("@ex", exp);
        cmd.Parameters.AddWithValue("@n",  note ?? "");
        cmd.ExecuteNonQuery();
        return key;
    }

    public void DeleteKey(string key) {
        if(_remote!=null){_remote.Action("keys_delete",key);return;}
        using var c=new SqliteConnection(_conn);c.Open();using var tx=c.BeginTransaction();using var cmd=c.CreateCommand();cmd.Transaction=tx;
        cmd.Parameters.AddWithValue("@k",key);
        cmd.CommandText="DELETE FROM key_ip_bindings WHERE key_value=@k";cmd.ExecuteNonQuery();
        cmd.CommandText="DELETE FROM key_ip_policy WHERE key_value=@k";cmd.ExecuteNonQuery();
        cmd.CommandText="DELETE FROM keys WHERE key_value=@k";
        if(cmd.ExecuteNonQuery()!=1)throw new ArgumentException("Chave não encontrada. Atualize a lista.");
        tx.Commit();
    }

    public void RevokeKey(string key) {
        if(_remote!=null){_remote.Action("keys_revoke",key);return;}
        using var c = new SqliteConnection(_conn);
        c.Open();
        using var cmd = c.CreateCommand();
        cmd.CommandText = "UPDATE keys SET status='revoked' WHERE key_value=@k";
        cmd.Parameters.AddWithValue("@k", key);
        cmd.ExecuteNonQuery();
    }

    public List<KeyRecord> GetAllKeys() {
        if(_remote!=null)return _remote.Keys();
        var list = new List<KeyRecord>();
        using var c = new SqliteConnection(_conn);
        c.Open();
        using var cmd = c.CreateCommand();
        cmd.CommandText = @"SELECT k.id,k.key_value,k.created_at,k.expires_at,k.hwid_bound,k.status,k.note,
            (SELECT COUNT(*) FROM key_ip_bindings b WHERE b.key_value=k.key_value),
            COALESCE((SELECT ip_limit FROM key_ip_policy p WHERE p.key_value=k.key_value),1),
            COALESCE((SELECT ip FROM key_ip_bindings b WHERE b.key_value=k.key_value ORDER BY rowid LIMIT 1),'') FROM keys k ORDER BY k.id DESC";
        using var r = cmd.ExecuteReader();
        while (r.Read()) {
            list.Add(new KeyRecord {
                Id        = r.GetInt32(0),
                KeyValue  = r.GetString(1),
                CreatedAt = r.GetString(2),
                ExpiresAt = r.GetString(3),
                HwidBound = r.IsDBNull(4) ? "" : r.GetString(4),
                Status    = r.GetString(5),
                Note      = r.IsDBNull(6) ? "" : r.GetString(6),
                IpCount=r.GetInt32(7),IpLimit=r.GetInt32(8),FirstIp=r.GetString(9)
            });
        }
        return list;
    }

    // Called by API server on HWID bind
    public List<KeyIp> GetKeyIps(string key){
        if(_remote!=null)return _remote.Ips(key);
        var result=new List<KeyIp>();using var c=new SqliteConnection(_conn);c.Open();using var cmd=c.CreateCommand();
        cmd.CommandText="SELECT ip,first_seen,last_seen FROM key_ip_bindings WHERE key_value=@k ORDER BY rowid";cmd.Parameters.AddWithValue("@k",key);
        using var r=cmd.ExecuteReader();while(r.Read())result.Add(new KeyIp(r.GetString(0),r.GetString(1),r.GetString(2)));return result;
    }
    public void SetIpLimit(string key,int limit){
        if(_remote!=null){_remote.Action("ips_limit",key,limit);return;}
        if(limit<1 || limit>32)throw new ArgumentException("O limite deve ficar entre 1 e 32 IPs.");
        using var c=new SqliteConnection(_conn);c.Open();using var tx=c.BeginTransaction();using var cmd=c.CreateCommand();cmd.Transaction=tx;
        cmd.CommandText="SELECT COUNT(*) FROM keys WHERE key_value=@k";cmd.Parameters.AddWithValue("@k",key);
        if(Convert.ToInt32(cmd.ExecuteScalar())!=1)throw new ArgumentException("Chave nao encontrada.");
        cmd.CommandText="INSERT INTO key_ip_policy(key_value,ip_limit) VALUES(@k,@n) ON CONFLICT(key_value) DO UPDATE SET ip_limit=excluded.ip_limit";cmd.Parameters.AddWithValue("@n",limit);cmd.ExecuteNonQuery();
        cmd.CommandText="DELETE FROM key_ip_bindings WHERE key_value=@k AND rowid NOT IN (SELECT rowid FROM key_ip_bindings WHERE key_value=@k ORDER BY rowid LIMIT @n)";cmd.ExecuteNonQuery();tx.Commit();
    }
    public void ResetKeyIps(string key){
        if(_remote!=null){_remote.Action("ips_reset",key);return;}
        using var c=new SqliteConnection(_conn);c.Open();using var cmd=c.CreateCommand();
        cmd.CommandText="DELETE FROM key_ip_bindings WHERE key_value=@k";cmd.Parameters.AddWithValue("@k",key);cmd.ExecuteNonQuery();
    }
    public void ActivateKey(string key){
        if(_remote!=null){_remote.Action("keys_activate",key);return;}
        using var c=new SqliteConnection(_conn);c.Open();using var cmd=c.CreateCommand();
        cmd.CommandText="UPDATE keys SET status='active' WHERE key_value=@k AND (expires_at='never' OR julianday(expires_at)>julianday('now'))";cmd.Parameters.AddWithValue("@k",key);
        if(cmd.ExecuteNonQuery()!=1)throw new ArgumentException("A chave expirou ou nao existe. Emita uma nova licenca.");
    }
    public bool BindHwid(string key, string hwid) {
        if(IsRemote)throw new InvalidOperationException("Operacao local indisponivel neste painel.");
        using var c = new SqliteConnection(_conn);
        c.Open();
        using var cmd = c.CreateCommand();
        cmd.CommandText = "SELECT hwid_bound,status,expires_at FROM keys WHERE key_value=@k";
        cmd.Parameters.AddWithValue("@k", key);
        using var r = cmd.ExecuteReader();
        if (!r.Read()) return false;

        string current_hwid = r.IsDBNull(0) ? "" : r.GetString(0);
        string status       = r.GetString(1);
        string expires_at   = r.GetString(2);
        r.Close();

        if (status != "active") return false;
        if (KeyExpiry.IsExpired(expires_at, DateTimeOffset.UtcNow)) {
            // Expire it
            using var upd = c.CreateCommand();
            upd.CommandText = "UPDATE keys SET status='expired' WHERE key_value=@k";
            upd.Parameters.AddWithValue("@k", key);
            upd.ExecuteNonQuery();
            return false;
        }
        if (string.IsNullOrEmpty(current_hwid)) {
            using var upd = c.CreateCommand();
            upd.CommandText = "UPDATE keys SET hwid_bound=@h WHERE key_value=@k";
            upd.Parameters.AddWithValue("@h", hwid);
            upd.Parameters.AddWithValue("@k", key);
            upd.ExecuteNonQuery();
            return true;
        }
        return current_hwid == hwid;
    }
    public ClientRelease? GetRelease(bool admin=false) {
        if(IsRemote)return null;
        using var c=new SqliteConnection(_conn);c.Open();using var cmd=c.CreateCommand();
        cmd.CommandText=$"SELECT build,version,url,notes,package_sha256,package_size FROM {(admin?"admin_release":"client_release")} WHERE id=1";using var r=cmd.ExecuteReader();
        return r.Read()?new ClientRelease(r.GetInt32(0),r.GetString(1),r.GetString(2),r.GetString(3),r.GetString(4),r.GetInt64(5)):null;
    }
    public void PublishRelease(int build,string version,string url,string notes,string packageHash="",long packageSize=0,bool admin=false) {
        if(IsRemote)throw new InvalidOperationException("Publique os pacotes no PC do servidor.");
        string table=admin?"admin_release":"client_release";
        if(build<1 || string.IsNullOrWhiteSpace(version) || version.Length>64 || notes.Length>2000 || url.Length>2048 ||
           !Uri.TryCreate(url,UriKind.Absolute,out var uri) || uri.Scheme!="https" || string.IsNullOrEmpty(uri.Host) || !string.IsNullOrEmpty(uri.UserInfo) || uri.AbsoluteUri.Length>2048)
            throw new ArgumentException("Informe build positivo, versao e link HTTPS valido.");
        if(packageHash!="" && (!Regex.IsMatch(packageHash,"^[a-f0-9]{64}$") || packageSize<=0 || packageSize>100L*1024*1024))throw new ArgumentException("Metadados do pacote invalidos.");
        using var c=new SqliteConnection(_conn);c.Open();using var transaction=c.BeginTransaction();using var cmd=c.CreateCommand();cmd.Transaction=transaction;
        cmd.CommandText=$"SELECT COALESCE(MAX(build),0) FROM {table}";var current=Convert.ToInt32(cmd.ExecuteScalar());
        if(build<current)throw new ArgumentException("O build deve ser igual ou maior que a ultima atualizacao publicada.");
        cmd.CommandText=$"SELECT package_sha256 FROM {table} WHERE id=1";var previousHash=cmd.ExecuteScalar() as string??"";
        if(build==current && previousHash!="" && previousHash!=packageHash)throw new ArgumentException("Arquivos novos precisam de um build maior. Nao substitua um build ja publicado.");
        cmd.CommandText=$"INSERT INTO {table}(id,build,version,url,notes,package_sha256,package_size) VALUES(1,@b,@v,@u,@n,@h,@s) ON CONFLICT(id) DO UPDATE SET build=excluded.build,version=excluded.version,url=excluded.url,notes=excluded.notes,package_sha256=excluded.package_sha256,package_size=excluded.package_size";
        cmd.Parameters.AddWithValue("@h",packageHash);cmd.Parameters.AddWithValue("@s",packageSize);
        cmd.Parameters.AddWithValue("@b",build);cmd.Parameters.AddWithValue("@v",version.Trim());cmd.Parameters.AddWithValue("@u",uri.AbsoluteUri);cmd.Parameters.AddWithValue("@n",notes.Trim());cmd.ExecuteNonQuery();transaction.Commit();
    }

    public ClientRelease InspectPackage(string path) {
        using(var probe=ZipFile.OpenRead(path))if(probe.GetEntry("admin-version.json")!=null)return InspectAdminPackage(path);
        var file=new FileInfo(path);if(!file.Exists || file.Length<=0 || file.Length>100L*1024*1024)throw new ArgumentException("Escolha Capri-Cliente.zip, de ate 100 MB.");
        using var zip=ZipFile.OpenRead(path);long expanded=0;var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);if(zip.Entries.Count>1024)throw new ArgumentException("Pacote com arquivos demais.");
        foreach(var entry in zip.Entries) {
            string name=entry.FullName;
            if(!names.Add(name.TrimEnd('/')))throw new ArgumentException("Arquivo repetido no ZIP.");
            if(name.Contains('\\') || name.StartsWith('/') || name.Contains(':') || Array.Exists(name.Split('/'),s=>s==".." || s==".") || ((entry.ExternalAttributes>>16)&0xF000)==0xA000)throw new ArgumentException("Caminho inseguro no ZIP.");
            if(Array.Exists(name.TrimEnd('/').Split('/'),part=>string.IsNullOrEmpty(part) || part.EndsWith('.') || part.EndsWith(' ') || part.IndexOfAny(Path.GetInvalidFileNameChars())>=0 || Regex.IsMatch(part,@"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\..*)?$",RegexOptions.IgnoreCase)))throw new ArgumentException("Nome de arquivo invalido no ZIP.");
            if(name.EndsWith('/'))continue;
            if(Array.Exists(new[]{".db",".sqlite",".sqlite3"},extension=>name.EndsWith(extension,StringComparison.OrdinalIgnoreCase)))throw new ArgumentException("O pacote contem banco de dados.");
            bool allowed=name is "Capri.exe" or "client-version.json" or "Atualizar-Capri.ps1" or "HillBlack.ini" or "COMO-ABRIR.txt" or "configs/default.hbc" || name.StartsWith("assets/") || name.StartsWith("licenses/");
            if(!allowed)throw new ArgumentException("O pacote deve conter somente o cliente, sem banco, administrador ou fontes.");
            expanded+=entry.Length;if(expanded>150L*1024*1024)throw new ArgumentException("Pacote expandido muito grande.");
        }
        var manifest=zip.GetEntry("client-version.json")??throw new ArgumentException("Manifesto de versao ausente.");
        if(manifest.Length>4096)throw new ArgumentException("Manifesto invalido.");
        using var stream=manifest.Open();using var data=JsonDocument.Parse(stream);
        int build=data.RootElement.GetProperty("build").GetInt32();string version=data.RootElement.GetProperty("version").GetString()??"";
        string expected=data.RootElement.GetProperty("sha256").GetString()??"";
        if(build<1 || string.IsNullOrWhiteSpace(version) || version.Length>64)throw new ArgumentException("Versao do pacote invalida.");
        var exe=zip.GetEntry("Capri.exe")??throw new ArgumentException("Capri.exe ausente.");
        if(exe.Length<1024)throw new ArgumentException("Executavel invalido.");
        using(var header=exe.Open())if(header.ReadByte()!=77 || header.ReadByte()!=90)throw new ArgumentException("Executavel invalido.");
        using(var binary=exe.Open())if(!Convert.ToHexString(SHA256.HashData(binary)).Equals(expected,StringComparison.OrdinalIgnoreCase))throw new ArgumentException("O EXE nao corresponde ao manifesto.");
        if(zip.GetEntry("Atualizar-Capri.ps1")==null)throw new ArgumentException("Atualizador ausente: gere o novo pacote pelo empacotador.");
        var profile=zip.GetEntry("configs/default.hbc");
        if(profile!=null) {
            if(profile.Length>65536)throw new ArgumentException("Perfil invalido.");
            using var input=profile.Open();using var memory=new MemoryStream();input.CopyTo(memory);var bytes=memory.ToArray();for(int i=0;i<bytes.Length;i++)bytes[i]^=0x4b;
            using var settings=JsonDocument.Parse(bytes);if(settings.RootElement.TryGetProperty("saved_key",out var key) && key.GetString()!="")throw new ArgumentException("O pacote contem chave salva. Gere pelo empacotador.");
        }
        using var archive=File.OpenRead(path);string hash=Convert.ToHexString(SHA256.HashData(archive)).ToLowerInvariant();
        return new ClientRelease(build,version,"","",hash,file.Length);
    }
    public ClientRelease PublishPackage(string path,string notes) {
        if(IsRemote)throw new InvalidOperationException("Publique os pacotes no PC do servidor.");
        bool admin;using(var probe=ZipFile.OpenRead(path))admin=probe.GetEntry("admin-version.json")!=null;
        var package=InspectPackage(path);var previous=GetRelease(admin);
        if(previous!=null && (package.Build<previous.Build || package.Build==previous.Build && previous.PackageHash!="" && package.PackageHash!=previous.PackageHash))throw new ArgumentException("Gere um novo build para publicar arquivos diferentes.");
        var ini=Path.Combine(_folder,"HillBlack.ini");string endpoint="";
        foreach(var line in File.ReadAllLines(ini))if(line.Trim().StartsWith("endpoint=",StringComparison.OrdinalIgnoreCase))endpoint=line.Trim().Substring(9).Trim();
        if(!Uri.TryCreate(endpoint,UriKind.Absolute,out var origin) || origin.Scheme!="https" || !string.IsNullOrEmpty(origin.UserInfo))throw new ArgumentException("Inicie o servidor de chaves antes de publicar.");
        string url=new Uri(origin,"/packages/"+package.PackageHash+".zip").AbsoluteUri;
        var folder=Path.Combine(_folder,"servidor","releases");Directory.CreateDirectory(folder);
        string target=Path.Combine(folder,package.PackageHash+".zip"),temporary=Path.Combine(folder,Guid.NewGuid().ToString("N")+".part");
        try {
            if(!File.Exists(target)){File.Copy(path,temporary,false);using(var check=File.OpenRead(temporary))if(Convert.ToHexString(SHA256.HashData(check)).ToLowerInvariant()!=package.PackageHash)throw new IOException("O arquivo mudou durante a copia.");File.Move(temporary,target);}
            else {using var check=File.OpenRead(target);if(Convert.ToHexString(SHA256.HashData(check)).ToLowerInvariant()!=package.PackageHash)throw new IOException("Pacote publicado esta corrompido.");}
            PublishRelease(package.Build,package.Version,url,notes,package.PackageHash,package.PackageSize,admin);
        } finally {if(File.Exists(temporary))File.Delete(temporary);}
        return package with {Url=url,Notes=notes};
    }
    private ClientRelease InspectAdminPackage(string path){
        var file=new FileInfo(path);if(file.Length<=0 || file.Length>104857600)throw new ArgumentException("Pacote ADM deve ter ate 100 MB.");
        using var archive=ZipFile.OpenRead(path);var expected=new HashSet<string>(StringComparer.Ordinal){"Capri.Admin.exe","admin-version.json","Atualizar-ADM.ps1"};
        long expanded=0;foreach(var item in archive.Entries){if(!expected.Remove(item.FullName) || ((item.ExternalAttributes>>16)&0xF000)==0xA000)throw new ArgumentException("Escolha Capri-ADM-Atualizacao.zip, sem banco, configuracao ou credenciais.");expanded+=item.Length;}
        if(expected.Count!=0 || expanded>104857600)throw new ArgumentException("Pacote ADM incompleto ou muito grande.");
        var manifest=archive.GetEntry("admin-version.json")!;if(manifest.Length>4096)throw new ArgumentException("Manifesto ADM invalido.");
        using var stream=manifest.Open();using var document=JsonDocument.Parse(stream);var data=document.RootElement;
        int build=data.GetProperty("build").GetInt32();string version=data.GetProperty("version").GetString()??"",hash=data.GetProperty("sha256").GetString()??"";
        if(build<1 || version.Length<1 || version.Length>64)throw new ArgumentException("Versao ADM invalida.");
        using var binary=archive.GetEntry("Capri.Admin.exe")!.Open();if(!Convert.ToHexString(SHA256.HashData(binary)).Equals(hash,StringComparison.OrdinalIgnoreCase))throw new ArgumentException("O EXE ADM difere do manifesto.");
        using var package=File.OpenRead(path);return new ClientRelease(build,version,"","",Convert.ToHexString(SHA256.HashData(package)).ToLowerInvariant(),file.Length);
    }

}
