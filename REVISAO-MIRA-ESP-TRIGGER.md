# Revisão da mira, esqueleto e triggerbot — 31/10/2026

## Mudanças

- **Mira:** execução independente da renderização/VSync, leitura atual dos ângulos e suavização calculada pelo tempo decorrido. A leitura de entidades publica cópias completas; a mira e o render não esperam o mutex que antes protegia toda a leitura e o desenho.
- **Resposta padrão:** atraso de aquisição em 0 ms e suavização em até 2. A tecla escolhida funciona sozinha; exigir o botão direito continua disponível como opção explícita. Suavização 1 oferece a resposta mais direta.
- **Esqueleto:** seleção de ossos pelos nomes do modelo. Nos modelos SAS e Phoenix da instalação aberta, `head_0` é 7, `neck_0` é 6, os braços terminam em 11/15 e as pernas usam 17–19/20–22. Os índices antigos atingiam olhos, arma e `eye_target`; este último estava a centenas de unidades do corpo. As conexões incluem clavículas e pelve e descartam pontos inválidos ou segmentos discrepantes.
- **Triggerbot:** atraso padrão em 0 ms, soltura imediata quando a tecla é liberada, execução independente dos quadros e recuperação após falha de envio. Um estado visível informa pausa, tecla, alvo, atraso ou rejeição da entrada pelo Windows.

O perfil conserva as teclas e os estados ligado/desligado das funções. Foram ajustados somente `aim_delay_ms`, `aim_smooth`, `aim_ads_only` e `tb_delay_ms`. O backup desta revisão fica em `backup-mira-esp-trigger/`.

## Uso

Reabra `Capri.exe`, ative as funções desejadas e feche o menu com **PgUp**. Segure a tecla mostrada no controle correspondente. O triggerbot dispara somente sobre um inimigo vivo; pausas, morte e perda de foco interrompem a entrada. O botão direito é a tecla padrão da mira; Alt é a tecla padrão do triggerbot, salvo configuração diferente no perfil.

## Verificação

- Compilação Release x64 para o mesmo CS2 build **14194**.
- Testes de identificação de cabeça/pernas por nome, exclusão de acessórios e leitura de um modelo com os ossos reordenados.
- Teste de cópias independentes do cache, incluindo o registro do jogador local.
- Testes de tecla configurada sem botão adicional, triggerbot com atraso zero, liberação antes do prazo, rejeição/recuperação da entrada e resposta da mira em intervalos de 2 ms e 16 ms.
- Testes de autenticação e perfis existentes mantidos.
- Sonda da partida real: modelos dos jogadores vivos reconhecidos por nome, segmentos anatômicos dentro dos limites e geração de desenho a partir desses dados. A sonda não envia entradas de mouse.

Os logs desta revisão estão em `verificacao/revisao-mira-esp-trigger/`. A observação do alvo sob a mira confirmou índices de entidades válidos durante o treino. O envio real de movimentos/disparos pela nova versão ainda depende da conferência no jogo após reabrir o aplicativo; os testes automatizados capturam os eventos sem enviá-los ao mouse.

## Proveniência

Os nomes foram lidos dos recursos SAS/Phoenix carregados pelo processo da instalação, cuja `client.dll` tem SHA-256 `d618bbf8e6afb36baa09b3a282035996a88300458c55d2aa5cb81f0495c0ec54`. Contagem e vetor de nomes no `CModel` foram verificados em `0x160`/`0x168`; o vínculo do modelo usa `CModelState.m_hModel` extraído do schema local. Registros em `ferramentas/cs2-local/rig_provenance.json`; reprodução em `ferramentas/record_rigs.py`.
