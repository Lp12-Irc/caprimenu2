# Atualização de mira e objetivos em tempo real — 31/10/2026

## Mira que se afasta ou sobe

A correção agora usa a cabeça/pescoço/peito projetado na câmera atual. Um comando de movimento aceito precisa receber resposta da câmera antes de outro comando ser acumulado sobre o mesmo quadro. Isso evita ultrapassar o alvo quando o ciclo de entrada é mais rápido que os quadros processados pelo jogo.

O alvo selecionado é mantido enquanto a tecla fica pressionada e ele continua vivo, válido e dentro do FOV. O código deixa de usar posições estimadas quando a leitura do osso falha. Leituras de pose divergentes, escala inválida, pontos acima/fora do corpo, mudança de pawn e valores anormais de recoil são rejeitados.

Recoil exige disparo ativo. A mira baseada na câmera não subtrai novamente o recoil já visível na projeção. A resposta possui limite de rotação por tempo decorrido e uma pequena zona abaixo de um passo do mouse para evitar oscilar em torno do ponto.

Os valores de mouse persistidos nesta instalação foram conferidos: `m_pitch=0.022`, `m_yaw=0.022`, sensibilidade `1.1`.

## Bomba e desarme

O painel lê o relógio e os estados do jogo a cada quadro, sem depender de um valor de segundos calculado no cache de entidades. Exibe os tempos da explosão e do desarme lado a lado, duas barras e a indicação de que o desarme termina antes ou depois da explosão.

A duração do desarme vem de `m_flDefuseLength`: não há suposição fixa de cinco ou dez segundos. Início, cancelamento, conclusão e explosão alteram o painel conforme os estados lidos. Dados inconsistentes não reutilizam um contador antigo. Pausas seguem o relógio do jogo.

O contador foi habilitado no perfil `default`; teclas e demais opções foram preservadas.

## Otimização

- Leituras de campos próximos de controllers e pawns agrupadas em blocos, reduzindo chamadas separadas ao sistema.
- Poses lidas em buffers reutilizáveis de pilha, com conferência de consistência.
- Sem varredura de itens, inventário ou ossos quando as funções correspondentes estão desativadas.
- Menos consultas para jogadores mortos; observadores continuam sendo resolvidos.
- Ciclos reduzidos quando o aplicativo está sem foco e rejeição de dados antigos para a entrada.

As amostras locais de 200 atualizações estão em `verificacao/tempo-real/antes.txt` e `depois.txt`. Foram observadas 685,3 e 357,5 chamadas de leitura por atualização, com medianas de 1,637 ms e 1,181 ms. A partida e o número de jogadores vivos variaram entre as amostras; isso não constitui uma comparação controlada de FPS. O agrupamento lê mais bytes por chamada e prioriza reduzir o custo das chamadas repetidas.

## Verificação

As regressões incluem convergência da cabeça para o centro da tela, direção de movimento, ausência de comandos repetidos sobre uma câmera ainda sem resposta, manutenção do alvo, ossos inválidos e recoil sem disparo. Também cobrem leitura fresca dos dois relógios, cancelamento, duração de kit, conclusão e deadlines inválidos.

A observação somente de leitura da partida registrou a bomba plantada e um desarme de 5 segundos começando com 4,918 segundos restantes e sendo cancelado. Registros em `verificacao/tempo-real/bomba-real.jsonl`.

Build do CS2: engine 14194, Steam 26334290. O campo adicional `m_flDefuseLength` foi extraído do schema local de `C_PlantedC4`, offset `0x12d4`; relógio `CGlobalVars + 0x30` já derivado e documentado em `clock_provenance.json`. `client.dll` SHA-256: `d618bbf8e6afb36baa09b3a282035996a88300458c55d2aa5cb81f0495c0ec54`.

Backup do executável, fontes e perfil anteriores em `backup-tempo-real/`. Os testes de entrada usam eventos capturados e câmera simulada; o envio real de mouse da nova versão deve ser conferido no treino após reabrir o Capri.
