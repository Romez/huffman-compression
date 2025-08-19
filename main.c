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

arena_t alloc_arena(size_t cap) {
    void* mem = malloc(cap);
    if (mem == NULL) {
        perror("arena");
        exit(1);
    }
    
    return (arena_t){
        .mem = mem,
        .cap = cap,
        .off = 0,
    };
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
    // printf("src off %ld\n", src->bits_off);
    int off = src->bits_off & 7;
    uint64_t mask = (1 << n) - 1;

    int item_index = src->bits_off >> 3;
    uint8_t* item = &((uint8_t*)src->items)[item_index];
    // uint64_t block = *(uint64_t*)item;

    uint8_t block = *item;
    *dest = 0UL;

    *dest = ((block >> off) & mask);

    src->bits_off += n;
}

void write_bits(bits_src_t* dest, uint64_t block, size_t n) {
    while(n) {
        size_t item_index = dest->bits_off / 64; // >> 6
        uint64_t* items = (uint64_t*)dest->items;
        uint64_t* item = &items[item_index];

        size_t curr_item_off = dest->bits_off % 64;

        *item |= (block << curr_item_off);

        size_t bits_avail = 64 - curr_item_off;

        size_t bits_to_write = bits_avail >= n ? n : bits_avail;
        dest->bits_off += bits_to_write;
        block = block >> bits_to_write;
        n -= bits_to_write;
    }
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

void swap_nodes(node_t** nodes, size_t index1, size_t index2) {
    node_t* tmp = nodes[index1];
    nodes[index1] = nodes[index2];
    nodes[index2] = tmp;
}

void push_node(all_nodes_t* all_nodes, node_t* node) {
    size_t index = all_nodes->size++;

    all_nodes->nodes[index] = node;

    while(index > 0) {
        size_t parent_index = (index - 1) / 2;

        if (all_nodes->nodes[parent_index]->weight < all_nodes->nodes[index]->weight) {
            break;
        }

        swap_nodes(all_nodes->nodes, parent_index, index);

        index = parent_index;
    }
}

node_t* pop_node(all_nodes_t* all_nodes) {
    node_t* top = all_nodes->nodes[0];
    all_nodes->size--;

    all_nodes->nodes[0] = all_nodes->nodes[all_nodes->size];

    size_t index = 0;

    while(true) {
        size_t left_index = index * 2 + 1;
        size_t right_index = index * 2 + 2;

        if (left_index >= all_nodes->size) left_index = all_nodes->size;
        if (right_index >= all_nodes->size) right_index = all_nodes->size;

        int curr_weight = all_nodes->nodes[index]->weight;
        int left_weight = all_nodes->nodes[left_index]->weight;
        int right_weight = all_nodes->nodes[right_index]->weight;

        if (left_weight >= curr_weight && right_weight >= curr_weight) break;

        size_t swap_index = left_weight < right_weight ? left_index : right_index;

        swap_nodes(all_nodes->nodes, index, swap_index);

        index = swap_index;
    }

    return top;
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

            push_node(&all_nodes, node);
        }
    }

    while (all_nodes.size > 1) {
        node_t* min_node_1 = pop_node(&all_nodes);
        node_t* min_node_2 = pop_node(&all_nodes);

        node_t* pair_node = init_node(arena);
        pair_node->left  = min_node_1;
        pair_node->right = min_node_2;

        pair_node->weight = min_node_1->weight + min_node_2->weight;

        push_node(&all_nodes, pair_node);
    }

    node_t* top = pop_node(&all_nodes);

    return top;
}

void build_prefix_codes(node_t* root, code_t* path, code_t* codes_mapping) {
    if (is_leaf(root)) {
        // printf("%c ", root->c);
        codes_mapping[(uint8_t)root->c] = *path;
        // print_bits(path->code, path->size);
        // printf("\n");
    } else {
        if (root->left != NULL) {
            path->size++;
            path->code = path->code << 1;

            build_prefix_codes(root->left, path, codes_mapping);

            path->code = path->code >> 1;
            path->size--;
        }

        if (root->right != NULL) {
            path->size++;
            path->code = path->code << 1;
            path->code |= 1;

            build_prefix_codes(root->right, path, codes_mapping);

            path->code = path->code >> 1;
            path->size--;
        }
    }
}

void flush(FILE* fd, void* buf, size_t size, size_t n) {
    size_t items_written = 0;
    do {
        items_written = fwrite((uint8_t*)buf + size * items_written, size, n, fd);
        if (items_written < n) {
            exit(1);
            // if (ferror(fd)) {
            //     perror("flush");
            //     exit(1);
            // }
        }
        n -= items_written;
    } while(n);
}

