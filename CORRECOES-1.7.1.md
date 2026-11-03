# Capri 1.7.1 — build 20

- ESP > Estilo do wall: Corpo neon (padrao) ou Esqueleto. O estilo e as cores sao salvos no perfil. No modo Esqueleto, a espessura tambem pode ser ajustada.
- O ESP ignora o jogador que esta sendo observado. A leitura do alvo da camera funciona mesmo com a lista de espectadores desligada e consulta o estado atual ao trocar de jogador. Quando o alvo da camera nao pode ser confirmado, o preenchimento fica suspenso nesse quadro.
- Download sem limite de duracao total ou prazo artificial de leitura. A janela pode ser fechada para cancelar. Verificacao de hash, tamanho, versao, caminhos e rollback mantidas.
- A atualizacao de instalacoes existentes conserva os mapas ja instalados e envia o cliente, o atualizador e todas as malhas dos agentes. Instalacoes novas devem usar Capri-Cliente.zip.

Validacao: Release x64; testes de entidades (espectador sem lista, troca de alvo em snapshot antigo e respawn); perfis; oito testes do atualizador, incluindo transferencia real de 125 segundos, integridade, preservacao de chave/configuracao e rollback.

A validacao do espectador foi automatizada com fixtures locais. O resultado dentro de uma partida ainda precisa ser conferido.

Clientes antigos ainda executam o atualizador da propria pasta ate a primeira atualizacao bem-sucedida. O pacote de atualizacao menor facilita essa transicao; o pacote Corrigir-Atualizador.zip permite substituir somente o atualizador antigo e iniciar a instalacao da versao publicada.

Publicado: 1.7.1, build 20. Pacote automatico: 12,93 MiB; download HTTPS e registro no servidor conferidos. Compatibilidade com o atualizador antigo testada com o pacote real.
