# supervisor.ps1 — Capri License Server watchdog
# runtime: Windows PowerShell 5.1+ / pwsh 7+ · target: license_server.py + cloudflared quick tunnel
# Mantem o servico de chaves vivo, detecta crash silencioso e avisa quando a URL muda.
param(
    [int]$Port = 18765,
    [int]$IntervalSec = 20,
    [switch]$Once,
    [switch]$Verbose
)
$ErrorActionPreference = 'Stop'
$root      = Split-Path $PSScriptRoot -Parent
$stateFile = Join-Path $PSScriptRoot 'server-state.json'
$watchLog  = Join-Path $PSScriptRoot 'supervisor.log'
$iniPath   = Join-Path $root 'HillBlack.ini'
$dbPath    = Join-Path $root 'hillblack.db'
$apiScript = Join-Path $PSScriptRoot 'license_server.py'

function Write-Log {
    param([string]$Message, [string]$Level = 'INFO')
    $line = '{0} [{1}] {2}' -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Level, $Message
    Add-Content -LiteralPath $watchLog -Value $line -Encoding UTF8
    if ($Level -in @('WARN','ERROR') -or $Verbose) { Write-Host $line }
}

function Get-PythonPath {
    $cmd = Get-Command python -ErrorAction SilentlyContinue
    if (-not $cmd) { throw 'python nao encontrado no PATH.' }
    return $cmd.Source
}

function Get-CloudflaredPath {
    $cmd = Get-Command cloudflared -ErrorAction SilentlyContinue
    if (-not $cmd) { throw 'cloudflared nao encontrado no PATH.' }
    return $cmd.Source
}

function Test-Health {
    param([int]$TimeoutSec = 5)
    try {
        $r = Invoke-RestMethod -Uri "http://127.0.0.1:$Port/health" -TimeoutSec $TimeoutSec
        return ($r.service -eq 'capri-license-api')
    } catch { return $false }
}

function Test-PublicHealth {
    param([string]$Url, [int]$TimeoutSec = 10)
    if (-not $Url) { return $false }
    try {
        $r = Invoke-RestMethod -Uri "$Url/health" -TimeoutSec $TimeoutSec
        return ($r.service -eq 'capri-license-api')
    } catch { return $false }
}

function Get-State {
    if (-not (Test-Path -LiteralPath $stateFile)) { return $null }
    try { return Get-Content $stateFile -Raw | ConvertFrom-Json } catch { return $null }
}

function Save-State {
    param($Url, $ApiPid, $TunnelPid)
    @{
        url        = $Url
        api_pid    = $ApiPid
        tunnel_pid = $TunnelPid
        port       = $Port
        database   = $dbPath
    } | ConvertTo-Json | Set-Content -LiteralPath $stateFile -Encoding UTF8
}

function Write-Endpoint {
    param([string]$Url)
    "[auth]`r`nmode=online`r`nendpoint=$Url/validate`r`n" | Set-Content -LiteralPath $iniPath -Encoding UTF8
}

function Sync-Address {
    try {
        & (Get-PythonPath) (Join-Path $root 'ferramentas/sincronizar_endereco.py') | Out-Null
        Write-Log 'Reconectar-Servidor.zip regenerado com o endereco atual.'
    } catch {
        Write-Log "Falha ao sincronizar endereco: $($_.Exception.Message)" 'WARN'
    }
}

