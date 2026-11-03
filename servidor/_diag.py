import sqlite3, sys
db = sqlite3.connect(r"C:\Users\leona\Downloads\Capri-Completo\hillblack.db")
db.row_factory = sqlite3.Row

def mask(v):
    s = str(v or "")
    return (s[:4] + "..." + s[-4:]) if len(s) > 10 else "***"

cols = {r[1] for r in db.execute("PRAGMA table_info(keys)")}
print("colunas de keys:", sorted(cols))
rows = db.execute("SELECT * FROM keys").fetchall()
print("total de chaves:", len(rows))
for r in rows:
    d = dict(r)
    print("  {0} | status={1} | expires={2}".format(mask(d.get("key_value")), d.get("status"), d.get("expires_at")))

print("\npolitica de IP (key_ip_policy):")
try:
    pol = db.execute("SELECT * FROM key_ip_policy").fetchall()
    if not pol:
        print("  (vazia - toda chave usa o padrao ip_limit=1)")
    for r in pol:
        d = dict(r)
        print("  {0} | ip_limit={1}".format(mask(d.get("key_value")), d.get("ip_limit")))
except Exception as e:
    print("  erro:", e)

print("\nbindings de IP (key_ip_bindings):")
try:
    b = db.execute("SELECT key_value, ip, last_seen FROM key_ip_bindings").fetchall()
    if not b:
        print("  (vazio - nenhum IP registrado ainda)")
    for r in b:
        d = dict(r)
        print("  {0} | ip={1} | last_seen={2}".format(mask(d.get("key_value")), d.get("ip"), d.get("last_seen")))
except Exception as e:
    print("  erro:", e)

print("\nrelease publicado (client_release):")
try:
    if db.execute("SELECT 1 FROM sqlite_master WHERE type='table' AND name='client_release'").fetchone():
        r = db.execute("SELECT build, version, url FROM client_release WHERE id=1").fetchone()
        if r:
            print("  build={0} version={1}".format(r[0], r[1]))
            print("  url={0}".format(r[2]))
        else:
            print("  (nenhum release publicado)")
    else:
        print("  (tabela inexistente)")
except Exception as e:
    print("  erro:", e)
