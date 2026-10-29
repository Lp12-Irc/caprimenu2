# CS2 / HillBlack

Fontes organizados a partir do anexo fornecido pelo proprietário do repositório. O material reúne um cliente C++ para Windows, um painel administrativo C# e uma API Python.

**Estado:** importação e organização do código recebido. Não é uma versão compilada ou validada em execução.

## Estrutura

```text
cs2_external/src/   Cliente C++20: 12 cabeçalhos e 10 implementações
admin_panel/        Painel WinForms: Database.cs, MainForm.cs e Program.cs
api_server/         API FastAPI e acesso SQLite
docs/              Documentação da importação e README original
```

| Componente | Entrada | Conteúdo |
| --- | --- | --- |
| Cliente Windows | [main.cpp](cs2_external/src/main.cpp) | Autenticação, configuração, memória, entidades, overlay, ESP, aim, menu e utilitários |
| Painel administrativo | [Program.cs](admin_panel/Program.cs) | Login administrativo, geração e revogação de chaves |
| API | [main.py](api_server/main.py) | Rotas `/validate` e `/health` |
| Persistência da API | [database.py](api_server/database.py) | Tabelas SQLite e operações sobre chaves |

## Dependências declaradas no material

- **Cliente:** Windows SDK, MSVC, C++20, Dear ImGui e nlohmann/json; bibliotecas WinHTTP, IP Helper, Direct3D 11 e DWM.
- **Painel:** .NET 8 / WinForms, Microsoft.Data.Sqlite e BCrypt.Net-Next.
- **API:** Python 3.11+, FastAPI, Uvicorn e Pydantic. O README recebido também cita python-dateutil.

As dependências de terceiros, arquivos de projeto C++/C# e versões fixadas não foram enviados. Não foram criados binários nem executados os programas nesta organização.

## Limitações identificadas na leitura

- O painel abre `hillblack.db`, enquanto a API abre `keys.db`; o material não fornece sincronização entre esses bancos.
- O cliente valida a chave salva em `configs/default.hbc`. A função de login interativo está incompleta e não é chamada por `WinMain`.
- O cliente ignora erros de certificado TLS e guarda a chave em configuração ofuscada com XOR. O painel contém uma credencial inicial fixa.
- As instruções e afirmações do README original foram preservadas como material de origem; não representam verificações realizadas nesta importação.

Veja [o fluxo de autenticação](docs/AUTHENTICATION.md), [o registro da organização](docs/IMPORT.md) e [o README original](docs/README.original.md).
