param([string]$Dotnet='dotnet')
$ErrorActionPreference='Stop'
Push-Location $PSScriptRoot
try {
    $localSdk=Join-Path $PSScriptRoot '../ferramentas/dotnet-sdk/dotnet.exe'
    if($Dotnet -eq 'dotnet' -and (Test-Path $localSdk)){$Dotnet=$localSdk}
    $out=Join-Path $PSScriptRoot 'build/admin-publish'
    $obj=(Join-Path $PSScriptRoot 'build/admin-obj')+'/'
    $bin=(Join-Path $PSScriptRoot 'build/admin-bin')+'/'
    & $Dotnet publish admin_panel/HillBlack.Admin.csproj -c Release -r win-x64 --self-contained true -o $out "-p:BaseIntermediateOutputPath=$obj" "-p:BaseOutputPath=$bin"
    if($LASTEXITCODE -ne 0){throw 'Falha ao publicar o administrador.'}
    $binary=Get-Item (Join-Path $out 'Capri.Admin.exe')
    if($binary.Length -ge 100MB){throw 'Executavel acima do limite de 100 MiB do GitHub.'}
    Copy-Item -LiteralPath $binary.FullName -Destination ../Capri.Admin.exe -Force
    $versionSource=Get-Content admin_panel/AdminUpdate.cs -Raw
    $adminBuild=[int][regex]::Match($versionSource,'const int Build=(\d+)').Groups[1].Value
    $adminVersion=[regex]::Match($versionSource,'const string Version="([^"]+)"').Groups[1].Value
    $adminManifest=@{build=$adminBuild;version=$adminVersion;sha256=(Get-FileHash -LiteralPath $binary.FullName -Algorithm SHA256).Hash.ToLowerInvariant()} | ConvertTo-Json
    [IO.File]::WriteAllText((Join-Path (Split-Path $PSScriptRoot -Parent) 'admin-version.json'),$adminManifest,[Text.UTF8Encoding]::new($false))
    Write-Host ('Capri.Admin.exe: {0:N2} MiB' -f ($binary.Length/1MB))
} finally {Pop-Location}
