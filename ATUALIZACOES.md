# Atualizacao pelo proprio Capri

Versao inicial com instalacao automatica: **1.4.0 / build 2**.

Pacote atual: **1.4.6 / build 8**.

## No painel do administrador

1. Gere o novo `entregas/Capri-Cliente.zip` depois de compilar o build novo.
2. Abra Capri.Admin.exe, entre e clique em **Publicar atualizacao**.
3. Clique em **Selecionar Capri-Cliente.zip**. O painel confere o pacote e le build e versao automaticamente.
4. Escreva as modificacoes, clique em **Publicar e exigir atualizacao** e confirme.

O painel copia o ZIP para servidor/releases, no seu PC, e registra seu checksum e tamanho no banco central. O endereco de download e gerado a partir do servico de chaves; nao precisa digitar link. O pacote precisa conter somente o cliente, seu atualizador e arquivos de execucao. Pacotes com banco de dados, administrador, fontes ou chave salva sao recusados.

## No computador do usuario

O aviso aparece ao validar a chave na tela de entrada ou no menu durante o uso (verificacao a cada 30 segundos). Os recursos ficam bloqueados enquanto a versao estiver antiga.

Ao clicar em **Atualizar agora**, o Capri abre a janela de progresso, baixa e confere o ZIP, fecha a versao antiga, substitui arquivos modificados, adiciona arquivos novos e reabre. As preferencias em configs/default.hbc, a chave protegida em credentials.dat, a configuracao do servidor em HillBlack.ini e o banco existente sao preservados. Arquivos iguais nao sao regravados. A partir da versao 1.4.2 a chave e protegida para o usuario atual do Windows e nao e incluida nos perfis. Na primeira entrada com uma chave antiga, ela e migrada depois da validacao. Em outro PC, o usuario informa sua chave normalmente.

O atualizador valida o checksum do ZIP e do EXE, o build esperado e os caminhos dos arquivos. Faz copia dos arquivos anteriores para recuperacao se a instalacao falhar. Depois de um sucesso, limpa o download e os arquivos extraidos e mantem ate duas copias de recuperacao concluidas dentro de atualizacao. O log fica em atualizacao/atualizacao.log.

Quem ainda estiver numa versao anterior a este atualizador precisa extrair o novo ZIP uma vez. Depois disso as proximas modificacoes podem ser aplicadas pelo botao. Publicacoes antigas feitas apenas por link continuam com o download manual.

## Preparar uma proxima versao

Exemplo para uma publicacao futura:

```powershell
./fontes/build.ps1 -BuildNumber 9 -Version 1.4.7
# Feche o Capri antes de substituir o EXE.
Copy-Item fontes/dist/Capri.exe Capri.exe -Force
python ferramentas/empacotar_cliente.py
```

O empacotador grava os ZIPs em entregas e verifica o manifesto da compilacao. Se modificar qualquer arquivo do pacote de execucao, gere um build maior antes de publicar. Um pacote ja publicado e imutavel; pode ser publicado novamente com os mesmos bytes e descricao atualizada.

## Servico

Mantenha o PC e o servico ligados para validar as chaves e servir o pacote. Continua valendo a limitacao do tunel temporario: ao recria-lo, o endereco muda. Um dominio permanente evita precisar reenviar HillBlack.ini; consulte CHAVES-ONLINE.md.

Nenhuma atualizacao obrigatoria foi publicada automaticamente durante esta implementacao. Os testes de instalacao, recuperacao e publicacao foram feitos em pastas e bancos de teste dentro do projeto.
