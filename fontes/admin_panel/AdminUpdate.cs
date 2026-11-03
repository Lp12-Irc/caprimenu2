using System;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Text.Json;
using System.Threading.Tasks;
using System.Windows.Forms;

public static class AdminUpdate {
    public const int Build=5;
    public const string Version="1.1.1";
    private static bool _checking;
    public static string? RemoteEndpoint(){
        string config=Path.Combine(AppContext.BaseDirectory,"admin-connection.json");if(!File.Exists(config))return null;
        using var data=JsonDocument.Parse(File.ReadAllText(config));return new RemoteAdminClient(data.RootElement.GetProperty("endpoint").GetString()!).Endpoint;
    }
    private static Uri? Origin(){
        string? remote=RemoteEndpoint();if(remote!=null)return new Uri(remote);
        string ini=Path.Combine(AppContext.BaseDirectory,"HillBlack.ini");if(!File.Exists(ini))return null;
        foreach(string line in File.ReadAllLines(ini))if(line.Trim().StartsWith("endpoint=",StringComparison.OrdinalIgnoreCase))return new Uri(new Uri(line.Trim().Substring(9)).GetLeftPart(UriPartial.Authority));
        return null;
    }
    public static async Task CheckAsync(Form owner,bool manual=false){
        if(_checking)return;_checking=true;
        try{
            var origin=Origin();if(origin==null){if(manual)MessageBox.Show(owner,"Configure o servidor antes de verificar atualizacoes.");return;}
            if(origin.Scheme!="https")throw new InvalidOperationException("A atualizacao exige HTTPS.");
            using var client=new HttpClient(new HttpClientHandler{AllowAutoRedirect=false}){Timeout=TimeSpan.FromSeconds(8)};
            using var response=await client.GetAsync(new Uri(origin,"/admin/release"),HttpCompletionOption.ResponseHeadersRead);
            response.EnsureSuccessStatusCode();await response.Content.LoadIntoBufferAsync(16384);
            using var data=JsonDocument.Parse(await response.Content.ReadAsStringAsync());var release=data.RootElement;
            int build=release.GetProperty("build").GetInt32();if(build<=Build){if(manual)MessageBox.Show(owner,"O ADM ja esta atualizado ("+Version+").","Atualizacao");return;}
            if(owner.OwnedForms.Length>0 || owner is MainForm main && !main.CanAutoUpdate){if(manual)MessageBox.Show(owner,"Conclua a operacao atual antes de atualizar o ADM.");return;}
            string hash=release.GetProperty("package_sha256").GetString()??"",url=release.GetProperty("url").GetString()??"";
            long size=release.GetProperty("package_size").GetInt64();
            if(hash.Length!=64 || !System.Text.RegularExpressions.Regex.IsMatch(hash,"^[a-f0-9]{64}$") || size<=0 || size>104857600 || !Uri.TryCreate(url,UriKind.Absolute,out var package) || package.Scheme!="https" || package.Authority!=origin.Authority || package.Query!="" || package.UserInfo!="")throw new InvalidOperationException("Metadados de atualizacao invalidos.");
            string script=Path.Combine(AppContext.BaseDirectory,"Atualizar-ADM.ps1");
            using(var resource=typeof(AdminUpdate).Assembly.GetManifestResourceStream("Capri.Atualizar-ADM.ps1")??throw new InvalidOperationException("Atualizador ausente."))
            using(var file=File.Create(script)){await resource.CopyToAsync(file);}
            string powershell=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.System),"WindowsPowerShell/v1.0/powershell.exe");
            var start=new ProcessStartInfo(powershell){UseShellExecute=false,CreateNoWindow=true,WorkingDirectory=AppContext.BaseDirectory};
            foreach(string arg in new[]{"-NoProfile","-ExecutionPolicy","Bypass","-File",script,"-DownloadUrl",url,"-ExpectedSha256",hash,"-ExpectedBuild",build.ToString(),"-PackageSize",size.ToString(),"-WaitProcessId",Environment.ProcessId.ToString()})start.ArgumentList.Add(arg);
            using var process=Process.Start(start)??throw new InvalidOperationException("Nao foi possivel iniciar o atualizador.");owner.Close();
        }catch(Exception){if(manual && !owner.IsDisposed)MessageBox.Show(owner,"Nao foi possivel verificar a atualizacao. Confira a conexao e o endereco do servidor.","Atualizacao");}
        finally{_checking=false;}
    }
}
