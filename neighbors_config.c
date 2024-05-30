#include "neighbors_config.h"

#include <stdlib.h>

neighbors_config_t* create_neighbors_config(size_t size) {
    neighbors_config_t *nc = malloc(sizeof(neighbors_config_t));
    if (!nc) {
        return NULL;
    }

    nc->neighbors = malloc(sizeof(*nc->neighbors) * size);
    if (!nc->neighbors) {
        free(nc);

        return NULL;
    }

    nc->nr_of_neighbors = size;

    return nc;
}

void destroy_neighbors_config_t(neighbors_config_t *nc) {
    if (!nc) {
        return;
    }

    free(nc->neighbors);
    free(nc);
}