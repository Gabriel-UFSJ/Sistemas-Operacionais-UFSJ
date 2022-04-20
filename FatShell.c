#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#define CLUSTER_SIZE 1024
#define FAT_SIZE 4096
#define TAM 1000

typedef struct
{
	uint8_t filename[18];
	uint8_t attributes;
	uint8_t reserved[7];
	uint16_t first_block;
	uint32_t size;
} dir_entry_t;

union data_cluster{
dir_entry_t dir[CLUSTER_SIZE / sizeof(dir_entry_t)];
uint8_t data[CLUSTER_SIZE];
} data_cluster;

uint16_t fat[FAT_SIZE];

union data_cluster readCluster(int index)
{
	FILE *arq;

	arq = fopen("fat.part", "rb");
	union data_cluster cluster;

	if (!arq)
	{
		printf("Nao foi possivel abrir o arquivo\n");
		return cluster;
	}

	fseek(arq, (CLUSTER_SIZE * index), SEEK_SET);
	fread(&cluster, sizeof(union data_cluster), 1, arq);

	fclose(arq);

	return cluster;
}

void writeCluster(int index, union data_cluster *cluster)
{
	FILE *arq;

	arq = fopen("fat.part", "rb+");

	if (!arq)
	{
		printf("Nao foi possivel abrir o arquivo\n");
		return;
	}

	fseek(arq, (index * CLUSTER_SIZE), SEEK_SET);
	fwrite(cluster, CLUSTER_SIZE, 1, arq);

	fclose(arq);
}

int FreeSpace()
{
	for (int i = 0; i < FAT_SIZE; i++)
	{
		if (fat[i] == 0)
		{
			return i;
		}
	}

	return -1;
}

void writeFat()
{
	FILE *arq;

	arq = fopen("fat.part", "rb+");

	if (!arq)
	{
		printf("Nao foi possivel abrir o arquivo\n");
		return;
	}

	fseek(arq, (CLUSTER_SIZE), SEEK_SET);
	fwrite(&fat, FAT_SIZE, 1, arq);

	fclose(arq);
}

void slice_str(char *str, char *buffer, int inicio, int fim)
{
	int j = 0;
	for (int i = inicio; i < fim; i++)
	{
		buffer[j++] = str[i];
	}
	buffer[j] = 0;
}

void resize(char* diretorio, size_t extend_size)
{
	char copia[strlen(diretorio)];
	strcpy(copia, diretorio);

	char *cpy = malloc(strlen(diretorio) * sizeof(char));
	strcpy(cpy, diretorio);

	char *token;
	token = strtok(cpy, "/"); // pega o primeiro elemento apos root

	int index_block_fat = 0;

	union data_cluster block;

	if (diretorio[0] == '/')
	{
		index_block_fat = 9;
		block = readCluster(9);
	}
	else
	{
		printf("Caminho não possui diretório\n");
		return;
	}

	int count = 0;

	// conta quantos direitorios há na string
	while (token != NULL)
	{
		token = strtok(NULL, "/");
		count++;
	}

	token = strtok(copia, "/");

	// caminha nos diretórios até chegar no penúltimo,
	//  pois o último é o arquivo que deve ser utilizado
	while (count > 1)
	{
		int i;
		int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
		int found_dir = 0;

		// procura o diretorio atual no anterior
		for (i = 0; i < size_dir; i++)
		{

			if (strcmp(block.dir[i].filename, token) == 0)
			{
				found_dir = 1;
				if (block.dir[i].attributes == 1)
				{

					block.dir[i].size += extend_size;
					writeCluster(index_block_fat, &block);
					block = readCluster(block.dir[i].first_block);
					index_block_fat = block.dir[i].first_block;
				}
				else
				{
					printf("%s não é um diretório!\n", token);
				}
				break;
			}
		}

		if (!found_dir)
		{
			printf("Não existe este diretório %s\n", token);
			return;
		}

		token = strtok(NULL, "/");
		count--;
	}
}

