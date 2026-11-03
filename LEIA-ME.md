Chaves online: veja CHAVES-ONLINE.md. O cliente atual valida pela chave no servidor do administrador; envie o novo Capri-Cliente.zip, que nao inclui banco nem chave salva.

Atualizacao de 31/10/2026: veja CORRECOES-CS2.md para a integracao atual, testes e limitacoes. O texto abaixo registra a versao anterior da interface.

# Capri — atualização da interface

Capri substitui a marca HillBlack. O avatar circular usa a imagem enviada; a paleta combina verde vivo, preto, azul e violeta.

## Alterações

- Menu ImGui com navegação lateral, área de conteúdo rolável e botões de ocultar/encerrar.
- Janela alterna entre interação e passagem de cliques; eventos de mouse/teclado chegam ao ImGui.
- Overlay acompanha a área cliente, libera o render target antes de redimensionar e se oculta quando o jogo perde o foco.
- Entrada e automação de mira/disparo ficam suspensas enquanto o menu está aberto.
- Encerramento acorda a thread de revalidação; acesso concorrente ao cache de entidades passa a usar mutex.
- Perfis restauram cores e temporizações antes omitidas; leitura valida valores e evita aplicar arquivos parcialmente corrompidos. Erros são exibidos no menu.
- Admin redesenhado com pesquisa, filtros, métricas, feedback e confirmação de revogação.
- Banco `hillblack.db`, credenciais existentes e diretório `configs` mantidos por compatibilidade. Não renomeie o banco.

## Limites conhecidos

O pedido de corrigir todas as opções não está integralmente concluído. O código original não contém implementação completa de Item ESP, inventário de utilitários, espectadores e tempo da bomba. Esses controles aparecem indisponíveis. A indicação de visibilidade continua sendo uma estimativa, não uma checagem de paredes.

A interface e os testes isolados não comprovam compatibilidade com a versão instalada do CS2. Offsets, recursos de leitura/escrita no jogo, alternância de recursos que exigem escrita após iniciar e integração online precisam de validação adicional. Nenhuma funcionalidade de anticheat/evasão foi desenvolvida nesta atualização.

## Uso

Abra `Capri.exe` na pasta que contém seu `hillblack.db`, `HillBlack.ini` e `configs`. Use PgUp para abrir/ocultar o menu, desde que seu perfil não configure outra tecla. Execute o CS2 em janela ou sem bordas. `Capri.Admin.exe` usa o banco ao lado do executável.

Os executáveis antigos devem ficar disponíveis como backup. Não execute os clientes antigo e novo simultaneamente.
