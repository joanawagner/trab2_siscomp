#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
 #include <libgen.h>
#include "libdisksimul.h"
#include "filesystem.h"

/**
 * @brief Format disk.
 * 
 */
int fs_format(){
	int ret, i;
	struct table_directory root_dir;
	struct sector_0 sector0;
	struct sector_data sector;
	
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 1)) != 0 ){
		return ret;
	}
	
	memset (&sector0, 0, sizeof(struct sector_0));
	
	/* first free sector. */
	sector0.free_sectors_list = 2;
	
	ds_write_sector(0, (void*)&sector0, SECTOR_SIZE);
	
	memset(&root_dir, 0, sizeof(root_dir));
	
	ds_write_sector(1, (void*)&root_dir, SECTOR_SIZE);
	
	/* Create a list of free sectors. */
	memset(&sector, 0, sizeof(sector));
	
	for(i=2;i<NUMBER_OF_SECTORS;i++){
		if(i<NUMBER_OF_SECTORS-1){
			sector.next_sector = i+1;
		}else{
			sector.next_sector = 0;
		}
		ds_write_sector(i, (void*)&sector, SECTOR_SIZE);
	}
	
	ds_stop();
	
	printf("Disk size %d kbytes, %d sectors.\n", (SECTOR_SIZE*NUMBER_OF_SECTORS)/1024, NUMBER_OF_SECTORS);
	
	return 0;
}

/**
 * @brief Create a new file on the simulated filesystem.
 * @param input_file Source file path.
 * @param simul_file Destination file path on the simulated file system.
 * @return 0 on success.
 */
int fs_create(char* input_file, char* simul_file){
	int ret, i, tamanho_arq, sector_dir;
	struct table_directory root_dir;
	struct sector_0 sector0;
	struct sector_data sector;
	
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}
	
	ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);	//Obtem os dados contidos na Sector 0
	ds_read_sector(0, &sector0,SECTOR_SIZE);		//Obtem os dados contidos na table de diretório raiz

	//Abrir o arquivo original
	FILE *arquivo = fopen(input_file, "rb");		//Abre o arquivo que está no disco rigido
	
	if(arquivo != NULL){					//Determina se o arquivo do disco existe
		fseek(arquivo, 0, SEEK_END); //move o ponteiro do arquivo para o final dele
		tamanho_arq = ftell(arquivo); //pega o tamanho do arquivo
		fseek(arquivo, 0, SEEK_SET); //move o ponteiro do arquivo de volta para o inicio
	} else {
		printf("Arquivo original não encontrado! \n");
		return 1;
	}
	char *nome = strdup(basename(simul_file));		//Pega o nome do arquivo que será criado no sistema simulado
	char *caminho_simul = strdup(dirname(simul_file));	//Pega o caminho do arquivo simulado
	char *limitador = "/";					//Determina o limitador entre pastas	
	char *str1 = malloc(sizeof(caminho_simul));		//Aloca memória para a str1 do tamanho do caminho_simul
	strcpy(str1, caminho_simul);				//Copia o caminho para a str1
	char *str2 = strtok(str1, "/");				//Separa o nome dos diretórios

	struct table_directory dir_atual;
	struct file_dir_entry *entrada;
	int dir_existe = 0;
	int arquivo_root = 0;
	entrada = root_dir.entries;
	
	//Procura diretório no sistema de arquivo simulado
	if(strcmp(caminho_simul, limitador) == 0){			//Determina se o arquivo será criado na pasta raíz
		arquivo_root = 1;
		ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);
		
	} 
	if(arquivo_root == 0) {						//Caso contrário inicia a busca pelos diretórios
		while(str2 != NULL){
			dir_existe = 0;	
			for(i = 0; i < 16; i++){					
				if(strcmp(str2, entrada[i].name) == 0 && entrada[i].dir == 1){	//compara o nome do diretório e verifica se é um diretório 
					sector_dir = entrada[i].sector_start;		//determina o setor de memório do próximo diretório
					entrada = dir_atual.entries;			//copia as entradas do diretório para 'entrada' para utilizar na procura
					dir_existe = 1;					//seta que o diretório atual existe
				}
			}
			ds_read_sector(sector_dir, (void*)&dir_atual, SECTOR_SIZE);	//lê a tabela do diretório atual
			str2 = strtok(NULL, limitador);
		}
		if(dir_existe == 0){						//caso não seja encontrado sai da função
			printf("Diretório de destino não encontrado!\n");
			return 1;
		}
	}
	
	//Determina se há um arquivo com o mesmo nome		
	for(i = 0; i < 16; i++){
		if(strcmp(nome, entrada[i].name) == 0 && entrada[i].dir == 0){
			printf("Um arquivo com o mesmo nome foi encontrado!\n");
			return 1;
		}
		if(entrada[i].sector_start == 0){
			break;
		}
		if(i == 16 - 1){
			printf("Diretório cheio!\n");
			return 1;
		}
	}

	//Ajusta parametros de entrada do arquivo	
	entrada[i].dir = 0;
	strcpy(entrada[i].name, nome);
	entrada[i].sector_start = sector0.free_sectors_list;
	entrada[i].size_bytes = tamanho_arq; 

	if(arquivo_root == 1){
		root_dir.entries[i] = entrada[i];
		ds_write_sector(1, (void*)&root_dir, SECTOR_SIZE);
	}else{
		dir_atual.entries[i] = entrada[i];
		ds_write_sector(sector_dir, (void*)&dir_atual, SECTOR_SIZE);
	}	
	
	//Inicia processo de escrita do arquivo nos setores vazios
	memset(&sector, sector0.free_sectors_list, sizeof(sector));
	int sector_prox = entrada[i].sector_start, aux;
	memset(sector.data, 0, sizeof(sector.data));
		
	int valor = tamanho_arq;

	while(valor > 0){	
		ds_read_sector(sector_prox, &sector,SECTOR_SIZE);		//lê o setor livre
		int data_amount = fread(sector.data, 1, 508, arquivo);		//lê o arquivo de entrada em porções de 508 bytes
		ds_write_sector(sector_prox, (void*)&sector, SECTOR_SIZE);	//escreve essa porção no setor disponivél

		aux = sector_prox;						//salva qual o próximo setor
		sector_prox = sector.next_sector;				//seta qual o próximo setor disponível
		valor = valor - data_amount;					//subtrai a quantidade de data restante do tamanho original do arquivo
	}
	
	sector0.free_sectors_list = sector_prox;				//seta o próximo setor livre
	sector.next_sector = 0;							//defini final dos setores do arquivo	
	ds_write_sector(aux, (void*)&sector, SECTOR_SIZE);			//salva modificações do último setor do arquivo
	ds_write_sector(1, (void*)&root_dir, SECTOR_SIZE);			//salva modificações da tabela raíz
	ds_write_sector(0, (void*)&sector0, SECTOR_SIZE);			//salva modificações sobre o sector0 e do ponteiro de setores livres
	fclose(arquivo); 

	ds_stop();
	
