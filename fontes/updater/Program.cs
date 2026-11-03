using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;

// Windows .NET Framework updater: no console, PowerShell or runtime installation.
internal sealed class UpdateOptions {
    public string Root,Url,Sha,Archive;
    public long Size;
    public int Build,WaitPid;
    public bool NoUi,NoRestart;
    public static UpdateOptions Parse(string[] args) {
        var o=new UpdateOptions();
        for(int i=0;i<args.Length;i++) {
            string key=args[i];
            if(key=="--no-ui"){o.NoUi=true;continue;}
            if(key=="--no-restart"){o.NoRestart=true;continue;}
            if(i+1>=args.Length)throw new Exception("Parametro incompleto.");
            string value=args[++i];
            switch(key) {
                case "--root":o.Root=value;break;case "--url":o.Url=value;break;
                case "--sha":o.Sha=value;break;case "--archive":o.Archive=value;break;
                case "--build":o.Build=int.Parse(value);break;case "--size":o.Size=long.Parse(value);break;
                case "--wait-pid":o.WaitPid=int.Parse(value);break;default:throw new Exception("Parametro desconhecido.");
            }
        }
        if(string.IsNullOrWhiteSpace(o.Root)||o.Build<1||o.WaitPid<0||o.Size<0||o.Size>Installer.MaxPackage||
           !Regex.IsMatch(o.Sha??"","\\A[a-fA-F0-9]{64}\\z"))throw new Exception("Abra o Capri e clique em Atualizar agora.");
        o.Root=Path.GetFullPath(o.Root).TrimEnd(Path.DirectorySeparatorChar);
        if(string.Equals(o.Root,Path.GetPathRoot(o.Root).TrimEnd('\\'),StringComparison.OrdinalIgnoreCase))throw new Exception("Pasta de instalacao invalida.");
        if(string.IsNullOrEmpty(o.Archive)) {
            Uri uri;
            if(!Uri.TryCreate(o.Url,UriKind.Absolute,out uri)||uri.UserInfo.Length!=0||
               !(uri.Scheme=="https"||(o.NoUi&&o.NoRestart&&uri.Scheme=="http"&&uri.IsLoopback)))throw new Exception("Endereco de atualizacao invalido.");
        }
        return o;
    }
}

