#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>

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
    uint64_t code;
    int size;
} code_t;

typedef struct {
    void* items;
    size_t bits_len;
    size_t bits_read;
} read_src_t;

typedef struct {
    void* items;
    size_t bits_written;
} write_dest_t;

void read_bits(read_src_t* src, uint64_t* dest, size_t n) {
    int off = src->bits_read & 7;
    uint64_t mask = (1 << n) - 1;

    int item_index = src->bits_read >> 3;
    uint8_t* item = &((uint8_t*)src->items)[item_index];
    uint64_t block = *(uint64_t*)item;

    *dest = ((block >> off) & mask);

    src->bits_read += n;
}

void write_bits(write_dest_t* dest, uint64_t block, size_t n) {
    int rem = dest->bits_written & 7;

    uint8_t* items = &((uint8_t*)dest->items)[dest->bits_written >> 3];
    uint64_t* item = (uint64_t*)items;

    *item = *item | (block << rem);
    dest->bits_written += n;
}

void print_bits(uint64_t code, size_t size) {
    uint64_t mask = 1ULL << (size - 1);

    for (int i = 0; i < size; i++) {
        if (code & mask) {
            printf("1");
        } else {
            printf("0");
        }
        mask = mask >> 1;
    }
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

void build_prefix_codes(node_t* root, code_t* path, code_t* codes_mapping) {
    if (root->left || root->right) {
        if (root->left) {
            path->size++;
            path->code = path->code << 1;

            build_prefix_codes(root->left, path, codes_mapping);

            path->code = path->code >> 1;
            path->size--;
        }

        if (root->right) {
            path->size++;
            path->code = path->code << 1;
            path->code++;

            build_prefix_codes(root->right, path, codes_mapping);

            path->code = path->code >> 1;
            path->size--;
        }


    } else {
        codes_mapping[(int)root->c] = *path;
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

    code_t path = {
        .code = 0,
        .size = 0,
    };

    code_t codes_mapping[256] = {0};

    build_prefix_codes(root, &path, codes_mapping);

    uint64_t compressed[256] = {0};
    write_dest_t compressed_dest = {
        .bits_written = 0,
        .items = compressed,
    };

    for (size_t i = 0; i < text_len; i++) {
        char c = text[i];

        code_t code = codes_mapping[(int)c];
        write_bits(&compressed_dest, code.code, code.size);

        printf("%c - ", c);
        print_bits(code.code, code.size);
        printf("\n");
    }

    //printf("n %lld\n", compressed[0]);

    //print_bits(compressed[0], 64);
    //printf("\n");

    //printf("%lld\n", compressed[0]);

    /*
    for (size_t i = 0; i < 255; i++) {
        code_t code = codes_mapping[i];
        if (code.size) {
            // char c = (char)i;
            //printf("%c (%d) ", c, code.size);
            //print_bits(code.code, code.size);
            //printf("\n");


        }
    }
    */

    read_src_t read_src = {
      .items = compressed,
      .bits_len = 8 * 256,
      .bits_read = 0,
    };

    uint64_t buf = 0;
    for (size_t i = 0; i < 126; i++) {
        read_bits(&read_src, &buf, 1);

        if (buf) {
            printf("1");
        } else {
            printf("0");
        }

        buf = 0ULL;
    }
    printf("\n");

    return 0;
}
