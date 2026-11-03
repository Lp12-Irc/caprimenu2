# Capri 1.8.0 / build 25 e ADM 1.1.0 / build 4

Direcao visual: superficies grafite, destaque rosa, navegacao clara e espaco para as tarefas principais. O tema escolhido nos perfis existentes continua sendo respeitado.

## Menu do CS2

Navegacao lateral compacta, cabecalho por secao, barra fixa com perfil selecionado, versao, tecla de abertura, Salvar e Restaurar. Os avisos de salvamento aparecem na mesma barra. Os cards de ajustes acompanham seu conteudo para evitar rolagens internas desnecessarias. A tela de visuais prioriza corpo, estilo e cores, deixando tela/proporcao e detalhes da caixa recolhiveis. O arranjo dos controles se adapta a janelas menores. A tela de login recebeu as mesmas cores do admin.

## Administracao

A tabela de licencas ocupa o centro do painel. Nova licenca abre e recolhe a emissao sem apagar os campos preenchidos. Totais mais compactos, cabecalho com acao principal, navegacao reorganizada e cores/contraste revisados. Mais detalhes revela data de criacao e primeiro IP; as informacoes e o gerenciamento de IPs foram preservados. Tela de entrada e botoes seguem a mesma identidade.

## Verificacao

Builds C++ e C# aprovados. Testes completos de interface do ADM com banco isolado aprovados, incluindo emissao, filtros, IPs, revogacao, reativacao e credenciais. Capturas do ADM em 1240x820 e 1000x700, incluindo emissao aberta e login. Capturas do menu em 1360x800 e 800x600, paginas de ajustes, visuais, perfis, tema alternativo, bloqueio e atualizacao.

O teste do botao Salvar usa eventos de mouse no menu e um diretorio isolado: ESP, mira, radar, estilo do wall e valores sao restaurados apos a leitura do perfil. Testes de configuracao e perfis tambem passaram. FOV permaneceu estavel em 600 frames com eventos de teclado e o arraste do radar foi confirmado. Nao houve teste de gameplay numa partida real, nem alteracao de offsets ou da logica de licencas.

Arquivos principais: fontes/cs2_external/src/menu.cpp, menu.hpp e login.cpp; fontes/admin_panel/MainForm.cs e ThemeControls.cs. Versoes em version.hpp e AdminUpdate.cs. Testes de UI em fontes/tests/overlay_test.cpp e fontes/tests/admin_ui_smoke/Program.cs.

Backup anterior em backups/antes-visual-1.8.0. Capturas em verificacao/visual-1.1.0 e verificacao/visual-1.8.0-*.png. Os pacotes nao incluem banco nem credenciais e preservam os perfis instalados.

Publicado: Capri 1.8.0 e ADM 1.1.0. Downloads HTTPS, hashes e registros de publicacao conferidos.
