#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// void compress(char* dest, size_t dest_size, src, src_size);
// void decompress(dest, dest_size, src, src_size);

typedef struct node_t node_t;

struct node_t {
    node_t* left;
    node_t* right;
    int weight;
    char c;
};

typedef struct {
    node_t* nodes[256];
    size_t size;
} all_nodes_t;

node_t* find_node_by_c(all_nodes_t* all_nodes, char c) {
    for (size_t i = 0; i < 256; i++) {
        if (all_nodes->nodes[i] == NULL) {
            return NULL;
        }

        if (all_nodes->nodes[i]->c == c) {
            return all_nodes->nodes[i];
        }
    }
}

node_t* init_node() {
    node_t* node = malloc(sizeof(node_t));
    memset(node, 0, sizeof(node_t));
}
size_t take_min_node_index(all_nodes_t* all_nodes) {
    size_t min_node_index = 0;

    for (size_t i = 1; i < 256; i++) {
        node_t* node = all_nodes->nodes[i];
        node_t* min_node = all_nodes->nodes[min_node_index];
        if (node->weight < min_node->weight) {
            min_node_index = i;
        }
    }

    all_nodes->nodes[min_node_index] = NULL;
    all_nodes->size--;

    return min_node_index;
}

node_t* build_tree(char* text, size_t text_len, all_nodes_t* all_nodes) {
    for (size_t i = 0; i < text_len; i++) {
        char c = text[i];

        node_t* node = all_nodes->nodes[c];

        if (node == NULL) {
            node = init_node();
            node->c = c;

            all_nodes->nodes[c] = node;
            all_nodes->size++;
        }

        node->weight++;
    }

    // while(all_nodes->size > 1) {
    //     size_t min_node_index1 = take_min_node_index(all_nodes);
    //     size_t min_node_index2 = take_min_node_index(all_nodes);

    //     node_t* pair_node = init_node();
    //     pair_node->left  = all_nodes->nodes[min_node_index1];
    //     pair_node->right = all_nodes->nodes[min_node_index1];

    //     all_nodes->nodes[min_node_index1] = NULL;
    //     all_nodes->nodes[min_node_index2] = pair_node;

    //     all_nodes->size--;
    //     printf('size %d\n', all_nodes->size);
    // }
}

int main() {
    char* text = "hello";
    size_t text_len = strlen(text);

    node_t* nodes[256];
    memset(nodes, NULL, sizeof(node_t*) * 256);

    all_nodes_t all_nodes = {
        .size = 0,
        .nodes = nodes,
    };
    
    node_t* root = build_tree(text, text_len, &all_nodes);

    for (size_t i = 0; i < 256; i++) {
        node_t* node = nodes[i];
        if (node != NULL) {
            printf("%d) char %c, %d\n", i, node->c, node->weight);
        }
    }

    return 0;
}
