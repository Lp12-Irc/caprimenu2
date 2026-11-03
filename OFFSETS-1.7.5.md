# Capri 1.7.5 — build 24

Offsets atualizados para CS2 engine build 14195, Steam build 26393695.

client.dll SHA256: 57ee728e15c50881070d847d7516d632d70456e0648d486a53f2528ec53f6ee0.

Os valores vieram de uma extracao local do processo aberto com cs2-dumper, seguida de verificacao independente de seis globais por assinaturas no binario instalado. O executavel local do dumper utilizado tem SHA256 971466e7d9f4fafdf7d5f4e216e88943578d03f58389745e44effe22a04e2ed8. As assinaturas consultadas estao no repositorio a2x/cs2-dumper, commit 21ee85107074fdf0347dc8afd4a8c3dd8d2398e2. Nenhum offset novo foi inferido por soma dos deslocamentos antigos.

Mudaram os enderecos da lista de entidades, controlador local, matriz da camera, angulos de visao e sensibilidade. Os campos de schema consumidos pelo cliente permaneceram nos mesmos deslocamentos. O controle de compatibilidade agora reconhece o numero e a assinatura PE da build atual, mantendo a rejeicao de builds diferentes.

O relogio foi confirmado pelos acessos das rotinas de C4 no binario atual. A lista de entidades, stride, handles, estrutura do modelo e os nomes dos 94 ossos do agente do menu foram verificados com leitura do processo. Os arquivos de geometria neon foram extraidos novamente do VPK instalado; o cache de exportacao agora e separado por build e hash do indice VPK, para evitar reaproveitar exportacoes de outra versao.

Validacao local: ferramentas/verify_live_offsets.py, usando os valores efetivos de generated_offsets.hpp. Relatorio em verificacao/live-offsets-14195.json. Diferencas em verificacao/offsets-build24-diff.json. A verificacao ocorreu no menu do jogo; nao equivale a testar uma partida completa.

As ferramentas de geracao agora leem o build ID instalado e validam os hashes dos arquivos extraidos, em vez de manter identificadores fixos da versao anterior. Configuracoes, perfis, credenciais e a tolerancia de conexao da 1.7.4 foram mantidos.

Backup anterior em backups/before-build24.

Testes aprovados: entities, config, profile_store e neon_models (80 agentes). As geometrias reexportadas coincidiram com os hashes anteriores; o manifesto foi atualizado para a build atual.

Publicado e conferido: 1.7.5, build 24. Pacote HTTPS e politica no servidor verificados.
