# Capri 1.7.4 — build 23

Uma falha de rede isolada substituia imediatamente o acesso valido por erro. Alem disso, o prazo de quatro segundos podia acabar enquanto uma requisicao estava em andamento. Essa transicao abria a tela de bloqueio e apagava as opcoes ativadas em memoria.

Uma validacao online bem-sucedida agora concede uma janela de 60 segundos para novas tentativas. Falhas de transporte, HTTP indisponivel e respostas incompletas nao interrompem esse acesso dentro da janela e nunca estendem seu prazo. Cada validacao bem-sucedida renova o prazo. Respostas explicitas de chave invalida, revogada, expirada ou atualizacao obrigatoria continuam sendo aplicadas imediatamente. O login inicial continua exigindo validacao online.

Os limites de cada etapa HTTP passaram de um para cinco segundos; a validacao continua fora das threads de interface e entrada. Se a indisponibilidade ultrapassar a janela, a execucao fica bloqueada ate uma nova validacao, preservando as preferencias em memoria e em disco. Ao recuperar, o menu volta ao estado aberto/fechado anterior.

Verificacao: testes C++ de configuracao e perfis; sequencia de falhas dentro da janela, expiracao sem renovacao, resposta explicita negada e recuperacao. Testes HTTP com o cliente compilado cobrem resposta lenta, HTTP 503, chave revogada, chave expirada e resposta incompleta. Nao houve simulacao de queda de rede durante uma partida real.

Backup anterior em backups/before-build23. Pacotes preservam as configuracoes e credenciais existentes.

Publicado: build 23, versao 1.7.4. Download HTTPS, hash do pacote e politica no servidor conferidos.