//**********************************//
//			FUNÇÕES SHELL			//
//**********************************//
void init()
{

	FILE *arq;

	arq = fopen("fat.part", "wb");

	if (!arq)
	{
		printf("Nao foi possivel abrir o arquivo\n");
		return;
	}

	union data_cluster boot_block;

	for (int i = 0; i < CLUSTER_SIZE; i++)
	{
		boot_block.data[i] = 0xbb;
	}

	// alocar endereço na fat para boot block
	fat[0] = 0xfffd;

	// alocar endereço fat para a propria fat
	for (int i = 1; i <= 8; i++)
	{
		fat[i] = 0xfffe;
	}

	// alocar endereço na fat para root block
	fat[9] = 0xffff;

	// para o restante dos endereços na fat
	for (int i = 10; i < FAT_SIZE; i++)
	{
		fat[i] = 0x0000;
	}

	union data_cluster clusters;

	fwrite(&boot_block, CLUSTER_SIZE, 1, arq);
	fwrite(&fat, sizeof(fat), 1, arq);

	// salva no arquivos cluster root + outros clusters
	for (int i = 0; i < 4086; i++)
	{
		memset(&clusters, 0x0000, CLUSTER_SIZE);
		memset(&clusters.data, 0x0000, CLUSTER_SIZE);
		fwrite(&clusters, CLUSTER_SIZE, 1, arq);
	}
	fclose(arq);
}

void load()
{
	FILE *arq;

	arq = fopen("fat.part", "rb");

	if (!arq)
	{
		printf("Impossivel abrir o arquivo!\n");
		return;
	}

	union data_cluster boot_block;

	// carrega o boot_block para a memoria
	fread(&boot_block, CLUSTER_SIZE, 1, arq);
	int i;

	for (i = 0; i < CLUSTER_SIZE; i++)
	{
		if (boot_block.data[i] != 0xbb)
		{
			printf("Problemas no endereços do boot_block\n");
			return;
		}
	}
	// carrega a fat para a memoria
	fread(&fat, sizeof(fat), 1, arq);

	fclose(arq);
}

void ls(char *diretorio)
{
	char *token;

	char *cpy = malloc(strlen(diretorio) * sizeof(char));
	strcpy(cpy, diretorio);

	token = strtok(cpy, "/"); // pega o primeiro elemento apos root

	union data_cluster block;

	if (diretorio[0] == '/')
	{
		block = readCluster(9);
	}
	else
	{
		printf("Caminho não possui diretório raiz\n");
		return;
	}

	// procura o diretorio atual no anterior, se encontrar
	// então pode-se procurar o proximo diretorio neste de agora
	// acontece isso até chegar no último diretório que no final
	// teremos o os diretorio / arquivos deste diretório.

	while (token != NULL)
	{
		int i;
		int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
		int found_dir = 0;

		// procura o diretorio atual no anterior
		for (i = 0; i < size_dir; i++)
		{

			if (strcmp(block.dir[i].filename, token) == 0)
			{
				found_dir = 1;
				if (block.dir[i].attributes == 1)
				{
					block = readCluster(block.dir[i].first_block);
				}
				else
				{
					printf("%s não é um diretório!\n", token);
				}
				break;
			}
		}
		if (!found_dir)
		{
			printf("Não existe este diretório %s\n", token);
		}

		token = strtok(NULL, "/");
	}

	// block possui agora o cluster do último diretório

	int i;
	int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
	printf("\n");
	for (i = 0; i < size_dir; i++)
	{
		if (block.dir[i].first_block != 0)
		{
			printf("%s \n", block.dir[i].filename);
		}
	}
	free(cpy);
}

