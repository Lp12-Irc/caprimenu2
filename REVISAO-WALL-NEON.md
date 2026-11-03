# Wall neon — Capri 1.7.0, build 19

Atualizacao de 02/11/2026.

- ESP com a malha real do agente preenchida, substituindo as linhas do esqueleto.
- Ciano neon como padrao; ajuste em ESP > Corpo neon preenchido > Cor neon do corpo.
- A cor e a ativacao do corpo ficam salvas no perfil. Perfis anteriores recebem o novo padrao.
- Inclui os 80 agentes encontrados na instalacao local, vinculados aos ossos por nome.

Validacao: compilacao Release x64; testes de entidades e perfis; renderizacao dos 80 agentes em pose de repouso e rotacionada, cores personalizadas, geometria finita e modelos ausentes. Previa: verificacao/neon-body-preview.png. Relatorio: verificacao/neon-models-test.txt.

A verificacao visual foi local, com poses de teste. O resultado dentro do CS2 ainda precisa ser conferido.

Origem: VPK local do CS2, build do motor 14194. O manifest.json registra a origem e os hashes. A compactacao conserva todos os triangulos e limita o erro das coordenadas a menos de 0,0025 unidade Source.

Backup anterior: backups/neon-before-build19. A publicacao verifica o download HTTPS antes de registrar o novo build.

Publicacao confirmada: 1.7.0, build 19; pacote de 96,96 MiB, download HTTPS e registro no servidor verificados.
