# Capri 1.8.2 — trigger e menu

- Somente cabeca fica marcado por padrao, inclusive ao carregar perfis antigos sem essa opcao.
- O trigger exige um inimigo confirmado pela mira do jogo e uma leitura atualizada da cabeca alinhada com o centro da camera.
- Sair da regiao cancela o atraso pendente e solta o clique. Dados ausentes ou invalidos bloqueiam novos disparos.
- A aba do trigger separa ativacao, regiao, tempos e estado atual. A margem da cabeca, o intervalo entre cliques e os outros ajustes podem ser salvos no perfil.
- Restaurar ajustes do trigger modifica apenas os controles dessa aba.
- O login Steam e a alteracao automatica do nome continuam removidos. A compilacao deixou de gerar e distribuir o auxiliar Steam.

## Uso

Abra a aba Triggerbot, ative o recurso e mantenha Somente cabeca marcado. Feche o painel, segure a tecla escolhida e posicione a mira na cabeca. Margem menor exige alinhamento mais central. Salve o perfil para manter suas preferencias.

O filtro usa uma regiao conservadora estimada em torno do osso da cabeca; nao le a hitbox exata do servidor. Dispersao, movimento, recuo e latencia ainda podem causar erros. O trigger nao move a mira.

Esta entrega e local; nao publica uma atualizacao no servidor.

## Validacao

Compilacao Release x64 concluida. Testes entities_test e config_test aprovados. Capturas do trigger em janela normal e 800x600 verificadas visualmente. O executavel instalado confere com o SHA-256 do manifesto. Nao houve teste de headshots em partida real.
