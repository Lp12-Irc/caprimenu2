# ESP e radar com mapa — 31/10/2026

## ESP

A projeção usa a matriz da câmera lida no momento do desenho. Duas leituras consecutivas conferem se a matriz está estável; falhas ou dados antigos deixam de produzir marcações deslocadas. A varredura de itens foi dividida em blocos para não atrasar periodicamente a atualização dos jogadores.

As caixas usam os limites reais de colisão do jogador e os ossos válidos do corpo. Isso acompanha a altura agachada, a largura e a inclinação da pose. Barra de vida, textos e linhas usam os mesmos limites, substituindo a proporção fixa anterior.

## Radar separado

- Fundo real do mapa atual, extraído da instalação do CS2.
- Coordenadas calibradas pelo arquivo de overview do mapa, com norte acima.
- Seta branca para você ou o jogador observado, azul para aliados e vermelho para inimigos.
- Número/nome dos jogadores, direção, pontos A/B e C4 plantada.
- Zoom de 1 a 4, tamanho ajustável e nomes opcionais.
- Seleção automática do piso nos mapas com overview inferior; `^` e `v` indicam outra altura.
- Marcadores fora da área ampliada são contados, sem serem acumulados artificialmente nas bordas.

Foram preparados 22 overviews: Baggage, Shoots, Italy, Office, Ancient e suas variantes, Anubis, Cache, Dust2, Inferno e variante, Mirage, Nuke, Overpass e variante, Train, Vertigo, Rush e Workshop Preview. Mapas sem imagem/coordenadas disponíveis mostram essa condição explicitamente.

O perfil foi ajustado para mapa completo, nomes ligados e tamanho mínimo de 320 pixels. Os estados ligado/desligado, as teclas e as outras opções foram preservados. Use PgUp para abrir/ocultar o menu.

**Mantenha a pasta `assets/radar` ao lado de `Capri.exe`.** A compilação também copia essa pasta para `fontes/dist/assets/radar`.

## Silent Aim: item pendente

Silent Aim ainda não tem implementação funcional nesta versão. O menu agora informa isso diretamente. Não foi criada uma ativação que apenas muda o estado visual de um botão.

A revisão confirmou que os ângulos do disparo pertencem aos comandos de input e ao seu histórico. Os descritores protobuf da `client.dll` local confirmam esse formato, mas não definem o layout nativo, a vida útil dos objetos nem a sincronização necessária para alterá-los. `m_angEyeAngles` e `dwViewAngles` não comprovam implementação de Silent Aim. Ativação e configuração funcionais permanecem sem entrega.

Referências do formato: [cs_usercmd.proto](https://github.com/SteamTracking/GameTracking-CS2/blob/master/Protobufs/cs_usercmd.proto) e [usercmd.proto](https://github.com/SteamTracking/GameTracking-CS2/blob/master/Protobufs/usercmd.proto). Na DLL local, a investigação somente de leitura identificou os descritores em file offsets `0x1bcf400` e `0x1b2fc30`; são posições no arquivo, não offsets para escrita em objetos.

## Verificação e proveniência

Os testes cobrem caixa em pé/agachada, movimento de câmera sem esperar a próxima leitura de entidades, rejeição de geometria atrás da câmera, calibração mundo/textura, escolha de pisos, perfis e autenticação. Todos os overviews foram decodificados de PNG para texturas Direct3D reais.

Assets extraídos de `game/csgo/pak01_dir.vpk`, Steam build 26334290, engine 14194, usando Source 2 Viewer CLI 20.1. [Documentação da ferramenta](https://s2v.app/ValveResourceFormat/guides/command-line.html). Os JSONs guardam a origem, escala e hash do overview original; a ferramenta de reprodução é `ferramentas/prepare_radar_assets.py`.

O nome do mapa em `CGlobalVars + 0x188` e os limites de colisão foram verificados na instalação em execução; evidência em `ferramentas/cs2-local/map_camera_provenance.json`. Campos de colisão vêm do schema extraído localmente. `client.dll` SHA-256: `d618bbf8e6afb36baa09b3a282035996a88300458c55d2aa5cb81f0495c0ec54`.

Logs e prévia visual ficam em `verificacao/esp-radar-mapa/`. Backup do executável, perfil e fontes anteriores em `backup-esp-radar-mapa/`.
