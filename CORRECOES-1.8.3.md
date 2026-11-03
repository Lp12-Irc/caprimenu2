# Capri 1.8.3 — resposta do trigger e continuidade do aim

- O aim deixa de esperar indefinidamente pelo retorno da camera. Um comando sem retorno pode ser reavaliado apos 80 ms, com a tecla ainda pressionada, usando novamente o alvo atual e as verificacoes existentes.
- Quadros repetidos continuam sem acumular movimentos a cada atualizacao. Soltar a tecla continua encerrando a correcao.
- O trigger permite intervalo adicional de 0 ms. O perfil local selecionado recebe atraso inicial 0 ms, intervalo adicional 0 ms e duracao do clique 16 ms.
- O botao Resposta imediata aplica esses tres ajustes no menu. Instalacoes remotas preservam os perfis existentes: use esse botao e salve o perfil se quiser remover os tempos antigos.
- O ciclo de entrada deixa de aguardar 20 ms quando so esta esperando novos dados com o jogo em foco.
- O filtro Somente cabeca e as verificacoes de dados permanecem ativos. A cadencia da arma, alinhamento, dispersao e latencia ainda afetam o resultado.
- Os novos pacotes deixam de incluir a antiga pasta do auxiliar Steam.

Os testes usam memoria local simulada e uma funcao que registra entradas, sem enviar tiros ao jogo. Esta verificacao nao comprova a correcao do sintoma em uma partida real.

## Entrega

Build final 29 publicado no catalogo do painel ADM. Download publico HTTPS do pacote completo conferido antes da publicacao. EXE local, manifesto e pacote conferidos por SHA-256. Testes entities_test, config_test e revisao visual da aba em janela normal e pequena aprovados.
