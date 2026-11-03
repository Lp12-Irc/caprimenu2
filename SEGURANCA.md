# Protecao aplicada ao Capri

## Cliente

A chave salva fica em credentials.dat usando Windows DPAPI, vinculada ao usuario atual do Windows. O arquivo nao e distribuido nos ZIPs. Perfis novos nao guardam a chave; perfis antigos sao migrados apos uma validacao bem-sucedida. A entrada oculta a chave por padrao e permite mostrar/ocultar explicitamente.

A validacao usa HTTPS com verificacao dos certificados do Windows, sem ignorar erros de certificado nem seguir redirecionamentos. HTTP e aceito somente no loopback para testes locais. O atualizador confere SHA-256 e tamanho do pacote recebido do servico HTTPS, manifesto e hash do executavel; preserva o arquivo de credenciais.

## Administrador

Nao existe senha administrativa embutida para novas instalacoes. O primeiro acesso cria uma conta escolhida pelo administrador. Senhas novas e alteradas usam bcrypt com custo 12. A troca de senha exige a senha atual; a interface exige confirmacao da nova senha. A verificacao roda fora da thread da interface. Apos cinco tentativas invalidas, a tela faz uma pausa de 30 segundos, limitada a essa instancia do aplicativo.

Contas existentes foram preservadas. Use Alterar senha para substituir qualquer senha padrao antiga. Nenhuma senha do banco real foi alterada durante a implementacao.

A opção Lembrar login neste PC guarda usuário e senha em admin-login.dat, protegidos com Windows DPAPI para o usuário atual e vinculados ao caminho do banco. Os campos são preenchidos ao reabrir, mas o banco continua validando a senha ao entrar. Desmarcar remove o arquivo salvo; a troca de senha atualiza o acesso lembrado. Esse arquivo não acompanha os ZIPs.

## Limites reais

Essas medidas protegem credenciais e transporte. Nao foi implementada criptografia ponta a ponta entre clientes: o servidor precisa ler a chave para valida-la. Os executaveis nao foram empacotados ou criptografados. O banco SQLite central continua no formato existente, com licencas legiveis por quem tem acesso ao arquivo; sua criptografia integral exigiria mudar tambem o servidor e o gerenciamento das chaves de criptografia. O checksum do pacote nao substitui assinatura digital do executavel; nao foi disponibilizado certificado de assinatura.

O dominio do tunel permanece temporario. O PC, o banco e as copias de seguranca precisam ficar sob controle do administrador.
