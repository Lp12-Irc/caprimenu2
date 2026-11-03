param([int]$BuildNumber=0,[string]$Version="")
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    if($BuildNumber -gt 0){
        if($Version -notmatch '^\d+\.\d+\.\d+$'){throw 'Informe -Version no formato 1.3.1.'}
        $header=Join-Path $PSScriptRoot 'cs2_external/src/version.hpp'
        $text=Get-Content $header -Raw
        $match=[regex]::Match($text,'CAPRI_CLIENT_BUILD=(\d+)')
        if($BuildNumber -le [int]$match.Groups[1].Value){throw 'O novo build deve ser maior que o build atual.'}
        $text=[regex]::Replace($text,'CAPRI_CLIENT_BUILD=\d+',('CAPRI_CLIENT_BUILD='+$BuildNumber))
        $text=[regex]::Replace($text,'CAPRI_CLIENT_VERSION="[^"]+"',('CAPRI_CLIENT_VERSION="'+$Version+'"'))
        $text | Set-Content $header -Encoding UTF8
    }
    $build=Join-Path $PSScriptRoot 'build'
    cmake -S . -B $build -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao configurar o projeto.' }
    cmake --build $build --config Release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar o projeto.' }
    New-Item -ItemType Directory -Force -Path dist/licenses | Out-Null
    if(Test-Path "$build/_deps/imgui-src/LICENSE.txt"){Copy-Item -LiteralPath "$build/_deps/imgui-src/LICENSE.txt" -Destination dist/licenses/imgui.txt}
    if(Test-Path "$build/_deps/json-src/LICENSE.MIT"){Copy-Item -LiteralPath "$build/_deps/json-src/LICENSE.MIT" -Destination dist/licenses/nlohmann-json.txt}
    Write-Host "Executavel: $PSScriptRoot\dist\Capri.exe"
} finally {
    Pop-Location
}

