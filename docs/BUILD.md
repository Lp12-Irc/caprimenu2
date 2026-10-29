# Executável Windows x64

O projeto CMake gera `dist/HillBlack.exe` usando MSVC, C++20 e runtime C++ estático. O programa continua dependendo das bibliotecas do sistema Windows.

## Compilar novamente

Pré-requisitos: Visual Studio 2022 Build Tools com ferramentas C++, Windows SDK, CMake e Git. Execute `./build.ps1` no PowerShell. A primeira configuração baixa Dear ImGui e nlohmann/json dos commits fixados em `CMakeLists.txt`.

O script copia também as licenças das dependências para `dist/licenses/`.

## Estado do aplicativo

Esta etapa acrescenta o build e corrige erros de compilação encontrados nos fontes. A compilação não valida os offsets do jogo, a integração com a API ou os recursos em execução.

O código recebido carrega `configs/default.hbc` relativo ao diretório de trabalho e exige uma chave de licença válida no serviço de autenticação configurado em `auth.cpp`. Ao iniciar, uma janela permite informar a key e tentar novamente em caso de falha. Após a validação online, a key é salva e o programa aguarda o CS2 para abrir o menu principal. As chaves existentes apenas no banco local do painel precisam estar também no banco usado pela API.

Não foram removidas verificações de licença nem executadas chamadas à API durante a compilação.
