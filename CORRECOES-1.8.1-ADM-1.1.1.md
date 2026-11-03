# Capri 1.8.1 / build 26 e ADM 1.1.1 / build 5

## Trigger

Mantida a ativacao por tecla segurada. O alvo sob a mira e relido no momento da decisao, com validacao de controlador, pawn e identidade. Indices simples e handles completos correspondentes sao aceitos; valores nulos ou identidades antigas nao geram cliques. A espera nao e reiniciada por uma breve lacuna de atualizacao do cache, e nenhuma pressao nova e emitida enquanto a leitura esta atrasada. Soltar a tecla ou sair do jogo cancela o acionamento. Ha um intervalo minimo de 20 ms apos soltar o mouse, evitando UP e DOWN no mesmo ciclo. O status fica visivel sempre que o trigger esta ligado com o painel fechado.

Testes C++ de entidades aprovados com mouse e relogio simulados: tecla, alvo atual, identidade antiga, handle completo, atraso, lacuna de cache, soltura e intervalo entre cliques. Nao houve teste de disparo em partida real.

## Apagar key

Botao Apagar key no ADM, com confirmacao e resposta padrao Nao. Remove atomicamente somente a key selecionada, seus IPs e sua politica de IP, tanto no banco local quanto pela API autenticada. Revogar continua disponivel para suspensao reversivel. Nenhuma key real foi apagada durante os testes. A API foi recarregada na mesma porta e com o mesmo tunel; o endpoint permaneceu igual.

Seis testes da API e a suite de interface do ADM passaram. Foram verificadas a exigencia de autenticacao, remocao dos vinculos, recusa da key apagada e preservacao de outra licenca.

## Nome Steam __proton__

Apos entrar no Capri, o helper conecta ao editor oficial do perfil Steam. No primeiro uso, cada pessoa precisa autenticar a propria Steam na janela Conectar Steam. O perfil de navegador fica separado por usuario Windows e conta Steam ativa, em LocalAppData/Capri/SteamConnect. O Capri nao le senhas, cookies ou tokens da pagina. A conta ativa da Steam e conferida antes de editar. O botao Conta > Conectar Steam permite tentar novamente.

O nome publico e alterado para __proton__, inclusive fora do CS2. O helper confirma o valor apos recarregar o editor; falhas de salvamento ou formularios nao reconhecidos nao sao tratados como sucesso. Sessoes anonimas exigem login. A integracao usa Microsoft Edge WebView2 Runtime; se ele estiver ausente, o aplicativo informa a dependencia.

O teste steam_name_browser usa paginas locais e intercepta todas as requisicoes. Foram aprovados os casos de origem indevida, conta diferente, ausencia de login, salvamento, verificacao depois de recarregar, nome ja aplicado, falha de salvamento e formulario desconhecido. A troca real na conta Steam depende da primeira autenticacao do usuario e nao foi executada durante os testes.

Builds e testes de configuracao/perfis aprovados. O botao Salvar tambem foi exercitado com perfil isolado. Backup em backups/antes-trigger-delete-1.8.1. SDK WebView2 1.0.4314.35; licenca em licenses/WebView2-LICENSE.txt. Os pacotes nao incluem banco, credenciais nem perfis de navegador.

Publicados e conferidos: Capri 1.8.1 / build 26 e ADM 1.1.1 / build 5. Downloads HTTPS, hashes, helper Steam e registro de publicacao verificados.
