$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    cmake -S . -B build -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao configurar o projeto.' }
    cmake --build build --config Release --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Falha ao compilar o projeto.' }
    New-Item -ItemType Directory -Force -Path dist/licenses | Out-Null
    Copy-Item -LiteralPath build/_deps/imgui-src/LICENSE.txt -Destination dist/licenses/imgui.txt
    Copy-Item -LiteralPath build/_deps/json-src/LICENSE.MIT -Destination dist/licenses/nlohmann-json.txt
    Write-Host "Executavel: $PSScriptRoot\dist\HillBlack.exe"
} finally {
    Pop-Location
}
