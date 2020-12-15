/**
 * Grupo 29
 * Ricardo Gonçalves fc52765
 * Rafael Abrantes fc52751
 * Daniel Batista fc52773
**/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
///////////fase4/////////////
#include <errno.h>
#include <unistd.h> //para funcao sleep()

#include <netdb.h> //ver se eh preciso
#include <sys/types.h> //ver se eh preciso
#include <sys/socket.h> //ver se eh preciso
#include <netinet/in.h> //ver se eh preciso
#include <pthread.h>
#include <arpa/inet.h> //ver se eh preciso

#include "client_stub-private.h"
#include "client_stub.h"
////////////////////////////////////

#include "tree.h"
#include "message-private.h"
#include "tree_skel-private.h"
#include "tree_skel.h"
#include "sdmessage.pb-c.h"
#include "tree-private.h"

#include <zookeeper/zookeeper.h>

struct tree_t *tree;

int last_assigned;
int op_count;

struct task_t *queue_head;

pthread_t thread;
pthread_mutex_t queue_lock, tree_lock;
pthread_cond_t queue_not_empty;

int termina_thread = 0; //False por default

/********************** 4 FASE ************************/
zhandle_t *zh;
int is_connected;
char *root_path = "/kvstore"; 
char *server_ID; //Identificador do node dester servidor no ZooKeeper
typedef struct String_vector zoo_string;
static char *watcher_ctx = "ZooKeeper Data Watcher";

struct rtree_t *backup = NULL;
char *IPBACKUP;
char *IPPrimary;

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

        // ---------------------------------------------------------------------------------------------------------
        if(children_list -> count == 0){
            //Do nothing
        } else if(children_list -> count == 1){
            printf("ENTRA NO IF TREE_SKEL\n");
            if(strcmp(children_list -> data[0], "primary") == 0){
                printf("1 Filho PRIMARY\n");
                
                if(backup != NULL){
                    rtree_disconnect(backup);
                }
                
                backup = NULL;
                
                IPBACKUP = NULL;
                //meter a null caso antes houvesse um backup;
            } else {
                printf("1 Filho BACKUP\n");
                
                int backupIPLen = 256;
                
                char backupIP[256] = "";
                
                if(ZOK != zoo_get(zh, "/kvstore/backup", 0, backupIP, &backupIPLen, NULL)){
                    printf("ERRO A IR BUSCAR DATA BACKUP 1 FILHO\n");
                    exit(-1);
                }
                IPPrimary = NULL;
                if(ZOK != zoo_delete(zh, "/kvstore/backup", -1)){
                    printf("DELETE DO BACKUP\n");
                    exit(-1);
                }
                char *node_path = "/kvstore/primary";
                if(ZOK != zoo_create(zh, node_path, backupIP, strlen(backupIP), &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, NULL, 0)){
                    fprintf(stderr, "Error creating znode from path %s!\n", node_path);
                    exit(-1);
                }
                server_ID = "/kvstore/primary";
                //TODO transformar backup em primary
                //Delete do znode atual
                //criação de um novo   
            }
        } else {
            printf("THIS SERVER ID: %s\n", server_ID);
            if(strcmp(server_ID, "/kvstore/primary") == 0){
                if(backup == NULL){
                    int backupIPLen = 100;
                    char backupIP[256] = "";
                    if(ZOK != zoo_get(zh, "/kvstore/backup", 0, backupIP, &backupIPLen, NULL)){
                        printf("ERRO A IR BUSCAR DATA BACKUP 2 FILHOS\n");
                        exit(-1);
                    }
                    printf("BACKIP: %s\n", backupIP);
                    backup = rtree_connect(backupIP);
                    IPBACKUP = backupIP;
                }
            } else {
                int primaryIPLen = 100;
                char primaryIP[256] = "";
                if(ZOK != zoo_get(zh, "/kvstore/primary", 0, primaryIP, &primaryIPLen, NULL)){
                    printf("ERRO A IR BUSCAR DATA BACKUP 2 FILHOS\n");
                    exit(-1);
                }
                IPPrimary = primaryIP;
            }
        }
	}
    free(children_list->data);
    free(children_list);
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

