#ifndef NODE_CONFIG_H
#define NODE_CONFIG_H

#include "neighbors_config.h"

#include <stddef.h>
#include <stdbool.h>

typedef struct node_config {
    neighbors_config_t *neighbors_config;

    int node_id;

    bool dropped;
} node_config_t;

node_config_t* create_node_config(int node_id);
void destroy_node_config(node_config_t *node_config);

#endif