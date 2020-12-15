/**
 * Grupo 29
 * Ricardo Gonçalves fc52765
 * Rafael Abrantes fc52751
 * Daniel Batista fc52773
**/
Usar o mem_server1 mem_server2 e client/script para correr o trabalho a 100%
--Makefile

run: compila o ficheiro proto e gera os executáveis para o treeServer e treeClient

server1: corre o treeServer com os argumentos necessários

mem_server1: corre o treeServer com os argumentos necessários com o valgrind

server2: corre o treeServer com os argumentos necessários

mem_server2: corre o treeServer com os argumentos necessários com o valgrind

script: corre o client com os comandos predefinidos

client: corre o treeClient com os argumentos necessários

mem_client: corre o treeClient com os argumentos necessários

proto: compila o ficheiro proto, e movo o .h para a pasta include e o .c para a pasta source

clean: limpa todos os executaveis, objetos e bibliotecas

--getkeys

Optamos por serializar manualmente a lista de keys de forma a termos
mais flexibilidade. Como as keys não podem conter caracteres de dois pontos, metemos
as keys todas numa string separadas por dois pontos, ao serializar.

--protobuf

Adicionamos parametros extra na nossa mensagem para tornar a rececao de
dados mais coerente, defenindo campos expessificos para certos datos
(key, data, data_size...).

--Limitações
O Nosso trabalho para correr bem tem de se usar o valgrind pois está com um problema de segmention fault que não conseguimos encontrar
Normalmente usamos o valgrind para detetar esses erros (ex: invalid reads ou invalid free etc) mas o valgrind está a tratar do erro por nós
sendo então complicado descobrir onde está o problema