int tree_skel_init(char *port, char *ip_port_zk){

    zh = zookeeper_init(ip_port_zk, connection_watcher, 2000, 0, NULL, 0); //Ligação ao servidor do ZooKeeper
    if(zh == NULL){
        perror("Erro a ligar ao servidor do ZooKeeper");
        return -1;
    }
    sleep(3); //Dar tempo para a ligação acontecer
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

        /*-----------------------mostrar os child nodes de /kvstore atuais----------------------*/
        if (ZOK != zoo_wget_children(zh, root_path, &child_watcher, watcher_ctx, children_list)) {/*quando apanha sinal, chama o child_watcher()*/
			fprintf(stderr, "Error setting watch at %s!\n", root_path); //compromete transparencia?
		}

        if(children_list -> count < 1){
            /********************************************/
            //Criar prefixo para o primary
            char node_path[50] = "";
            strcat(node_path, root_path);
            strcat(node_path, "/primary");
            /********************************************/
            int server_ID_len = 1024;
            server_ID = malloc(server_ID_len);

            /********************************************/
            char hostBuffer[256];
            char *IPBuffer;
            struct hostent *host_entry;
            int hostname;

            hostname = gethostname(hostBuffer, sizeof(hostBuffer));
            if(hostname == -1){
                perror("gethostname");
                return -1;
            }
            host_entry = gethostbyname(hostBuffer);
            if(host_entry == NULL){
                perror("gethostbyname");
                return -1;
            }

            // To convert an Internet network address into ASCII string 
            IPBuffer = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])); 

            /*---------------concatenar ip com port do servidor---------------*/
            char *ip_port = IPBuffer; 
            strcat(ip_port, ":");
            strcat(ip_port, port);
            /*----------------------------------------------------------------*/
            if(ZOK != zoo_create(zh, node_path, ip_port, strlen(ip_port), &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, server_ID, server_ID_len)){
                fprintf(stderr, "Error creating znode from path %s!\n", node_path);
                return -1;
            }
            fprintf(stderr, "Ephemeral ZNode created! ZNode path: %s\n", server_ID);
            sleep(5);
        } else if(children_list -> count == 1){
            
            /********************************************/
            //Criar prefixo para o primary
            char node_path[50] = "";
            strcat(node_path, root_path);
            strcat(node_path, "/backup");
            /********************************************/
            int server_ID_len = 1024;
            server_ID = malloc(server_ID_len);

            /********************************************/
            char hostBuffer[256];
            char *IPBuffer;
            struct hostent *host_entry;
            int hostname;

            hostname = gethostname(hostBuffer, sizeof(hostBuffer));
            if(hostname == -1){
                perror("gethostname");
                return -1;
            }
            host_entry = gethostbyname(hostBuffer);
            if(host_entry == NULL){
                perror("gethostbyname");
                return -1;
            }

            // To convert an Internet network address into ASCII string 
            IPBuffer = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0])); 

            /*---------------concatenar ip com port do servidor---------------*/
            char *ip_port = IPBuffer; 
            strcat(ip_port, ":");
            strcat(ip_port, port);
            /*----------------------------------------------------------------*/
            if(ZOK != zoo_create(zh, node_path, ip_port, strlen(ip_port), &ZOO_OPEN_ACL_UNSAFE, ZOO_EPHEMERAL, server_ID, server_ID_len)){
                fprintf(stderr, "Error creating znode from path %s!\n", node_path);
                return -1;
            }
            fprintf(stderr, "Ephemeral ZNode created! ZNode path: %s\n", server_ID);
            sleep(5);
        } 
        else {
            printf("Demasiados servers");
            return -1;
        }
        free(children_list->data);
        free(children_list);
    }

    if(tree == NULL){
        tree = tree_create();
    }

    last_assigned = 0;
    op_count = 0;
    queue_head = NULL;

    if(init_mutex_cond() < 0){
        return -1;
    }

    if(pthread_create(&thread, NULL, process_task, NULL) < 0){
        return -1;
    }

    return tree == NULL? -1 : 0; 
}

