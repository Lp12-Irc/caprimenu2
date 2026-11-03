"""Authenticated administration over the same HTTPS tunnel; database never leaves the host."""
import bcrypt,hashlib,hmac,secrets,sqlite3,threading,time,datetime
from contextlib import closing

class AdminError(Exception):
    def __init__(self,status,message):self.status=status;super().__init__(message)

class AdminApi:
    def __init__(self,database):
        self.database=database;self.sessions={};self.failures={};self.lock=threading.Lock();self.capacity=threading.BoundedSemaphore(4);self.requests=threading.BoundedSemaphore(8)
        self.dummy=bcrypt.hashpw(secrets.token_bytes(24),bcrypt.gensalt(rounds=12))
    def connect(self):return sqlite3.connect(self.database.as_uri()+'?mode=rw',uri=True,timeout=2)
    @staticmethod
    def password(value):
        if not isinstance(value,str) or not 1<=len(value.encode('utf-8'))<=72:raise AdminError(400,'Senha invalida.')
        return value.encode('utf-8')
    def login(self,payload,ip):
        user=payload.get('username');password=self.password(payload.get('password'))
        if not isinstance(user,str) or not 1<=len(user)<=64:raise AdminError(400,'Usuario invalido.')
        now=time.monotonic()
        with self.lock:
            self.failures={k:v for k,v in self.failures.items() if now-v[0]<900}
            start,count=self.failures.get(ip,(now,0))
            if count>=30:raise AdminError(429,'Muitas tentativas. Aguarde antes de entrar novamente.')
            self.failures[ip]=(start,count+1)
        if not self.capacity.acquire(blocking=False):raise AdminError(503,'Validacao ocupada. Tente novamente.')
        try:
            with closing(self.connect()) as db:row=db.execute('SELECT password_hash FROM admins WHERE username=?',(user,)).fetchone()
            try:valid=bcrypt.checkpw(password,row[0].encode() if row and isinstance(row[0],str) else self.dummy)
            except ValueError:valid=False
            if not row or not isinstance(row[0],str) or not valid:raise AdminError(401,'Usuario ou senha invalidos.')
            token=secrets.token_urlsafe(32);digest=hashlib.sha256(token.encode()).hexdigest()
            with self.lock:
                self.sessions={k:v for k,v in self.sessions.items() if v['expires']>now}
                if len(self.sessions)>=1024:raise AdminError(503,'Sessoes ocupadas. Tente mais tarde.')
                self.sessions[digest]={'user':user,'ip':ip,'hash':row[0],'expires':now+1800,'window':now,'requests':0}
            return {'token':token,'expires_in':1800,'username':user}
        finally:self.capacity.release()
    def authorize(self,authorization,ip):
        if not isinstance(authorization,str) or not authorization.startswith('Bearer ') or len(authorization)>256:raise AdminError(401,'Entre no painel para continuar.')
        digest=hashlib.sha256(authorization[7:].encode()).hexdigest();now=time.monotonic()
        with self.lock:session=self.sessions.get(digest)
        if not session or session['expires']<=now or session['ip']!=ip:raise AdminError(401,'Sessao encerrada. Entre novamente.')
        with self.lock:
            if now-session['window']>=60:session['window']=now;session['requests']=0
            session['requests']+=1
            if session['requests']>300:raise AdminError(429,'Muitas operacoes. Aguarde um instante.')
        with closing(self.connect()) as db:row=db.execute('SELECT password_hash FROM admins WHERE username=?',(session['user'],)).fetchone()
        if not row or not hmac.compare_digest(row[0],session['hash']):
            with self.lock:self.sessions.pop(digest,None)
            raise AdminError(401,'Sessao encerrada. Entre novamente.')
        return session
    @staticmethod
    def key(payload):
        key=payload.get('key')
        if not isinstance(key,str) or not 1<=len(key)<=128:raise AdminError(400,'Chave invalida.')
        return key
    def execute(self,payload,authorization,ip):
        session=self.authorize(authorization,ip);action=payload.get('action')
        if action=='logout':
            with self.lock:self.sessions.pop(hashlib.sha256(authorization[7:].encode()).hexdigest(),None)
            return True
        with closing(self.connect()) as db:
            db.row_factory=sqlite3.Row
            if action=='keys_list':
                rows=db.execute("""SELECT k.id AS Id,k.key_value AS KeyValue,k.created_at AS CreatedAt,k.expires_at AS ExpiresAt,
                    COALESCE(k.hwid_bound,'') AS HwidBound,k.status AS Status,COALESCE(k.note,'') AS Note,
                    (SELECT COUNT(*) FROM key_ip_bindings b WHERE b.key_value=k.key_value) AS IpCount,
                    COALESCE((SELECT ip_limit FROM key_ip_policy p WHERE p.key_value=k.key_value),1) AS IpLimit,
                    COALESCE((SELECT ip FROM key_ip_bindings b WHERE b.key_value=k.key_value ORDER BY rowid LIMIT 1),'') AS FirstIp
                    FROM keys k ORDER BY k.id DESC LIMIT 10000""").fetchall()
                return [dict(row) for row in rows]
            if action=='keys_create':
                note=payload.get('note','');expires=payload.get('expires')
                if not isinstance(note,str) or len(note)>300:raise AdminError(400,'Observacao invalida.')
                if expires is None:expires='never'
                else:
                    try:
                        date=datetime.datetime.fromisoformat(expires.replace('Z','+00:00'))
                        if date.tzinfo is None or date<=datetime.datetime.now(datetime.timezone.utc):raise ValueError()
                    except (ValueError,TypeError,AttributeError):raise AdminError(400,'Validade invalida.')
                key=secrets.token_hex(16).upper()
                with db:db.execute("INSERT INTO keys(key_value,created_at,expires_at,note) VALUES(?,?,?,?)",(key,datetime.datetime.now(datetime.timezone.utc).isoformat(),expires,note))
                return key
            if action=='password_change':
                old=self.password(payload.get('current'));new=self.password(payload.get('password'))
                if len(new.decode())<12:raise AdminError(400,'Use pelo menos 12 caracteres na nova senha.')
                if not bcrypt.checkpw(old,session['hash'].encode()):return False
                password_hash=bcrypt.hashpw(new,bcrypt.gensalt(rounds=12)).decode()
                with db:changed=db.execute('UPDATE admins SET password_hash=? WHERE username=? AND password_hash=?',(password_hash,session['user'],session['hash'])).rowcount
                return changed==1
            key=self.key(payload)
            if not db.execute('SELECT 1 FROM keys WHERE key_value=?',(key,)).fetchone():raise AdminError(404,'Chave nao encontrada.')
            if action=='ips_list':return [dict(row) for row in db.execute('SELECT ip AS Ip,first_seen AS FirstSeen,last_seen AS LastSeen FROM key_ip_bindings WHERE key_value=? ORDER BY rowid',(key,))]
            with db:
                if action=='keys_delete':
                    db.execute('DELETE FROM key_ip_bindings WHERE key_value=?',(key,))
                    db.execute('DELETE FROM key_ip_policy WHERE key_value=?',(key,))
                    if db.execute('DELETE FROM keys WHERE key_value=?',(key,)).rowcount!=1:raise AdminError(404,'Chave nao encontrada.')
                elif action=='keys_revoke':db.execute("UPDATE keys SET status='revoked' WHERE key_value=?",(key,))
                elif action=='keys_activate':
                    if not db.execute("UPDATE keys SET status='active' WHERE key_value=? AND (expires_at='never' OR julianday(expires_at)>julianday('now'))",(key,)).rowcount:raise AdminError(400,'Esta chave expirou.')
                elif action=='ips_reset':db.execute('DELETE FROM key_ip_bindings WHERE key_value=?',(key,))
                elif action=='ips_limit':
                    limit=payload.get('limit')
                    if type(limit) is not int or not 1<=limit<=32:raise AdminError(400,'Use um limite entre 1 e 32.')
                    db.execute('INSERT INTO key_ip_policy VALUES(?,?) ON CONFLICT(key_value) DO UPDATE SET ip_limit=excluded.ip_limit',(key,limit))
                    db.execute('DELETE FROM key_ip_bindings WHERE key_value=? AND rowid NOT IN (SELECT rowid FROM key_ip_bindings WHERE key_value=? ORDER BY rowid LIMIT ?)',(key,key,limit))
                else:raise AdminError(400,'Operacao indisponivel.')
            return True