function Start-Api {
    if (Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue) {
        throw "Porta $Port ocupada por outro processo. Nao vou encerrar nada automaticamente."
    }
    $py   = Get-PythonPath
    $log  = Join-Path $PSScriptRoot 'api.log'
    $err  = Join-Path $PSScriptRoot 'api.err.log'
    # O servico precisa sobreviver ao supervisor: Start-Process amarraria o filho
    # ao job deste script e o servico morreria junto. Win32_Process.Create cria
    # processo sem vinculo de parentesco. Sem cmd.exe e sem redirecionamento
    # shell (nao sobrevive as aspas aninhadas): o wrapper -c redireciona stdio
    # por dentro do proprio Python, com os caminhos passados por argv.
    $wrapper = 'import runpy,sys;f=open(sys.argv[1],"a",encoding="utf-8",buffering=1);e=open(sys.argv[2],"a",encoding="utf-8",buffering=1);sys.stdout=f;sys.stderr=e;sys.argv=sys.argv[3:];runpy.run_path(sys.argv[0],run_name="__main__")'
    $args = @('-u','-c',$wrapper,$log,$err,$apiScript,'--db',$dbPath,'--port',$Port)
    $quoted = ($args | ForEach-Object { if ($_ -match '[\s"]') { '"' + ($_ -replace '"','\"') + '"' } else { $_ } }) -join ' '
    $cmdline = '"{0}" {1}' -f $py, $quoted
    $res = Invoke-CimMethod -ClassName Win32_Process -MethodName Create `
        -Arguments @{ CommandLine = $cmdline; CurrentDirectory = $root }
    if ($res.ReturnValue -ne 0) { throw "Win32_Process.Create falhou (codigo $($res.ReturnValue))." }
    # O wrapper e o processo python: localiza pelo CommandLine.
    $apiPid = $null
    for ($i = 0; $i -lt 60; $i++) {
        Start-Sleep -Milliseconds 250
        $found = Get-CimInstance Win32_Process -Filter "Name='python.exe'" -ErrorAction SilentlyContinue |
                 Where-Object { $_.CommandLine -and $_.CommandLine -like "*license_server.py*" } |
                 Select-Object -First 1
        if ($found) { $apiPid = [int]$found.ProcessId; break }
    }
    if (-not $apiPid) {
        $why = Get-Content $log,$err -Raw -ErrorAction SilentlyContinue
        throw "license_server.py nao subiu. log: $why"
    }
    for ($i = 0; $i -lt 40; $i++) {
        if (Test-Health) { Write-Log "license_server.py reativado (PID $apiPid, desacoplado)."; return $apiPid }
        Start-Sleep -Milliseconds 250
    }
    $why = Get-Content $log,$err -Raw -ErrorAction SilentlyContinue
    throw "license_server.py nao respondeu ao health em 10s. log: $why"
}

function Start-Tunnel {
    $cf  = Get-CloudflaredPath
    $log = Join-Path $PSScriptRoot ('tunnel-' + [guid]::NewGuid().ToString('N') + '.log')
    $proc = Start-Process -FilePath $cf `
        -ArgumentList @('tunnel','--no-autoupdate','--protocol','http2','--url',"http://127.0.0.1:$Port",'--logfile',('"' + $log + '"')) `
        -WindowStyle Hidden `
        -RedirectStandardOutput (Join-Path $PSScriptRoot 'tunnel.out.log') `
        -RedirectStandardError  (Join-Path $PSScriptRoot 'tunnel.err.log') -PassThru
    $url = $null
    for ($i = 0; $i -lt 120; $i++) {
        if (Test-Path $log) {
            $m = [regex]::Match((Get-Content $log -Raw), 'https://[a-z0-9-]+\.trycloudflare\.com')
            if ($m.Success) { $url = $m.Value; break }
        }
        $proc.Refresh()
        if ($proc.HasExited) { throw "cloudflared encerrou (exit $($proc.ExitCode)). Veja tunnel.err.log." }
        Start-Sleep -Milliseconds 500
    }
    if (-not $url) { throw 'Nao foi possivel obter o endereco HTTPS do tunel.' }
    Write-Log "Tunel novo criado (PID $($proc.Id)): $url"
    return @{ Process = $proc; Url = $url }
}

# ---------------------------------------------------------------------------
Write-Log ('Supervisor iniciado. porta={0} intervalo={1}s once={2}' -f $Port, $IntervalSec, [bool]$Once)

while ($true) {
    try {
        if (-not (Test-Path -LiteralPath $dbPath)) { throw 'hillblack.db ausente na pasta principal.' }

        $state    = Get-State
        $apiAlive = $false
        $tunAlive = $false
        $url      = $null

        if ($state) {
            $url = $state.url
            $ap  = Get-Process -Id $state.api_pid    -ErrorAction SilentlyContinue
            $tp  = Get-Process -Id $state.tunnel_pid -ErrorAction SilentlyContinue
            $apiAlive = ($ap -and $ap.ProcessName -match '^python') -and (Test-Health)
            $tunAlive = ($tp -and $tp.ProcessName -eq 'cloudflared')
        }

        # --- API morta: causa raiz do incidente -----------------------------
        if (-not $apiAlive) {
            if ($state) { Write-Log "license_server.py nao responde (PID $($state.api_pid) ausente ou travado). Reiniciando." 'WARN' }
            if ($state) {
                Get-Process -Id $state.api_pid -ErrorAction SilentlyContinue | ForEach-Object {
                    Write-Log "Encerrando processo python zumbi PID $($_.Id)." 'WARN'
                    Stop-Process -Id $_.Id -Force -ErrorAction SilentlyContinue
                }
            }
            Start-Sleep -Seconds 2
            $api = Start-Api

            # Tunel orfao aponta para a porta recem-reaberta; reaproveita se vivo.
            if ($tunAlive -and $url -and (Test-PublicHealth $url)) {
                Write-Log 'Tunel existente continua publicando. Endpoint preservado.'
                Save-State -Url $url -ApiPid $api -TunnelPid $state.tunnel_pid
                Write-Endpoint $url
                Sync-Address
            } else {
                if ($tunAlive) {
                    Write-Log 'Tunel sem resposta publica. Recriando.' 'WARN'
                    Get-Process -Id $state.tunnel_pid -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
                    Start-Sleep -Seconds 2
                }
                $t = Start-Tunnel
                Save-State -Url $t.Url -ApiPid $api -TunnelPid $t.Process.Id
                Write-Endpoint $t.Url
                Sync-Address
                Write-Log "ENDPOINT MUDOU: $($t.Url) - reenvie entregas/Reconectar-Servidor.zip ao cliente." 'WARN'
            }
        }
        # --- API viva mas tunel morto --------------------------------------
        elseif (-not $tunAlive) {
            Write-Log 'Tunel ausente com API viva. Recriando tunel.' 'WARN'
            $t = Start-Tunnel
            Save-State -Url $t.Url -ApiPid $state.api_pid -TunnelPid $t.Process.Id
            Write-Endpoint $t.Url
            Sync-Address
            Write-Log "ENDPOINT MUDOU: $($t.Url) - reenvie entregas/Reconectar-Servidor.zip ao cliente." 'WARN'
        }
        # --- Tudo vivo: checa a rota publica, nao so o loopback -------------
        else {
            if (-not (Test-PublicHealth $url)) {
                Write-Log "Rota publica $url nao respondeu. Recriando tunel." 'WARN'
                Get-Process -Id $state.tunnel_pid -ErrorAction SilentlyContinue | Stop-Process -Force -ErrorAction SilentlyContinue
                Start-Sleep -Seconds 2
                $t = Start-Tunnel
                Save-State -Url $t.Url -ApiPid $state.api_pid -TunnelPid $t.Process.Id
                Write-Endpoint $t.Url
                Sync-Address
                Write-Log "ENDPOINT MUDOU: $($t.Url) - reenvie entregas/Reconectar-Servidor.zip ao cliente." 'WARN'
            } elseif ($Verbose) {
                Write-Log "OK - API e rota publica saudaveis ($url)."
            }
        }
    }
    catch {
        Write-Log "Erro no ciclo do supervisor: $($_.Exception.Message)" 'ERROR'
    }

    if ($Once) { break }
    Start-Sleep -Seconds $IntervalSec
}

Write-Log 'Supervisor encerrado.'