void tree_skel_destroy(){
    //modificar a variavel de terminacao da thread para True
    termina_thread = 1;
    //Manda um sinal para queue_get_task sair do cond_wait uma ultima vez
    pthread_cond_signal(&queue_not_empty);
    //Esperar pela thread secundaria dar join
    pthread_join(thread, NULL);
    //Destruir os mutexes e variaveis de condicao
    mutex_cond_destroy();
    tree_destroy(tree);
    
    zookeeper_close(zh);
    
    
    if(server_ID != NULL)
        free(server_ID);
    /*
    if(IPPrimary != NULL)
        free(IPPrimary);
    if(IPBACKUP != NULL)
        free(IPBACKUP);
    
    if(zh != NULL)
        free(zh);
    */
    
}

int invoke(struct message_t *msg){
    if(tree == NULL){
        perror("Erro na criação da tree, invoke()");
        return -1;
    }

    int op = msg -> msg -> opcode;
    int c_type = msg -> msg -> c_type;
    /*************************************PUT*****************************************/

    if(op == MESSAGE_T__OPCODE__OP_PUT && c_type == MESSAGE_T__C_TYPE__CT_ENTRY){
        printf("PUT\n");
        //Aumentar o numero de pedidos de escrita RECEBIDOS DOS CLIENTES (colocados na fila)
        last_assigned++;
        //Cria uma nova tarefa
        struct task_t *task = task_create(last_assigned, 1, msg -> msg -> key, msg -> msg -> data, msg -> msg -> data_size);
        //Adicionar tarefa ah lista e sinaliza ah variavel de condicao que foi adicionado algo ah lista
        add_task(task);

        //Responder com o estado atual de last_assigned
        //Atualizar o OPCODE e o C_TYPE
        //Libertar memoria utilizada
        msg -> msg -> last_assigned = last_assigned;
        msg -> msg -> opcode = MESSAGE_T__OPCODE__OP_PUT+1;
        free(msg -> msg -> key);
        free(msg -> msg -> data); 
        msg -> msg -> data = NULL;
        msg -> msg -> c_type = MESSAGE_T__C_TYPE__CT_RESULT;
        msg -> msg -> data_size = 0;
        msg -> msg -> key = NULL;
        
    /*************************************SIZE*****************************************/
    } else if(op == MESSAGE_T__OPCODE__OP_SIZE && c_type == MESSAGE_T__C_TYPE__CT_NONE){
        printf("SIZE\n");
        pthread_mutex_lock(&tree_lock);
        int size = tree_size(tree); //Vai buscar o tamanho da tree
        pthread_mutex_unlock(&tree_lock);
        msg -> msg -> opcode = MESSAGE_T__OPCODE__OP_SIZE+1;
        msg -> msg -> c_type = MESSAGE_T__C_TYPE__CT_RESULT;
        msg -> msg -> tree_size = size;

    /*************************************DEL*****************************************/ 
    } else if(op == MESSAGE_T__OPCODE__OP_DEL && c_type == MESSAGE_T__C_TYPE__CT_KEY){
        printf("DELETE\n");
        //Aumentar o numero de pedidos de escrita RECEBIDOS DOS CLIENTES (colocados na fila)
        last_assigned++;        
        //Cria uma nova tarefa
        struct task_t *task = task_create(last_assigned, 0, msg -> msg -> key, NULL, 0);
        //Adicionar tarefa ah lista e sinaliza ah variavel de condicao que foi adicionado algo ah lista
        add_task(task);

        //Responder com o estado atual de last_assigned
        //Atualizar o OPCODE e o C_TYPE
        msg -> msg -> last_assigned = last_assigned;
        msg -> msg -> opcode = MESSAGE_T__OPCODE__OP_DEL + 1;
        msg -> msg -> c_type = MESSAGE_T__C_TYPE__CT_RESULT;

    /*************************************GET*****************************************/     
    } else if(op == MESSAGE_T__OPCODE__OP_GET && c_type == MESSAGE_T__C_TYPE__CT_KEY){
        printf("GET\n");
        char *key = msg -> msg -> key;
        pthread_mutex_lock(&tree_lock);
        struct data_t *data = tree_get(tree, key); //Vai buscar a data associada a uma key
        pthread_mutex_unlock(&tree_lock);
        free(key);
        
        msg -> msg -> key = NULL;
        if(data == NULL){ //Caso a key não esteja na arvore
            msg -> msg -> opcode = MESSAGE_T__OPCODE__OP_ERROR;
            msg -> msg -> c_type = MESSAGE_T__C_TYPE__CT_NONE;
            return -1;
        } else { //Caso a key esteja na arvore
            msg->msg->opcode = MESSAGE_T__OPCODE__OP_GET+1;
            msg -> msg -> c_type = MESSAGE_T__C_TYPE__CT_VALUE;
            msg -> msg -> data_size = data -> datasize;
            msg -> msg -> data = strdup(data -> data);
            data_destroy(data);
            return 0;
        }
    
    /*************************************HEIGHT*****************************************/ 
    } else if(op == MESSAGE_T__OPCODE__OP_HEIGHT && c_type == MESSAGE_T__C_TYPE__CT_NONE){
        printf("HEIGHT\n");
        pthread_mutex_lock(&tree_lock);
        int height = tree_height(tree); //Vai buscar a altura de uma arvore
        pthread_mutex_unlock(&tree_lock);
        msg -> msg -> opcode = MESSAGE_T__OPCODE__OP_HEIGHT + 1;
        msg -> msg -> c_type = MESSAGE_T__C_TYPE__CT_RESULT;
        msg -> msg -> tree_height = height;

    /*************************************GETKEYS*****************************************/     
    } else if(op == MESSAGE_T__OPCODE__OP_GETKEYS && c_type == MESSAGE_T__C_TYPE__CT_NONE){
        printf("GETKEYS\n");
        pthread_mutex_lock(&tree_lock);
        char **keys = tree_get_keys(tree); //Vai buscar as varias keys que estão dentro da arvore
        int size = tree_size(tree);
        pthread_mutex_unlock(&tree_lock);
        msg -> msg -> opcode = MESSAGE_T__OPCODE__OP_GETKEYS + 1;
        msg -> msg -> c_type = MESSAGE_T__C_TYPE__CT_KEYS;
        msg -> msg -> tree_size = size;

        int lengthTotal = 0;
        int i = 0;
        if(size > 0){
            while(keys[i] != NULL){
                lengthTotal += strlen(keys[i]);
                i++;
            }

            i = 0;
            int begin = 0;
            char *new = malloc(lengthTotal + size);

            if(new == NULL){
                perror("Erro na alocação de memoria, getkeys tree_skel.c");
                return -1;
            }

            while(keys[i] != NULL){ //Mete na variavel new todas as keys que estão na arvore separadas por dois pontos
                memcpy(new+begin, keys[i], strlen(keys[i]));
                begin += strlen(keys[i]);
                memcpy(new+begin, ":", sizeof(char));
                begin++;
                i++;
            }
            //As strings têm de acabar com \0
            memcpy(new+begin-1, "\0", sizeof(char));
            msg -> msg -> key = new;
        }
        tree_free_keys(keys); //Liberta a memoria que foi alocada anteriormente

    /*************************************VERIFY*****************************************/  
    } else if(op == MESSAGE_T__OPCODE__OP_VERIFY && c_type == MESSAGE_T__C_TYPE__CT_RESULT){
        printf("VERIFY\n");
        int op_n = msg -> msg -> verify;
        int verify_ret = verify(op_n);
        if(verify_ret == 0){
            msg -> msg -> opcode = MESSAGE_T__OPCODE__OP_ERROR;
            msg -> msg -> c_type = MESSAGE_T__C_TYPE__CT_NONE;
        } else {
            msg -> msg -> opcode = MESSAGE_T__OPCODE__OP_VERIFY + 1;
            msg -> msg -> c_type = MESSAGE_T__C_TYPE__CT_RESULT;
        }  
    }
    return 0;
}

