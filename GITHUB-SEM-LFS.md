# Envio pelo Git comum

Capri.exe e Capri.Admin.exe ficam como executaveis normais no repositorio. A publicacao do administrador usa compressao nativa do bundle .NET e verifica o limite de 100 MiB; nao ha remocao de funcionalidades por trimming.

A ferramenta de terceiros Source2Viewer e distribuida pelo arquivo `ferramentas/source2viewer/cli-windows-x64.zip`, ja presente no repositorio. O ZIP foi comparado ao EXE local e contem exatamente a mesma versao. Extraia-o nessa pasta quando precisar da ferramenta. O EXE extraido fica ignorado no Git.

Para recompilar o administrador com o SDK .NET 8:

```powershell
./fontes/build-admin.ps1
```

Depois de conferir as alteracoes, use Commit e Push origin no GitHub Desktop. Nao e necessario force push. Os commits antigos que ja foram publicados com LFS continuam no historico; estas alteracoes retiram o uso de LFS dos arquivos atuais, sem reescrever o historico remoto.
