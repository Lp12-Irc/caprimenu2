# Chaves online no PC do administrador

O servidor le o mesmo hillblack.db usado pelo Capri.Admin.exe, na pasta principal. A API aceita apenas POST /validate com uma chave e verifica existencia, status e prazo no banco central. A validacao online e somente pela chave: nao vincula ao HWID. A revalidacao do cliente continua ocorrendo a cada cinco minutos. Uma chave revogada no painel deixa de ser aceita na proxima validacao.

## Como usar

1. No seu PC, abra Iniciar-Servidor-Chaves.cmd. Python e cloudflared precisam estar instalados. O iniciador reaproveita um servico saudavel que ja esteja ativo.
2. Gere os pacotes com `python ferramentas/empacotar_cliente.py`.
3. Envie entregas/Capri-Cliente.zip ao amigo. Ele extrai tudo e informa a chave. Nao precisa instalar Python, cloudflared ou receber o banco.
4. Mantenha seu PC conectado, sem suspensao, com os processos do servidor e do tunel funcionando.

A API escuta apenas em 127.0.0.1. O tunel fornece um endereco HTTPS publico somente para as rotas de validacao e saude. Nao existe rota para baixar o banco, gerar, listar ou revogar chaves. O banco e os perfis locais nao sao alterados ao validar online. Os pacotes nao incluem hillblack.db nem a chave salva do administrador.

O endereco atual fica em HillBlack.ini [auth] endpoint; os PIDs e logs ficam em servidor/server-state.json e servidor/*.log. Para verificar o servico sem enviar chave, abra o endereco publico seguido de /health.

## Endereco temporario

O Quick Tunnel gera um hostname temporario e diferente quando um novo tunel e criado. Ao reiniciar o PC ou recriar o tunel, execute o iniciador novamente, gere um novo pacote ou envie somente o novo HillBlack.ini ao amigo. Nao e preciso recompilar o EXE para atualizar a URL. Para um endereco permanente, configure um tunel nomeado com dominio na sua conta Cloudflare e use seu endereco HTTPS no INI.

Referencia: https://developers.cloudflare.com/tunnel/get-started/quick-tunnels/

## Atualizacoes obrigatorias

O cliente distribuido exige validacao online. Consulte ATUALIZACOES.md para publicar uma versao minima pelo painel do administrador.
