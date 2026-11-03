# Copia preparada para GitHub

Os arquivos grandes foram divididos em partes de 90 MB. Depois de clonar, execute `python restaurar_arquivos.py` (Python 3.8 ou superior). O script verifica tamanho e SHA-256 antes de restaurar cada arquivo.

Backups, pacotes de distribuicao, compilacoes intermediarias e dados locais foram excluidos conforme .gitignore. A pasta original foi preservada.

Para publicar pelo GitHub Desktop, adicione esta pasta como repositorio local, crie o commit e escolha Publish repository. Revise a visibilidade antes de publicar.
