# Conexao fixa gratuita

Endereco dos clientes: https://capri-conexao.leonardofernando06022013.workers.dev

Inicie `Iniciar-Servidor-Chaves.cmd` na pasta principal. O inicializador recupera a API e o tunel quando necessario, registra o tunel atual no Worker e mantem o endereco fixo em `HillBlack.ini` e nos pacotes de entrega. Mantenha o computador e o servico ligados.

Instalacoes antigas precisam receber `entregas/Reconectar-Servidor.zip` uma vez: extraia `HillBlack.ini` junto de `Capri.exe`. Para o administrador remoto, use `entregas/Reconectar-ADM.zip` uma vez. Nao copie a configuracao de ADM remoto para o PC que hospeda o banco.

O banco permanece neste computador. O Worker encaminha apenas as rotas de validacao, administracao autenticada, saude e pacotes. Credenciais nao sao armazenadas no KV nem registradas em logs do Worker. O KV guarda somente o endereco do tunel. A comunicacao usa HTTPS, os cabecalhos de IP encaminhados exigem um segredo de gateway e o cliente continua verificando o SHA-256 das atualizacoes.

Os dois segredos do gateway ficam em bindings secretos do Worker e em `servidor/cloudflare-gateway.dpapi`, protegido pelo Windows para este usuario. Esse arquivo nao deve ser enviado aos clientes nem publicado no Git. Nao o apague: a inicializacao do servidor depende dele. Em outro Windows ou usuario, sera necessario configurar novas credenciais e atualizar os bindings do Worker.

O plano gratuito tem limites de requisicoes e armazenamento; nao foi contratado plano pago. Cada validacao de chave tambem conta como uma requisicao ao Worker. Alteracoes do tunel podem demorar para propagar no KV, que utiliza cache de 30 segundos. A indisponibilidade nunca concede acesso sem validacao.

Dependencias Python do servidor e das ferramentas: `servidor/requirements.txt`. A biblioteca truststore usa a validacao de certificados do Windows; nenhuma verificacao TLS foi desativada.

O teste de recuperacao trocou o tunel real sem mudar o endereco fixo e repetiu login, validacao, revogacao, reativacao e limite de IP com contas temporarias removidas ao final.
