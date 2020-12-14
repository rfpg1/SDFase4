/**
 * Grupo 29
 * Ricardo Gonçalves fc52765
 * Rafael Abrantes fc52751
 * Daniel Batista fc52773
**/ 

#ifndef _TREE_CLIENT_PRIVATE_H
#define _TREE_CLIENT_PRIVATE_H

#include "zookeeper/zookeeper.h"

void connection_watcher(zhandle_t *zzh, int type, int state, const char *path, void* context);

#endif