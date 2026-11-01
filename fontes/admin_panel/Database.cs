// language: C#, file: Database.cs
// NuGet: Microsoft.Data.Sqlite, BCrypt.Net-Next
using System;
using System.Collections.Generic;
using Microsoft.Data.Sqlite;

public class KeyRecord {
    public int    Id         { get; set; }
    public string KeyValue   { get; set; } = "";
    public string CreatedAt  { get; set; } = "";
    public string ExpiresAt  { get; set; } = "";  // "never" for lifetime
    public string HwidBound  { get; set; } = "";
    public string Status     { get; set; } = "active"; // active/revoked/expired
    public string Note       { get; set; } = "";
}

public class Database {
    private readonly string _conn;

    public Database(string path) {
        _conn = $"Data Source={path}";
    }

    public void Init() {
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

        // Apply this account migration once, including existing installations.
        cmd.CommandText = "CREATE TABLE IF NOT EXISTS capri_migrations (name TEXT PRIMARY KEY)";
        cmd.ExecuteNonQuery();
        using var transaction = c.BeginTransaction();
        cmd.Transaction = transaction;
        cmd.CommandText = "SELECT COUNT(*) FROM capri_migrations WHERE name='admin-caprivac-v1'";
        if ((long)(cmd.ExecuteScalar() ?? 0L) == 0) {
            cmd.CommandText = "INSERT INTO admins(username,password_hash) VALUES(@u,@h) ON CONFLICT(username) DO UPDATE SET password_hash=excluded.password_hash";
            cmd.Parameters.AddWithValue("@u", "CapriVAC");
            cmd.Parameters.AddWithValue("@h", BCrypt.Net.BCrypt.HashPassword("1994Senna"));
            cmd.ExecuteNonQuery();
            cmd.Parameters.Clear();
            cmd.CommandText = "INSERT INTO capri_migrations(name) VALUES('admin-caprivac-v1')";
            cmd.ExecuteNonQuery();
        }
        transaction.Commit();
    }

    public bool CheckLogin(string user, string password) {
        using var c = new SqliteConnection(_conn);
        c.Open();
        using var cmd = c.CreateCommand();
        cmd.CommandText = "SELECT password_hash FROM admins WHERE username=@u";
        cmd.Parameters.AddWithValue("@u", user);
        var hash = cmd.ExecuteScalar() as string;
        return hash != null && BCrypt.Net.BCrypt.Verify(password, hash);
    }

    public string GenerateKey(DateTime? expires, string note) {
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

    public void RevokeKey(string key) {
        using var c = new SqliteConnection(_conn);
        c.Open();
        using var cmd = c.CreateCommand();
        cmd.CommandText = "UPDATE keys SET status='revoked' WHERE key_value=@k";
        cmd.Parameters.AddWithValue("@k", key);
        cmd.ExecuteNonQuery();
    }

    public List<KeyRecord> GetAllKeys() {
        var list = new List<KeyRecord>();
        using var c = new SqliteConnection(_conn);
        c.Open();
        using var cmd = c.CreateCommand();
        cmd.CommandText = "SELECT id,key_value,created_at,expires_at,hwid_bound,status,note FROM keys ORDER BY id DESC";
        using var r = cmd.ExecuteReader();
        while (r.Read()) {
            list.Add(new KeyRecord {
                Id        = r.GetInt32(0),
                KeyValue  = r.GetString(1),
                CreatedAt = r.GetString(2),
                ExpiresAt = r.GetString(3),
                HwidBound = r.IsDBNull(4) ? "" : r.GetString(4),
                Status    = r.GetString(5),
                Note      = r.IsDBNull(6) ? "" : r.GetString(6)
            });
        }
        return list;
    }

    // Called by API server on HWID bind
    public bool BindHwid(string key, string hwid) {
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
        if (expires_at != "never" && DateTime.Parse(expires_at) < DateTime.UtcNow) {
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
}
