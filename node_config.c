#include "node_config.h"

#include <stdlib.h>

node_config_t* create_node_config(int rank) {
    node_config_t *config = malloc(sizeof(*config));
    if (!config) {
        return NULL;
    }

    config->neighbors_config = NULL;
    config->node_id = rank;

    return config;
}

void destroy_node_config(node_config_t *config) {
	free(config->neighbors_config);

	free(config);
}