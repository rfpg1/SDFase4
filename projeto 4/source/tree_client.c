/**
 * Grupo 29
 * Ricardo Gonçalves fc52765
 * Rafael Abrantes fc52751
 * Daniel Batista fc52773
**/ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "client_stub.h"
#include "zookeeper/zookeeper.h"


static struct rtree_t *primary;
static struct rtree_t *backup;
static zhandle_t *zh;
static int is_connected; 
static char *root_path = "/kvstore";

typedef struct String_vector zoo_string;
static char *watcher_ctx = "ZooKeeper Data Watcher";

char str[50]; 

void closeClient(){
    if(primary != NULL){
        printf("Adeus primary\n");
        rtree_disconnect(primary);
    }
    
    if(backup != NULL){
        printf("Adeus backup\n");
        rtree_disconnect(backup); 
    }
         
    
    exit(0);
}

void connection_watcher(zhandle_t *zzh, int type, int state, const char *path, void* context) {
	if (type == ZOO_SESSION_EVENT) {
		if (state == ZOO_CONNECTED_STATE) {
			is_connected = 1; 
		} else {
			is_connected = 0; 
		}
	}
}

void child_watcher(zhandle_t *wzh, int type, int state, const char *zpath, void *watcher_ctx) {
	zoo_string* children_list =	(zoo_string *) malloc(sizeof(zoo_string));
	//int zoo_data_len = ZDATALEN; //TODO nao estah a ser usado 
	if (state == ZOO_CONNECTED_STATE) { //TODO ver se dah para por simplesmente is_connected
		if (type == ZOO_CHILD_EVENT) {
	 	   /* Get the updated children and reset the watch */ 
 			if (ZOK != zoo_wget_children(zh, root_path, child_watcher, watcher_ctx, children_list)) {
 				fprintf(stderr, "Error setting watch at %s!\n", root_path); 
 			}
            //TODO apagar a parte de imprimir a lista
			fprintf(stderr, "\n=== znode listing func=== [ %s ]", root_path); 
			for (int i = 0; i < children_list->count; i++)  {
				fprintf(stderr, "\n(%d): %s", i+1, children_list->data[i]);
			}
			fprintf(stderr, "\n=== done ===\n");
	    } 
	}
    
    if(children_list -> count == 0){
            //Do nothing
    } else if(children_list -> count == 1){
        printf("ENTRA NO ELSEIF TREE_CLIENT\n");
        if(strcmp(children_list -> data[0], "primary") == 0){
            printf("ENTRA NO ELSEIF TREE_CLIENT PRIMARY\n");
            
            if(backup != NULL)
                rtree_disconnect(backup);
            backup = NULL;
            
            //meter a null caso antes houvesse um backup;
        } else {
            printf("ENTRA NO ELSEIF TREE_CLIENT ELSE\n");
            
            if(primary != NULL)
                rtree_disconnect(primary);
            primary = NULL;
            
            int backupIPLen = 100;
            char backupIP[256] = "";
            if(ZOK != zoo_get(zh, "/kvstore/backup", 0, backupIP, &backupIPLen, NULL)){
                printf("ERRO A IR BUSCAR DATA BACKUP 1 FILHO\n");
            }
            /*
            if(backup != NULL)
                rtree_disconnect(backup); //Problema, fazer esta linha fecha a linha de comandos atual e dá merda como atualizar isto?
            backup = NULL;
            */
            //primary = rtree_connect(backupIP);
            primary = backup;
            /*
            if(primary != NULL){
                primary = backup;
            }
            */
            backup = NULL;
        }
    } else {
        printf("ENTRA NO ELSE\n");
        if(backup == NULL){
            int backupIPLen = 100;
            char backupIP[256] = "";
            if(ZOK != zoo_get(zh, "/kvstore/backup", 0, backupIP, &backupIPLen, NULL)){
                printf("ERRO A IR BUSCAR DATA BACKUP 2 FILHOS\n");
            }
            backup = rtree_connect(backupIP);
        }
    }


}