void mkdir(char *diretorio)
{
	char copia[strlen(diretorio)];
	strcpy(copia, diretorio);

	char *cpy = malloc(strlen(diretorio) * sizeof(char));
	strcpy(cpy, diretorio);

	char *token;
	token = strtok(cpy, "/"); // pega o primeiro elemento apos root

	int index_block_fat = 0;

	union data_cluster block;

	if (diretorio[0] == '/')
	{
		index_block_fat = 9;
		block = readCluster(9);
	}
	else
	{
		printf("Caminho não possui diretório root!\n");
		return;
	}

	int count = 0;

	// conta quantos direitorios há na string
	while (token != NULL)
	{
		token = strtok(NULL, "/");
		count++;
	}

	token = strtok(copia, "/");

	// caminha nos diretórios até chegar no penúltimo,
	//  pois o último é o arquivo que deve ser criado
	while (count > 1)
	{
		;
		int i;
		int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
		int found_dir = 0;

		// procura o diretorio atual no anterior
		for (i = 0; i < size_dir; i++)
		{

			if (strcmp(block.dir[i].filename, token) == 0)
			{
				found_dir = 1;
				if (block.dir[i].attributes == 1)
				{
					index_block_fat = block.dir[i].first_block;
					block = readCluster(block.dir[i].first_block);
				}
				else
				{
					printf("%s não é um diretório!\n", token);
				}
				break;
			}
		}

		if (!found_dir)
		{
			printf("Não existe este diretório %s\n", token);
			return;
		}

		token = strtok(NULL, "/");
		count--;
	}

	int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);

	// tendo o diretorio no qual queremos criar o novo (token)
	// basta verificar se nao existe um arquivo com este mesmo nome
	// verificar se possui um bloco livre no diretório e na fat
	for (int i = 0; i < size_dir; i++)
	{

		if (strcmp(block.dir[i].filename, token) == 0)
		{
			printf("Já possui uma entrada neste diretório com este mesmo nome!\n");
			return;
		}

		if (block.dir[i].first_block == 0)
		{

			int index_fat = FreeSpace();

			if (index_fat == -1)
			{
				printf("Fat não possui espaço vazio!\n");
				return;
			}

			fat[index_fat] = 0xffff;
			dir_entry_t new_dir;
			// limpa o novo diretorio a ser criado (apaga os lixos da memoria)

			memset(&new_dir, 0x0000, sizeof(dir_entry_t));
			memcpy(new_dir.filename, token, sizeof(char) * strlen(token));
			new_dir.attributes = 1;
			new_dir.first_block = index_fat;
			new_dir.size = 0;

			// salva este novo diretorio no bloco do pai
			block.dir[i] = new_dir;
			writeCluster(index_block_fat, &block);
			writeFat();
			break;
		}
	}

	free(cpy);
}

void create(char *diretorio)
{
	char copia[strlen(diretorio)];
	strcpy(copia, diretorio);

	char *cpy = malloc(strlen(diretorio) * sizeof(char));
	strcpy(cpy, diretorio);

	char *token;
	token = strtok(cpy, "/"); // pega o primeiro elemento apos root

	int index_block_fat = 0;
	union data_cluster block;
	if (diretorio[0] == '/')
	{
		index_block_fat = 9;
		block = readCluster(9);
	}
	else
	{
		printf("Caminho não possui diretório root!\n");
		return;
	}
	int count = 0;
	// conta quantos direitorios há na string
	while (token != NULL)
	{
		token = strtok(NULL, "/");
		count++;
	}
	token = strtok(copia, "/");
	// caminha nos diretórios até chegar no penúltimo,pois o último é o arquivo que deve ser criado
	while (count > 1)
	{
		;
		int i;
		int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
		int found_dir = 0;
		// procura o diretorio atual no anterior
		for (i = 0; i < size_dir; i++)
		{
			if (strcmp(block.dir[i].filename, token) == 0)
			{
				found_dir = 1;
				if (block.dir[i].attributes == 1)
				{
					index_block_fat = block.dir[i].first_block;
					block = readCluster(block.dir[i].first_block);
				}
				else
				{
					printf("%s não é um diretório!\n", token);
				}
				break;
			}
		}
		if (!found_dir)
		{
			printf("Não existe este diretório %s\n", token);
			return;
		}
		token = strtok(NULL, "/");
		count--;
	}
	int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
	int i;
	// tendo o diretorio no qual queremos criar o novo arquivo (token) basta verificar se nao existe um arquivo com este mesmo nome

	for (i = 0; i < size_dir; i++)
	{
		if (strcmp(block.dir[i].filename, token) == 0)
		{
			printf("Já possui uma entrada neste diretório com este mesmo nome!\n");
			return;
		}
		if (block.dir[i].first_block == 0)
		{
			int index_fat = FreeSpace();
			if (index_fat == -1)
			{
				printf("Fat não possui espaço vazio!\n");
				return;
			}
			fat[index_fat] = 0xffff;
			dir_entry_t new_arq;
			// limpa o novo arquivo a ser criado (apaga os lixos da memoria)
			memset(&new_arq, 0x0000, sizeof(dir_entry_t));
			memcpy(new_arq.filename, token, sizeof(char) * strlen(token));
			new_arq.attributes = 0;
			new_arq.first_block = index_fat;
			new_arq.size = 0;
			// salva este novo arquivo no bloco do pai
			block.dir[i] = new_arq;
			writeCluster(index_block_fat, &block);
			writeFat();
			break;
		}
	}
	free(cpy);
}

