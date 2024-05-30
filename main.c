#include <stdio.h>
#include <mpi.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

#include "neighbors_config.h"
#include "node_config.h"

#define OPEN_FILE_ERR -1
#define INVALID_FILE_FORMAT_ERR -2
#define MEMORY_ERR -3
#define INVALID_INPUT -4

#define COMMAND_LENGTH 100


#define RUN 1
#define STOP 2
#define DROP_OFFSET 1000

typedef struct leader_info {
    int node_id;
    int processing_power;
} leader_info_t;

void setup_node(node_config_t *config) {
    char neighbors_filename[] = "neighbors.txt";
    FILE *neighbors_file;

    if (!config) {
        MPI_Abort(MPI_COMM_WORLD, INVALID_INPUT);
    }
    
    neighbors_file = fopen(neighbors_filename, "rt");
    if (!neighbors_file) {
        fprintf(stderr, "Error opening file %s\n", neighbors_filename);
        MPI_Abort(MPI_COMM_WORLD, OPEN_FILE_ERR);
    }

    int current_node_id;
    uint8_t found_config = 0;
    while (fscanf(neighbors_file, "%d\n", &current_node_id) != EOF) {
        if (config->node_id != current_node_id) {
            fscanf(neighbors_file, "%*[^\n]\n");
            fscanf(neighbors_file, "%*[^\n]\n");

            continue;
        }

        found_config = 1;
        break;
    }

    // This node doesn't have neighbors
    if (!found_config) {
        neighbors_config_t *neighbors_config = create_neighbors_config(0);

        fclose(neighbors_file);

        if (!neighbors_config) {
            MPI_Abort(MPI_COMM_WORLD, MEMORY_ERR);
        }

        config->neighbors_config = neighbors_config;

        return;
    }

    int nr_of_neighbors;
    fscanf(neighbors_file, "%d", &nr_of_neighbors);

    if (nr_of_neighbors < 0) {
        MPI_Abort(MPI_COMM_WORLD, INVALID_FILE_FORMAT_ERR);
    }

    neighbors_config_t *neighbors_config = create_neighbors_config(nr_of_neighbors);
    if (!neighbors_config) {
        // Free memory
        fclose(neighbors_file);

        MPI_Abort(MPI_COMM_WORLD, MEMORY_ERR);
    }

    neighbors_config->nr_of_neighbors = nr_of_neighbors;

    for (int i = 0; i < nr_of_neighbors; ++i) {
        fscanf(neighbors_file, "%d", &neighbors_config->neighbors[i]);
    }

    fclose(neighbors_file);

    config->neighbors_config = neighbors_config;
}

void shut_down(node_config_t *node_config) {
    if (!node_config) {
        return;
    }

    destroy_neighbors_config_t(node_config->neighbors_config);
}

void initialize_requests(MPI_Request *requests, size_t nr_of_requests) {
    for (size_t i = 0; i < nr_of_requests; ++i) {
        requests[i] = MPI_REQUEST_NULL;
    }
}

