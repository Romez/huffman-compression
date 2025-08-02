#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

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

typedef struct {
    char code[128];
    int size;
} path_t;


node_t* find_node_by_char(all_nodes_t* all_nodes, char c) {
    for (size_t i = 0; i < 256; i++) {
        if (all_nodes->nodes[i]->c == c) {
            return all_nodes->nodes[i];
        }
    }
    return NULL;
}

node_t* init_node() {
    node_t* node = malloc(sizeof(node_t));
    memset(node, 0, sizeof(node_t));

    return node;
}

size_t find_min_node_index(all_nodes_t* all_nodes) {
    size_t min_node_index = 0;
    while(all_nodes->nodes[min_node_index] == NULL && min_node_index < 256) {
        min_node_index++;
    }

    for (size_t i = min_node_index + 1; i < 256; i++) {
        node_t* node = all_nodes->nodes[i];
        if (node != NULL) {
            node_t* min_node = all_nodes->nodes[min_node_index];
            if (node->weight < min_node->weight) {
                min_node_index = i;
            }
        }
    }

    return min_node_index;
}

node_t* build_tree(char* text, size_t text_len, all_nodes_t* all_nodes) {
    for (size_t i = 0; i < text_len; i++) {
        char c = text[i];
        int node_index = (int)c;

        node_t* node = all_nodes->nodes[node_index];

        if (node == NULL) {
            node = init_node();
            node->c = c;

            all_nodes->nodes[node_index] = node;
            all_nodes->size++;
        }

        node->weight++;
    }

    while(all_nodes->size > 1) {
        size_t min_node_index1 = find_min_node_index(all_nodes);
        node_t* min_node1 = all_nodes->nodes[min_node_index1];
        all_nodes->nodes[min_node_index1] = NULL;

        size_t min_node_index2 = find_min_node_index(all_nodes);
        node_t* min_node2 = all_nodes->nodes[min_node_index2];
        all_nodes->nodes[min_node_index2] = NULL;

        node_t* pair_node = init_node();
        pair_node->left  = min_node1;
        pair_node->right = min_node2;
        pair_node->weight = min_node1->weight + min_node2->weight;

        all_nodes->nodes[min_node_index1] = pair_node;

        all_nodes->size--;
    }

    node_t* root = NULL;
    for (size_t i = 0; i < 256; i++) {
        if (all_nodes->nodes[i] != NULL) {
            root = all_nodes->nodes[i];
            break;
        }
    }

    return root;
}

void build_prefix_codes(node_t* root, path_t* path) {
    if (root->left || root->right) {
        if (root->left) {
            path->code[path->size++] = '0';
            build_prefix_codes(root->left, path);
            path->size--;
        }

        if (root->right) {
            path->code[path->size++] = '1';
            build_prefix_codes(root->right, path);
            path->size--;
        }
    } else {
        printf("%c (%d) %.*s\n", root->c, path->size, path->size, path->code);
    }

    return;
}

int main() {
    char* text = "hello world";
    size_t text_len = strlen(text);

    all_nodes_t all_nodes = {
        .size = 0,
        .nodes = {0}
    };
    
    node_t* root = build_tree(text, text_len, &all_nodes);

    path_t path = {
        .code = {0},
        .size = 0,
    };

    build_prefix_codes(root, &path);

    // printf("root %p\n", root);
    // for (size_t i = 0; i < 256; i++) {
    //     node_t* node = all_nodes.nodes[i];
    //     if (node != NULL) {
    //         printf("%ld) char %c, %d\n", i, node->c, node->weight);
    //     }
    // }

    return 0;
}
