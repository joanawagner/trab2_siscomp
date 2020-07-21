# Trabalho 2 da disciplina de Sistemas Computacionas para Automação - UFSC Blumenau

## Tabela de Conteúdos 
- [Objetivos](#objetivos)
- [Desenvolvimento](#desenvolvimento)
- [Implementação](#implementação)

## Objetivos

- Implementação de um sistema de arquivos FAT simplificado.

## Desenvolvimento

 - O sistema de arquivos utiliza método de alocação de blocos encadeado (conforme
explicado em aula).
- O sistema de arquivos deve considerar setores de 512 bytes.
- A tabela de diretório deve armazenar uma lista de nomes de arquivos, tamanho em
bytes (0 em se tratando de diretório), ponteiro para o bloco inicial do arquivo ou
ponteiro para diretório.
- Os setores são numerados de 0 a n.
- O sistema de arquivos utiliza 4 bytes (32 bits) para numeração dos blocos.
Assim, são possíveis 2 32 blocos de 512 bytes cada, totalizando 2 terabytes de
espaço total suportado pelo sistema de arquivos.
- O sistema de arquivos utiliza mapeamento de blocos livres por encadeamento.
- O setor 0 contem o ponteiro para a lista de blocos livres.
- O diretório raíz ocupa o setor 1.

  Suporte para as segiuntes operações:
  
  - Inicializar
       exemplo: simulfs -format <tamanho em megabytes>
 
  - Criar (-create <arquivo original> <destino no sistema virtual>)
       exemplo: simulfs -create /home/user/classe.xls /<caminho>/alunos.xls
 
  - Ler (-read <arquivo no disco> <caminho no sistema virtual> )
       exemplo: simulfs -read /home/user/classe.xls <caminho>/alunos.xls 
 
  - Apagar
       exemplo: simulfs -del <caminho>/aluno.xls
 
  - Listar arquivos ou diretórios.
       exemplo: simulfs -ls <caminho>
            f paisagem.jpg    2048 bytes
            d viagem
            f lobo.jpg        5128 bytes 
 
  - Criar diretório
       exemplo: simulfs -mkdir <caminho>/aulas
        
  - Apagar diretório - Somente apaga se o diretório estiver vazio.
       exemplo: simulfs -rmdir <caminho>/aulas
  
  Implementação baseada no código disponível em: https://github.com/crmoratelli/fs_simul/
  
  
## Implemetação
  - O comando apagar não está funcionando corretamente.