return 0;
}

/**
 * @brief Read file from the simulated filesystem.
 * @param output_file Output file path.
 * @param simul_file Source file path from the simulated file system.
 * @return 0 on success.
 */
int fs_read(char* output_file, char* simul_file){
	int ret, i, sector_dir;
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}
	
	struct table_directory root_dir;
	struct sector_data sector;
	
	ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);
	
	char *nome = strdup(basename(simul_file));
	char *caminho_simul = strdup(dirname(simul_file));
	char *limitador = "/";
	char *str1 = malloc(sizeof(caminho_simul));
	strcpy(str1, caminho_simul);
	char *str2 = strtok(str1, limitador);
	
	struct table_directory dir_atual;
	struct file_dir_entry *entrada;
	
	int dir_existe = 0;
	int arquivo_root = 0;
	entrada = root_dir.entries;

	//Informações do arquivo
	FILE *arquivo = fopen(output_file, "w");			//defini arquivo de saída para escrita
	
	//Procura diretório no sistema de arquivo simulado
	if(strcmp(caminho_simul, limitador) == 0){
		arquivo_root = 1;
		ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);
	} 
	if(arquivo_root == 0) {
		while(str2 != NULL){
			dir_existe = 0;	
			for(i = 0; i < 16; i++){					
				if(strcmp(str2, entrada[i].name) == 0 && entrada[i].dir == 1){
					sector_dir = entrada[i].sector_start;
					entrada = dir_atual.entries;
					dir_existe = 1;	
				}
			}
			ds_read_sector(sector_dir, (void*)&dir_atual, SECTOR_SIZE);
			str2 = strtok(NULL, limitador);
		}
		if(dir_existe == 0){
			printf("Diretório de destino não encontrado!\n");
			return 1;
		}
	}

	//Determina se o arquivo está dentro do diretório
	for(i = 0; i < 16; i++){
		if(strcmp(nome, entrada[i].name) == 0 && entrada[i].dir == 0){
			break;
		}
		if(i == 16 - 1){
			printf("Arquivo não encontrado!\n");
			return 1;
		}
	}
	//Inicia processo de escrita
	int sector_prox = entrada[i].sector_start;
	int valor = entrada[i].size_bytes, data_amount;
	
	while(valor > 0){	
		ds_read_sector(sector_prox, &sector,SECTOR_SIZE);	//lê o setor do arquivo da simulação
	
		if(valor >= 508){					//determina qual é o resto em relação a blocos de 508bytes
			data_amount = 508;				//se for maior, bloco de 508 bytes
		} else {
			data_amount = valor;				//se for menor, bloco do tamanho do que restou de bytes
		}
		fwrite(&sector.data, 1, data_amount, arquivo);		//escreve os dados no arquivo do disco	
		sector_prox = sector.next_sector;			//avança para o próximo setor
		valor = valor - data_amount;				//subtrai da variável do while
	}
	
	ds_write_sector(1, (void*)&root_dir, SECTOR_SIZE);		
	fclose(arquivo);
	
	ds_stop();
	
	return 0;
}