void __unlink__(char *diretorio)
{

	char copia[strlen(diretorio)];
	strcpy(copia, diretorio);

	char *cpy = malloc(strlen(diretorio) * sizeof(char));
	strcpy(cpy, diretorio);

	char *token;
	token = strtok(cpy, "/"); // pega o primeiro elemento apos root

	int index_block_fat = 0;

	union data_cluster block;

	if (diretorio[0] == '/')
	{
		index_block_fat = 9;
		block = readCluster(9);
	}
	else
	{
		printf("Caminho não possui diretório root!\n");
		return;
	}

	int count = 0;

	// conta quantos direitorios há na string
	while (token != NULL)
	{
		token = strtok(NULL, "/");
		count++;
	}

	token = strtok(copia, "/");

	// caminha nos diretórios até chegar no penúltimo,pois o último é o arquivo que deve ser removido
	while (count > 1)
	{
		int i;
		int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
		int found_dir = 0;

		// procura o diretorio atual no anterior
		for (i = 0; i < size_dir; i++)
		{

			if (strcmp(block.dir[i].filename, token) == 0)
			{
				found_dir = 1;
				if (block.dir[i].attributes == 1)
				{
					index_block_fat = block.dir[i].first_block;
					block = readCluster(block.dir[i].first_block);
				}
				else
				{
					printf("%s não é um diretório!\n", token);
				}
				break;
			}
		}

		if (!found_dir)
		{
			printf("Não existe este diretório %s\n", token);
			return;
		}

		token = strtok(NULL, "/");
		count--;
	}

	int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
	int i;
	int found_unlink = 0;

	// verificar se possui um bloco livre no diretório e na fat
	for (i = 0; i < size_dir; i++)
	{

		if (strcmp(block.dir[i].filename, token) == 0)
		{
			if (block.dir[i].attributes == 1)
			{
				found_unlink = 1;
				union data_cluster cluster_dir = readCluster(block.dir[i].first_block);

				int j;
				for (j = 0; j < size_dir; j++)
				{
					if (cluster_dir.dir[j].first_block != 0)
					{
						printf("Diretório ainda possui elementos!\n");
						return;
					}
				}

				memset(&cluster_dir, 0x0000, CLUSTER_SIZE);
				fat[block.dir[i].first_block] = 0x0000;

				writeCluster(block.dir[i].first_block, &cluster_dir);
				memset(&block.dir[i], 0x0000, sizeof(dir_entry_t));

				resize(diretorio, -block.dir[i].size);

				writeCluster(index_block_fat, &block);
				writeFat();
				break;
			}
			else if (block.dir[i].attributes == 0)
			{
				found_unlink = 1;

				uint16_t current = block.dir[i].first_block;
				uint16_t temp = block.dir[i].first_block;
				// vai apagando os clusters  até o penultimo cluster
				while (fat[current] != 0xffff)
				{
					union data_cluster cluster_dir = readCluster(current);
					memset(&cluster_dir, 0x0000, CLUSTER_SIZE);
					writeCluster(current, &cluster_dir);
					temp = fat[current];
					fat[current] = 0x0000;
					current = temp;
				}

				// ultimo cluster que possui o valor 0xffff
				union data_cluster cluster_dir = readCluster(current);
				memset(&cluster_dir, 0x0000, CLUSTER_SIZE);
				writeCluster(current, &cluster_dir);
				fat[current] = 0x0000;

				resize(diretorio, -block.dir[i].size);

				memset(&block.dir[i], 0x0000, sizeof(dir_entry_t));
				writeCluster(index_block_fat, &block);
				writeFat();
				break;
			}
		}
	}

	if (!found_unlink)
	{
		printf("Arquivo não encontrado!\n");
		return;
	}

	free(cpy);
}

