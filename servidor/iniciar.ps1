param([int]$Port=18765)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
$sha=[Security.Cryptography.SHA256]::Create()
try{$instanceId=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($root.ToLowerInvariant())))).Replace('-','').Substring(0,20)}finally{$sha.Dispose()}
$startupMutex=New-Object Threading.Mutex($false,('Local\CapriServer-'+$instanceId));$startupOwned=$false
try{$startupOwned=$startupMutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$startupOwned=$true}
if(-not $startupOwned){$startupMutex.Dispose();Write-Host 'Outra inicializacao ja esta em andamento. Aguarde a primeira janela.';return}
try{
$stateFile=Join-Path $PSScriptRoot 'server-state.json'
$python=(Get-Command python -ErrorAction Stop).Source
$cloudflared=(Get-Command cloudflared -ErrorAction Stop).Source
$script=Join-Path $PSScriptRoot 'license_server.py'
$db=Join-Path $root 'hillblack.db'
$api=$null;$tunnel=$null;$reuseUrl=$null
$startedApi=$false;$startedTunnel=$false
function Is-OurApi($process){
 if(-not $process -or -not $process.CommandLine){return $false}
 $command=$process.CommandLine.Replace('/','\')
 return ($command.Contains($script.Replace('/','\')) -or $command.Contains('servidor\license_server.py'))
}
function Component-Pid($process){if($process.ProcessId){return [int]$process.ProcessId};return [int]$process.Id}
if(-not(Test-Path -LiteralPath $db)){throw 'Banco hillblack.db nao encontrado na pasta principal.'}
if(Test-Path $stateFile){
 $old=Get-Content $stateFile -Raw | ConvertFrom-Json
 $api=Get-CimInstance Win32_Process -Filter "ProcessId=$($old.api_pid)" -ErrorAction SilentlyContinue
 $tunnel=Get-CimInstance Win32_Process -Filter "ProcessId=$($old.tunnel_pid)" -ErrorAction SilentlyContinue
 $listener=Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
 $apiOwned=(Is-OurApi $api) -and $listener -and $listener.OwningProcess -contains $api.ProcessId
 $tunnelOwned=$tunnel -and $tunnel.ExecutablePath -eq $cloudflared -and $tunnel.CommandLine.Contains("127.0.0.1:$Port")
 if($apiOwned -and $tunnelOwned){
  $health=$null;try{$health=Invoke-RestMethod -Uri ($old.url+'/health') -TimeoutSec 10}catch{}
  if($health -and $health.service -eq 'capri-license-api'){
   if(-not(Test-Path (Join-Path $PSScriptRoot 'cloudflare-gateway.dpapi'))){"[auth]`r`nmode=online`r`nendpoint=$($old.url)/validate`r`n" | Set-Content (Join-Path $root 'HillBlack.ini')}
   Write-Host 'Servidor ja ativo. Conferindo o endereco publico dos clientes...'
   & $python (Join-Path $root 'ferramentas/sincronizar_endereco.py')
   if($LASTEXITCODE -ne 0){throw 'Nao foi possivel atualizar a conexao publica. Confira a internet e tente iniciar novamente.'}
   return
  }
  Write-Host 'Servico e tunel ja ativos; endereco publico temporariamente indisponivel. Nenhuma instancia duplicada foi criada.'
  return
 }
 if(-not $apiOwned){$api=$null}
 if($tunnelOwned){$reuseUrl=$old.url}else{$tunnel=$null}
 $history=Join-Path $PSScriptRoot 'state-history';[IO.Directory]::CreateDirectory($history)|Out-Null
 Copy-Item -LiteralPath $stateFile -Destination (Join-Path $history ('antes-recuperacao-'+[guid]::NewGuid().ToString('N')+'.json'))
 Write-Host 'Recuperando os componentes ausentes do servidor...'
}
if(-not $api -and (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue)){throw 'Porta ocupada por outro processo. Nenhum processo foi encerrado.'}
if(-not $api){
$api=Start-Process -FilePath $python -ArgumentList @('-u',('"'+$script+'"'),'--db',('"'+$db+'"'),'--port',$Port) -WorkingDirectory $root -WindowStyle Hidden -RedirectStandardOutput (Join-Path $PSScriptRoot 'api.out.log') -RedirectStandardError (Join-Path $PSScriptRoot 'api.err.log') -PassThru
$startedApi=$true
}
try{
 $ready=$false
 for($i=0;$i -lt 20;$i++){
  try{$health=Invoke-RestMethod -Uri "http://127.0.0.1:$Port/health" -TimeoutSec 1;if($health.service -eq 'capri-license-api'){$ready=$true;break}}catch{}
  Start-Sleep -Milliseconds 250
 }
 if(-not $ready){throw 'Servico local nao iniciou. Consulte api.err.log.'}
 Write-Host 'Servico local pronto. Criando conexao HTTPS...'
 if(-not $tunnel){
 $log=Join-Path $PSScriptRoot ('tunnel-'+[guid]::NewGuid().ToString('N')+'.log')
 $tunnel=Start-Process -FilePath $cloudflared -ArgumentList @('tunnel','--no-autoupdate','--protocol','http2','--url',"http://127.0.0.1:$Port",'--logfile',('"'+$log+'"')) -WindowStyle Hidden -RedirectStandardOutput (Join-Path $PSScriptRoot 'tunnel.out.log') -RedirectStandardError (Join-Path $PSScriptRoot 'tunnel.err.log') -PassThru
$startedTunnel=$true
 $url=$null
 for($i=0;$i -lt 120;$i++){
  if(Test-Path $log){$match=[regex]::Match((Get-Content $log -Raw),'https://[a-z0-9-]+\.trycloudflare\.com');if($match.Success){$url=$match.Value;break}}
  $tunnel.Refresh();if($tunnel.HasExited){throw 'Conexao HTTPS encerrou. Consulte tunnel.err.log.'}
  Start-Sleep -Milliseconds 500
 }
 if(-not $url){throw 'Nao foi possivel obter o endereco HTTPS.'}
 }else{$url=$reuseUrl}
 $publicReady=$false
 for($i=0;$i -lt 30;$i++){
  try{$health=Invoke-RestMethod -Uri ($url+'/health') -TimeoutSec 5;if($health.service -eq 'capri-license-api'){$publicReady=$true;break}}catch{}
  if(($i % 5) -eq 0){Write-Host 'Aguardando a publicacao do endereco HTTPS temporario...'}
  Start-Sleep -Seconds 2
 }
 if(-not $publicReady){throw 'Endereco publico nao respondeu. Confira a conexao de internet.'}
 @{url=$url;api_pid=(Component-Pid $api);tunnel_pid=(Component-Pid $tunnel);port=$Port;database=$db} | ConvertTo-Json | Set-Content $stateFile
 if(-not(Test-Path (Join-Path $PSScriptRoot 'cloudflare-gateway.dpapi'))){"[auth]`r`nmode=online`r`nendpoint=$url/validate`r`n" | Set-Content (Join-Path $root 'HillBlack.ini')}
 & $python (Join-Path $root 'ferramentas/sincronizar_endereco.py')
 if($LASTEXITCODE -ne 0){throw 'Nao foi possivel atualizar a conexao publica. Confira a internet e tente iniciar novamente.'}
 $clientEndpoint=(Get-Content (Join-Path $root 'HillBlack.ini') | Where-Object {$_ -like 'endpoint=*'} | Select-Object -First 1)
 Write-Host ('Servidor de chaves ativo: '+$clientEndpoint.Substring(9))
 Write-Host 'Mantenha o PC ligado. O endereco fixo dos clientes foi sincronizado automaticamente.'
}catch{
 if($startedTunnel -and $tunnel -and -not $tunnel.HasExited){Stop-Process -Id $tunnel.Id}
 if($startedApi -and $api -and -not $api.HasExited){Stop-Process -Id $api.Id}
 throw
}
}finally{if($startupOwned){$startupMutex.ReleaseMutex()};$startupMutex.Dispose()}