/**
 * @brief Delete file from file system.
 * @param simul_file Source file path.
 * @return 0 on success.
 */
int fs_del(char* simul_file){
	int ret, i, sector_dir, sector_number;
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}
	
	struct table_directory root_dir;
	struct sector_0 sector0;
	struct sector_data sector;
	
	ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);
	ds_read_sector(0, &sector0,SECTOR_SIZE);

	char *nome = strdup(basename(simul_file));
	char *caminho_simul = strdup(dirname(simul_file));
	char *limitador = "/";
	char *str1 = malloc(sizeof(caminho_simul));
	strcpy(str1, caminho_simul);
	char *str2 = strtok(str1, limitador);

	struct table_directory dir_atual;
	struct file_dir_entry *entrada;
	
	int dir_existe = 0;
	int arquivo_root = 0;
	entrada = root_dir.entries;

	//Procura diretório no sistema de arquivo simulado
	if(strcmp(caminho_simul, limitador) == 0){
		arquivo_root = 1;
		ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);
	} 
	if(arquivo_root == 0) {
		while(str2 != NULL){
			dir_existe = 0;	
			for(i = 0; i < 16; i++){					
				if(strcmp(str2, entrada[i].name) == 0 && entrada[i].dir == 1){
					sector_dir = entrada[i].sector_start;
					entrada = dir_atual.entries;
					dir_existe = 1;	
				}
			}
			ds_read_sector(sector_dir, (void*)&dir_atual, SECTOR_SIZE);
			str2 = strtok(NULL, limitador);
		}
		if(dir_existe == 0){
			printf("Diretório de destino não encontrado!\n");
			return 1;
		}
	}

	for(i = 0; i < 16; i++){
		if(strcmp(nome, entrada[i].name) == 0 && entrada[i].dir == 0){
			break;
		}
		if(i == 16 - 1){
			printf("Arquivo não encontrado!\n");
			return 1;
		}
	}

	int first_sector = entrada[i].sector_start;	//salva o setor de entrada
	sector_number = entrada[i].sector_start;	//inicia váriavel com o valor do setor de entrada
	
	while(sector.next_sector != 0){			//enquanto não encontrar o final do arquivo
		ds_read_sector(sector_number, (void*)&sector, SECTOR_SIZE);	//lê setor atual
		memset(&sector, 0, 508);					//escreve todos seus bits para zero
		ds_write_sector(sector_number, (void*)&sector, SECTOR_SIZE);	//salva alteração no setor
		sector_number = sector.next_sector;				//avança para o próximo setor
	}
	
	sector.next_sector = sector0.free_sectors_list;				//determinar que a lista de setores livres será o próximo setor		
	ds_write_sector(sector_number, (void*)&sector, SECTOR_SIZE);		//salva as alterações no último setor
	sector0.free_sectors_list = first_sector;				//defini que a lista de setores livres começa no setor de inicio do arquivo
	ds_write_sector(sector0.free_sectors_list, (void*)&sector, SECTOR_SIZE);//salva as alterações
	
	entrada[i].sector_start = 0;				//zerá o valor do setor inicial do arquivo
	memset(&entrada[i], 0, sizeof(entrada[i]));		//apaga o arquivo da tabela do diretório

	//Salva as alterações no diretório atual ou root
	if(arquivo_root == 0){
		ds_write_sector(sector_dir, (void*)&dir_atual, SECTOR_SIZE); 
	} else {
		ds_write_sector(1, (void*)&root_dir, SECTOR_SIZE);
	}
	
	ds_write_sector(0, (void*)&sector0, SECTOR_SIZE);
	printf("Arquivo deletado com sucesso!\n");
	
	ds_stop();
	
	return 0;
}