void write(char *words, char *diretorio)
{
	char copia[strlen(diretorio)];
	strcpy(copia, diretorio);

	char *cpy = malloc(strlen(diretorio) * sizeof(char));
	strcpy(cpy, diretorio);

	char *token;
	token = strtok(cpy, "/"); // pega o primeiro elemento apos root

	int index_block_fat = 0;

	union data_cluster block;

	if (diretorio[0] == '/')
	{
		index_block_fat = 9;
		block = readCluster(9);
	}
	else
	{
		printf("Caminho não possui diretório root!\n");
		return;
	}

	int count = 0;

	// conta quantos direitorios há na string
	while (token != NULL)
	{
		token = strtok(NULL, "/");
		count++;
	}

	token = strtok(copia, "/");

	// caminha nos diretórios até chegar no penúltimo, pois o último é o arquivo que deve ser criado
	while (count > 1)
	{
		;
		int i;
		int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
		int found_dir = 0;

		// procura o diretorio atual no anterior
		for (i = 0; i < size_dir; i++)
		{

			if (strcmp(block.dir[i].filename, token) == 0)
			{
				found_dir = 1;
				if (block.dir[i].attributes == 1)
				{
					index_block_fat = block.dir[i].first_block;
					block = readCluster(block.dir[i].first_block);
				}
				else
				{
					printf("%s não é um diretório!\n", token);
				}
				break;
			}
		}

		if (!found_dir)
		{
			printf("Não existe este diretório %s\n", token);
			return;
		}

		token = strtok(NULL, "/");
		count--;
	}

	int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
	int i;
	int found_unlink = 0;

	// verifica se possui um bloco livre no diretório e na fat
	for (i = 0; i < size_dir; i++)
	{

		if (strcmp(block.dir[i].filename, token) == 0)
		{

			if (block.dir[i].attributes == 0)
			{
				found_unlink = 1;

				uint16_t current = block.dir[i].first_block;
				uint16_t temp = block.dir[i].first_block;

				// vai apagando os clusters até o penultimo cluster
				while (fat[current] != 0xffff)
				{
					union data_cluster cluster_dir = readCluster(current);
					memset(&cluster_dir, 0x0000, CLUSTER_SIZE);
					writeCluster(current, &cluster_dir);
					temp = fat[current];
					fat[current] = 0x0000;
					current = temp;
				}

				// ultimo cluster que possui o valor 0xffff
				union data_cluster cluster_dir = readCluster(current);
				memset(&cluster_dir, 0x0000, CLUSTER_SIZE);
				block.dir[i].first_block = current;
				writeCluster(current, &cluster_dir);

				int len = strlen(words);

				block.dir[i].size = len;
				resize(diretorio, len);

				int number_clusters = ceil(len / (CLUSTER_SIZE * 1.0));

				char buffer[len + 1];

				int j = 0;
				while (1)
				{
					int offset = j * CLUSTER_SIZE;
					slice_str(words, buffer, offset, CLUSTER_SIZE + offset);

					cluster_dir = readCluster(current);
					memcpy(cluster_dir.data, buffer, sizeof(char) * strlen(buffer));
					writeCluster(current, &cluster_dir);

					fat[current] = 0xffff;
					j++;

					if (j < number_clusters)
					{
						int next_index_fat = FreeSpace();

						if (next_index_fat == -1)
						{
							printf("Fat não possui espaço vazio!\n");
							return;
						}

						fat[current] = next_index_fat;
						current = next_index_fat;
					}
					else
					{
						break;
					}
				}

				writeCluster(index_block_fat, &block);
				writeFat();
				break;
			}
		}
	}

	if (!found_unlink)
	{
		printf("Arquivo não encontrado!\n");
		return;
	}
	free(cpy);
}

