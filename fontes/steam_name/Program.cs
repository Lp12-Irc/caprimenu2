using System;
using System.Drawing;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;

// Uses the authenticated Steam website. No password, cookie or session token is
// read by Capri, and the browser profile stays in this Windows user's AppData.
internal sealed class SteamNameForm : Form {
    private string expectedId="";
    private readonly WebView2 browser=new WebView2 {DefaultBackgroundColor=Color.FromArgb(18,21,27)};
    private readonly Label banner=new Label();
    private readonly System.Windows.Forms.Timer timer=new System.Windows.Forms.Timer {Interval=700};
    private readonly JavaScriptSerializer json=new JavaScriptSerializer();
    private readonly string folder=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Capri","SteamConnect");
    private bool polling,prepared,submitted,verifying,finished;
    private DateTime submittedAt,documentAt;
    private string lastState="";
    private int savedResponses;

    public SteamNameForm() {
        Text="Capri - Conectar Steam";ClientSize=new Size(920,700);MinimumSize=new Size(640,500);
        StartPosition=FormStartPosition.CenterScreen;BackColor=Color.FromArgb(18,21,27);ForeColor=Color.White;
        Font=new Font("Segoe UI",10);Opacity=0;ShowInTaskbar=false;
        var top=new Panel{Dock=DockStyle.Top,Height=88,Padding=new Padding(18,12,18,12)};
        banner.Dock=DockStyle.Fill;banner.Text="Conectando ao perfil Steam...";
        var cancel=new Button{Text="Cancelar",Dock=DockStyle.Right,Width=104,FlatStyle=FlatStyle.Flat,BackColor=Color.FromArgb(48,34,43)};
        cancel.Click+=delegate{Close();};top.Controls.Add(banner);top.Controls.Add(cancel);
        browser.Dock=DockStyle.Fill;Controls.Add(browser);Controls.Add(top);
        timer.Tick+=async delegate{await Poll();};
        Shown+=async delegate{await Initialize();};
        FormClosing+=delegate{timer.Stop();if(!finished)WriteStatus("cancelled");};
    }
    protected override bool ShowWithoutActivation {get{return true;}}
    private void WriteStatus(string state) {
        if(lastState==state)return;lastState=state;
        try{Directory.CreateDirectory(folder);File.WriteAllText(Path.Combine(folder,"nickname-status.json"),json.Serialize(new{state=state,updated=DateTime.UtcNow.ToString("O")}));}catch(IOException){}catch(UnauthorizedAccessException){}
    }
    private void Reveal(string message) {
        if(IsDisposed)return;banner.Text=message;Opacity=1;ShowInTaskbar=true;Show();
    }
    private static bool Trusted(Uri uri) {
        return uri.Scheme=="https" && (uri.Host=="steamcommunity.com" || uri.Host=="steampowered.com" || uri.Host.EndsWith(".steampowered.com",StringComparison.OrdinalIgnoreCase));
    }
    private static bool IsProfile(Uri uri) {
        return uri.Scheme=="https" && uri.Host=="steamcommunity.com" &&
            System.Text.RegularExpressions.Regex.IsMatch(uri.AbsolutePath,@"^/(my|profiles/\d+|id/[^/]+)/edit(/info)?/?$");
    }
    private async Task Initialize() {
        try {
            WriteStatus("connecting");Directory.CreateDirectory(folder);
            uint account=0;using(var key=Microsoft.Win32.Registry.CurrentUser.OpenSubKey(@"Software\Valve\Steam\ActiveProcess")){
                object value=key==null?null:key.GetValue("ActiveUser");if(value is int)account=unchecked((uint)(int)value);
            }
            if(account==0){Fail("steam_not_running","Entre na Steam antes de conectar seu perfil no Capri.");return;}
            expectedId=(((1UL<<56)|(1UL<<52)|(1UL<<32))|account).ToString(System.Globalization.CultureInfo.InvariantCulture);
            var environment=await CoreWebView2Environment.CreateAsync(null,Path.Combine(folder,"Browser-"+expectedId));
            if(IsDisposed)return;await browser.EnsureCoreWebView2Async(environment);
            browser.CoreWebView2.Settings.AreDevToolsEnabled=false;
            browser.CoreWebView2.PermissionRequested+=delegate(object sender,CoreWebView2PermissionRequestedEventArgs e){e.State=CoreWebView2PermissionState.Deny;};
            browser.CoreWebView2.DownloadStarting+=delegate(object sender,CoreWebView2DownloadStartingEventArgs e){e.Cancel=true;};
            browser.CoreWebView2.NavigationStarting+=delegate(object sender,CoreWebView2NavigationStartingEventArgs e){Uri target;if(!Uri.TryCreate(e.Uri,UriKind.Absolute,out target)||!Trusted(target))e.Cancel=true;};
            browser.CoreWebView2.NewWindowRequested+=delegate(object sender,CoreWebView2NewWindowRequestedEventArgs e){e.Handled=true;Uri target;if(Uri.TryCreate(e.Uri,UriKind.Absolute,out target)&&Trusted(target))browser.CoreWebView2.Navigate(e.Uri);};
            browser.CoreWebView2.NavigationCompleted+=async delegate(object sender,CoreWebView2NavigationCompletedEventArgs e){
                if(!e.IsSuccess){Fail("network_error","Não foi possível abrir a Steam. Confira a conexão e tente novamente pelo menu Conta.");return;}
                Uri uri;if(!Uri.TryCreate(browser.Source.AbsoluteUri,UriKind.Absolute,out uri))return;
                documentAt=DateTime.UtcNow;
                if(IsProfile(uri)){WriteStatus(verifying?"verifying":"connected");timer.Start();}
                else{
                    if(uri.Host=="steamcommunity.com" && !uri.AbsolutePath.Contains("/login")){
                        try{string same=await browser.CoreWebView2.ExecuteScriptAsync("typeof g_steamID!=='undefined' && String(g_steamID)==='"+expectedId+"'");if(!IsDisposed && same=="true"){browser.CoreWebView2.Navigate("https://steamcommunity.com/profiles/"+expectedId+"/edit/info?l=english");return;}}catch{}
                    }
                    timer.Stop();WriteStatus("login_required");Reveal("STEAM · Entre na sua conta para aplicar __proton__ ao nome público. A mudança também aparece fora do CS2.");}
            };
            browser.CoreWebView2.WebResourceResponseReceived+=delegate(object sender,CoreWebView2WebResourceResponseReceivedEventArgs e){
                Uri uri;if(submitted && e.Request.Method=="POST" && Uri.TryCreate(e.Request.Uri,UriKind.Absolute,out uri) && uri.Host=="steamcommunity.com" && e.Response.StatusCode>=200 && e.Response.StatusCode<300)savedResponses++;
            };
            browser.CoreWebView2.Navigate("https://steamcommunity.com/profiles/"+expectedId+"/edit/info?l=english");
        } catch(WebView2RuntimeNotFoundException) {Fail("runtime_missing","Microsoft Edge WebView2 não está instalado. Instale o runtime oficial da Microsoft e tente novamente.");}
        catch(Exception){Fail("error","Não foi possível iniciar a conexão Steam. Feche esta janela e tente novamente pelo menu Conta.");}
    }
    private void Fail(string state,string message) {if(IsDisposed)return;timer.Stop();finished=true;WriteStatus(state);Reveal(message+" Feche esta janela antes de tentar novamente.");}
    private async Task Poll() {
        if(polling || IsDisposed || finished)return;polling=true;
        try {
            if(submitted && !verifying){
                // Reload the editor only after the save request has had time to finish.
                // Success is reported only when a fresh editor returns the target name.
                if((DateTime.UtcNow-submittedAt).TotalSeconds<(savedResponses>0?2:10))return;
                verifying=true;timer.Stop();browser.CoreWebView2.Reload();return;
            }
            string raw=await browser.CoreWebView2.ExecuteScriptAsync(ProfileScript(verifying?"verify":prepared?"submit":"prepare",expectedId));
            if(IsDisposed)return;
            var result=json.Deserialize<System.Collections.Generic.Dictionary<string,object>>(raw);
            string state=result!=null && result.ContainsKey("state")?Convert.ToString(result["state"]):"waiting";
            if(state=="done") {finished=true;WriteStatus("done");timer.Stop();Close();return;}
            if(state=="prepared"){prepared=true;WriteStatus("renaming");return;}
            if(state=="submitted"){submitted=true;submittedAt=DateTime.UtcNow;WriteStatus("renaming");return;}
            if(state=="login_required"){WriteStatus("login_required");documentAt=DateTime.UtcNow;Reveal("STEAM � Entre na mesma conta ativa no aplicativo Steam para aplicar __proton__ ao nome p�blico.");return;}
            if(state=="wrong_account"){Fail("wrong_account","Entre na mesma conta que est� ativa no aplicativo Steam. Nenhum nome foi alterado.");return;}
            if(state=="not_saved"){Fail("not_saved","A Steam não confirmou a mudança. Confira o perfil nesta janela ou tente novamente pelo menu Conta.");return;}
            if((DateTime.UtcNow-documentAt).TotalSeconds>35)Fail("form_unavailable","Não foi possível reconhecer o editor de perfil da Steam. Nenhuma alteração foi confirmada. Tente novamente pelo menu Conta.");
        }catch(Exception){if(!IsDisposed)Fail("error","Não foi possível confirmar o nome na Steam. Tente novamente pelo menu Conta.");}
        finally{polling=false;}
    }
    internal static string ProfileScript(string mode,string expectedId) {
        return @"(() => {
          if(location.protocol!=='https:'||location.hostname!=='steamcommunity.com'||!/^\/(my|profiles\/\d+|id\/[^/]+)\/edit(\/info)?\/?$/.test(location.pathname))return {state:'wrong_page'};
          const expected='"+expectedId+@"';
          if(typeof g_steamID!=='undefined'){if(!g_steamID)return {state:'login_required'};if(String(g_steamID)!==expected)return {state:'wrong_account'};}
          const numeric=/^\/profiles\/(\d+)\//.exec(location.pathname);
          if(numeric?numeric[1]!==expected:(typeof g_steamID==='undefined'||String(g_steamID)!==expected))return {state:'wrong_account'};
          let field=document.querySelector('input[name=personaName],input#personaName');
          if(!field){
            const label=Array.from(document.querySelectorAll('label,.DialogLabel')).find(e=>/^(profile name|nome do perfil|nome de perfil)$/i.test(e.textContent.trim()));
            if(label){let box=label.parentElement;for(let i=0;box&&i<4;i++,box=box.parentElement){const inputs=box.querySelectorAll('input:not([type=hidden])');if(inputs.length===1){field=inputs[0];break;}}}
          }
          if(!field||!['text',''].includes(field.type)||field.disabled||field.readOnly)return {state:'waiting'};
          const mode='"+mode+@"';
          if(mode==='verify')return {state:field.value==='__proton__'?'done':'not_saved'};
          if(mode==='prepare'&&field.value==='__proton__')return {state:'done'};
          const buttons=Array.from(document.querySelectorAll('button,input[type=submit]')).filter(e=>/^(save|save changes|salvar|salvar alterações)$/i.test((e.innerText||e.value||'').trim())&&e.getClientRects().length);
          if(buttons.length!==1)return {state:'waiting'};
          if(mode==='prepare'){
            Object.getOwnPropertyDescriptor(HTMLInputElement.prototype,'value').set.call(field,'__proton__');
            field.dispatchEvent(new Event('input',{bubbles:true}));field.dispatchEvent(new Event('change',{bubbles:true}));return {state:'prepared'};
          }
          if(field.value!=='__proton__'||buttons[0].disabled)return {state:'waiting'};
          buttons[0].click();return {state:'submitted'};
        })()";
    }
    protected override void Dispose(bool disposing){if(disposing){timer.Dispose();browser.Dispose();}base.Dispose(disposing);}
    [STAThread] private static int Main(string[] args) {
        if(args.Length!=1||args[0]!="--activate")return 2;
        using(var mutex=new Mutex(false,"Local\\CapriSteamConnect-"+System.Security.Principal.WindowsIdentity.GetCurrent().User.Value)){
            if(!mutex.WaitOne(0))return 0;
            try{Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);using(var form=new SteamNameForm())Application.Run(form);return 0;}
            finally{mutex.ReleaseMutex();}
        }
    }
}