//Metodo para inicializar os mutexes e variaveis de condicao
int init_mutex_cond(){
    //Inicializar mutex da tree
    if(pthread_mutex_init(&tree_lock, NULL) < 0){ //Criação do mutex 
        return -1;
    }
    //Inicializar mutex da queue
    if(pthread_mutex_init(&queue_lock, NULL) < 0){ //Criação do mutex 
        return -1;
    }
    //Inicializar variavel de condicao da queue
    if(pthread_cond_init(&queue_not_empty, NULL) < 0){
        return -1;
    }

    return 0;
}
//Metodo para destruir os mutexes e variaveis de condicao
void mutex_cond_destroy(){
    //Inicializar mutex da queue
    pthread_mutex_destroy(&queue_lock);
    //Inicializar mutex da tree
    pthread_mutex_destroy(&tree_lock);
    //Destruir variavel de condicao da queue
    pthread_cond_destroy(&queue_not_empty);
}

//Metodo do thread que vai processar pedidos de escrita.
void *process_task(){
    while(1){
        struct task_t *task = getTask();
        if(task == NULL){
            pthread_exit(NULL);//Entra nesta condicao quando o servidor eh terminado com SIGINT (fazemos signal ah variavel de condicao queue_not_empty)
        }
        //Dar lock no mutex da tree
        pthread_mutex_lock(&tree_lock);
        if(task -> op == 0){// É o Del
            tree_del(tree, task -> key);
            if(backup != NULL){
                printf("BACKUP\n");
                rtree_del(backup, strdup(task -> key));
            }
            op_count++; //Mesmo se der erro devemos contar a operacao como executada
        }

        if(task -> op == 1){// É o put
            struct data_t *data = data_create2(task -> datasize, task -> data);
            tree_put(tree, task -> key, data);
            if(backup != NULL){
                printf("BACKUP\n");
                struct entry_t *entrada = entry_create(strdup(task -> key), data_dup(data));
                if(entrada != NULL){
                    rtree_put(backup, entrada);
                }  
            }
            op_count++; //Mesmo se der erro devemos contar a operacao como executada
            data_destroy(data);
        }
        task_destroy(task);
        //Dar unlock do mutex da tree
        pthread_mutex_unlock(&tree_lock);
    }

    pthread_exit(NULL);
}