void append(char *words, char *diretorio)
{
	char copia[strlen(diretorio)];
	strcpy(copia, diretorio);

	char *cpy = malloc(strlen(diretorio) * sizeof(char));
	strcpy(cpy, diretorio);

	char *token;
	token = strtok(cpy, "/"); // pega o primeiro elemento apos root

	int index_block_fat = 0;

	union data_cluster block;

	if (diretorio[0] == '/')
	{
		index_block_fat = 9;
		block = readCluster(9);
	}
	else
	{
		printf("Caminho não possui diretório root!\n");
		return;
	}

	int count = 0;

	// conta quantos direitorios há na string
	while (token != NULL)
	{
		token = strtok(NULL, "/");
		count++;
	}

	token = strtok(copia, "/");

	// caminha nos diretórios até chegar no penúltimo,
	//  pois o último é o arquivo que deve ser utilzizado
	while (count > 1)
	{
		;
		int i;
		int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
		int found_dir = 0;

		// procura o diretorio atual no anterior
		for (i = 0; i < size_dir; i++)
		{

			if (strcmp(block.dir[i].filename, token) == 0)
			{
				found_dir = 1;
				if (block.dir[i].attributes == 1)
				{
					index_block_fat = block.dir[i].first_block;
					block = readCluster(block.dir[i].first_block);
				}
				else
				{
					printf("%s não é um diretório!\n", token);
				}
				break;
			}
		}

		if (!found_dir)
		{
			printf("Não existe este diretório %s\n", token);
			return;
		}

		token = strtok(NULL, "/");
		count--;
	}

	int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
	int i;
	int found_unlink = 0;
	// tendo o diretorio no qual queremos criar o novo (token)
	// basta verificar se nao existe um arquivo com este mesmo nome
	// verificar se possui um bloco livre no diretório e na fat
	for (i = 0; i < size_dir; i++)
	{

		if (strcmp(block.dir[i].filename, token) == 0)
		{

			if (block.dir[i].attributes == 0)
			{
				found_unlink = 1;

				uint16_t current = block.dir[i].first_block;
				uint16_t temp = block.dir[i].first_block;

				int len = strlen(words);

				block.dir[i].size += len;
				resize(diretorio, len);

				char buffer[len + 1];

				int count_letters = 0;

				// procura o último cluster do arquivo
				while (fat[current] != 0xffff)
				{
					union data_cluster cluster_dir = readCluster(current);
					current = fat[current];
				}

				//preenche os espaços livres com a palavra
				union data_cluster cluster_dir = readCluster(current);
				int j;
				for (j = 0; j < CLUSTER_SIZE; j++)
				{
					if (count_letters >= len)
					{
						break;
					}
					if (cluster_dir.data[j] == 0x0000)
					{
						cluster_dir.data[j] = words[count_letters];
						count_letters++;
					}
				}
				writeCluster(current, &cluster_dir);

				// se no espaço livre do cluster final coube toda a palavra
				if (count_letters == strlen(words))
				{
					writeCluster(index_block_fat, &block);
					writeFat();
					return;
				}
				// string no qual deve ser salva em clusters livres
				slice_str(words, buffer, count_letters, strlen(words));

				strcpy(words, buffer);
				len = strlen(words);
				int number_clusters = ceil(len / (CLUSTER_SIZE * 1.0));

				uint16_t final_cluster = current;
				current = FreeSpace();
				fat[final_cluster] = current;
				j = 0;
				while (1)
				{
					int offset = j * CLUSTER_SIZE;
					slice_str(words, buffer, offset, CLUSTER_SIZE + offset);

					cluster_dir = readCluster(current);
					memcpy(cluster_dir.data, buffer, sizeof(char) * strlen(buffer));
					writeCluster(current, &cluster_dir);

					fat[current] = 0xffff;
					j++;

					if (j < number_clusters)
					{
						int next_index_fat = FreeSpace();

						if (next_index_fat == -1)
						{
							printf("Fat não possui espaço vazio!\n");
							return;
						}

						fat[current] = next_index_fat;
						current = next_index_fat;
					}
					else
					{
						break;
					}
				}

				writeCluster(index_block_fat, &block);
				writeFat();
				break;
			}
		}
	}

	if (!found_unlink)
	{
		printf("Arquivo não encontrado!\n");
		return;
	}
	free(cpy);
}

