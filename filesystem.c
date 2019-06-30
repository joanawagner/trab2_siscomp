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
	
	ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);	
	ds_read_sector(0, &sector0,SECTOR_SIZE);	

	//Abrir o arquivo original
	FILE *arquivo = fopen(input_file, "rb");
	char *nome;
	if(arquivo != NULL){
		fseek(arquivo, 0, SEEK_END); //move o ponteiro do arquivo para o final dele
		tamanho_arq = ftell(arquivo); //pega o tamanho do arquivo
		fseek(arquivo, 0, SEEK_SET); //move o ponteiro do arquivo de volta para o inicio

		nome = strdup(basename(simul_file));

	} else {
		printf("Arquivo original não encontrado! \n");
		return 1;
	}

	char *caminho_simul = strdup(dirname(simul_file));
	char *limitador = "/";
	char *str1 = malloc(sizeof(caminho_simul));
	strcpy(str1, caminho_simul);	
	char *str2 = strtok(str1, "/");

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
	ds_write_sector(aux, (void*)&sector, SECTOR_SIZE);			
	ds_write_sector(1, (void*)&root_dir, SECTOR_SIZE);
	ds_write_sector(0, (void*)&sector0, SECTOR_SIZE);
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
	FILE *arquivo = fopen(output_file, "w");
	
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

	for(i = 0; i < 16; i++){
		if(strcmp(nome, entrada[i].name) == 0 && entrada[i].dir == 0){
			break;
		}

		if(i == 16 - 1){
			printf("Arquivo não encontrado!\n");
			return 1;
		}

	}
	
	int sector_prox = entrada[i].sector_start;
	int valor = entrada[i].size_bytes, data_amount;
	
	while(valor > 0){	
		ds_read_sector(sector_prox, &sector,SECTOR_SIZE);	
	
		if(valor >= 508){
			data_amount = 508;	
		} else {
			data_amount = valor;
		}
		fwrite(&sector.data, 1, data_amount, arquivo);	
		sector_prox = sector.next_sector;
		valor = valor - data_amount;
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
	//printf();
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

	for(i = 0; i < 16; i++){
		if(strcmp(nome, entrada[i].name) == 0 && entrada[i].dir == 0){
			break;
		}

		if(i == 16 - 1){
			printf("Arquivo não encontrado!\n");
			return 1;
		}

	}

	ds_read_sector(entrada[i].sector_start, (void*)&sector, SECTOR_SIZE);
	if(sector.next_sector != 0){
		sector_number = sector.next_sector;
		ds_read_sector(sector.next_sector, (void*)&sector, SECTOR_SIZE);
	}

	
	sector.next_sector = sector0.free_sectors_list;
	
	sector0.free_sectors_list = entrada[i].sector_start;
	
	entrada[i].dir = 0;
	memset(entrada[i].name, 0, strlen(entrada[i].name));
	entrada[i].size_bytes = 0;
	entrada[i].sector_start = 0;	

	if(arquivo_root == 0){
		ds_write_sector(sector_dir, (void*)&dir_atual, SECTOR_SIZE);
	}
	ds_write_sector(sector_number, (void*)&sector, SECTOR_SIZE);
	ds_write_sector(1, (void*)&root_dir, SECTOR_SIZE);
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
	//struct table_directory *novo_dir = malloc(sizeof (struct table_directory));
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

	ds_read_sector(entrada[i].sector_start, &sector,SECTOR_SIZE);
		
	sector0.free_sectors_list = sector.next_sector;

	memset(&novo_dir, 0, sizeof(novo_dir));	
	ds_write_sector(sector_number, (void*)&novo_dir, SECTOR_SIZE);

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
	char *caminho_simul = directory_path;
	char *limitador = "/";
	char *str1 = malloc(sizeof(caminho_simul));
	strcpy(str1, caminho_simul);	
	char *str2 = strtok(str1, limitador);
	
	
	struct table_directory dir_ex;
	struct file_dir_entry *entrada;
	
	int dir_existe = 0;
	int arquivo_root = 0;
	entrada = root_dir.entries;
		
	//Procura diretório no sistema de arquivo simulado
	if(strcmp(caminho_simul, limitador) == 0){
		printf("O Diretório '/' root não pode ser excluído!");
		return 1;
	} 
	
	while(str2 != NULL){
		dir_existe = 0;		
		for(i = 0; i < 16; i++){					
			if(strcmp(str2, entrada[i].name) == 0 && entrada[i].dir == 1){
				sector_dir = entrada[i].sector_start;
				entrada = dir_ex.entries;
				dir_existe = 1;	
			}
		}
		ds_read_sector(sector_dir, (void*)&dir_ex, SECTOR_SIZE);
		str2 = strtok(NULL, limitador);
	}
		if(dir_existe == 0){
			printf("Diretório de destino não encontrado!\n");
			return 1;
		}
	
	
	//Determina se o diretório se encontra na pasta '/'
	
	char *caminho_simul2 = strdup(dirname(directory_path));
	char *str3 = malloc(sizeof(caminho_simul2));
	strcpy(str3, caminho_simul2);	
	char *str4 = strtok(str3, limitador);
	int j;	

	struct table_directory dir_atual;
	struct file_dir_entry *entrada2;

	entrada2 = root_dir.entries;

	if(strcmp(caminho_simul2, limitador) == 0){
		arquivo_root = 1;
		ds_read_sector(1, (void*)&root_dir, SECTOR_SIZE);
	} 

	if(arquivo_root == 0) {
		while(str4 != NULL){
			for(j = 0; j < 16; j++){					
				if(strcmp(str2, entrada2[j].name) == 0 && entrada2[j].dir == 1){
					sector_number = entrada2[j].sector_start;
					entrada2 = dir_atual.entries;
				}
			}
			ds_read_sector(sector_number, (void*)&dir_atual, SECTOR_SIZE);
			str4 = strtok(NULL, limitador);
		}
	}
	
	for(j = 0; j <16; j++){
		if(strcmp(nome, entrada[j].name) == 0 && entrada[j].dir == 1){
			break;
		}
	}
	
	int vazio = 1;
	for(k=0; k < 16; k++){			
		if(dir_ex.entries[k].sector_start != 0){
			vazio = 0;
		}
	}

	ds_read_sector(entrada[i].sector_start,(void*)&dir_ex, SECTOR_SIZE);

	if(vazio == 1){
		memset(&sector, 0, SECTOR_SIZE);
		
		sector.next_sector = sector0.free_sectors_list;
		ds_write_sector(entrada2[j].sector_start,(void*)&sector, SECTOR_SIZE);
				
		sector0.free_sectors_list = entrada[i].sector_start;
		entrada2[j].sector_start = 0;

		if(arquivo_root == 1){
			root_dir.entries[j] = entrada2[j];
			ds_write_sector(1, (void*)&root_dir, SECTOR_SIZE);
		}else{
			dir_atual.entries[j] = entrada2[j];
			ds_write_sector(sector_number, (void*)&dir_atual, SECTOR_SIZE);
		}
	
		printf("Diretório excluído com sucesso!\n");

	}else{
		printf("Diretório não está vazio! Exclua os arquivos contidos em '%s' para prosseguir com a ação.\n", nome );
		return 1;
	}
	
	
	ds_write_sector(0, (void*)&sector0, SECTOR_SIZE);

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

