# Capri — integração com CS2, 31/10/2026

Atualização mais recente de mira, otimização e bomba/desarme: [REVISAO-TEMPO-REAL.md](REVISAO-TEMPO-REAL.md).

Revisão mais recente de precisão do ESP e radar com fundo do mapa: [REVISAO-ESP-RADAR-MAPA.md](REVISAO-ESP-RADAR-MAPA.md).

Revisão posterior de resposta da mira, conexões do corpo e triggerbot: [REVISAO-MIRA-ESP-TRIGGER.md](REVISAO-MIRA-ESP-TRIGGER.md).

O executável é compilado para o CS2 aberto durante a correção: engine build **14194**, Steam build **26334290**, patch **1.41.9.4**. O menu informa se a conexão está pronta e quantos jogadores foram encontrados. Uma versão incompatível interrompe o uso dos offsets.

## Uso

1. Feche as instâncias antigas do Capri e abra `Capri.exe` nesta pasta.
2. Use o CS2 em janela ou sem bordas. **PgUp** abre e oculta o menu.
3. Ajuste as funções e feche o menu para usar mira, recoil e disparo. As entradas ficam suspensas enquanto o menu está aberto ou o jogo perde o foco.
4. A tecla padrão da mira é o botão direito; a do triggerbot é Alt. Ambas podem ser alteradas. A opção “Exigir mouse direito” exige esse botão mesmo se outra tecla de mira estiver selecionada.
5. A sensibilidade do jogo é lida automaticamente; mantenha o multiplicador em 1 para usar o valor do CS2.
6. Salve o perfil `default` para carregar os ajustes na próxima abertura. Carregar outro perfil também pode alterar a tecla do menu.

## Correções

- Resolução de lista de entidades, índices, máscara de handles, registros de 112 bytes e travessia entre blocos.
- Offsets de campos extraídos da instalação em execução; transformações dos ossos corrigidas para posição, escala e quaternion em 32 bytes.
- Leitura de arma ativa, munição e inventário pelo serviço de armas; item econômico com seus níveis corretos de campos.
- Inventário de utilitários, armas soltas no chão e espectadores implementados e liberados nos controles. Espectadores são comparados ao alvo realmente observado, incluindo jogadores mortos.
- Bomba com ponteiro direto, relógio do jogo e contagem de explosão/desarme; remoção de estado vencido.
- Mira com direção horizontal corrigida, posição dos olhos, FOV respeitado em ambos os modos e atraso de aquisição sem bloquear o render. Recoil funciona com a mira desligada e compensa a diferença entre amostras.
- Triggerbot usa a entidade identificada pelo jogo na mira. Sobreposição visual de ossos por si só não dispara. Pressionar e soltar usam prazos, sem `Sleep` no render; pausa libera o botão injetado.
- No Flash solicita a permissão de escrita ao ser ativado e restaura o alpha original ao ser desativado ou suspenso.
- Teclas de menu, mira e disparo configuráveis; rótulos refletem a tecla atual. PgUp é o padrão do código e do perfil existente.
- Status de leitura e permissão de escrita visíveis no menu. Configuração preserva o status da conexão ao carregar perfis.

## Evidências

- Compilação Release x64 com MSVC 2022.
- Testes de autenticação e perfis existentes, mais regressões de entidades/handles, ossos, armas, espectadores, direção/FOV/atraso da mira, recoil independente, sequência do triggerbot, restauração do No Flash e relógios da bomba.
- Testes de entrada gravam eventos em memória; não movimentam o mouse nem disparam no jogo real.
- Teste da janela: passagem/captura de cliques, encaminhamento de texto ao ImGui, alinhamento, redimensionamento Direct3D e render passaram.
- Sonda somente de leitura na partida aberta encontrou 10 jogadores, ossos dos jogadores vivos, armas, utilitários, espectadores e itens. Também gerou vértices/índices de desenho a partir desses dados.
- Com a bomba plantada, a sonda leu `remaining=25.15`, `24.72`, `24.31`, `23.89`, `23.50`, todos com relógio válido.

Os logs finais estão em `verificacao/`. O binário original e o código anterior estão em `backup-antes-integracao/`; o perfil anterior à mudança de tecla está em `configs/default.hbc.before-pgup.bak`.

## Limitações verificáveis

**Silent Aim permanece indisponível.** A rotina anterior escrevia em `m_angEyeAngles`, sem implementar alteração válida dos comandos de disparo. Ela foi removida do fluxo e o controle foi identificado como indisponível.

A cor de avistamento usa a máscara de jogadores avistados fornecida pelo jogo. Ela não representa um teste geométrico instantâneo de paredes.

A leitura e a geração dos desenhos foram verificadas com a partida real. Mira, disparo e No Flash foram testados com memória local e eventos capturados; o efeito desses comandos durante o uso real do CS2 ainda precisa de confirmação do usuário. Não foi realizado bypass de anticheat.

## Proveniência dos offsets

Extração local com [a2x/cs2-dumper](https://github.com/a2x/cs2-dumper), commit `21ee85107074fdf0347dc8afd4a8c3dd8d2398e2`, em `2026-10-31T16:30:19.994388300+00:00`.

`client.dll` SHA-256: `d618bbf8e6afb36baa09b3a282035996a88300458c55d2aa5cb81f0495c0ec54`.

Os arquivos `ferramentas/cs2-local/{client_dll,offsets,info,provenance}.json` registram campos, RVAs, build e hash. O cabeçalho utilizado na compilação é `fontes/cs2_external/src/generated_offsets.hpp`.

O relógio em `CGlobalVars + 0x30` foi confirmado nas instruções da DLL instalada, incluindo as rotinas em RVA `0xd179c2` e `0xd1a3a4`, que o subtraem do horário de explosão/desarme. Evidências em `ferramentas/cs2-local/clock_provenance.json`; a reprodução está em `ferramentas/derive_clock.py`.

Uma atualização do CS2 exige nova extração e validação. Os scripts desta entrega registram os caminhos desta instalação e devem ser ajustados se o jogo for movido.
