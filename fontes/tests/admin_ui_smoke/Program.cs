// language: C# / file: Program.cs / runtime: .NET 8 / target: isolated Windows x64 admin UI smoke tests
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text.Json;
using System.Linq;
using System.Reflection;
using System.Windows.Forms;

internal static class Smoke {
    private static readonly BindingFlags Private = BindingFlags.NonPublic | BindingFlags.Instance;
    private static T Field<T>(MainForm form, string name) => (T)(typeof(MainForm).GetField(name, Private)?.GetValue(form) ?? throw new Exception("Field missing: " + name));
    private static void Call(MainForm form, string name, params object?[] args) => typeof(MainForm).GetMethod(name, Private)!.Invoke(form, args);
    private static void Check(bool result, string message) { if (!result) throw new Exception(message); Console.WriteLine("PASS " + message); }

    private static void WaitLogin(MainForm form) {
        var until=DateTime.UtcNow.AddSeconds(10);
        while(Field<bool>(form,"_loginPending") && DateTime.UtcNow<until){Application.DoEvents();System.Threading.Thread.Sleep(5);}
        Check(!Field<bool>(form,"_loginPending"),"login task completed");
    }

    [STAThread]
    private static int Main(string[] args) {
        int remoteIndex=Array.IndexOf(args,"--remote");if(remoteIndex>=0)return Remote(args[remoteIndex+1],args.Length>remoteIndex+2?args[remoteIndex+2]:null);
        string folder = Path.Combine(Environment.CurrentDirectory, "verificacao", "testes-admin", Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(folder);
        string path = Path.Combine(folder, "hillblack.db");
        Console.WriteLine("Isolated database: " + path);
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        using var form = new MainForm(path);
        int result = 0;
        form.Shown += (_, _) => form.BeginInvoke(new Action(() => {
            try {
                var db = Field<Database>(form, "_db");
                Check(!db.CheckLogin("nonexistent", "invalid"), "unknown login rejected");
                Check(!db.HasAdmins(),"fresh install has no built-in admin password");
                db.CreateFirstAdmin("fixture-admin","Fixture-Secret-2026!");
                Check(!db.CheckLogin("fixture-admin", "wrong"), "wrong password rejected");
                Check(db.CheckLogin("fixture-admin", "Fixture-Secret-2026!"), "created account authenticates");
                db.Init();Check(db.CheckLogin("fixture-admin","Fixture-Secret-2026!"),"initialization preserves existing credentials");
                Check(!db.ChangePassword("fixture-admin","wrong","Another-Fixture-2026!"),"password change rejects incorrect current password");
                Check(db.ChangePassword("fixture-admin","Fixture-Secret-2026!","Another-Fixture-2026!"),"authenticated password change succeeds");
                Check(!db.CheckLogin("fixture-admin","Fixture-Secret-2026!") && db.CheckLogin("fixture-admin","Another-Fixture-2026!"),"previous password no longer authenticates");
                var vault=new AdminLoginStore(path);
                Check(vault.Load() is null,"no remembered admin on a fresh installation");
                Field<TextBox>(form,"_user").Text="fixture-admin";
                Field<TextBox>(form,"_password").Text="Another-Fixture-2026!";
                Call(form,"Login");WaitLogin(form);
                Check(Field<string>(form,"_signedInUser")=="fixture-admin","remembering still requires valid authentication");
                Check(vault.Load()==new SavedAdminLogin("fixture-admin","Another-Fixture-2026!"),"successful login saves protected credentials");
                string vaultPath=Path.Combine(folder,"admin-login.dat");byte[] encrypted=File.ReadAllBytes(vaultPath);
                Check(!System.Text.Encoding.UTF8.GetString(encrypted).Contains("Another-Fixture-2026!") && !System.Text.Encoding.Unicode.GetString(encrypted).Contains("Another-Fixture-2026!"),"remembered password is not plaintext");
                using(var reopened=new MainForm(path)) {
                    reopened.Show();Application.DoEvents();
                    Check(Field<TextBox>(reopened,"_user").Text=="fixture-admin" && Field<TextBox>(reopened,"_password").Text=="Another-Fixture-2026!","reopening fills remembered username and password");
                    Check(Field<TextBox>(reopened,"_password").UseSystemPasswordChar && Field<string>(reopened,"_signedInUser")=="","restored password remains masked and does not bypass login");
                    reopened.Close();
                }
                Call(form,"ShowDashboard",false);Field<TextBox>(form,"_password").Text="wrong";
                Call(form,"Login");WaitLogin(form);
                Check(Field<string>(form,"_signedInUser")=="" && vault.Load()?.Password=="Another-Fixture-2026!","failed login cannot overwrite saved credentials");
                encrypted[encrypted.Length/2]^=1;File.WriteAllBytes(vaultPath,encrypted);
                bool corruptRejected=false;try{vault.Load();}catch(CryptographicException){corruptRejected=true;}
                Check(corruptRejected,"tampered remembered credentials are rejected");
                Field<TextBox>(form,"_password").Clear();Call(form,"RestoreSavedLogin");
                Check(Field<TextBox>(form,"_password").Text=="" && Field<Label>(form,"_loginFeedback").Text.Length>0,"unreadable vault falls back to manual login");
                Field<CheckBox>(form,"_rememberLogin").Checked=false;
                Check(vault.Load() is null,"unchecking remember removes stored credentials");
                Field<CheckBox>(form,"_rememberLogin").Checked=true;
                vault.Save("fixture-admin","Another-Fixture-2026!");
                string other=Path.Combine(folder,"other");Directory.CreateDirectory(other);
                File.Copy(vaultPath,Path.Combine(other,"admin-login.dat"));
                bool otherRejected=false;try{new AdminLoginStore(Path.Combine(other,"hillblack.db")).Load();}catch(CryptographicException){otherRejected=true;}
                Check(otherRejected,"remembered credentials are bound to their database location");
                vault.Clear();Call(form,"ShowDashboard",false);
                bool creationRejected=false;try{db.CreateFirstAdmin("second-admin","Fixture-Secret-2026!");}catch(InvalidOperationException){creationRejected=true;}Check(creationRejected,"first-admin setup cannot replace an existing account");
                Field<Label>(form,"_loginFeedback").Text="";Field<Button>(form,"_loginButton").Text="Entrar no painel";
                Field<Label>(form,"_loginHeading").Text="Bem-vindo de volta";Field<Label>(form,"_loginDescription").Text="Entre com suas credenciais para continuar.";
                Check(db.GetRelease()==null,"no release published by default");
                db.PublishRelease(2,"1.4.0","https://example.com/client.zip","Teste isolado");
                Check(db.GetRelease()?.Build==2,"required build persisted");
                db.PublishRelease(2,"1.4.0","https://example.com/fixed.zip","Corrigir link");
                Check(db.GetRelease()?.Url.EndsWith("fixed.zip")==true,"same build URL can be corrected");
                bool rejected=false;try{db.PublishRelease(1,"1.3.0","https://example.com/old.zip","");}catch(ArgumentException){rejected=true;}Check(rejected,"release rollback rejected");
                rejected=false;try{db.PublishRelease(3,"1.5.0","http://example.com/client.zip","");}catch(ArgumentException){rejected=true;}Check(rejected,"non HTTPS releases rejected");
                File.WriteAllText(Path.Combine(folder,"HillBlack.ini"),"[auth]\nmode=online\nendpoint=https://example.com/validate\n");
                string package=Path.Combine(folder,"client.zip");byte[] executable=new byte[2048];executable[0]=77;executable[1]=90;
                string digest=Convert.ToHexString(SHA256.HashData(executable)).ToLowerInvariant();
                using(var zip=ZipFile.Open(package,ZipArchiveMode.Create)) {
                    using(var entry=zip.CreateEntry("Capri.exe").Open())entry.Write(executable);
                    using(var writer=new StreamWriter(zip.CreateEntry("client-version.json").Open()))writer.Write(JsonSerializer.Serialize(new {build=3,version="1.4.1",sha256=digest}));
                    using(var writer=new StreamWriter(zip.CreateEntry("Atualizar-Capri.ps1").Open()))writer.Write("# test fixture, never executed");
                }
                var inspected=db.InspectPackage(package);Check(inspected.Build==3 && inspected.PackageHash.Length==64,"ZIP version and integrity validated");
                var publication=db.PublishPackage(package,"Pacote isolado");Check(db.GetRelease()?.PackageHash==publication.PackageHash,"ZIP publication stores package checksum");
                Check(File.Exists(Path.Combine(folder,"servidor","releases",publication.PackageHash+".zip")),"ZIP copied to local package service");
                using(var zip=ZipFile.Open(package,ZipArchiveMode.Update)){using var writer=new StreamWriter(zip.CreateEntry("hillblack.db").Open());writer.Write("private");}
                rejected=false;try{db.InspectPackage(package);}catch(ArgumentException){rejected=true;}Check(rejected,"ZIP containing database rejected");
                string admPackage=Path.Combine(folder,"adm.zip");
                using(var zip=ZipFile.Open(admPackage,ZipArchiveMode.Create)){
                    using(var entry=zip.CreateEntry("Capri.Admin.exe").Open())entry.Write(executable);
                    using(var writer=new StreamWriter(zip.CreateEntry("admin-version.json").Open()))writer.Write(JsonSerializer.Serialize(new{build=1,version="1.0.0",sha256=digest}));
                    using(var writer=new StreamWriter(zip.CreateEntry("Atualizar-ADM.ps1").Open()))writer.Write("# isolated fixture");
                }
                db.PublishPackage(admPackage,"ADM fixture");Check(db.GetRelease(true)?.Build==1 && db.GetRelease()?.Build==3,"ADM release is independent of client release");
                bool invalidOwner=false;try{db.AddAdmin("fixture-admin","wrong","coowner","Coowner-Fixture-2026!");}catch(ArgumentException){invalidOwner=true;}Check(invalidOwner,"creating co-owner requires owner password");
                db.AddAdmin("fixture-admin","Another-Fixture-2026!","coowner","Coowner-Fixture-2026!");Check(db.CheckLogin("coowner","Coowner-Fixture-2026!"),"separate co-owner account authenticates");
                var timed=db.GenerateKey(DateTime.UtcNow.AddHours(1),"Timezone fixture");
                using(var c=new Microsoft.Data.Sqlite.SqliteConnection("Data Source="+path)) {
                    c.Open();using var cmd=c.CreateCommand();cmd.CommandText="UPDATE keys SET expires_at=@e WHERE key_value=@k";
                    cmd.Parameters.AddWithValue("@e",DateTimeOffset.UtcNow.AddHours(1).ToOffset(TimeSpan.FromHours(-3)).ToString("O"));cmd.Parameters.AddWithValue("@k",timed);cmd.ExecuteNonQuery();
                }
                Check(db.BindHwid(timed,"fixture"),"local validation accepts future UTC-3 expiry");
                using(var c=new Microsoft.Data.Sqlite.SqliteConnection("Data Source="+path)) {
                    c.Open();using var cmd=c.CreateCommand();cmd.CommandText="DELETE FROM keys WHERE key_value=@k";cmd.Parameters.AddWithValue("@k",timed);cmd.ExecuteNonQuery();
                }
                var disposable=db.GenerateKey(null,"Delete fixture");var retained=db.GenerateKey(null,"Retain fixture");
                db.SetIpLimit(disposable,2);
                using(var c=new Microsoft.Data.Sqlite.SqliteConnection("Data Source="+path)){c.Open();using var cmd=c.CreateCommand();cmd.CommandText="INSERT INTO key_ip_bindings(key_value,ip) VALUES(@k,'198.51.100.7')";cmd.Parameters.AddWithValue("@k",disposable);cmd.ExecuteNonQuery();}
                db.DeleteKey(disposable);
                Check(!db.GetAllKeys().Any(k=>k.KeyValue==disposable) && db.GetAllKeys().Any(k=>k.KeyValue==retained),"delete removes only selected license");
                Check(db.GetKeyIps(disposable).Count==0,"delete removes bound IPs");
                using(var c=new Microsoft.Data.Sqlite.SqliteConnection("Data Source="+path)){c.Open();using var cmd=c.CreateCommand();cmd.CommandText="SELECT COUNT(*) FROM key_ip_policy WHERE key_value=@k";cmd.Parameters.AddWithValue("@k",disposable);Check((long)cmd.ExecuteScalar()! == 0,"delete removes IP policy");}
                db.DeleteKey(retained);
                var active = db.GenerateKey(DateTime.UtcNow.AddDays(30), "Equipe Atlas · mensal");
                var boundary=DateTimeOffset.Parse("2026-11-03T15:00:00Z");
                Check(!KeyExpiry.IsExpired("2026-11-03T12:00:00-03:00",boundary.AddTicks(-1)),"offset timestamp remains valid before deadline");
                Check(KeyExpiry.IsExpired("2026-11-03T12:00:00-03:00",boundary),"expires at exact deadline across time zones");
                Check(KeyExpiry.IsExpired("2026-11-03T15:00:00",boundary),"legacy timestamp without zone interpreted as UTC");
                Check(KeyExpiry.IsExpired("invalid",boundary) && !KeyExpiry.IsExpired("never",boundary),"invalid expiry rejected and lifetime preserved");
                Check(KeyExpiry.Remaining("2026-11-03T15:00:01Z",boundary)=="00h 00m 01s","countdown includes seconds");
                db.GenerateKey(null, "Studio Norte · vitalícia");
                db.GenerateKey(DateTime.UtcNow.AddDays(-1), "Conta de teste · expirada");
                var revoked = db.GenerateKey(null, "Acesso encerrado · revogada");
                db.RevokeKey(revoked);
                Call(form, "ShowDashboard", true);
                var grid = Field<DataGridView>(form, "_grid");
                var search = Field<TextBox>(form, "_search");
                Check(grid.Rows.Count == 4, "all database records rendered");
                Check(Field<Label>(form, "_active").Text == "2", "expired records excluded from active metric");
                search.Text = "Atlas";
                Check(grid.Rows.Count == 1, "search filters observations");
                search.Text = active.Substring(0, 12);
                Check(grid.Rows.Count == 1, "search filters key values");
                search.Clear();
                Call(form, "RefreshRecords", active);
                Check(Field<TextBox>(form, "_selectedKey").Text == active, "selection restored by key");
                Field<TextBox>(form, "_note").Text = "Emitida pelo teste da interface";
                Call(form, "GenerateKey");
                Check(db.GetAllKeys().Count == 5, "generate action persists license");
                Check(grid.Rows.Count == 5, "generated license reflected in grid");
                string generated = Field<TextBox>(form, "_selectedKey").Text;
                Check(generated.Length == 32, "generated license selected");
                Field<Button>(form, "_revoke").PerformClick();
                Check(Field<Button>(form, "_confirm").Visible, "revocation requires explicit confirmation");
                Field<Button>(form, "_cancelRevoke").PerformClick();
                Check(db.GetAllKeys().Single(k => k.KeyValue == generated).Status == "active", "cancel keeps license active");
                Field<Button>(form, "_revoke").PerformClick();
                Field<Button>(form, "_confirm").PerformClick();
                Check(db.GetAllKeys().Single(k => k.KeyValue == generated).Status == "revoked", "confirmed revocation persists");
                Check(Field<Button>(form, "_revoke").Text=="Ativar", "revoked license offers reactivation");
                Field<Button>(form,"_revoke").PerformClick();Check(db.GetAllKeys().Single(k=>k.KeyValue==generated).Status=="active","ADM reactivates selected license");
                Check(db.GetAllKeys().Single(k=>k.KeyValue==active).IpLimit==1,"new licenses default to one IP");
                db.SetIpLimit(active,2);
                using(var c=new Microsoft.Data.Sqlite.SqliteConnection("Data Source="+path)){
                    c.Open();using var command=c.CreateCommand();command.CommandText="INSERT INTO key_ip_bindings(key_value,ip) VALUES(@key,'198.51.100.1'),(@key,'2001:db8::1')";command.Parameters.AddWithValue("@key",active);command.ExecuteNonQuery();
                }
                Check(db.GetAllKeys().Single(k=>k.KeyValue==active).IpCount==2 && db.GetAllKeys().Single(k=>k.KeyValue==active).FirstIp=="198.51.100.1","ADM shows count and first IP");
                db.SetIpLimit(active,1);Check(db.GetKeyIps(active).Count==1 && db.GetKeyIps(active)[0].Ip=="198.51.100.1","reducing limit preserves earliest IP");
                db.ResetKeyIps(active);Check(db.GetKeyIps(active).Count==0 && db.GetAllKeys().Single(k=>k.KeyValue==active).IpLimit==1,"IP reset preserves license policy");
                bool badLimit=false;try{db.SetIpLimit(active,0);}catch(ArgumentException){badLimit=true;}Check(badLimit,"zero IP limit rejected");
                using(var c=new Microsoft.Data.Sqlite.SqliteConnection("Data Source="+path)){c.Open();using var command=c.CreateCommand();command.CommandText="INSERT INTO key_ip_bindings(key_value,ip) VALUES(@key,'198.51.100.42')";command.Parameters.AddWithValue("@key",active);command.ExecuteNonQuery();}
                Call(form, "RefreshRecords", active);
                int renderIndex = Array.IndexOf(args, "--render");
                if (renderIndex >= 0 && renderIndex + 1 < args.Length) {
                    string output = Path.GetFullPath(args[renderIndex + 1]); Directory.CreateDirectory(output);
                    form.Refresh();Application.DoEvents();
                    using (var bitmap = new Bitmap(form.Width, form.Height)) { form.DrawToBitmap(bitmap, form.ClientRectangle); bitmap.Save(Path.Combine(output, "admin-dashboard.png"), ImageFormat.Png); }
                    Call(form,"ToggleGenerator");Application.DoEvents();form.Refresh();
                    using(var bitmap=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(bitmap,form.ClientRectangle);bitmap.Save(Path.Combine(output,"admin-emissao.png"),ImageFormat.Png);}
                    Call(form,"ToggleGenerator");var normalSize=form.Size;form.Size=new Size(1000,700);Application.DoEvents();form.Refresh();
                    using(var bitmap=new Bitmap(form.Width,form.Height)){form.DrawToBitmap(bitmap,form.ClientRectangle);bitmap.Save(Path.Combine(output,"admin-compacto.png"),ImageFormat.Png);}
                    form.Size=normalSize;Application.DoEvents();
                    using(var timer=new System.Windows.Forms.Timer{Interval=300}){
                        timer.Tick+=(_,_)=>{var editor=Application.OpenForms.Cast<Form>().FirstOrDefault(f=>f.Text=="IPs por chave");if(editor==null)return;timer.Stop();editor.Refresh();using var bitmap=new Bitmap(editor.Width,editor.Height);editor.DrawToBitmap(bitmap,new Rectangle(0,0,editor.Width,editor.Height));bitmap.Save(Path.Combine(output,"admin-ip.png"),ImageFormat.Png);editor.Close();};timer.Start();Call(form,"ShowIpEditor");
                    }
                    Call(form, "ShowDashboard", false); form.Refresh();Application.DoEvents();
                    using (var bitmap = new Bitmap(form.Width, form.Height)) { form.DrawToBitmap(bitmap, form.ClientRectangle); bitmap.Save(Path.Combine(output, "admin-login.png"), ImageFormat.Png); }
                    Call(form, "ShowDashboard", true);
                    typeof(MainForm).GetField("_signedInUser",Private)!.SetValue(form,"fixture-admin");
                    var ambient=(AmbientPanel)Field<Panel>(form,"_dashboard");long before=ambient.Frames;
                    using(var timer=new System.Windows.Forms.Timer{Interval=300}) {
                        timer.Tick+=(_,_)=>{var editor=Application.OpenForms.Cast<Form>().FirstOrDefault(f=>f.Text=="Alterar senha");if(editor==null)return;timer.Stop();using var bitmap=new Bitmap(editor.Width,editor.Height);editor.DrawToBitmap(bitmap,new Rectangle(0,0,editor.Width,editor.Height));bitmap.Save(Path.Combine(output,"admin-senha.png"),ImageFormat.Png);editor.Close();};
                        timer.Start();Call(form,"ChangePassword");
                    }
                    using (var timer = new System.Windows.Forms.Timer { Interval = 300 }) {
                        timer.Tick += (_, _) => {
                            var editor = Application.OpenForms.Cast<Form>().FirstOrDefault(f => f.Text == "Publicar atualizacao obrigatoria");
                            if (editor == null) return;
                            timer.Stop();editor.Refresh();
                            using var bitmap = new Bitmap(editor.Width, editor.Height);
                            editor.DrawToBitmap(bitmap, new Rectangle(0,0,editor.Width,editor.Height));
                            bitmap.Save(Path.Combine(output,"admin-atualizacao.png"),ImageFormat.Png);
                            editor.Close();
                        };
                        timer.Start();Call(form,"ShowReleaseEditor");
                    }
                    Check(File.Exists(Path.Combine(output,"admin-atualizacao.png")),"publish update dialog renders");
                    form.Activate();var activeUntil=DateTime.UtcNow.AddMilliseconds(160);while(DateTime.UtcNow<activeUntil){Application.DoEvents();System.Threading.Thread.Sleep(5);}
                    if(Palette.MotionEnabled)Check(ambient.Frames>before,"administrator background advances frames");
                    long beforeFocus=ambient.Frames;bool pausedWithoutFocus=false;
                    using(var focusWindow=new Form{Text="Focus fixture",ClientSize=new Size(220,80),StartPosition=FormStartPosition.CenterParent,BackColor=Palette.Background,ShowInTaskbar=false})
                    using(var focusTimer=new System.Windows.Forms.Timer{Interval=140}){
                        focusTimer.Tick+=(_,_)=>{focusTimer.Stop();pausedWithoutFocus=ambient.Frames==beforeFocus;focusWindow.Close();};
                        focusTimer.Start();focusWindow.ShowDialog(form);
                    }
                    Check(pausedWithoutFocus,"background pauses when another window has focus");
                    form.WindowState=FormWindowState.Minimized;long paused=ambient.Frames;
                    var pauseUntil=DateTime.UtcNow.AddMilliseconds(120);while(DateTime.UtcNow<pauseUntil){Application.DoEvents();System.Threading.Thread.Sleep(5);}
                    Check(ambient.Frames==paused,"background pauses while minimized");
                    form.WindowState=FormWindowState.Normal;
                    var resumeUntil=DateTime.UtcNow.AddMilliseconds(160);while(DateTime.UtcNow<resumeUntil){Application.DoEvents();System.Threading.Thread.Sleep(5);}
                    if(Palette.MotionEnabled)Check(ambient.Frames>paused,"background resumes after restore");
                    Console.WriteLine("RENDERED " + output);
                }
                if (args.Contains("--preview")) { Console.WriteLine("PREVIEW READY: isolated dashboard. Close window to end."); }
                else { form.Close(); }
            } catch (Exception ex) { result = 1; Console.Error.WriteLine(ex.InnerException ?? ex); form.Close(); }
        }));
        Application.Run(form);
        Console.WriteLine(result == 0 ? "Admin smoke tests passed." : "Admin smoke tests failed.");
        return result;
    }
    [STAThread]
    private static int Remote(string endpoint,string? output){
        Application.SetHighDpiMode(HighDpiMode.PerMonitorV2);Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        string folder=Path.Combine(Environment.CurrentDirectory,"verificacao","remote-ui-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(folder);string database=Path.Combine(folder,"absent.db");
        using var form=new MainForm(database,endpoint);int result=0;
        form.Shown+=(_,_)=>form.BeginInvoke(new Action(()=>{try{
            var db=Field<Database>(form,"_db");Check(db.IsRemote && !File.Exists(database),"remote ADM never creates local database");
            Field<TextBox>(form,"_user").Text="coowner";Field<TextBox>(form,"_password").Text="Coowner-Fixture-2026!";Call(form,"Login");WaitLogin(form);
            var until=DateTime.UtcNow.AddSeconds(12);while(Field<bool>(form,"_refreshPending") && DateTime.UtcNow<until){Application.DoEvents();System.Threading.Thread.Sleep(5);}
            Check(Field<string>(form,"_signedInUser")=="coowner","remote UI authenticates C# BCrypt account through Python API: "+Field<Label>(form,"_loginFeedback").Text);
            var records=db.GetAllKeys();Check(records.Count==5,"remote ADM sees same server records");
            string key=db.GenerateKey(DateTime.UtcNow.AddDays(7),"Remote fixture");Check(db.GetAllKeys().Any(k=>k.KeyValue==key),"remote key creation persists centrally");
            db.SetIpLimit(key,2);Check(db.GetAllKeys().Single(k=>k.KeyValue==key).IpLimit==2,"remote IP limit persists centrally");
            db.RevokeKey(key);Check(db.GetAllKeys().Single(k=>k.KeyValue==key).Status=="revoked","remote revocation persists centrally");db.ActivateKey(key);db.ResetKeyIps(key);
            Check(!File.Exists(database),"remote actions keep local database absent");
            if(output!=null){Directory.CreateDirectory(output);form.Refresh();using var image=new Bitmap(form.Width,form.Height);form.DrawToBitmap(image,form.ClientRectangle);image.Save(Path.Combine(output,"admin-remote.png"),ImageFormat.Png);}
            Call(form,"ShowDashboard",false);Check(Field<DataGridView>(form,"_grid").Rows.Count==0,"remote logout hides license data");form.Close();
        }catch(Exception e){result=1;Console.Error.WriteLine(e.InnerException??e);form.Close();}}));
        Application.Run(form);return result;
    }
}
