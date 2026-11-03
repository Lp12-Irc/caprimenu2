using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Threading.Tasks;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Web.WebView2.Core;
using Microsoft.Web.WebView2.WinForms;

// Test-only executable. Every browser request is answered from this in-memory
// fixture. This executable and its empty browser profile are never packaged.
internal static class SteamNameTests {
    private const string Id="76561197960265729";
    private static string document="";
    private static readonly JavaScriptSerializer Json=new JavaScriptSerializer();
    private static void Check(bool condition,string message){if(!condition)throw new Exception(message);Console.WriteLine("PASS "+message);}
    private static async Task Navigate(WebView2 web,string address,string html){
        document=html;var ready=new TaskCompletionSource<bool>();
        EventHandler<CoreWebView2NavigationCompletedEventArgs> handler=null;
        handler=delegate(object sender,CoreWebView2NavigationCompletedEventArgs e){web.CoreWebView2.NavigationCompleted-=handler;ready.TrySetResult(e.IsSuccess);};
        web.CoreWebView2.NavigationCompleted+=handler;web.CoreWebView2.Navigate(address);
        Check(await ready.Task,"fixture document loaded");
    }
    private static async Task<string> State(WebView2 web,string mode,string id=Id){
        string raw=await web.CoreWebView2.ExecuteScriptAsync(SteamNameForm.ProfileScript(mode,id));
        return Convert.ToString(Json.Deserialize<Dictionary<string,object>>(raw)["state"]);
    }
    private static string Html(bool persist,bool named=true,bool duplicate=false){
        return "<!doctype html><label class='DialogLabel'>Profile Name</label><div><input type='text' "+(named?"id='personaName' name='personaName'":"")+" value='Before'></div><button>Save</button>"+(duplicate?"<button>Save</button>":"")+"<script>const f=document.querySelector('input');f.value=localStorage.getItem('fixture-name')||'Before';document.querySelector('button').onclick=()=>{"+(persist?"localStorage.setItem('fixture-name',f.value);":"")+"window.saved=true};</script>";
    }
    private static async Task Run(WebView2 web,string folder){
        var env=await CoreWebView2Environment.CreateAsync(null,folder);await web.EnsureCoreWebView2Async(env);
        web.CoreWebView2.AddWebResourceRequestedFilter("*",CoreWebView2WebResourceContext.All);
        web.CoreWebView2.WebResourceRequested+=delegate(object sender,CoreWebView2WebResourceRequestedEventArgs e){e.Response=env.CreateWebResourceResponse(new MemoryStream(Encoding.UTF8.GetBytes(document)),200,"OK","Content-Type: text/html; charset=utf-8");};
        string url="https://steamcommunity.com/profiles/"+Id+"/edit/info";
        await Navigate(web,"https://capri.test/profiles/"+Id+"/edit/info",Html(true));
        Check(await State(web,"prepare")=="wrong_page","untrusted host cannot change a field");
        await Navigate(web,url,"<script>var g_steamID=false;</script>"+Html(true));
        Check(await State(web,"prepare")=="login_required","anonymous profile requires Steam sign-in before any field edit");
        await Navigate(web,url,Html(true));await web.ExecuteScriptAsync("localStorage.clear()");
        Check(await State(web,"prepare","76561197960265730")=="wrong_account","another account cannot be renamed");
        Check(await State(web,"prepare")=="prepared","profile name field prepared");
        Check(await State(web,"submit")=="submitted","single save action submitted");
        await Navigate(web,url,Html(true));Check(await State(web,"verify")=="done","fresh profile verifies persisted nickname");
        Check(await State(web,"prepare")=="done","already correct name needs no extra submission");
        await web.ExecuteScriptAsync("localStorage.clear()");
        await Navigate(web,url,Html(false));
        Check(await State(web,"prepare")=="prepared" && await State(web,"submit")=="submitted","failed-save fixture attempted");
        await Navigate(web,url,Html(false));Check(await State(web,"verify")=="not_saved","failed save cannot report success");
        await Navigate(web,url,Html(true,true,true));Check(await State(web,"prepare")=="waiting","ambiguous save actions are not clicked");
        await Navigate(web,url,"<input type='text' value='Real name'><button>Save</button>");
        Check(await State(web,"prepare")=="waiting","unrecognized profile form is not modified");
        await Navigate(web,url,"<div><label class='DialogLabel'>Profile Name</label><input type='text' value='Before'></div><button>Save</button>");
        Check(await State(web,"prepare")=="prepared","semantic label supports unnamed profile input");
    }
    [STAThread] private static int Main(){
        int code=0;Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
        using(var form=new Form{Opacity=0,ShowInTaskbar=false,Width=700,Height=600})
        using(var web=new WebView2{Dock=DockStyle.Fill}){
            form.Controls.Add(web);form.Shown+=async delegate{try{string folder=Path.Combine(Environment.CurrentDirectory,"verificacao","steam-name-fixture",Guid.NewGuid().ToString("N"));Directory.CreateDirectory(folder);await Run(web,folder);Console.WriteLine("Steam name browser tests passed; no Steam network requests or real account changes.");}catch(Exception e){Console.WriteLine(e);code=1;}finally{form.Close();}};Application.Run(form);
        }
        return code;
    }
}
