using System;
using System.Collections.Generic;
using System.Net.Http;
using System.Net.Http.Headers;
using System.Text;
using System.Text.Json;

public sealed class RemoteAdminClient {
    private readonly Uri _origin;private string _token="",_user="";
    private static readonly HttpClient Http=new(new HttpClientHandler{AllowAutoRedirect=false}){Timeout=TimeSpan.FromSeconds(6)};
    private static readonly JsonSerializerOptions Json=new(){PropertyNameCaseInsensitive=true};
    public string Endpoint=>_origin.AbsoluteUri;
    public RemoteAdminClient(string endpoint){
        var uri=new Uri(endpoint,UriKind.Absolute);
        if(uri.UserInfo!="" || uri.Query!="" || uri.Fragment!="" || uri.Scheme!="https" && !(uri.Scheme=="http" && uri.IsLoopback))throw new ArgumentException("Use o endereco HTTPS do servidor.");
        _origin=new Uri(uri.GetLeftPart(UriPartial.Authority)+"/");
    }
    public void Logout(){string token=_token;_token="";_user="";if(token=="")return;
        _=System.Threading.Tasks.Task.Run(async()=>{try{using var request=new HttpRequestMessage(HttpMethod.Post,new Uri(_origin,"admin/action")){Content=new StringContent("{\"action\":\"logout\"}",Encoding.UTF8,"application/json")};request.Headers.Authorization=new AuthenticationHeaderValue("Bearer",token);using var response=await Http.SendAsync(request);}catch{}});
    }
    private T Send<T>(string route,object payload,bool authenticated=true){
        try{return SendCore<T>(route,payload,authenticated);}
        catch(System.Threading.Tasks.TaskCanceledException){throw new InvalidOperationException("O servidor demorou a responder. Confira a conexao.");}
        catch(HttpRequestException){throw new InvalidOperationException("Nao foi possivel conectar ao servidor. Confira o endereco e se o PC principal esta ligado.");}
        catch(JsonException){throw new InvalidOperationException("O endereco nao respondeu como servidor Capri. Confira a configuracao recebida do dono.");}
    }
    private T SendCore<T>(string route,object payload,bool authenticated=true){
        using var request=new HttpRequestMessage(HttpMethod.Post,new Uri(_origin,route)){Content=new StringContent(JsonSerializer.Serialize(payload),Encoding.UTF8,"application/json")};
        if(authenticated){if(_token=="")throw new UnauthorizedAccessException("Entre novamente no painel.");request.Headers.Authorization=new AuthenticationHeaderValue("Bearer",_token);}
        using var response=Http.SendAsync(request,HttpCompletionOption.ResponseHeadersRead).GetAwaiter().GetResult();
        response.Content.LoadIntoBufferAsync(8*1024*1024).GetAwaiter().GetResult();
        using var data=JsonDocument.Parse(response.Content.ReadAsStringAsync().GetAwaiter().GetResult());
        if(!response.IsSuccessStatusCode){string error=data.RootElement.TryGetProperty("error",out var value)?value.GetString()??"Falha no servidor.":"Falha no servidor.";
            if(response.StatusCode==System.Net.HttpStatusCode.Unauthorized){Logout();throw new UnauthorizedAccessException(error);}throw new InvalidOperationException(error);}
        return data.RootElement.GetProperty("result").Deserialize<T>(Json)!;
    }
    public bool Login(string user,string password){
        Logout();try{var result=Send<JsonElement>("admin/login",new{username=user,password},false);_token=result.GetProperty("token").GetString()!;_user=user;return true;}
        catch(UnauthorizedAccessException){return false;}
    }
    public List<KeyRecord> Keys()=>Send<List<KeyRecord>>("admin/action",new{action="keys_list"});
    public string Create(DateTime? expires,string note)=>Send<string>("admin/action",new{action="keys_create",expires=expires?.ToUniversalTime().ToString("O"),note});
    public void Action(string action,string key,int? limit=null)=>Send<bool>("admin/action",new{action,key,limit});
    public List<KeyIp> Ips(string key)=>Send<List<KeyIp>>("admin/action",new{action="ips_list",key});
    public bool ChangePassword(string user,string current,string password){bool changed=Send<bool>("admin/action",new{action="password_change",current,password});return changed && Login(user,password);}
}
