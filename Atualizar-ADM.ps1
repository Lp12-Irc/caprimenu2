param(
 [string]$DownloadUrl='',
 [string]$ArchivePath='',
 [Parameter(Mandatory=$true)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedSha256,
 [Parameter(Mandatory=$true)][ValidateRange(1,2147483647)][int]$ExpectedBuild,
 [ValidateRange(0,104857600)][long]$PackageSize=0,
 [int]$WaitProcessId=0,
 [switch]$NoRestart,
 [switch]$NoUi
)
$ErrorActionPreference='Stop'
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Utility/Microsoft.PowerShell.Utility.psd1') -Force
Import-Module (Join-Path $PSHOME 'Modules/Microsoft.PowerShell.Management/Microsoft.PowerShell.Management.psd1') -Force
Add-Type -AssemblyName System.IO.Compression.FileSystem
$root=[IO.Path]::GetFullPath($PSScriptRoot).TrimEnd('\')
if($root -eq [IO.Path]::GetPathRoot($root).TrimEnd('\')){throw 'Nao instale o Capri na raiz do disco.'}
$work=Join-Path $root 'atualizacao-adm'
[IO.Directory]::CreateDirectory($work) | Out-Null
$log=Join-Path $work 'atualizacao.log'
$stage=Join-Path $work ('stage-'+[guid]::NewGuid().ToString('N'))
$payload=Join-Path $stage 'payload'
$backup=Join-Path $stage 'backup'
$form=$null;$bar=$null;$label=$null;$mutex=$null;$acquired=$false;$applied=New-Object System.Collections.ArrayList
function Report([string]$text,[int]$percent){
 ('{0:u} {1}' -f [DateTime]::UtcNow,$text) | Add-Content -LiteralPath $log
 if($form){$label.Text=$text;$bar.Value=[Math]::Max(0,[Math]::Min(100,$percent));$form.Refresh();[Windows.Forms.Application]::DoEvents()}
}
function Wait-Task($task){
 $deadline=[DateTime]::UtcNow.AddSeconds(30)
 while(-not $task.IsCompleted){if([DateTime]::UtcNow -gt $deadline){throw 'Tempo limite ao receber o pacote.'};if($form){[Windows.Forms.Application]::DoEvents()};Start-Sleep -Milliseconds 30}
 return $task.GetAwaiter().GetResult()
}
function Inside([string]$parent,[string]$relative){
 $value=[IO.Path]::GetFullPath((Join-Path $parent $relative))
 if(-not $value.StartsWith($parent.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Caminho fora da pasta de atualizacao.'}
 return $value
}
function Safe-Entry([string]$name){
 return @('Capri.Admin.exe','admin-version.json','Atualizar-ADM.ps1') -ccontains $name
}
function Check-LocalPath([string]$path){
 $current=[IO.Path]::GetFullPath($path)
 while($current -eq $root -or $current.StartsWith($root+'\',[StringComparison]::OrdinalIgnoreCase)){
  if((Test-Path -LiteralPath $current) -and (([IO.File]::GetAttributes($current) -band [IO.FileAttributes]::ReparsePoint) -ne 0)){throw 'A pasta contem um link ou redirecionamento. Use uma pasta local do Capri.'}
  if($current -eq $root){break};$current=[IO.Path]::GetDirectoryName($current)
 }
}
function Clean-Success {
 try{
  if([IO.Directory]::Exists($payload)){[IO.Directory]::Delete($payload,$true)}
  $download=Join-Path $stage 'pacote.zip';if([IO.File]::Exists($download)){[IO.File]::Delete($download)}
  @{owner='capri-admin-updater-v1';root=$root;build=$ExpectedBuild;completed=$true} | ConvertTo-Json | Set-Content (Join-Path $stage '.capri-update.json')
  $completed=@(Get-ChildItem -LiteralPath $work -Directory -Filter 'stage-*' | Where-Object {Test-Path -LiteralPath (Join-Path $_.FullName '.capri-update.json')} | Sort-Object LastWriteTime -Descending)
  foreach($old in @($completed | Select-Object -Skip 2)){
   if($old.Name -notmatch '^stage-[0-9a-f]{32}$'){continue}
   $marker=Get-Content -LiteralPath (Join-Path $old.FullName '.capri-update.json') -Raw | ConvertFrom-Json
   if($marker.owner -ne 'capri-admin-updater-v1' -or $marker.root -ne $root -or -not $marker.completed){continue}
   $resolved=[IO.Path]::GetFullPath($old.FullName)
   if(-not $resolved.StartsWith($work+'\',[StringComparison]::OrdinalIgnoreCase)){continue}
   Check-LocalPath $resolved
   [IO.Directory]::Delete($resolved,$true)
  }
 }catch{('Limpeza pendente: '+$_.Exception.Message) | Add-Content -LiteralPath $log}
}
$exitCode=0
try {
 $sha=[Security.Cryptography.SHA256]::Create()
 try{$id=([BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($root.ToUpperInvariant())))).Replace('-','').Substring(0,24)}finally{$sha.Dispose()}
 $mutex=New-Object Threading.Mutex($false,('Local\CapriAdminUpdate-'+$id))
 try{$acquired=$mutex.WaitOne(0)}catch [Threading.AbandonedMutexException]{$acquired=$true}
 if(-not $acquired){throw 'Ja existe uma atualizacao em andamento.'}
 if(-not $NoUi){
  Add-Type -AssemblyName System.Windows.Forms
  Add-Type -AssemblyName System.Drawing
  [Windows.Forms.Application]::EnableVisualStyles()
  $form=New-Object Windows.Forms.Form
  $form.Text='Capri ADM / Atualizacao';$form.ClientSize=New-Object Drawing.Size(500,150);$form.StartPosition='CenterScreen';$form.FormBorderStyle='FixedDialog';$form.ControlBox=$false
  $form.BackColor=[Drawing.Color]::FromArgb(12,15,20);$form.ForeColor=[Drawing.Color]::White
  $label=New-Object Windows.Forms.Label;$label.SetBounds(24,25,450,55);$label.Font=New-Object Drawing.Font('Segoe UI',10);$form.Controls.Add($label)
  $bar=New-Object Windows.Forms.ProgressBar;$bar.SetBounds(24,100,450,22);$form.Controls.Add($bar);$form.Show();$form.Refresh()
 }
 [IO.Directory]::CreateDirectory($payload) | Out-Null
 [IO.Directory]::CreateDirectory($backup) | Out-Null
 if($DownloadUrl){
  $uri=New-Object Uri($DownloadUrl)
  if($uri.UserInfo -or ($uri.Scheme -ne 'https' -and -not ($NoUi -and $NoRestart -and $uri.Scheme -eq 'http' -and $uri.IsLoopback))){throw 'O pacote precisa de um endereco HTTPS valido.'}
  Add-Type -AssemblyName System.Net.Http
  [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
  $handler=New-Object Net.Http.HttpClientHandler;$handler.AllowAutoRedirect=$false
  $client=New-Object Net.Http.HttpClient($handler);$client.Timeout=[TimeSpan]::FromSeconds(120)
  $response=$null;$downloadStream=$null;$output=$null
  try{
   Report 'Baixando o pacote de atualizacao...' 5
   $response=Wait-Task ($client.GetAsync($uri,[Net.Http.HttpCompletionOption]::ResponseHeadersRead))
   $response.EnsureSuccessStatusCode() | Out-Null
   $length=$response.Content.Headers.ContentLength
   if($length -and ($length -gt 104857600 -or ($PackageSize -gt 0 -and $length -ne $PackageSize))){throw 'Tamanho do pacote diferente do publicado.'}
   $downloadStream=Wait-Task ($response.Content.ReadAsStreamAsync())
   $ArchivePath=Join-Path $stage 'pacote.zip';$output=[IO.File]::Create($ArchivePath)
   $buffer=New-Object byte[] 65536;$total=0L;$clock=[Diagnostics.Stopwatch]::StartNew()
   while($true){
    if($clock.Elapsed.TotalSeconds -gt 120){throw 'Tempo limite ao baixar o pacote.'}
    $count=Wait-Task ($downloadStream.ReadAsync($buffer,0,$buffer.Length))
    if($count -eq 0){break}
    $total+=$count;if($total -gt 104857600 -or ($PackageSize -gt 0 -and $total -gt $PackageSize)){throw 'Pacote maior que o esperado.'}
    $output.Write($buffer,0,$count)
    if($form){$bar.Value=5+[int](60*[Math]::Min(1,$total/[double][Math]::Max(1,$PackageSize)));[Windows.Forms.Application]::DoEvents()}
   }
  }finally{if($output){$output.Dispose()};if($downloadStream){$downloadStream.Dispose()};if($response){$response.Dispose()};$client.Dispose();$handler.Dispose()}
 }
 if(-not $ArchivePath -or -not [IO.File]::Exists($ArchivePath)){throw 'Pacote nao encontrado.'}
 if($PackageSize -gt 0 -and (Get-Item -LiteralPath $ArchivePath).Length -ne $PackageSize){throw 'Download incompleto.'}
 Report 'Conferindo a integridade do pacote...' 68
 if((Get-FileHash -LiteralPath $ArchivePath -Algorithm SHA256).Hash -ne $ExpectedSha256){throw 'A integridade do pacote nao confere. Nenhum arquivo foi aplicado.'}
 $zip=[IO.Compression.ZipFile]::OpenRead($ArchivePath)
 try{
  if($zip.Entries.Count -gt 1024){throw 'Pacote com arquivos demais.'}
  $names=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
  $expanded=0L
  foreach($entry in $zip.Entries){
   $name=$entry.FullName
   if(-not (Safe-Entry $name) -or -not $names.Add($name.TrimEnd('/')) -or (($entry.ExternalAttributes -shr 16) -band 0xF000) -eq 0xA000){throw 'O ZIP contem um caminho ou arquivo nao permitido.'}
   $expanded+=$entry.Length;if($expanded -gt 157286400){throw 'Pacote expandido muito grande.'}
   if($name.EndsWith('/')){continue}
   $target=Inside $payload $name
   [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
   $source=$entry.Open();$dest=[IO.File]::Create($target)
   try{
    $written=0L;$copyBuffer=New-Object byte[] 65536
    while(($read=$source.Read($copyBuffer,0,$copyBuffer.Length)) -gt 0){$written+=$read;if($written -gt $entry.Length){throw 'Conteudo expandido diferente do ZIP.'};$dest.Write($copyBuffer,0,$read)}
    if($written -ne $entry.Length){throw 'Arquivo incompleto no ZIP.'}
   }finally{$dest.Dispose();$source.Dispose()}
  }
 }finally{$zip.Dispose()}
 $manifestPath=Join-Path $payload 'admin-version.json'
 if(-not (Test-Path -LiteralPath $manifestPath)){throw 'Manifesto de versao ausente.'}
 $manifest=Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
 if($manifest.build -ne $ExpectedBuild -or $manifest.sha256 -notmatch '^[a-fA-F0-9]{64}$'){throw 'O pacote nao corresponde a versao exigida.'}
 $exe=Join-Path $payload 'Capri.Admin.exe'
 if(-not(Test-Path -LiteralPath $exe) -or (Get-FileHash -LiteralPath $exe).Hash -ne $manifest.sha256){throw 'O executavel nao corresponde ao manifesto.'}
 $installed=Join-Path $root 'admin-version.json'
 if(Test-Path -LiteralPath $installed){$old=Get-Content -LiteralPath $installed -Raw | ConvertFrom-Json;if($old.build -gt $ExpectedBuild){throw 'Uma versao mais nova ja esta instalada.'}}
 Report 'Preparando os arquivos. Seu banco, sua conexao e seu login salvo serao mantidos.' 78
 $plan=New-Object System.Collections.ArrayList
 foreach($file in Get-ChildItem -LiteralPath $payload -File -Recurse){
  $relative=$file.FullName.Substring($payload.Length+1)
  if(-not (Safe-Entry ($relative.Replace('\','/')))){throw 'Arquivo fora da lista permitida.'}
  $destination=Inside $root $relative
  Check-LocalPath $destination
  if(($relative -eq 'configs\default.hbc' -or $relative -eq 'HillBlack.ini') -and (Test-Path -LiteralPath $destination)){continue}
  $exists=Test-Path -LiteralPath $destination
  if($exists -and (Get-FileHash -LiteralPath $destination).Hash -eq (Get-FileHash -LiteralPath $file.FullName).Hash){continue}
  $null=$plan.Add([pscustomobject]@{Source=$file.FullName;Destination=$destination;Relative=$relative;Existed=$exists;Backup=(Inside $backup $relative)})
 }
 Report 'Fechando a versao anterior...' 82
 if($WaitProcessId -gt 0){
  $deadline=[DateTime]::UtcNow.AddSeconds(60)
  while(Get-Process -Id $WaitProcessId -ErrorAction SilentlyContinue){if([DateTime]::UtcNow -gt $deadline){throw 'Feche o ADM e tente novamente.'};if($form){[Windows.Forms.Application]::DoEvents()};Start-Sleep -Milliseconds 200}
 }
 foreach($item in $plan){if($item.Existed){[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($item.Backup)) | Out-Null;[IO.File]::Copy($item.Destination,$item.Backup,$false)}}
 Report 'Aplicando as modificacoes...' 88
 $ordered=@($plan | Sort-Object @{Expression={if($_.Relative -eq 'Capri.Admin.exe'){2}elseif($_.Relative -eq 'admin-version.json'){3}else{1}}})
 $done=0
 foreach($item in $ordered){
  [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($item.Destination)) | Out-Null
  $null=$applied.Add($item)
  [IO.File]::Copy($item.Source,$item.Destination,$true)
  $done++;if($form){$bar.Value=88+[int](10*$done/[Math]::Max(1,$ordered.Count));[Windows.Forms.Application]::DoEvents()}
 }
 if((Get-FileHash -LiteralPath (Join-Path $root 'Capri.Admin.exe')).Hash -ne $manifest.sha256){throw 'Falha ao conferir o executavel instalado.'}
 @{build=$ExpectedBuild;version=$manifest.version;backup=$backup;files=$ordered.Relative;completed=[DateTime]::UtcNow.ToString('o')} | ConvertTo-Json -Depth 4 | Set-Content (Join-Path $work 'ultima-atualizacao.json')
 Report 'Reabrindo o ADM atualizado...' 99
 if(-not $NoRestart){Start-Process -FilePath (Join-Path $root 'Capri.Admin.exe') -WorkingDirectory $root | Out-Null}
 Report 'Atualizacao concluida.' 100
 Clean-Success
}catch{
 $exitCode=1;$message=$_.Exception.Message
 for($i=$applied.Count-1;$i -ge 0;$i--){
  $item=$applied[$i]
  try{if($item.Existed){
       if((Test-Path -LiteralPath $item.Destination) -and (Get-FileHash -LiteralPath $item.Backup).Hash -eq (Get-FileHash -LiteralPath $item.Destination).Hash){continue}
       [IO.File]::Copy($item.Backup,$item.Destination,$true)}elseif([IO.File]::Exists($item.Destination)){[IO.File]::Delete($item.Destination)}}catch{('Falha ao restaurar: '+$item.Relative) | Add-Content -LiteralPath $log}
 }
 ('Falha: '+$message) | Add-Content -LiteralPath $log
 [Console]::Error.WriteLine($message)
 if($form){[Windows.Forms.MessageBox]::Show('A atualizacao nao foi concluida. '+$message+' Consulte atualizacao/atualizacao.log.','Capri',[Windows.Forms.MessageBoxButtons]::OK,[Windows.Forms.MessageBoxIcon]::Error) | Out-Null}
}finally{
 if($exitCode -ne 0 -and $applied.Count -eq 0 -and [IO.Directory]::Exists($stage)){
  try{if([IO.Path]::GetFullPath($stage).StartsWith($work+'\',[StringComparison]::OrdinalIgnoreCase)){Check-LocalPath $stage;[IO.Directory]::Delete($stage,$true)}}catch{}
 }
 if($form){$form.Close();$form.Dispose()}
 if($acquired){$mutex.ReleaseMutex()}
 if($mutex){$mutex.Dispose()}
}
exit $exitCode