int main(int argc, char** argv){

    signal(SIGINT,closeClient); //Se fechar do nada, interrupção forçada ele faz a função fechar
    signal(SIGPIPE, SIG_IGN); //Se receber o sinal SIGPIPE, ele ignora (SIG_IGN)

    if(argc != 2){ //Tem de ter exatamente 2 argumentos, ./nomeDaClasse IPServer:Porta
        perror("Chame a função da seguinte maneira: ./tree:client <ip do servidor>:<porta>\n");
        return -1;
    }
    char *adress_port_zk = strdup(argv[1]);

    zh = zookeeper_init(adress_port_zk, connection_watcher, 2000, 0, NULL, 0);
    if (zh == NULL)	{
		printf("Error connecting to ZooKeeper server!\n"); //compromete transparencia
		exit(-1);
	}
    int primaryIPLen = 100;
    char primaryIP[256] = "";
    int backupIPLen = 100;
    char backupIP[256] = "";
    sleep(3);
    if(is_connected){
        if(ZNONODE == zoo_exists(zh, root_path, 0, NULL)){ //Caso /kvstore não exista
            int new_root_path_len = 1024;
            char *new_root_path = malloc(new_root_path_len);
            if(ZOK != zoo_create(zh, root_path, NULL, 0, &ZOO_OPEN_ACL_UNSAFE, 0, new_root_path, new_root_path_len)){
                fprintf(stderr, "Erro a criar znode no path %s!\n", root_path);
                return -1;
            }
            printf("Normal ZNode criado! ZNode path: %s\n", new_root_path);
            free(new_root_path);
        }
        
        zoo_string* children_list =	(zoo_string *) malloc(sizeof(zoo_string));
        if (ZOK != zoo_wget_children(zh, root_path, &child_watcher, watcher_ctx, children_list)) {
			fprintf(stderr, "Error setting watch at %s!\n", root_path); 
		}
        
        switch(children_list -> count){
            case 0:
            printf("CASO 0\n");
                perror("Não há servidores ligados");
                return -1;
            break;
            case 1:
                printf("CASO 1\n");
                if(ZOK != zoo_get(zh, "/kvstore/primary", 0, primaryIP, &primaryIPLen, NULL)){
                    printf("ERRO A IR BUSCAR DATA SWITCH 1 FILHO PRIMARY\n");
                    return -1;
                }
                primary = rtree_connect(primaryIP); //Ligação ao servidor!
            break;
            case 2:
            printf("CASO 2\n");
                if(ZOK != zoo_get(zh, "/kvstore/primary", 0, primaryIP, &primaryIPLen, NULL)){
                    printf("ERRO A IR BUSCAR DATA SWITCH 2 FILHOS PRIMARY \n");
                    return -1;
                }
                if(ZOK != zoo_get(zh, "/kvstore/backup", 0, backupIP, &backupIPLen, NULL)){
                    printf("ERRO A IR BUSCAR DATA SWITCH 2 FILHOS BACKUP \n");
                    return -1;
                }
                printf("IPPRIMARY: %s\n", primaryIP);
                primary = rtree_connect(primaryIP); //Ligação ao servidor!
                printf("IPBACKUP: %s\n", backupIP);
                backup = rtree_connect(backupIP); //Ligação ao servidor!
            break;
            default:
                printf("Erro\n");
            //do nothing
        }             
        
    } 
    int last_assigned = 0;
    if(primary == NULL){
        perror("Erro a fazer a ligação ao server");
        return -1;
    }
    //While true, para executar tantos comandos como pretender!
    while(1){
        printf("Insira o comando que pretende executar\n");
        //Lê 50 caracteres do teclado
        if(fgets(str, 50, stdin) == NULL){
            perror("Erro ao ler do stdin");
            return -1;
        }


        char *metodo = strtok(str, " "); //primeira palavra do stdin

        if(strncmp(metodo, "\n", strlen("\n")) != 0){ //Não dar simplesmente enter, se der enter volta ao while
            if(strncmp(metodo, "quit", strlen("quit")) == 0){
                //Desliga-se do servidor
                closeClient();
                break;
            /*************************************PUT*****************************************/ 
            }else if(strncmp(metodo, "put", strlen("put")) == 0){
                if(backup == NULL){
                    printf("Servidor backup desligado não é possivel executar pedidos de escrita/leitura\n");
                    continue;
                }
                //Criar a entry
                char *key = strtok(NULL, " "); //ir buscar a 2 palavra do stdin
                char *value = strtok(NULL, "\n"); //ir buscar a 3 palavra do stdin

                if(key == NULL || value == NULL){
                    perror("Falta de argumentos, o put precisa de 2 argumentos, a key e o value!");
                    perror("Ex: put A 2");
                    continue;
                }
                //Cria a data e a entry
                struct data_t *data = data_create2(strlen(value) + 1, strdup(value));
                struct entry_t *entry = entry_create(strdup(key), data);
                //Chama o método para meter a entry na arvore
                int result = rtree_put(primary, entry);
                last_assigned++;
                if(result < 0){
                    printf("Erro no put!\n");
                } else {
                    printf("A operação put (operação numero %d) foi executada!\n", result);
                }

            /*************************************SIZE*****************************************/ 
            } else if(strncmp(metodo, "size", strlen("size")) == 0){
                if(backup == NULL){
                    printf("Servidor backup desligado não é possivel executar pedidos de escrita/leitura\n");
                    continue;
                }
                //Chama o método para saber o size da tree
                int temp = rtree_verify(backup, last_assigned);
                while(temp != 1){
                    temp = rtree_verify(backup, last_assigned);
                }
                int size = rtree_size(backup);
                printf("Tree size: %d\n", size);

            /*************************************DEL*****************************************/ 
            } else if(strncmp(metodo, "del", strlen("del")) == 0){
                if(backup == NULL){
                    printf("Servidor backup desligado não é possivel executar pedidos de escrita/leitura\n");
                    continue;
                }
                //Vai buscar a key que é para apagar
                char *key = strtok(NULL, "\n");

                if(key == NULL){
                    perror("Falta de argumentos, o del precisa de 1 argumento, a key!");
                    perror("Ex: del A");
                    continue;
                }

                //Chama o método para apagar a key da tree
                int result = rtree_del(primary, strdup(key));
                last_assigned++;
                if(result < 0){
                    printf("A key não existe!\n");
                } else {
                    printf("A operacao del (operacao numero %d) foi executada!\n", result);
                }
            
            /*************************************GETKEYS*****************************************/ 
            } else if(strncmp(metodo, "getkeys", strlen("getkeys")) == 0){
                if(backup == NULL){
                    printf("Servidor backup desligado não é possivel executar pedidos de escrita/leitura\n");
                    continue;
                }
                //Chama o método para ir buscar as varias keys da tree
                int temp = rtree_verify(backup, last_assigned);
                while(temp != 1){
                    temp = rtree_verify(backup, last_assigned);
                }

                char ** keys = rtree_get_keys(backup);
                //Caso isto esteja NULL é porque houve um erro
                if(keys == NULL){
                    printf("A tree esta vazia\n");
                    continue;
                }
                int size = rtree_size(backup);
                printf("As várias keys são: \n");
                int i = 0;
                if(size > 0){
                    while(keys[i] != NULL){
                        printf("%s ", keys[i]);
                        i++;
                    }
                    //Adiciona um \n para ficar mais bonito
                    printf("\n");
                }
                
                rtree_free_keys(keys);
            
            /*************************************GET*****************************************/ 
            }else if(strncmp(metodo, "get", strlen("get")) == 0){
                if(backup == NULL){
                    printf("Servidor backup desligado não é possivel executar pedidos de escrita/leitura\n");
                    continue;
                }
                //Vai buscar a key que é para saber o data associado
                char *key = strtok(NULL, "\n");

                if(key == NULL){
                    perror("Falta de argumentos, o get precisa de 1 argumento, a key!");
                    perror("Ex: get A");
                    continue;
                }
                int temp = rtree_verify(backup, last_assigned);
                while(temp != 1){
                    temp = rtree_verify(backup, last_assigned);
                }
                //Chama o método para ir buscar a data associada à key
                struct data_t *data = rtree_get(backup, strdup(key));

                if(data == NULL){
                    printf("A key não existe\n");
                    //free(key);
                } else {
                    printf("O valor associado à key %s é: %s.\n", key, (char *) data -> data);
                    data_destroy(data);
                }

            /*************************************HEIGHT*****************************************/ 
            } else if(strncmp(metodo, "height", strlen("height")) == 0){
                if(backup == NULL){
                    printf("Servidor backup desligado não é possivel executar pedidos de escrita/leitura\n");
                    continue;
                }
                //Chama o método para saber a height da tree
                int temp = rtree_verify(backup, last_assigned);
                while(temp != 1){
                    temp = rtree_verify(backup, last_assigned);
                }
                int height = rtree_height(backup);
                printf("Tree height: %d\n", height);

            /*************************************VERIFY*****************************************/ 
            } else if(strncmp(metodo, "verify", strlen("verify")) == 0){
                if(backup == NULL){
                    printf("Servidor backup desligado não é possivel executar pedidos de escrita/leitura\n");
                    continue;
                }
                //Operalão a verificar se foi feita
                char *key = strtok(NULL, "\n");

                if(key == NULL){
                    perror("Falta de argumentos, o verify precisa de 1 argumento, a operação!");
                    perror("Ex: verify 1");
                    continue;
                }
                int temp = rtree_verify(backup, last_assigned);
                while(temp != 1){
                    temp = rtree_verify(backup, last_assigned);
                }
                //Chama o método para ir verificar se a operação foi feita
                int verify = rtree_verify(backup, atoi(key));

                if(verify < 0){
                    printf("A operação %d ainda não foi feita\n", atoi(key));
                } else {
                    printf("A operação %d já foi feita\n", atoi(key));
                }  
                              
            } else {
                printf("Método não existe!\n");
            }
        }

    }
}