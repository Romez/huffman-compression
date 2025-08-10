#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define FREQ_TABLE_SIZE 256

size_t compress(void* dest, size_t dest_size, char* src, size_t src_size);
// void decompress(dest, dest_size, src, src_size);

typedef uint32_t freq_t;

typedef struct node_t node_t;

struct node_t {
    node_t* left;
    node_t* right;
    int weight;
    uint8_t c;
};

// TODO: heap
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

    for (size_t i = 0; i < size; i++) {
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

void build_freq_table(char* text, size_t text_size, freq_t* freq_table) {
    for (size_t i = 0; i <= text_size; i++) {
        unsigned char c = text[i];
        uint8_t node_index = (int)c;

        freq_table[node_index]++;
    }
}

node_t* build_tree(freq_t* freq_table) {
    all_nodes_t all_nodes = {
        .nodes = {0},
        .size = 0,
    };

    for (int i = 0; i < 256; i++) {
        if (freq_table[i]) {
            node_t* node = init_node();

            node->c = (char)i;
            node->weight = freq_table[i];

            all_nodes.nodes[all_nodes.size++] = node;
        }
    }

    // printf("node_index %d\n", node_index);

    while(all_nodes.size > 1) {
        size_t min_node_index1 = find_min_node_index(&all_nodes);
        node_t* min_node1 = all_nodes.nodes[min_node_index1];
        all_nodes.nodes[min_node_index1] = NULL;
        
        size_t min_node_index2 = find_min_node_index(&all_nodes);
        node_t* min_node2 = all_nodes.nodes[min_node_index2];
        all_nodes.nodes[min_node_index2] = NULL;

        node_t* pair_node = init_node();
        pair_node->left  = min_node1;
        pair_node->right = min_node2;
        pair_node->weight = min_node1->weight + min_node2->weight;

        all_nodes.nodes[min_node_index1] = pair_node;

        all_nodes.size--;
    }

    node_t* root = all_nodes.nodes[find_min_node_index(&all_nodes)];
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
        
        // printf("%c (%08b) [% 10ld]- ", root->c, (unsigned char)root->c, root->weight);
        // print_bits(path->code, path->size);
        // printf("\n");
    }
        
    return;
}

size_t read_file_size(FILE* fd) {
    fseek(fd, 0, SEEK_END);
    size_t text_size = ftell(fd);
    fseek(fd, 0, SEEK_SET);
    return text_size;
}

void free_tree(node_t* node) {
    if (node->left) {
        free_tree(node->left);
    }
    
    if (node->right) {
        free_tree(node->right);
    }
    
    free(node);
}

size_t compress(void* dest, size_t dest_size, char* src, size_t src_size) {
    return 0;
}

char* read_src_file(char* file_path, size_t* text_size) {
    FILE *fd_src = fopen(file_path, "r");
    if (fd_src == NULL) {
        perror("open src file");
        exit(1);
    }

    size_t n = read_file_size(fd_src);
    // printf("source: %ld bytes\n", n);

    char* src = malloc(n + 1);
    if (src == NULL) {
        perror("malloc text");
        exit(1);
    }

    int read_bytes = fread(src, sizeof(char), n, fd_src);
    if (read_bytes < n) {
        perror("read");
        exit(EXIT_FAILURE);
    }

    src[n] = '\0';

    fclose(fd_src);

    *text_size = n;

    return src;
}

int main() {
    // Compress ------------------

    {
        FILE *fd_dest = fopen("result", "w+");
        if (fd_dest == NULL) {
            perror("open result file");
            exit(1);
        }

        size_t text_size;
        char* text = read_src_file("files/nska.utf.txt", &text_size);

        // divide by 2 if > uint_8/uint_16
        freq_t freq_table[FREQ_TABLE_SIZE] = {0};
        build_freq_table(text, text_size, freq_table);

        node_t* root = build_tree(freq_table);
        
        code_t path = {0};
        code_t codes_table[256] = {0};
        build_prefix_codes(root, &path, codes_table);
        
        // Write frequency table to the beginnig of the file
        size_t items_written = fwrite(freq_table, sizeof(freq_t), FREQ_TABLE_SIZE, fd_dest);
        if (items_written < FREQ_TABLE_SIZE) {
            if (ferror(fd_dest)) {
                perror("write frequency table");
            } else {
                printf("Not all freqs were written %ld/%d\n", items_written, FREQ_TABLE_SIZE);
            }
            exit(1);
        }

        uint64_t* compressed = malloc(text_size + 1 + (FREQ_TABLE_SIZE * sizeof(freq_t)));
        if (compressed == NULL) {
            perror("malloc src");
            exit(1);
        }

        memset(compressed, 0, text_size);
        
        bits_src_t compressed_dest = {
            .items = compressed,
            .bits_off = 0,
        };

        size_t counter = FREQ_TABLE_SIZE * sizeof(freq_t) * 8;
        printf("text_size %ld \n", text_size);
        
        for (size_t i = 0; i <= text_size; i++) {
            uint64_t c = text[i];

            code_t code = codes_table[(uint8_t)c];
            write_bits(&compressed_dest, revert_bits(code.code, code.size), code.size);

            counter += code.size;
            // printf("%c - ", c);
            // print_bits(code.code, code.size);
            // printf("\n");
        }
        printf("written: %ld bits\n", counter);      

        size_t compressed_size = (compressed_dest.bits_off + 7) / 8;

        printf("compressed data size: %ld bytes\n", compressed_size);
        size_t bytes_written = fwrite(compressed, sizeof(uint8_t), compressed_size, fd_dest);
        if (bytes_written < compressed_size) {
            if (ferror(fd_dest)) {
                perror("write compressed");
            }
            perror("Not all bytes were written");
            exit(1);
        }

        free_tree(root);

        fclose(fd_dest);
    }

    printf("Decompress -------------------\n");
    // Decompress -------------------
    {
        FILE* fd_in = fopen("result", "r");
        if (fd_in == NULL) {
            perror("open result file");
            exit(1);
        }

        size_t compressed_size = read_file_size(fd_in);

        void* compressed = malloc(compressed_size * 3);
        assert(compressed != NULL && "malloc read compressed");
        memset(compressed, 0, compressed_size);

        size_t read_bytes = fread(compressed, sizeof(char), compressed_size, fd_in);
        if (read_bytes == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        // Read frequency table;

        freq_t freq_table[FREQ_TABLE_SIZE] = {0};
        memcpy(freq_table, compressed, FREQ_TABLE_SIZE * sizeof(freq_t));

        node_t* root = build_tree(freq_table);

        bits_src_t read_src = {
            .items = &((uint8_t*)compressed)[FREQ_TABLE_SIZE * sizeof(freq_t)],
            .bits_off = 0,
        };

        node_t* curr_node = root;

        uint64_t curr_bit = 0;

        for (;;) {
            read_bits(&read_src, &curr_bit, 1);

            // printf("%ld", curr_bit);

            curr_node = curr_bit ? curr_node->right : curr_node->left;
            
            if (!curr_node->left && !curr_node->right) {
                unsigned char c = curr_node->c;
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
    }

    return 0;
}