internal sealed class Installer {
    public const long MaxPackage=100L*1024*1024;
    private readonly UpdateOptions options;
    private readonly Action<string,int> progress;
    private readonly CancellationToken cancel;
    private readonly List<Change> applied=new List<Change>();
    private string work,stage,payload,backup;
    private sealed class Change { public string Source,Destination,Relative,Backup; public bool Existed; }
    private static readonly JavaScriptSerializer Json=new JavaScriptSerializer { MaxJsonLength=8192 };
    public Installer(UpdateOptions o,Action<string,int> p,CancellationToken c){options=o;progress=p;cancel=c;}
    public static string Hash(string file) {using(var stream=File.OpenRead(file))using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(stream)).Replace("-","").ToLowerInvariant();}
    private static string TextHash(string value) {using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(value))).Replace("-","");}
    private string Inside(string parent,string relative) {
        string full=Path.GetFullPath(Path.Combine(parent,relative));
        if(!full.StartsWith(parent.TrimEnd('\\')+"\\",StringComparison.OrdinalIgnoreCase))throw new Exception("Caminho fora da pasta de atualizacao.");
        return full;
    }
    private void CheckPath(string path) {
        string current=Path.GetFullPath(path),root=options.Root;
        while(current!=null&&(string.Equals(current,root,StringComparison.OrdinalIgnoreCase)||current.StartsWith(root+"\\",StringComparison.OrdinalIgnoreCase))) {
            if((File.Exists(current)||Directory.Exists(current))&&(File.GetAttributes(current)&FileAttributes.ReparsePoint)!=0)throw new Exception("A pasta contem um link ou redirecionamento.");
            if(string.Equals(current,root,StringComparison.OrdinalIgnoreCase))break;
            current=Path.GetDirectoryName(current);
        }
    }
    private static bool SafeEntry(string name) {
        if(string.IsNullOrEmpty(name)||name.Contains('\\')||name.Contains(':')||name.StartsWith("/")||Regex.IsMatch(name,"\\.(db|sqlite|sqlite3)$",RegexOptions.IgnoreCase))return false;
        foreach(string part in name.TrimEnd('/').Split('/')) {
            if(part.Length==0||part=="."||part==".."||part.EndsWith(".")||part.EndsWith(" ")||part.IndexOfAny(Path.GetInvalidFileNameChars())>=0||
               Regex.IsMatch(part,"^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\\..*)?$",RegexOptions.IgnoreCase))return false;
        }
        if(name.EndsWith("/"))return name.StartsWith("assets/")||name.StartsWith("licenses/")||name=="configs/";
        return new[]{"Capri.exe","client-version.json","Atualizar-Capri.ps1","HillBlack.ini","COMO-ABRIR.txt","configs/default.hbc"}.Contains(name)||name.StartsWith("assets/")||name.StartsWith("licenses/");
    }
    private void Report(string text,int percent) {
        File.AppendAllText(Path.Combine(work,"atualizacao.log"),DateTime.UtcNow.ToString("u")+" "+text+Environment.NewLine);
        progress(text,percent);
    }
    private void Download(string path) {
        ServicePointManager.SecurityProtocol=SecurityProtocolType.Tls12;
        using(var handler=new HttpClientHandler {AllowAutoRedirect=false})
        using(var client=new HttpClient(handler)) {
            client.Timeout=Timeout.InfiniteTimeSpan;
            client.DefaultRequestHeaders.UserAgent.ParseAdd("Capri-Updater/2.0");
            using(var response=client.GetAsync(options.Url,HttpCompletionOption.ResponseHeadersRead,cancel).GetAwaiter().GetResult()) {
                response.EnsureSuccessStatusCode();long? length=response.Content.Headers.ContentLength;
                if(length.HasValue&&(length.Value>MaxPackage||(options.Size>0&&length.Value!=options.Size)))throw new Exception("Tamanho do pacote diferente do publicado.");
                using(var source=response.Content.ReadAsStreamAsync().GetAwaiter().GetResult())
                using(var output=File.Create(path))
                using(cancel.Register(delegate {try{source.Dispose();}catch{}})) {
                    byte[] buffer=new byte[65536];long total=0;int lastPercent=-1;
                    while(true) {
                        cancel.ThrowIfCancellationRequested();
                        int count=source.ReadAsync(buffer,0,buffer.Length,cancel).GetAwaiter().GetResult();if(count==0)break;
                        total+=count;if(total>MaxPackage||(options.Size>0&&total>options.Size))throw new Exception("Pacote maior que o esperado.");
                        output.Write(buffer,0,count);
                        int percent=options.Size>0?(int)(total*60/options.Size):5;
                        if(percent!=lastPercent){Report("Baixando atualizacao... "+(options.Size>0?(total*100/options.Size).ToString()+"%":""),5+percent);lastPercent=percent;}
                    }
                }
            }
        }
    }
    private Dictionary<string,object> Manifest(string path) {
        if(new FileInfo(path).Length>4096)throw new Exception("Manifesto invalido.");
        return Json.Deserialize<Dictionary<string,object>>(File.ReadAllText(path));
    }
    public void Run() {
        CheckPath(options.Root);work=Inside(options.Root,"atualizacao");CheckPath(work);Directory.CreateDirectory(work);
        CheckPath(Path.Combine(work,"atualizacao.log"));
        using(var mutex=new Mutex(false,"Local\\CapriUpdate-"+TextHash(options.Root.ToUpperInvariant()).Substring(0,24))) {
            bool acquired=false;
            try {
                try{acquired=mutex.WaitOne(0);}catch(AbandonedMutexException){acquired=true;}
                if(!acquired)throw new Exception("Ja existe uma atualizacao em andamento.");
                stage=Inside(work,"native-"+Guid.NewGuid().ToString("N"));payload=Inside(stage,"payload");backup=Inside(stage,"backup");
                Directory.CreateDirectory(payload);Directory.CreateDirectory(backup);
                string archive=options.Archive;
                if(string.IsNullOrEmpty(archive)){archive=Inside(stage,"pacote.zip");Report("Conectando ao servidor...",2);Download(archive);}
                cancel.ThrowIfCancellationRequested();
                long size=new FileInfo(archive).Length;
                if(size<1||size>MaxPackage||(options.Size>0&&size!=options.Size))throw new Exception("Download incompleto.");
                Report("Conferindo a integridade...",68);
                if(!string.Equals(Hash(archive),options.Sha,StringComparison.OrdinalIgnoreCase))throw new Exception("A integridade do pacote nao confere. Nenhum arquivo foi aplicado.");
                using(var zip=ZipFile.OpenRead(archive)) {
                    if(zip.Entries.Count>1024)throw new Exception("Arquivos demais no pacote.");
                    var names=new HashSet<string>(StringComparer.OrdinalIgnoreCase);long expanded=0;
                    foreach(var entry in zip.Entries) {
                        cancel.ThrowIfCancellationRequested();string name=entry.FullName;
                        if(!SafeEntry(name)||!names.Add(name.TrimEnd('/'))||((entry.ExternalAttributes>>16)&0xF000)==0xA000)throw new Exception("O ZIP contem um caminho ou arquivo nao permitido.");
                        expanded+=entry.Length;if(expanded>150L*1024*1024)throw new Exception("Pacote expandido muito grande.");
                        if(name.EndsWith("/"))continue;
                        string target=Inside(payload,name);Directory.CreateDirectory(Path.GetDirectoryName(target));
                        using(var source=entry.Open())using(var output=File.Create(target)) {
                            var bytes=new byte[65536];long written=0;int count;
                            while((count=source.Read(bytes,0,bytes.Length))>0){cancel.ThrowIfCancellationRequested();written+=count;if(written>entry.Length)throw new Exception("Conteudo expandido diferente do ZIP.");output.Write(bytes,0,count);}
                            if(written!=entry.Length)throw new Exception("Arquivo incompleto no ZIP.");
                        }
                    }
                }
                string versionFile=Inside(payload,"client-version.json"),exe=Inside(payload,"Capri.exe");
                var manifest=Manifest(versionFile);int build=Convert.ToInt32(manifest["build"]);string exeHash=Convert.ToString(manifest["sha256"]);
                if(build!=options.Build||!Regex.IsMatch(exeHash??"","\\A[a-fA-F0-9]{64}\\z"))throw new Exception("O pacote nao corresponde a versao exigida.");
                if(!File.Exists(exe)||!string.Equals(Hash(exe),exeHash,StringComparison.OrdinalIgnoreCase))throw new Exception("O executavel nao corresponde ao manifesto.");
                string installed=Inside(options.Root,"client-version.json");CheckPath(installed);
                if(File.Exists(installed)&&Convert.ToInt32(Manifest(installed)["build"])>build)throw new Exception("Uma versao mais nova ja esta instalada.");
                var changes=new List<Change>();
                foreach(string file in Directory.GetFiles(payload,"*",SearchOption.AllDirectories)) {
                    string relative=file.Substring(payload.Length+1),destination=Inside(options.Root,relative);CheckPath(destination);
                    if((relative=="HillBlack.ini"||relative=="configs\\default.hbc")&&File.Exists(destination))continue;
                    bool exists=File.Exists(destination);if(exists&&Hash(file)==Hash(destination))continue;
                    changes.Add(new Change{Source=file,Destination=destination,Relative=relative,Existed=exists,Backup=Inside(backup,relative)});
                }
                Report("Preparando a instalacao. Sua chave e configuracoes serao mantidas.",78);
                if(options.WaitPid>0) {
                    Report("Aguardando o Capri fechar...",80);
                    try {using(var old=Process.GetProcessById(options.WaitPid)){while(!old.WaitForExit(100))cancel.ThrowIfCancellationRequested();}}catch(ArgumentException){}
                }
                foreach(var change in changes)if(change.Existed){CheckPath(change.Destination);Directory.CreateDirectory(Path.GetDirectoryName(change.Backup));File.Copy(change.Destination,change.Backup,false);}
                int done=0;
                foreach(var change in changes.OrderBy(c=>c.Relative=="client-version.json"?2:c.Relative=="Capri.exe"?1:0)) {
                    cancel.ThrowIfCancellationRequested();CheckPath(change.Destination);Directory.CreateDirectory(Path.GetDirectoryName(change.Destination));
                    applied.Add(change);File.Copy(change.Source,change.Destination,true);
                    Report("Instalando atualizacao...",85+(++done*13/Math.Max(1,changes.Count)));
                }
                if(!string.Equals(Hash(Inside(options.Root,"Capri.exe")),exeHash,StringComparison.OrdinalIgnoreCase))throw new Exception("Falha ao conferir o executavel instalado.");
                string audit=Inside(work,"ultima-atualizacao.json");CheckPath(audit);
                File.WriteAllText(audit,Json.Serialize(new{build=build,backup=backup,completed=DateTime.UtcNow.ToString("o"),native=true}));
                Report("Atualizacao concluida.",100);
                // Installation is committed. A restart failure must not roll back while a new process may be starting.
                applied.Clear();
                if(!options.NoRestart)Process.Start(new ProcessStartInfo(Inside(options.Root,"Capri.exe")){WorkingDirectory=options.Root,UseShellExecute=false});
                // Keep the backup for recovery, discard only this run's verified staging files.
                try {
                    CheckPath(payload);Directory.Delete(payload,true);
                    if(string.IsNullOrEmpty(options.Archive))File.Delete(archive);
                } catch(Exception cleanup) {File.AppendAllText(Path.Combine(work,"atualizacao.log"),"Limpeza pendente: "+cleanup.Message+Environment.NewLine);}
            } catch(Exception error) {
                for(int i=applied.Count-1;i>=0;i--) {
                    Change change=applied[i];
                    try {CheckPath(change.Destination);if(change.Existed)File.Copy(change.Backup,change.Destination,true);else if(File.Exists(change.Destination))File.Delete(change.Destination);}
                    catch(Exception restore){File.AppendAllText(Path.Combine(work,"atualizacao.log"),"Falha ao restaurar "+change.Relative+": "+restore.Message+Environment.NewLine);}
                }
                Report("Falha: "+error.Message,0);throw;
            } finally {if(acquired)mutex.ReleaseMutex();}
        }
    }
}

