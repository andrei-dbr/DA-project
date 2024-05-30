#ifndef NEIGHBORS_CONFIG_H
#define NEIGHBORS_CONFIG_H

#include <stddef.h>

typedef struct neighbors_config {
    size_t nr_of_neighbors;
    int *neighbors;
} neighbors_config_t;

neighbors_config_t* create_neighbors_config(size_t size);
void destroy_neighbors_config_t(neighbors_config_t *nc);

#endif