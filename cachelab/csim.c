/**
 * @file csim.c
 * @author harvey-zhou
 * @brief Cache Simulator for cachelab(cmu 15213) 
 */
#include <stdio.h>

#include "cachelab.h"
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

/* Global variables to count the number hits, misses and evictions */
int hits;
int misses;
int evictions;

typedef struct  //struct of line in sets 
{
    int valid;
    unsigned int tag;
    int LRU_age;
} cache_line;

typedef struct //struct of sets in cache
{
    int E;
    cache_line* lines;
} cache_set;

typedef struct //struct of cache
{
    cache_set* sets;
    uint64_t tag_mask;
    uint64_t set_mask;
    int S;
    int E;
    int B;
    int set_idx;
    int block_offset;
} cache;

/**
 * @brief Generate mask to get tag/set bits in a cache address
 * @param[in]  size   size of the mask  
 * @param[in]  bias   offset where the mask should start
 * @return generated mask in uint64_t 
 */
uint64_t generate_mask(int size, int bias){
    uint64_t res = 0;
    for(int i=0; i<size; i++){
        res |= 0x1;
        res <<= 1;
    }
    res >>= 1;
    res <<= bias;
    return res;
}

/**
 * @brief Create a new cache simlulaor based on three params
 * @param[in] s  
 * @param[in] E 
 * @param[in] b 
 * @return pointer to the new cache simulator 
 */
cache *init_cache(int s, int E, int b)
{
    cache *c = malloc(sizeof(cache));
    int S = (1 << s);
    c->sets = (cache_set *)malloc(sizeof(cache_set) * (unsigned long)S);
    //init sets
    for(int i=0; i<S; i++){
        cache_set *set = c->sets + i;
        set->E = E;
        set->lines = (cache_line *)malloc(sizeof(cache_line) * (unsigned long)E);
        //init lines
        for(int j=0; j<E; j++){
            cache_line *line = set->lines + j;
            line->valid = 0;
            line->tag = 0;
            line->LRU_age = 0;
        }
    }
    //init cache variables
    /* format of a cache address: tag - set - block */
    c->tag_mask = generate_mask(64 - s - b, s + b);   
    c->set_mask = generate_mask(s, b);
    c->B = b;
    c->S = (1 << s);
    c->E = E;
    c->set_idx = s;
    c->block_offset = b;
    return c;
}

/**
 * @brief Free allocated memory of a cache
 * @param[in] c    pointer to the cache simulator
 */
void free_cache(cache *c){
    for(int i=0; i<c->S; i++){
        free(c->sets[i].lines);
    }
    free(c->sets);
    free(c);
}

/**
 * @brief Update line's LRU_age in target sets after hits
 * 
 * 
 * @param c 
 * @param set_idx 
 * @param line_idx 
 */
void update_LRU(cache *c, uint64_t set_idx, int line_idx){
    cache_set *target_set = c->sets + set_idx;
    for(int i=0; i<c->E; i++){
        cache_line *curr_line = target_set->lines + i;
        curr_line->LRU_age++;
    }
    cache_line *target_line = target_set->lines + line_idx;
    target_line->LRU_age = 0;  //least recently used
}

/**
 * @brief Judege if the address hits in cache memory
 * 
 * @param[in]   c 
 * @param[in]   tag 
 * @param[in]   set_idx 
 * @return if hits return 1, else 0
 */
int is_hit(cache *c, uint64_t tag, uint64_t set_idx){
    cache_set *target_set = c->sets + set_idx;
    for(int i=0; i<c->E; i++){
        cache_line *curr_line = target_set->lines + i;
        if(curr_line->valid == 1 && curr_line->tag == tag){
            update_LRU(c, set_idx, i);
            return 1;
        }
    }
    return 0;
}

/**
 * @brief 
 * 
 * @param c 
 * @param set_idx 
 * @return index of empty line in target set 
 */
