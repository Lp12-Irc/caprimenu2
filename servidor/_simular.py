import sqlite3, sys, json
# Simula a validacao do license_server.py para um IP, SEM escrever no banco.
# Uso: python _simular.py <prefixo_da_chave> <ip>
db = sqlite3.connect(r"C:\Users\leona\Downloads\Capri-Completo\hillblack.db")
db.row_factory = sqlite3.Row

prefix = sys.argv[1] if len(sys.argv) > 1 else ""
ip = sys.argv[2] if len(sys.argv) > 2 else ""

rows = db.execute("SELECT key_value, status, expires_at FROM keys").fetchall()
alvo = [r for r in rows if str(r["key_value"]).startswith(prefix)]
if not alvo:
    print("Nenhuma chave comecando com '{0}'".format(prefix))
    sys.exit(1)
if len(alvo) > 1:
    print("Prefixo ambiguo, {0} chaves".format(len(alvo)))
    sys.exit(1)

key = alvo[0]["key_value"]
print("Chave: {0}...{1}".format(key[:4], key[-4:]))
print("status={0} expires={1}".format(alvo[0]["status"], alvo[0]["expires_at"]))

pol = db.execute("SELECT ip_limit FROM key_ip_policy WHERE key_value=?", (key,)).fetchone()
limit = pol[0] if pol else 1
print("ip_limit={0}{1}".format(limit, "" if pol else " (padrao)"))

bindings = db.execute("SELECT ip FROM key_ip_bindings WHERE key_value=?", (key,)).fetchall()
ips = [b["ip"] for b in bindings]
print("IPs vinculados ({0}): {1}".format(len(ips), ips if ips else "nenhum"))

alvo_ip = ip.strip()
known = any(x.strip() == alvo_ip for x in ips)
print("\nSimulando validacao do IP: {0}".format(alvo_ip))
if known:
    print("  -> IP ja vinculado. Validacao PASSARIA.")
    print("  -> Resultado: valid=true")
elif len(ips) < limit:
    print("  -> IP novo, mas ha vaga ({0}/{1}). Validacao PASSARIA e vincularia.".format(len(ips), limit))
    print("  -> Resultado: valid=true")
else:
    print("  -> IP novo e sem vaga ({0}/{1}). Validacao FALHARIA.".format(len(ips), limit))
    print("  -> Resultado: valid=false reason=ip_limit")
    print('  -> Mensagem: "Limite de IPs por chave excedido. Solicite a liberacao de IP ao administrador."')