void compress(char* src_file_path, char* dest_file_path) {
    // printf("compress %s\n", src_file_path);

    FILE *fd_src = fopen(src_file_path, "r");
    if (fd_src == NULL) {
        perror("open src file");
        exit(1);
    }

    FILE *fd_dest = fopen(dest_file_path, "w+");
    if (fd_dest == NULL) {
        perror("open result file");
        exit(1);
    }

    arena_t arena = alloc_arena(512 * sizeof(node_t));

    freq_t freq_table[FREQ_TABLE_SIZE] = {0};
    size_t text_size = 0;

    uint8_t read_buf[READ_BUF_SIZE];
    size_t bytes_read = 0;

    while((bytes_read = fread(read_buf, 1, READ_BUF_SIZE, fd_src)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            uint8_t node_index = read_buf[i];
            freq_table[node_index]++;
        }

        text_size += bytes_read;
    }

    if (ferror(fd_src)) {
        perror("read src");
        exit(1);
    }

    node_t* root = build_tree(&arena, freq_table);

    // Build codes
    code_t path = {0};
    code_t codes_table[256] = {0};
    build_prefix_codes(root, &path, codes_table);

    // Write text size
    flush(fd_dest, &text_size, sizeof(text_size), 1);

    // Write frequency table to the beginnig of the file
    flush(fd_dest, freq_table, sizeof(freq_t), FREQ_TABLE_SIZE);

    // Write compressed data
    fseek(fd_src, 0, SEEK_SET);

    uint8_t write_buf[WRITE_BUF_SIZE] = {0};

    bits_src_t write_bits_buf = {
        .items = write_buf,
        .bits_off = 0,
    };

    while((bytes_read = fread(read_buf, 1, READ_BUF_SIZE, fd_src))) {
        if (bytes_read == -1) {
            perror("read src");
            exit(EXIT_FAILURE);
        }

        for (size_t i = 0; i < bytes_read; i++) {
            uint8_t c = read_buf[i];

            code_t code = codes_table[c];

            size_t bytes_size = write_bits_buf.bits_off / 8;
            if (bytes_size >= WRITE_BUF_SIZE - 1) {
                flush(fd_dest, write_buf, 1, bytes_size);

                uint8_t tmp = write_buf[bytes_size];
                memset(write_buf, 0, WRITE_BUF_SIZE);
                
                write_buf[0] = tmp;
                write_bits_buf.bits_off = write_bits_buf.bits_off % 8;
            }

            write_bits(&write_bits_buf, revert_bits(code.code, code.size), code.size);
        }
    }

    flush(fd_dest, write_buf, 1, (write_bits_buf.bits_off + 7) / 8);

    free_arena(&arena);

    fclose(fd_dest);

    fclose(fd_src);
}

void decompress(char* src_file_path, char* dest_file_path) {
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

    // read text size

    size_t text_size;
    if (fread(&text_size, sizeof(size_t), 1, fd_src) < 1) {
        perror("read text size");
        exit(EXIT_FAILURE);
    }

    // Read frequency table;
    freq_t freq_table[FREQ_TABLE_SIZE] = {0};
    if (fread(freq_table, sizeof(freq_t), FREQ_TABLE_SIZE, fd_src) < FREQ_TABLE_SIZE) {
        perror("read freq table");
        exit(EXIT_FAILURE);
    }

    arena_t arena = alloc_arena(512 * sizeof(node_t));

    node_t* root = build_tree(&arena, freq_table);
    node_t* curr_node = root;

    size_t chars_read = 0;

    char write_buf[WRITE_BUF_SIZE];
    size_t write_buf_len = 0;

    uint8_t read_buf[READ_BUF_SIZE];
    size_t bytes_read = 0;

    do {
        bytes_read = fread(read_buf, 1, READ_BUF_SIZE, fd_src);
        if (bytes_read == -1) {
            perror("read src");
            exit(EXIT_FAILURE);
        }

        bits_src_t read_src = {
            .items = read_buf,
            .bits_off = 0,
        };

        for (size_t i = 0; i < bytes_read; i++) {
            uint64_t curr_byte = 0;
            read_bits(&read_src, &curr_byte, 8);

            for (size_t j = 0; j < 8; j++) {
                uint8_t mask = 1 << j;
                uint8_t curr_bit = curr_byte & mask;

                curr_node = curr_bit ? curr_node->right : curr_node->left;

                if (is_leaf(curr_node)) {
                    char c = curr_node->c;

                    write_buf[write_buf_len++] = c;

                    if (write_buf_len == WRITE_BUF_SIZE) {
                        flush(fd_dest, write_buf, 1, write_buf_len);
                        write_buf_len = 0;
                    }

                    chars_read++;
                    if (chars_read == text_size) break;
                    
                    curr_node = root;
                }
            }
        }
    } while(bytes_read);

    flush(fd_dest, write_buf, 1, write_buf_len);

    free_arena(&arena);

    if (fclose(fd_src) != 0) {
        perror("fclose src");
        exit(1);
    }

    if (fclose(fd_dest) != 0) {
        perror("fclose dest");
        exit(1);
    }
}

int main() {
    compress("./files/pg11.txt", "./files/compressed");

    decompress("./files/compressed", "./files/decompressed.txt");

    int exit_code = system("diff files/pg11.txt files/decompressed.txt");
    printf("diff exit code %d\n", WEXITSTATUS(exit_code));

    return 0;
}