void read(char *diretorio)
{

	char copia[strlen(diretorio)];
	strcpy(copia, diretorio);

	char *cpy = malloc(strlen(diretorio) * sizeof(char));
	strcpy(cpy, diretorio);

	char *token;
	token = strtok(cpy, "/"); // pega o primeiro elemento apos root

	int index_block_fat;

	union data_cluster block;

	if (diretorio[0] == '/')
	{
		index_block_fat = 9;
		block = readCluster(9);
	}
	else
	{
		printf("Caminho não possui diretório root!\n");
		return;
	}

	int count = 0;

	// conta quantos direitorios há na string
	while (token != NULL)
	{
		token = strtok(NULL, "/");
		count++;
	}

	token = strtok(copia, "/");

	// caminha nos diretórios até chegar no ultimo
	// no qual é o que deve ser lido
	while (count > 1)
	{
		int i;
		int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
		int found_dir = 0;

		// procura o diretorio atual no anterior
		for (i = 0; i < size_dir; i++)
		{

			if (strcmp(block.dir[i].filename, token) == 0)
			{
				found_dir = 1;
				if (block.dir[i].attributes == 1)
				{
					index_block_fat = block.dir[i].first_block;
					block = readCluster(block.dir[i].first_block);
				}
				else
				{
					printf("%s não é um diretório!\n", token);
				}
				break;
			}
		}

		if (!found_dir)
		{
			printf("Não existe este diretório %s\n", token);
			return;
		}

		token = strtok(NULL, "/");
		count--;
	}

	int size_dir = CLUSTER_SIZE / sizeof(dir_entry_t);
	int found_unlink = 0;
	// tendo o diretorio no qual queremos criar o novo (token)
	// basta verificar se nao existe um arquivo com este mesmo nome
	// verificar se possui um bloco livre no diretório e na fat
	for (int i = 0; i < size_dir; i++)
	{

		if (block.dir[i].attributes == 0)
		{
			found_unlink = 1;

			uint16_t current = block.dir[i].first_block;
			uint16_t temp = block.dir[i].first_block;

			char result[block.dir[i].size];
			int count_letters = 0;
			// vai andando nos clusters até o penultimo cluster
			//  e salvando seus valores em result
			while (fat[current] != 0xffff)
			{
				union data_cluster cluster_dir = readCluster(current);
				int j;
				for (j = 0; j < CLUSTER_SIZE; j++)
				{
					if (cluster_dir.data[j] == 0x0000)
					{
						break;
					}
					result[count_letters] = cluster_dir.data[j];
					count_letters++;
				}
				current = fat[current];
			}
			// ultimo cluster com valor na fat de 0xffff
			union data_cluster cluster_dir = readCluster(current);
			int j;
			for (j = 0; j < CLUSTER_SIZE; j++)
			{
				if (cluster_dir.data[j] == 0x0000)
				{
					break;
				}
				result[count_letters] = cluster_dir.data[j];
				count_letters++;
			}

			// imprime os dados do arquivo
			for (j = 0; j < block.dir[i].size; j++)
			{
				printf("%c", result[j]);
			}
			printf("\n");
			break;
		}
	}

	if (!found_unlink)
	{
		printf("Arquivo não encontrado!\n");
		return;
	}
	free(cpy);
}

//**********************************//
//				MAIN				//
//**********************************//

int main()
{
	char input_str[4194304];
	int ch;
	int i;
	while (1)
	{
		memset(input_str, 0x0000, sizeof(input_str));
		printf(">> ");
		for (i = 0; (i < (sizeof(input_str) - 1) && ((ch = fgetc(stdin)) != EOF) && (ch != '\n')); i++)
		{
			input_str[i] = ch;
		}

		input_str[i] = '\0';

		if (strcmp(input_str, "init") == 0)
		{
			init();
		}
		else if (strcmp(input_str, "load") == 0)
		{
			load();
		}
		else if (strcmp(input_str, "exit") == 0)
		{
			exit(0);
		}
		else if (strcmp(input_str, "clear") == 0)
		{
			system("clear");
		}
		else if (strstr(input_str, "\"") != NULL)
		{
			char *cpy = malloc(strlen(input_str) * sizeof(char));
			strcpy(cpy, input_str);

			char *token;

			token = strtok(cpy, " \"");

			if (strcmp(token, "write") == 0)
			{

				char *string = strtok(NULL, "\"");

				char *path = strtok(NULL, " \"");
				write(string, path);
			}
			else if (strcmp(token, "append") == 0)
			{

				char *string = strtok(NULL, "\"");

				char *path = strtok(NULL, " \"");
				append(string, path);
			}
		}
		else
		{

			char *cpy = malloc(strlen(input_str) * sizeof(char));
			strcpy(cpy, input_str);

			char *token;

			token = strtok(cpy, " ");

			if (strcmp(token, "ls") == 0)
			{

				token = strtok(NULL, " ");
				char *teste = malloc(strlen(token) * sizeof(char));
				memcpy(teste, token, strlen(token) * sizeof(char));
				ls(token);
			}
			else if (strcmp(token, "mkdir") == 0)
			{

				token = strtok(NULL, " ");
				mkdir(token);
			}
			else if (strcmp(token, "create") == 0)
			{
				char *path = strtok(NULL, " ");
				create(path);
			}
			else if (strcmp(token, "unlink") == 0)
			{

				char *path = strtok(NULL, " ");
				unlink(path);
			}
			else if (strcmp(token, "read") == 0)
			{

				char *path = strtok(NULL, " ");
				read(path);
			}
			else
			{
				printf("Comando nao existe\n");
			}

			free(cpy);
		}
	}

	return 0;
}