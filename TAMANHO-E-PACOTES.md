# Pacotes leves com autenticacao online

Capri.exe e os mapas em assets compoem o cliente. A configuracao HillBlack.ini aponta para o servico de chaves no PC do administrador. O banco hillblack.db permanece apenas nesse PC; nao faz parte dos pacotes. A chave salva e removida da copia do perfil incluida no ZIP.

1. Execute Iniciar-Servidor-Chaves.cmd no seu PC.
2. Gere o pacote: `python ferramentas/empacotar_cliente.py`.
3. Envie entregas/Capri-Cliente.zip. O amigo extrai tudo e informa a propria chave.

Para incluir o EXE do administrador: `python ferramentas/empacotar_cliente.py --incluir-admin`. O banco local continua excluido.

Os ZIPs ficam em entregas, dentro da pasta Capri-Completo. Caches, ferramentas e backups ficam separados conforme verificacao/limpeza-tamanho.json. Veja CHAVES-ONLINE.md para iniciar o servidor e atualizar o endereco temporario apos reiniciar o tunel.
