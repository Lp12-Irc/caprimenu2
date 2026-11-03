# configurar-tunel.ps1 — cria e configura o tunel nomeado do servidor de chaves Capri
# runtime: Windows PowerShell 5.1+ / pwsh 7+ · requer: cloudflared autenticado (cert.pem)
# Uso:  .\configurar-tunel.ps1 -Dominio seumadruga.com -Hostname keys.seumadruga.com
param(
    [Parameter(Mandatory = $true)][string]$Hostname,
    [string]$TunnelName = 'capri-keys',
    [int]$Port = 18765,
    [switch]$Force
)
$ErrorActionPreference = 'Stop'
$root      = Split-Path $PSScriptRoot -Parent
$cfDir     = Join-Path $env:USERPROFILE '.cloudflared'
$certPath  = Join-Path $cfDir 'cert.pem'
$configYml = Join-Path $cfDir 'config.yml'
$iniPath   = Join-Path $root 'HillBlack.ini'

function Fail($msg) { Write-Host "ERRO: $msg" -ForegroundColor Red; exit 1 }
function Ok($msg)   { Write-Host "OK: $msg" -ForegroundColor Green }
function Step($msg) { Write-Host "`n>>> $msg" -ForegroundColor Cyan }

# --- pre-requisitos ---------------------------------------------------------
Step 'Verificando pre-requisitos'
$cf = Get-Command cloudflared -ErrorAction SilentlyContinue
if (-not $cf) { Fail 'cloudflared nao encontrado no PATH.' }
Ok "cloudflared: $($cf.Source)"

if (-not (Test-Path -LiteralPath $certPath)) {
    Fail @"
cert.pem ausente. Rode primeiro, numa janela interativa:

    cloudflared tunnel login

Isso abre o navegador, voce autoriza o dominio na sua conta Cloudflare,
e o cert.pem e gravado em $cfDir
"@
}
Ok "certificado de origem presente"

# valida formato do hostname
if ($Hostname -notmatch '^[a-z0-9]([a-z0-9-]*[a-z0-9])?(\.[a-z0-9]([a-z0-9-]*[a-z0-9])?)+$') {
    Fail "Hostname invalido: '$Hostname'. Use algo como keys.seudominio.com"
}
# extrai o dominio raiz (ultimos dois rotulos, com tratamento pra .com.br)
$labels = $Hostname.Split('.')
$tld = $labels[-1]; $sld = $labels[-2]
$rootDomain = "$sld.$tld"
if ($labels.Count -ge 3 -and $tld -eq 'br' -and $labels[-2] -in @('com','net','org','gov','edu')) {
    $rootDomain = "$($labels[-3]).$sld.$tld"
    $sub = ($labels[0..($labels.Count - 4)] -join '.')
} else {
    $sub = ($labels[0..($labels.Count - 3)] -join '.')
}
Write-Host "  dominio raiz : $rootDomain"
Write-Host "  subdominio   : $(if($sub){$sub}else{'(nenhum)'})"
Write-Host "  hostname     : $Hostname"

# --- cria ou reaproveita o tunel --------------------------------------------
Step "Tunel nomeado '$TunnelName'"
$existing = $null
try {
    $list = & cloudflared tunnel list --output json 2>$null | ConvertFrom-Json
    $existing = $list | Where-Object { $_.name -eq $TunnelName }
} catch { }

if ($existing) {
    $tunnelId = $existing.id
    Ok "tunel ja existe: $TunnelName ($tunnelId)"
} else {
    $created = & cloudflared tunnel create $TunnelName 2>&1
    if ($LASTEXITCODE -ne 0) { Fail "Falha ao criar tunel: $created" }
    $list = & cloudflared tunnel list --output json 2>$null | ConvertFrom-Json
    $t = $list | Where-Object { $_.name -eq $TunnelName }
    if (-not $t) { Fail 'Tunel criado mas nao localizado na lista.' }
    $tunnelId = $t.id
    Ok "tunel criado: $TunnelName ($tunnelId)"
}

$credFile = Join-Path $cfDir "$tunnelId.json"
if (-not (Test-Path -LiteralPath $credFile)) {
    Fail "Credencial do tunel ausente: $credFile"
}
Ok "credencial presente"

# --- rota DNS ---------------------------------------------------------------
Step "Rota DNS $Hostname -> tunel"
$null = & cloudflared tunnel route dns $TunnelName $Hostname 2>&1
if ($LASTEXITCODE -ne 0) {
    Write-Host "  rota ja existente ou ja apontada; seguindo." -ForegroundColor Yellow
} else {
    Ok "rota DNS registrada"
}

# --- config.yml -------------------------------------------------------------
Step 'Escrevendo config.yml'
if ((Test-Path -LiteralPath $configYml) -and -not $Force) {
    $backup = "$configYml.bak-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
    Copy-Item -LiteralPath $configYml -Destination $backup
    Write-Host "  config.yml anterior salvo em $backup" -ForegroundColor Yellow
}
$yaml = @"
# Capri license server - tunel nomeado (URL fixa)
tunnel: $tunnelId
credentials-file: $credFile

ingress:
  - hostname: $Hostname
    service: http://127.0.0.1:$Port
  - service: http_status:404
"@
$yaml | Set-Content -LiteralPath $configYml -Encoding UTF8
Ok "config.yml gravado"

# --- endpoint fixo ----------------------------------------------------------
Step 'Gravando endpoint fixo no HillBlack.ini'
if ((Test-Path -LiteralPath $iniPath) -and -not $Force) {
    $backup = "$iniPath.bak-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
    Copy-Item -LiteralPath $iniPath -Destination $backup
    Write-Host "  HillBlack.ini anterior salvo em $backup" -ForegroundColor Yellow
}
"[auth]`r`nmode=online`r`nendpoint=https://$Hostname/validate`r`n" | Set-Content -LiteralPath $iniPath -Encoding UTF8
Ok "endpoint: https://$Hostname/validate"

# --- resumo -----------------------------------------------------------------
Step 'Concluido'
@"

Tunel     : $TunnelName
ID        : $tunnelId
Hostname  : $Hostname
Endpoint  : https://$Hostname/validate
Config    : $configYml

Proximo passo - subir o servico com o tunel nomeado:

    .\servidor\iniciar.ps1 -Named

A URL agora e PERMANENTE. Depois de distribuir o cliente uma vez,
nunca mais e preciso reenviar HillBlack.ini.

"@ | Write-Host -ForegroundColor Green