internal sealed class UpdateWindow:Form {
    private readonly Label label=new Label();private readonly ProgressBar bar=new ProgressBar();
    private readonly Button stop=new Button();private readonly CancellationTokenSource cancellation=new CancellationTokenSource();
    private bool completed;
    public int Result=1;
    public UpdateWindow(UpdateOptions options) {
        Text="Capri / Atualizacao";ClientSize=new Size(500,170);StartPosition=FormStartPosition.CenterScreen;
        FormBorderStyle=FormBorderStyle.FixedDialog;MaximizeBox=false;BackColor=Color.FromArgb(12,15,20);ForeColor=Color.White;
        Font=new Font("Segoe UI",10);label.SetBounds(24,22,450,50);label.Text="Preparando atualizacao...";Controls.Add(label);
        bar.SetBounds(24,82,450,22);Controls.Add(bar);stop.SetBounds(364,122,110,30);stop.Text="Cancelar";stop.FlatStyle=FlatStyle.Flat;stop.ForeColor=Color.White;stop.BackColor=Color.FromArgb(36,41,52);stop.FlatAppearance.BorderColor=Color.FromArgb(68,77,94);Controls.Add(stop);
        stop.Click+=delegate{Cancel();};FormClosing+=delegate(object sender,FormClosingEventArgs e){if(!completed){e.Cancel=true;Cancel();}};
        Shown+=delegate {
            Task.Run(delegate {
                try {new Installer(options,Progress,cancellation.Token).Run();Result=0;}
                catch(Exception error) {if(!cancellation.IsCancellationRequested)BeginInvoke((Action)delegate{MessageBox.Show(this,"A atualizacao nao foi concluida. "+error.Message,"Capri",MessageBoxButtons.OK,MessageBoxIcon.Error);});}
                finally {BeginInvoke((Action)delegate{completed=true;Close();});}
            });
        };
    }
    private void Cancel(){if(completed)return;stop.Enabled=false;label.Text="Cancelando atualizacao...";cancellation.Cancel();}
    private void Progress(string text,int percent){BeginInvoke((Action)delegate{label.Text=text;bar.Value=Math.Max(0,Math.Min(100,percent));});}
}

internal static class Program {
    [STAThread] private static int Main(string[] args) {
        try {
            UpdateOptions options=UpdateOptions.Parse(args);
            if(options.NoUi){new Installer(options,delegate{},CancellationToken.None).Run();return 0;}
            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
            using(var window=new UpdateWindow(options)){Application.Run(window);return window.Result;}
        } catch(Exception error) {Console.Error.WriteLine(error.Message);if(!args.Contains("--no-ui"))MessageBox.Show(error.Message,"Capri",MessageBoxButtons.OK,MessageBoxIcon.Error);return 1;}
    }
}
