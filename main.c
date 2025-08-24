#include <stddef.h>
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

typedef uint64_t freq_t;

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

size_t leb_encode(uint8_t* arr, uint64_t n) {
    uint64_t group_mask = 0x7f;
    uint64_t next_seg_mask = 0x80;

    uint64_t val = n;
    size_t bytes_written = 0;

    do {
        uint64_t segment = 0;
        segment = (~((val >> 7) - 1) >> 56) & next_seg_mask;

        segment |= val & group_mask;
        arr[bytes_written++] = segment;

        val >>= 7;
    } while(val);
    return bytes_written;
}

uint64_t leb_decode(uint8_t* arr, uint64_t* result) {
    uint64_t group_mask = 0x7f;
    uint64_t next_seg_mask = 0x80;

    uint64_t val = 0;
    size_t bytes_read = 0;

    uint64_t segment;

    do {
        segment = arr[bytes_read];

        val |= (segment & group_mask) << (bytes_read * 7);
        bytes_read++;
    } while(segment & next_seg_mask);

    *result = val;

    return bytes_read;
}

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
    int index = all_nodes->size++;

    all_nodes->nodes[index] = node;

    while(index > 0) {
        int parent_index = (index - 1) / 2;

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
        codes_mapping[(uint8_t)root->c] = *path;
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

size_t read_file_size(FILE* fd) {
    if (fseek(fd, 0, SEEK_END) == -1) {
        perror("fseek");
        goto error;
    }

    size_t file_size = ftell(fd);
    if (file_size == -1) {
        perror("ftell");
        goto error;
    }

    if (fseek(fd, 0, SEEK_SET)) {
        perror("rewind");
        goto error;
    }

    return file_size;
error:
    fclose(fd);
    exit(1);
}


void compress(char* src_file_path, char* dest_file_path) {
    // Open src and dest files
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

    size_t text_size = read_file_size(fd_src);

    uint8_t read_buf[READ_BUF_SIZE];
    size_t bytes_read = 0;

    freq_t freq_table[FREQ_TABLE_SIZE] = {0};
    while((bytes_read = fread(read_buf, 1, READ_BUF_SIZE, fd_src)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            uint8_t node_index = read_buf[i];
            freq_table[node_index]++;
        }
    }
    if (ferror(fd_src)) {
        perror("read src");
        exit(1);
    }

    // Allocate arena for tree
    arena_t tree_arena = alloc_arena(512 * sizeof(node_t));
    node_t* root = build_tree(&tree_arena, freq_table);

    // Write text size to the file
    flush(fd_dest, &text_size, sizeof(text_size), 1);

    // Write frequency table to the of the file
    uint8_t leb_compressed[256 * sizeof(code_t)];
    size_t leb_compressed_len = 0;
    for (size_t i = 0; i < FREQ_TABLE_SIZE; i++) {
        freq_t freq = freq_table[i];
        leb_compressed_len += leb_encode(&leb_compressed[leb_compressed_len], freq);
    }
    flush(fd_dest, leb_compressed, sizeof(uint8_t), leb_compressed_len);

    // Build codes
    code_t path = {0};
    code_t codes_table[256] = {0};
    build_prefix_codes(root, &path, codes_table);

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
            exit(1);
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

    free_arena(&tree_arena);

    fclose(fd_dest);

    fclose(fd_src);
}

size_t decode_file_size(FILE* fd) {
    size_t text_size;
    if (fread(&text_size, sizeof(size_t), 1, fd) < 1) {
        perror("read text size");
        exit(1);
    }
    return text_size;
}

void decode_freq_table(FILE* fd, freq_t* freq_table) {
    for (size_t i = 0; i < FREQ_TABLE_SIZE; i++) {
        uint8_t leb_src[8] = {0};

        fread(leb_src, sizeof(uint8_t), 8, fd);

        uint64_t freq;
        size_t val_len = leb_decode(leb_src, &freq);
        freq_table[i] = freq;

        fseek(fd, val_len - 8, SEEK_CUR);
    }
}

void decode_text(FILE* fd_src, FILE* fd_dest, node_t* root, size_t text_size) {
    node_t* curr_node = root;

    size_t chars_read = 0;

    char write_buf[WRITE_BUF_SIZE];
    size_t write_buf_len = 0;

    uint8_t read_buf[READ_BUF_SIZE];
    size_t bytes_read = 0;

    do {
        bytes_read = fread(read_buf, 1, READ_BUF_SIZE, fd_src);
        if (ferror(fd_src)) {
            perror("read src");
            exit(1);
        }

        for (size_t i = 0; i < bytes_read; i++) {
            uint8_t curr_byte = read_buf[i];

            for (size_t j = 0; j < 8; j++) {
                uint8_t mask = 1 << j;
                uint8_t curr_bit = curr_byte & mask;

                curr_node = curr_bit ? curr_node->right : curr_node->left;

                if (is_leaf(curr_node)) {
                    char c = curr_node->c;

                    write_buf[write_buf_len++] = c;

                    if (write_buf_len >= WRITE_BUF_SIZE) {
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

    size_t text_size = decode_file_size(fd_src);

    freq_t freq_table[FREQ_TABLE_SIZE] = {0};
    decode_freq_table(fd_src, freq_table);

    arena_t tree_arena = alloc_arena(512 * sizeof(node_t));
    node_t* root = build_tree(&tree_arena, freq_table);

    decode_text(fd_src, fd_dest, root, text_size);

    free_arena(&tree_arena);

    fclose(fd_src);
    fclose(fd_dest);
}

int main() {
    compress("./files/pg11.txt", "./files/compressed");

    decompress("./files/compressed", "./files/decompressed.txt");

    int exit_code = system("diff files/pg11.txt files/decompressed.txt");
    printf("diff exit code %d\n", WEXITSTATUS(exit_code));

    return 0;
}