/**
 * @brief List files from a directory.
 * @param simul_file Source file path.
 * @return 0 on success.
 */
int fs_ls(char *dir_path){
	
	int ret, i, sector_dir;
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}
	
	struct table_directory root_dir;
	
	ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);

	char *nome = strdup(basename(dir_path));
	char *caminho_simul = dir_path;
	char *limitador = "/";
	char *str1 = malloc(sizeof(caminho_simul));
	strcpy(str1, caminho_simul);
	char *str2 = strtok(str1, limitador);

	struct table_directory dir_atual;
	struct file_dir_entry *entrada;
	
	int dir_existe = 0;
	int arquivo_root = 0;
	entrada = root_dir.entries;

	//Procura diretório no sistema de arquivo simulado
	if(strcmp(caminho_simul, limitador) == 0){
		arquivo_root = 1;
		ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);
	} 
	if(arquivo_root == 0) {
		while(str2 != NULL){
			dir_existe = 0;	
			for(i = 0; i < 16; i++){					
				if(strcmp(str2, entrada[i].name) == 0 && entrada[i].dir == 1){
					sector_dir = entrada[i].sector_start;
					entrada = dir_atual.entries;
					dir_existe = 1;	
				}
			}
			ds_read_sector(sector_dir, (void*)&dir_atual, SECTOR_SIZE);
			str2 = strtok(NULL, limitador);
		}
		if(dir_existe == 0){
			printf("Diretório de destino não encontrado!\n");
			return 1;
		}
	}

	printf("Lista de arquivos do diretório %s: \n", nome);

	int num_entradas = 0;	
	for(i = 0; i < 16; i++){
		if(entrada[i].dir == 0 && entrada[i].size_bytes != 0){
			num_entradas++;			
			printf("f      %s       %d bytes\n", entrada[i].name, entrada[i].size_bytes);
		}
		if(entrada[i].dir == 1 && entrada[i].size_bytes >= 0){
			num_entradas++;	
			printf("d      %s       %d bytes\n", entrada[i].name, entrada[i].size_bytes);
		}
	}

	if(num_entradas == 0){
		printf("Diretório vazio! \n");
	} else {
		printf("Total de entradas encontradas: %d \n", num_entradas);	
	}
	
	ds_stop();
	
	return 0;

}

/**
 * @brief Create a new directory on the simulated filesystem.
 * @param directory_path directory path.
 * @return 0 on success.
 */
