# Perfis — Capri 1.7.3, build 22

Salvar perfil grava imediatamente, inclusive substituindo um perfil existente. O ultimo perfil salvo ou carregado e lembrado para a proxima abertura. Selecionar um perfil na lista tambem o carrega. Cores, estilo do wall, teclas, valores e opcoes ativadas sao restaurados; os recursos so podem executar depois da validacao do acesso.

O login e a preparacao da atualizacao deixaram de sobrescrever configs/default.hbc. A chave continua protegida separadamente pelo Windows. Bloqueios temporarios de acesso desligam os recursos em memoria e nao alteram o arquivo salvo.

A escolha fica em configs/active-profile.txt. Esse arquivo e os perfis nomeados existentes sao preservados pelo atualizador. Pacotes novos recebem os recursos inicialmente desligados; os perfis ja existentes do usuario sao mantidos.

Validacao: testes de configuracao e persistencia, incluindo reinicio, dois perfis, preservacao da sessao, marcador sem permissao, arquivo corrompido, gravacao invalida, nomes indevidos e perfil removido. O teste do atualizador confirmou a preservacao do perfil nomeado e da escolha ativa.

Backup dos perfis anteriores: backups/before-build22/configs.

Publicado e conferido: 1.7.3, build 22. Download HTTPS, pacote real e registro no servidor verificados.