struct task_t *task_create(int op_n, int op, char* key, char* data, int dataSize){
    
    if(op_n < 0 || op < 0 || key == NULL || dataSize < 0){ //Data pode ser NULL, no del
        return NULL;
    }

    struct task_t *task = malloc(sizeof(struct task_t));
    
    if(task != NULL){
        task -> op_n = op_n;
        task -> op = op;
        task -> key = strdup(key); //Nem na operacao put nem na del data eh NULL
        task -> datasize = dataSize;
        task -> nextTask = NULL;
        if(data == NULL){ //Na operacao del, data eh NULL
            task -> data = data;
        } else {
            task -> data = strdup(data);
        }
    }

    return task;
}

void task_destroy(struct task_t *tarefa){
    free(tarefa -> key);
    free(tarefa);
}

void add_task(struct task_t *task){
    if(task != NULL){
        pthread_mutex_lock(&queue_lock);
        if(queue_head == NULL){ //Adiciona na cabeça da fila
            queue_head = task;
            queue_head -> nextTask = NULL;
        } else { //Adiciona no fim da fila
            struct task_t *t = queue_head;
            while(t -> nextTask != NULL){ //encontra ultima task
                t = t -> nextTask;
            }
            t -> nextTask = task;
            task -> nextTask = NULL;
        }
        pthread_cond_signal(&queue_not_empty); //Avisa que a queue não está empty
        pthread_mutex_unlock(&queue_lock);
    }
}

struct task_t *getTask(){
    pthread_mutex_lock(&queue_lock);
    while(queue_head == NULL){ //Espera haver algo
        pthread_cond_wait(&queue_not_empty, &queue_lock);
        if(termina_thread){ //Quando for para terminar a thread devolvemos NULL
            pthread_mutex_unlock(&queue_lock);
            return NULL;
        }
    }

    struct task_t *task = queue_head;
    queue_head = task -> nextTask;
    pthread_mutex_unlock(&queue_lock);
    return task;
}
//Verifica se a operacao identificada por op_n foi executada
//return 1 se operacao op_n jah foi executada e 0 caso contrario
int verify(int op_n){
    return op_n <= op_count;
}