int fs_mkdir(char* directory_path){
	int ret, i, sector_dir, sector_number;
	struct table_directory root_dir;
	struct sector_0 sector0;
	struct sector_data sector;
		
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}
	
	ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);
	ds_read_sector(0, (void*)&sector0, SECTOR_SIZE);
	
	char *nome = strdup(basename(directory_path));
	char *caminho_simul = strdup(dirname(directory_path));
	char *limitador = "/";
	char *str1 = malloc(sizeof(caminho_simul));
	strcpy(str1, caminho_simul);	
	char *str2 = strtok(str1, limitador);
	
	struct table_directory dir_atual, novo_dir; 
	struct file_dir_entry *entrada;
	
	int dir_existe = 0;
	int arquivo_root = 0;
	entrada = root_dir.entries;
	
	if(strcmp(nome, limitador) == 0){
		printf("Diretório root '/' já existente!\n");
		return 1;
	} 
	
	//Procura diretório no sistema de arquivo simulado
	if(strcmp(caminho_simul, limitador) == 0){
		arquivo_root = 1;
		ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);
	} 
	if(arquivo_root == 0) {
		while(str2 != NULL){
			dir_existe = 0;	
			for(i = 0; i < 16; i++){					
				if(strcmp(str2, entrada[i].name) == 0 && entrada[i].dir == 1){
					sector_dir = entrada[i].sector_start;
					entrada = dir_atual.entries;
					dir_existe = 1;	
				}
			}
			ds_read_sector(sector_dir, (void*)&dir_atual, SECTOR_SIZE);
			str2 = strtok(NULL, limitador);
		}
		if(dir_existe == 0){
			printf("Diretório de destino não encontrado!\n");
			return 1;
		}
	}
	
	//Determina se há um diretório com o mesmo nome	
	for(i = 0; i < 16; i++){	
		if(strcmp(nome, entrada[i].name) == 0 && entrada[i].dir == 1){
			printf("Um diretório com o mesmo nome foi encontrado!\n");
			return 1;
		}
		if(entrada[i].sector_start == 0){
			break;
		}
		if(i == 16 - 1){
			printf("Diretório raiz cheio!\n");
			return 1;
		}
	}

	//Ajusta parametros de entrada do arquivo	
	entrada[i].dir = 1;
	strcpy(entrada[i].name, nome);
	entrada[i].sector_start = sector0.free_sectors_list;
	entrada[i].size_bytes = 0; 	

	//Inícia processo de criação do diretório
	sector_number = sector0.free_sectors_list;

	ds_read_sector(entrada[i].sector_start, &sector,SECTOR_SIZE);  //lê o setor de entrada
		
	sector0.free_sectors_list = sector.next_sector;			//escreve a lista de setores livres como o próximo setor

	memset(&novo_dir, 0, sizeof(novo_dir));				//escreve zero na struct do novo diretório
	ds_write_sector(sector_number, (void*)&novo_dir, SECTOR_SIZE);		//salva a tabela do novo diretório

	//Determina se está na pasta raiz ou em outro diretório e salva as modificações
	if(arquivo_root == 1){
		root_dir.entries[i] = entrada[i];
	}else{
		dir_atual.entries[i] = entrada[i];
		ds_write_sector(sector_dir, (void*)&dir_atual, SECTOR_SIZE);
	}
	
	ds_write_sector(1, (void*)&root_dir, SECTOR_SIZE);
	ds_write_sector(0, (void*)&sector0, SECTOR_SIZE);
	
	printf("Diretório criado com sucesso! \n");

	ds_stop();
	
	return 0;
}

/**
 * @brief Remove directory from the simulated filesystem.
 * @param directory_path directory path.
 * @return 0 on success.
 */
int fs_rmdir(char *directory_path){
	int ret, i, sector_dir, sector_number, k;
	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}
	struct table_directory root_dir;
	struct sector_0 sector0;
	struct sector_data sector;

	ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);
	ds_read_sector(0, (void*)&sector0, SECTOR_SIZE);
	
	char *nome = strdup(basename(directory_path));
	char *caminho_simul = strdup(dirname(directory_path));
	char *limitador = "/";
	char *str1 = malloc(sizeof(caminho_simul));
	strcpy(str1, caminho_simul);	
	char *str2 = strtok(str1, limitador);
	
	struct table_directory dir_atual;
	struct file_dir_entry *entrada;
	
	int dir_existe = 0;
	int arquivo_root = 0;
	entrada = root_dir.entries;
		
	//Determina se o diretório é o root e proibi sua exclusão
	if(strcmp(caminho_simul, nome) == 0){
		printf("O Diretório '/' root não pode ser excluído!");
		return 1;
	} 
	
	//Procura diretório no sistema de arquivo simulado
	if(strcmp(caminho_simul, limitador) == 0){
		arquivo_root = 1;
		ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);
	} 
	if(arquivo_root == 0) {
		while(str2 != NULL){
			dir_existe = 0;	
			for(i = 0; i < 16; i++){					
				//printf("%s\n", entrada[i].name);				
				if(strcmp(str2, entrada[i].name) == 0 && entrada[i].dir == 1){
					sector_dir = entrada[i].sector_start;
					entrada = dir_atual.entries;
					dir_existe = 1;	
				}
			}
			ds_read_sector(sector_dir, (void*)&dir_atual, SECTOR_SIZE);
			str2 = strtok(NULL, limitador);
		}
		if(dir_existe == 0){
			printf("Diretório de destino não encontrado!\n");
			return 1;
		}
	}
	
	
	for(i = 0; i <16; i++){
		if(strcmp(nome, entrada[i].name) == 0 && entrada[i].dir == 1){
			break;
		}
		if(i == 16 - 1){
			printf("Diretório não encontrado!\n");
			return 1;
		}
	}
	
	int vazio = 1;
	struct file_dir_entry *entrada_dir;
	struct table_directory dir_ex;

	int sector_ex = entrada[i].sector_start;	//salva o setor inicial do diretório a ser apagado
	entrada_dir = dir_ex.entries;			//determina suas entradas

	ds_read_sector(sector_ex, (void*)&dir_ex, SECTOR_SIZE);		//lê as informações do diretório a ser excluído
	
	//Determina se o diretório a ser excluído está vazio
	for(k=0; k < 16; k++){			
		if(entrada_dir[k].sector_start != 0){
			vazio = 0;
		}
	}

	//Se estiver vazio inicia processo de exclusão caso contrário sai da função
	if(vazio == 1){
		int first_sector = entrada[i].sector_start;		//salva o setor inicial
		sector_number = entrada[i].sector_start;
	
		while(sector.next_sector != 0){		
			ds_read_sector(sector_number, (void*)&sector, SECTOR_SIZE);	//lê o setor	
			memset(&sector, 0, 508);					//escreve 0 para todas as suas posições
			ds_write_sector(sector_number, (void*)&sector, SECTOR_SIZE);	//salva alterações	
			sector_number = sector.next_sector;				//estabelece próximo setor
		}
		
		sector.next_sector = sector0.free_sectors_list;				//aponta o próximo setor como setor livre
		
		ds_write_sector(sector_number, (void*)&sector, SECTOR_SIZE);		//salva alteraçoes
		sector0.free_sectors_list = first_sector;				//aponta a lista de setores livres como o setor inicial do diretório
		ds_write_sector(sector0.free_sectors_list, (void*)&sector, SECTOR_SIZE);	//salva alterações
		
		entrada[i].sector_start = 0;			//zera setor inicial	
		memset(&entrada[i], 0, sizeof(entrada[i]));	//zera a struct do diretório

		if(arquivo_root == 0){
			ds_write_sector(sector_dir, (void*)&dir_atual, SECTOR_SIZE);
		} else {
			ds_write_sector(1, (void*)&root_dir, SECTOR_SIZE);
		}
	
		printf("Diretório excluído com sucesso!\n");

	}else{
		printf("Diretório não está vazio! Exclua os arquivos contidos em '%s' para prosseguir com a ação.\n", nome );
		return 1;
	}
		
	ds_write_sector(0, (void*)&sector0, SECTOR_SIZE);	//salva informação do próximo setor livre

	ds_stop();
	
	return 0;
}

