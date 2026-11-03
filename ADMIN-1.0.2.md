# Administração 1.0.2 — build 3

A validade em dias começa na emissão: um dia corresponde a 24 horas, não ao fim do dia do calendário. Licenças vitalícias não têm vencimento. O servidor compara o instante UTC armazenado no banco em cada validação e rejeita uma licença quando seu prazo é menor ou igual ao instante atual, independentemente de o painel estar aberto.

Foi corrigida a validação local Database.BindHwid: DateTime.Parse podia converter uma data com fuso para horário local e compará-la incorretamente com UTC. KeyExpiry usa DateTimeOffset, normaliza fusos, considera datas legadas sem fuso como UTC e rejeita datas inválidas. O status exibido no painel usa a mesma regra. O caminho online do servidor já comparava corretamente os horários.

A interface agora exibe data e hora do vencimento, tempo restante, resumo da licença selecionada e previsão do prazo antes de emitir. As cores foram suavizadas, o contraste da tabela foi ajustado e a hierarquia de títulos, totais e emissão foi revisada. Licenças expiradas não oferecem reativação. As datas exibidas seguem o fuso do PC que abre o painel.

Limite do cliente atual: validações periódicas ocorrem a cada dois segundos; durante falhas de rede, uma autorização anterior pode durar até 60 segundos. Este trabalho não altera essa tolerância nem promete interrupção instantânea do cliente no exato segundo do vencimento.

Verificação: 17 testes Python do servidor aprovados, incluindo licença ativa que vence durante a sessão com timestamp UTC−3. Testes C# da interface aprovados, incluindo instante exato do vencimento, UTC−3, data legada, data inválida, vitalícia e validação local de chave futura. Testes de criação, filtros, revogação, reativação, IPs e credenciais também passaram. Capturas da interface desktop em verificacao/admin-1.0.2, usando banco isolado e dados fictícios.

Fontes principais alteradas: fontes/admin_panel/KeyExpiry.cs, Database.cs, MainForm.cs, ThemeControls.cs e AdminUpdate.cs. Backup do painel anterior em backups/admin-expiracao-20261103.