int elect_leader(node_config_t *config) {
    int NUM_ROUNDS = 5;

    printf("[NODE %d] entering leader election\n", config->node_id);

    if (!config || !config->neighbors_config) {
        MPI_Abort(MPI_COMM_WORLD, INVALID_INPUT);
    }

    int node_id = config->node_id;  
    leader_info_t leader_info = {node_id, node_id};

    size_t nr_of_neighbors = config->neighbors_config->nr_of_neighbors;
    int *neighbors = config->neighbors_config->neighbors;

    MPI_Request *sent_requests = malloc(nr_of_neighbors * sizeof(*sent_requests));
    if (!sent_requests) {
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ERR);
    }

    MPI_Request *recv_requests = malloc(nr_of_neighbors * sizeof(*recv_requests) * NUM_ROUNDS);
    if (!recv_requests) {
        free(sent_requests);
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ERR);
    }

    leader_info_t *received_info = calloc(nr_of_neighbors * NUM_ROUNDS, sizeof(*received_info));
    if (!received_info) {
        free(sent_requests);
        free(recv_requests);
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ERR);
    }

    int *indices = malloc(nr_of_neighbors * sizeof(*indices));
    if (!indices) {
        free(sent_requests);
        free(recv_requests);
        free(received_info);
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ERR);
    }

    MPI_Status *statuses = malloc(nr_of_neighbors * sizeof(*statuses));
    if (!statuses) {
        free(sent_requests);
        free(recv_requests);
        free(received_info);
        free(indices);
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ERR);
    }

    for (int round = 0; round < NUM_ROUNDS; ++round) {
        initialize_requests(sent_requests, nr_of_neighbors);
        initialize_requests(recv_requests, nr_of_neighbors);

        if (!config->dropped) {
            for (size_t i = 0; i < nr_of_neighbors; ++i) {
                int neighbor_id = neighbors[i];

                MPI_Isend(&leader_info, sizeof(leader_info), MPI_BYTE, neighbor_id, 0, MPI_COMM_WORLD, &sent_requests[i]);
            }
        }

        for (size_t i = 0; i < nr_of_neighbors; ++i) {
            MPI_Irecv(&received_info[i + (round * nr_of_neighbors)], sizeof(received_info), MPI_BYTE,
            neighbors[i], 0, MPI_COMM_WORLD, &recv_requests[i + (round * nr_of_neighbors)]);
        }

        time_t start_time;
        time_t now;

        double timeout = 0.1;
        bool timeout_reached = false;

        size_t total_recv_requests = 0;
        int recv_requests_count = 0;

        time(&start_time);

        while ((total_recv_requests < nr_of_neighbors) && !timeout_reached) {
            MPI_Testsome(nr_of_neighbors, (recv_requests + round * nr_of_neighbors), &recv_requests_count, indices, statuses);

            // If some requests have completed, update leader info for these
            if (recv_requests_count > 0) {
                for (int i = 0; i < recv_requests_count; ++i) {
                    int index = indices[i];
                    if (received_info[index].processing_power > leader_info.processing_power) {
                        leader_info.processing_power = received_info[index].processing_power;
                        leader_info.node_id = received_info[index].node_id;
                    }
                }

                total_recv_requests += recv_requests_count;
            }

            time(&now);

            if (difftime(now, start_time) >= timeout) {
                timeout_reached = true;
            }

           usleep(1000);
        }
    }

    free(sent_requests);
    free(recv_requests);
    free(received_info);
    free(indices);
    free(statuses);

    printf("[NODE %d] selected leader: %d with power %d\n", config->node_id, leader_info.node_id, leader_info.processing_power);

    return leader_info.node_id;
}

int encode_drop_command(int node_id) {
    return node_id + DROP_OFFSET;
}

int decode_drop_command(int command) {
    return command - DROP_OFFSET;
}

int main(int argc, char *argv[]) {
    int rank, size;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    node_config_t *config = create_node_config(rank);
    if (!config) {
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ERR);
    }

    setup_node(config);

    elect_leader(config);

    bool running = true;
    char shell_command[COMMAND_LENGTH];

    int command;

    bool dropped = false;

    while (running) {
        if (rank != 0) {
            MPI_Bcast(&command, 1, MPI_INT, 0, MPI_COMM_WORLD);

            if (command == STOP) {
                break;
            }

            if (command == RUN) {
                elect_leader(config);
            }

            if (command >= DROP_OFFSET) {
                int dropped_node_id = decode_drop_command(command);

                if (rank == dropped_node_id) {
                    printf("[NODE %d] Shutting down\n", rank);
                    dropped = true;
                }
            }

            continue;
        }

        if (fgets(shell_command, COMMAND_LENGTH, stdin) != NULL) {
            shell_command[strlen(shell_command) - 1] = 0;

            printf("%s\n", shell_command);

            if (!strcmp(shell_command, "run")) {
                command = RUN;
                MPI_Bcast(&command, 1, MPI_INT, 0, MPI_COMM_WORLD);

                elect_leader(config);

                continue;
            }
            if (!strcmp(shell_command, "stop")) {
                printf("Stopping...\n");

                command = STOP;
                MPI_Bcast(&command, 1, MPI_INT, 0, MPI_COMM_WORLD);

                break;
            }

            if (strstr(shell_command, "drop")) {
                int node_id;
                sscanf(shell_command, "drop %d", &node_id);

                printf("Dropping node %d\n", node_id);

                command = encode_drop_command(node_id);
                MPI_Bcast(&command, 1, MPI_INT, 0, MPI_COMM_WORLD);
            }
        }
    }

    shut_down(config);

    MPI_Finalize();

    return 0;
}