/**
 * @brief Generate a map of used/available sectors. 
 * @param log_f Log file with the sector map.
 * @return 0 on success.
 */
int fs_free_map(char *log_f){
	int ret, i, next;
	//struct root_table_directory root_dir;
	struct sector_0 sector0;
	struct sector_data sector;
	char *sector_array;
	FILE* log;
	int pid, status;
	int free_space = 0;
	char* exec_params[] = {"gnuplot", "sector_map.gnuplot" , NULL};

	if ( (ret = ds_init(FILENAME, SECTOR_SIZE, NUMBER_OF_SECTORS, 0)) != 0 ){
		return ret;
	}
	
	/* each byte represents a sector. */
	sector_array = (char*)malloc(NUMBER_OF_SECTORS);

	/* set 0 to all sectors. Zero means that the sector is used. */
	memset(sector_array, 0, NUMBER_OF_SECTORS);
	
	/* Read the sector 0 to get the free blocks list. */
	ds_read_sector(0, (void*)&sector0, SECTOR_SIZE);
	
	next = sector0.free_sectors_list;

	while(next){
		/* The sector is in the free list, mark with 1. */
		sector_array[next] = 1;
		
		/* move to the next free sector. */
		ds_read_sector(next, (void*)&sector, SECTOR_SIZE);
		
		next = sector.next_sector;
		
		free_space += SECTOR_SIZE;
	}

	/* Create a log file. */
	if( (log = fopen(log_f, "w")) == NULL){
		perror("fopen()");
		free(sector_array);
		ds_stop();
		return 1;
	}
	
	/* Write the the sector map to the log file. */
	for(i=0;i<NUMBER_OF_SECTORS;i++){
		if(i%32==0) fprintf(log, "%s", "\n");
		fprintf(log, " %d", sector_array[i]);
	}
	
	fclose(log);
	
	/* Execute gnuplot to generate the sector's free map. */
	pid = fork();
	if(pid==0){
		execvp("gnuplot", exec_params);
	}
	/* Wait gnuplot to finish */
	wait(&status);
	
	free(sector_array);
	
	ds_stop();
	
	printf("Free space %d kbytes.\n", free_space/1024);
	
	return 0;
}