int find_empty_line(cache *c, uint64_t set_idx){
    cache_set *target_set = c->sets + set_idx;
    for(int i=0; i<c->E; i++){
        cache_line *curr_line = target_set->lines + i;
        if(curr_line->valid == 0)
            return i;
    }
    return 0;
}

/**
 * @brief 
 * 
 * @param c 
 * @param tag 
 * @param set_idx 
 */
void evict_line(cache *c, uint64_t tag, uint64_t set_idx){
    int evict_line_idx = 0;
    int max_LRU_age = -1;
    cache_set *target_set = c->sets + set_idx;
    for(int i=0; i<c->E; i++){
        cache_line *curr_line = target_set->lines + i;
        if(curr_line->LRU_age > max_LRU_age){
            evict_line_idx = i;
            max_LRU_age = curr_line->LRU_age;
        }
    }
    cache_line *target_line = target_set->lines + evict_line_idx;
    target_line->valid = 1;
    target_line->tag = tag;
}

/**
 * @brief 
 * 
 * @param c 
 * @param tag 
 * @param set_idx 
 * @param line_idx 
 */
void update_cache(cache *c, uint64_t tag, uint64_t set_idx, int line_idx){
    cache_set *target_set = c->sets + set_idx;
    cache_line *target_line = target_set->lines + line_idx;
    target_line->tag = tag;
    update_LRU(c, set_idx, line_idx);
    target_line->valid = 1;
}

/**
 * @brief Handle a single record in trace file
 * 
 * This fuction is the core fuction of cache memory
 * 
 * @param[in]   c       a pointer to simulate cache 
 * @param[in]   oper    operation type (read/write)
 * @param[in]   addr    memory address
 */
void handle_mem_trace(cache *c, char oper, uint64_t addr){
    uint64_t tag = addr & c->tag_mask;
    uint64_t set_idx = addr & c->set_mask;
    set_idx >>= c->block_offset;
    
    if(is_hit(c, tag, set_idx)){
        hits++;
    }else{
        misses++;
        int empty_line_idx = find_empty_line(c, set_idx);
        if(!empty_line_idx){
            evict_line(c, tag, set_idx);
            evictions++;
        }else{
            update_cache(c, tag, set_idx, empty_line_idx);
        }
    }
}

/**
 * @brief operate 'Valgrind' memory traces
 * 
 * Format of each line in trace file: (operation, address, size)
 *
 * @param[in] c 
 * @param[in] trace_file 
 */
void handle_trace_file(cache *c, char *trace_file){
    FILE *fp = fopen(trace_file, "r");
    if (fp == NULL) {
        perror("Error! Cannot open file!");
        exit(EXIT_FAILURE);
    }
    /* format of each line in trace file */
    int size;
    char oper;
    uint64_t addr;  //64bits-hexdecimal address
    
    while(fscanf(fp, " %c %llx,%d", &oper, &addr, &size) == 3){
        handle_mem_trace(c, oper, addr);
    }
    fclose(fp);
}

int main(int argc, char const **argv)
{
    int s;            /* Number of set index bits */
    int E;            /* Number of Lines per set */
    int b;            /* Number of Block bits */
    char *trace_file;
    char ch;
    int S, B;
    /* use getopt to parse the command line arguments */
    while((ch = getopt(argc, (char *const *)argv, "b:s:E:t")) != 1){
        switch (ch)
        {
        case 'b':
            b = atoi(optarg);
            B = (1 << b);   /* number of blocks (B) = 2 to the b */
            break;
        case 's':
            s = atoi(optarg);
            S = (1 << s);     /* number of sets (S) = 2 to the s */
            break;
        case 'E':
            E = atoi(optarg);
            break;
        case 't':
            trace_file = optarg;
            break;
        default:
            break;
        }
    }
    cache *c = init_cache(s, E, b);
    handle_trace_file(&c, trace_file);
    printSummary(hits, misses, evictions);
    free_cache(c);
    return 0;
}
