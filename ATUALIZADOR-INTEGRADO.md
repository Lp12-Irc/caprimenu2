# Atualizador integrado — Capri 1.7.2, build 21

Quando houver uma nova versao, clique em Atualizar agora dentro do Capri. O atualizador mostra o progresso, instala e reabre o cliente. Nao e necessario abrir terminal, executar comandos ou procurar scripts.

O fluxo desta versao usa assets/updater/Capri.Updater.exe diretamente, copiado para o cache de atualizacao antes de iniciar. Isso permite atualizar o proprio componente enquanto ele esta em execucao. O atualizador antigo e mantido no pacote apenas para a transicao das versoes anteriores.

O download continua sem limite total de duracao. A janela permite cancelar. O instalador preserva chave, credenciais, configuracoes e mapas existentes; verifica tamanho, SHA-256, versao, caminhos do ZIP e a integridade final. Falhas durante a substituicao restauram os arquivos anteriores.

Validacao: nove testes do atualizador executavel, incluindo download de 125 segundos, reabertura do cliente, troca do proprio componente, preservacao de configuracoes, hashes invalidos, pacote incorreto, arquivos proibidos e rollback.

Publicacao confirmada: 1.7.2, build 21. Pacote real testado tanto com o instalador antigo quanto com o executavel integrado; download HTTPS e registro no servidor verificados. Os testes visuais da tela de espera foram pulados porque o CS2 estava aberto.
