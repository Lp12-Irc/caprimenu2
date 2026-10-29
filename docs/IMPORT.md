# Organização dos arquivos

Foi utilizado exclusivamente o segundo anexo `Texto colado.txt`, enviado em 29/10/2026, com 115.583 bytes. Ele substitui o primeiro envio, que estava truncado.

## O que foi feito

- Separados os blocos pelos nomes de arquivo presentes no texto.
- Mantidos os 22 arquivos C++ juntos em `cs2_external/src/`, conforme a estrutura descrita no README recebido.
- Separados `Database.cs`, `MainForm.cs` e `Program.cs` em `admin_panel/`. O bloco `Program.cs` estava ao final de `MainForm.cs` no anexo.
- Separados `main.py` e `database.py` em `api_server/`.
- Preservado o README recebido em `README.original.md`.
- Removidos apenas títulos externos e marcadores de linguagem; normalizadas as quebras de linha para LF e a codificação para UTF-8 sem BOM.
- Adicionados README de navegação, documentação da autenticação e regras de arquivos ignorados/formatação.

O código dos módulos foi preservado. Não foram implementadas funcionalidades, corrigidos problemas de lógica, instaladas dependências nem criados projetos de build.

## Verificação

A organização é conferida contra os blocos do segundo anexo. A análise sintática dos dois arquivos Python não importa nem executa a API. Compilação C++/C#, execução do cliente, acesso ao jogo e chamadas ao serviço remoto não fazem parte desta importação.
