#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define CODES_TABLE_SIZE sizeof(code_t) * 256

size_t compress(void* dest, size_t dest_size, char* src, size_t src_size);
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
    uint64_t size;
} code_t;

typedef struct {
    void* items;
    size_t bits_off;
} bits_src_t;

uint64_t revert_bits(uint64_t code, size_t size) {
    uint64_t result = 0;
    for (size_t i = 0; i < size; i++) {
        if (code & (1 << i)) {
            result |= (1 << (size - i - 1));
        }
    }
    return result;
}

void read_bits(bits_src_t* src, uint64_t* dest, size_t n) {
    int off = src->bits_off & 7;
    uint64_t mask = (1 << n) - 1;

    int item_index = src->bits_off >> 3;
    uint8_t* item = &((uint8_t*)src->items)[item_index];
    uint64_t block = *(uint64_t*)item;

    *dest = ((block >> off) & mask);

    src->bits_off += n;
}

void write_bits(bits_src_t* dest, uint64_t block, size_t n) {
    uint8_t* items = &((uint8_t*)dest->items)[dest->bits_off >> 3];
    uint64_t* item = (uint64_t*)items;

    int rem = dest->bits_off & 7;
    *item |= (block << rem);
    dest->bits_off += n;
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

void build_freq_table(char* text, size_t text_size, all_nodes_t* all_nodes) {
    for (size_t i = 0; i <= text_size; i++) {
        unsigned char c = text[i];
        uint8_t node_index = (int)c;

        // printf("node_index %d\n", node_index);
        node_t* node = all_nodes->nodes[node_index];

        if (node == NULL) {
            node = init_node();
            node->c = c;

            all_nodes->nodes[node_index] = node;
            all_nodes->size++;
        }
// 
        node->weight++;
    }
}

node_t* build_tree(all_nodes_t* all_nodes) {
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

    node_t* root = all_nodes->nodes[find_min_node_index(all_nodes)];
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
            path->code |= 1;

            build_prefix_codes(root->right, path, codes_mapping);

            path->code = path->code >> 1;
            path->size--;
        }
    } else {
        codes_mapping[(int)root->c] = *path;
    }
        
    return;
}

size_t read_file_size(FILE* fd) {
    fseek(fd, 0, SEEK_END);
    size_t text_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    return text_size;
}

size_t compress(void* dest, size_t dest_size, char* src, size_t src_size) {
    return 0;
}

int main() {
    FILE *fd_in = fopen("./files/pg100.txt", "r");
    assert(fd_in != NULL && "Error open file");

    size_t text_size = read_file_size(fd_in);
    printf("source: %ld bytes\n", text_size);

    uint64_t* compressed = malloc(text_size);
    memset(compressed, 0, text_size);

    char text[text_size + 1];
    int read_bytes = fread(text, sizeof(char), text_size, fd_in);
	if (read_bytes == -1) {
	    perror("read");
	    exit(EXIT_FAILURE);
	}

    text[text_size] = '\0';
    
    fclose(fd_in);

    // Compress ------------------

    //size_t compressed_size = compress(compressed, text_size, text, text_size);

    all_nodes_t all_nodes = {
        .nodes = {0},
        .size = 0,
    };

    build_freq_table(text, text_size, &all_nodes);

    node_t* root = build_tree(&all_nodes);

    code_t path = {0};

    code_t codes_table[256] = {0};

    build_prefix_codes(root, &path, codes_table);
    
    bits_src_t compressed_dest = {
        .items = compressed,
        .bits_off = 0,
    };

    for (size_t i = 0; i <= text_size; i++) {
        char c = text[i];

        code_t code = codes_table[(int)c];
        write_bits(&compressed_dest, revert_bits(code.code, code.size), code.size);

    //     printf("%c - ", c);
    //     print_bits(code.code, code.size);
    //     printf("\n");
    }

    size_t compressed_size = compressed_dest.bits_off / 8 + 1;

    // Save compressed to a file

    FILE *fd_out = fopen("./result", "w+");
    if (fd_out == NULL) {
        perror("open result file");
        exit(1);
    }

    size_t bytes_written;

    // Write codes table to the beginnig of the file
    // printf("codes table size: %ld bytes\n", CODES_TABLE_SIZE);
    // bytes_written = fwrite(codes_table, sizeof(code_t), 256, fd_out);
    // if (bytes_written < compressed_size) {
    //     if (ferror(fd_out)) {
    //         perror("write compressed");
    //     }
    //     perror("Not all bytes were written");
    //     exit(1);
    // }

    printf("compressed data size: %ld bytes\n", compressed_size);
    bytes_written = fwrite(compressed, sizeof(char), compressed_size, fd_out);
    if (bytes_written < compressed_size) {
        if (ferror(fd_out)) {
            perror("write compressed");
        }
        perror("Not all bytes were written");
        exit(1);
    }

    // TODO: free the tree

    free(compressed);

    fclose(fd_out);

    // Decompress -------------------
    fd_in = fopen("./result", "r");
    if (fd_in == NULL) {
        perror("open result file");
        exit(1);
    }

    compressed_size = read_file_size(fd_in);

    compressed = realloc(compressed, compressed_size);
    assert(compressed != NULL && "malloc read compressed");
    memset(compressed, 0, compressed_size);

    read_bytes = fread(compressed, sizeof(char), compressed_size, fd_in);
	if (read_bytes == -1) {
	    perror("read");
	    exit(EXIT_FAILURE);
	}

    printf("bytes read %ld\n", read_bytes);

    bits_src_t read_src = {
      .items = compressed,
      .bits_off = 0,
    };

    node_t* curr_node = root;

    uint64_t curr_bit = 0;

    for (;;) {
        read_bits(&read_src, &curr_bit, 1);

        // printf("%ld", curr_bit);

        curr_node = curr_bit ? curr_node->right : curr_node->left;
        
        if (!curr_node->left && !curr_node->right) {
            char c = curr_node->c;
            curr_node = root;

            printf("%c", c);
            if (c == '\0') {
                return 0;
            }
        }

        curr_bit = 0ULL;
    }

    fclose(fd_in);
    printf("\n");

    return 0;
}
