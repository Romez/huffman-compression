#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>

#define FREQ_TABLE_SIZE 256
#define READ_BUF_SIZE 1024
#define WRITE_BUF_SIZE 1024

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

typedef struct {
    void* mem;
    size_t cap;
    size_t off;
} arena_t;

void alloc_arena(arena_t* arena) {
    arena->mem = malloc(arena->cap);
    if (arena->mem == NULL) {
        perror("arena");
        exit(1);
    }
}

void* alloc_arena_block(arena_t* arena, size_t block_size) {
    uint8_t* bytes = (uint8_t*)arena->mem;
    arena->off = arena->off + block_size;
    return &bytes[arena->off];
}

void free_arena(arena_t* arena) {
    free(arena->mem);
}

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

bool is_leaf(node_t* node) {
    return !node->left && !node->right;
}

node_t* init_node(arena_t* arena) {
    node_t* node = alloc_arena_block(arena, sizeof(node_t));

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

node_t* build_tree(arena_t* arena, freq_t* freq_table) {
    all_nodes_t all_nodes = {
        .nodes = {0},
        .size = 0,
    };

    for (int i = 0; i < FREQ_TABLE_SIZE; i++) {
        if (freq_table[i]) {
            node_t* node = init_node(arena);

            node->c = (char)i;
            node->weight = freq_table[i];

            all_nodes.nodes[all_nodes.size++] = node;
        }
    }

    while(all_nodes.size > 1) {
        size_t min_node_index1 = find_min_node_index(&all_nodes);
        node_t* min_node1 = all_nodes.nodes[min_node_index1];
        all_nodes.nodes[min_node_index1] = NULL;
        
        size_t min_node_index2 = find_min_node_index(&all_nodes);
        node_t* min_node2 = all_nodes.nodes[min_node_index2];
        all_nodes.nodes[min_node_index2] = NULL;

        node_t* pair_node = init_node(arena);
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

size_t compress(void* dest, size_t dest_size, char* src, size_t src_size) {
    return 0;
}

void flush(FILE* fd, void* buf, size_t size, size_t n) {
    do {
        size_t items_written = fwrite(buf, size, n, fd);
        if (items_written < n) {
            if (ferror(fd)) {
                perror("flush");
                exit(1);
            }
        }
        n -= items_written;
    } while(n);
}

int main() {
    {
        char* src_file_path = "./files/lorem.txt";
        char* dest_file_path = "./files/compressed";

        FILE *fd_dest = fopen(dest_file_path, "w+");
        if (fd_dest == NULL) {
            perror("open result file");
            exit(1);
        }

        FILE *fd_src = fopen(src_file_path, "r");
        if (fd_src == NULL) {
            perror("open src file");
            exit(1);
        }

        arena_t arena = {
            .cap = 512 * sizeof(node_t),
            .off = 0,
        };
        alloc_arena(&arena);

        freq_t freq_table[FREQ_TABLE_SIZE] = {0};
        freq_table[FREQ_TABLE_SIZE] = 1;
        size_t text_size = 0;

        char read_buf[READ_BUF_SIZE];
        size_t bytes_read = 0;

        while((bytes_read = fread(read_buf, sizeof(char), READ_BUF_SIZE, fd_src))) {
            if (bytes_read == -1) {
                perror("read src");
                exit(EXIT_FAILURE);
            }

            for (size_t i = 0; i <= bytes_read; i++) {
                unsigned char c = read_buf[i];
                uint8_t node_index = (int)c;

                freq_table[node_index]++;
            }

            text_size += bytes_read;
        }
        
        node_t* root = build_tree(&arena, freq_table);
        
        code_t path = {0};
        code_t codes_table[256] = {0};
        build_prefix_codes(root, &path, codes_table);

        printf("text size %ld bytes\n", text_size);

        // Write text size
        flush(fd_dest, &text_size, sizeof(text_size), 1);

        // Write frequency table to the beginnig of the file
        flush(fd_dest, freq_table, sizeof(freq_t), FREQ_TABLE_SIZE);

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

        fseek(fd_src, 0, SEEK_SET);

        while((bytes_read = fread(read_buf, sizeof(char), READ_BUF_SIZE, fd_src))) {
            if (bytes_read == -1) {
                perror("read src");
                exit(EXIT_FAILURE);
            }

            for (size_t i = 0; i <= bytes_read; i++) {
                unsigned char c = read_buf[i];
                
                code_t code = codes_table[(uint8_t)c];

                write_bits(&compressed_dest, revert_bits(code.code, code.size), code.size);

                counter += code.size;
            }   
        }

        printf("written: %ld bits\n", counter);      

        size_t compressed_size = (compressed_dest.bits_off + 7) / 8;

        printf("compressed data size: %ld bytes\n", compressed_size);
        flush(fd_dest, compressed, sizeof(uint8_t), compressed_size);

        free_arena(&arena);

        fclose(fd_dest);

        fclose(fd_src);
    }

    printf("Decompress -------------------\n");
    // Decompress -------------------
    {
        char* src_file_path = "./files/compressed";
        char* dest_file_path = "./files/decompressed.txt";

        FILE* fd_src = fopen(src_file_path, "r");
        if (fd_src == NULL) {
            perror("open compressed file");
            exit(1);
        }

        FILE* fd_dest = fopen(dest_file_path, "w+");
        if (fd_dest == NULL) {
            perror("open decompressed file");
            exit(1);
        }

        size_t compressed_size = read_file_size(fd_src);

        void* compressed = malloc(compressed_size * 3);
        assert(compressed != NULL && "malloc read compressed");
        memset(compressed, 0, compressed_size);

        size_t text_size;
        if (fread(&text_size, sizeof(size_t), 1, fd_src) < 1) {
            perror("read text size");
            exit(EXIT_FAILURE);
        }

        printf("text size: %ld bytes\n", text_size);

        size_t read_bytes = fread(compressed, sizeof(char), compressed_size, fd_src);
        if (read_bytes == -1) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        arena_t arena = {
            .cap = 512 * sizeof(node_t),
            .off = 0,
        };
        alloc_arena(&arena);

        // Read frequency table;

        freq_t freq_table[FREQ_TABLE_SIZE] = {0};
        memcpy(freq_table, compressed, FREQ_TABLE_SIZE * sizeof(freq_t));

        node_t* root = build_tree(&arena, freq_table);

        bits_src_t read_src = {
            .items = &((uint8_t*)compressed)[FREQ_TABLE_SIZE * sizeof(freq_t)],
            .bits_off = 0,
        };

        node_t* curr_node = root;

        uint64_t curr_bit = 0;

        char write_buf[WRITE_BUF_SIZE];
        size_t write_buf_len = 0;

        size_t chars_read = 0;

        while (chars_read < text_size) {
            read_bits(&read_src, &curr_bit, 1);

            curr_node = curr_bit ? curr_node->right : curr_node->left;

            if (is_leaf(curr_node)) {
                unsigned char c = curr_node->c;

                write_buf[write_buf_len++] = c;

                if (write_buf_len == WRITE_BUF_SIZE) {
                    flush(fd_dest, write_buf, sizeof(char), write_buf_len);
                }

                chars_read++;

                curr_node = root;
            }
        }

        flush(fd_dest, write_buf, sizeof(char), write_buf_len);

        free_arena(&arena);

        fclose(fd_src);
        printf("\n");
    }

    return 0;
}
