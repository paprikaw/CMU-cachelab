#include "cachelab.h"
#include <stdio.h>
#include "getopt.h"
#include <stdlib.h>
#include <stdint.h>

#define INVALID_ADDRESS 0xFFFFFFFFFFFFFFFF
struct cacheAddress
{
    uint64_t address;
    size_t accessTime;
};
typedef struct cacheAddress CACHE_ADDRESS;

size_t parse_set_index(uint64_t my_address, short set_bit, short block_bits);
char cmp_tag(uint64_t source_address, uint64_t target_address, short set_bit, short block_bit);
size_t LRU_index(CACHE_ADDRESS *set, size_t length);

int main(int argc, char *argv[])
{
    // Parsing arguments
    int ch = -1;
    short needed_argument = 0; // 输入的needed arguments的数量
    short set_bit = 0;         // set的bits数量
    size_t associativity = 1;  // associativity (每个set中有几个line)
    short block_bit = 0;       // block bits的数量
    char *file_name = NULL;
    char is_verbose = 0;
    while ((ch = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch (ch)
        {
        case 'h':
            return 0;
        case 'v':
            is_verbose = 1;
            break;
        case 's':
            if ((set_bit = atoi(optarg)) == 0)
            {
                break;
            }
            needed_argument++;
            break;
        case 'E':
            if ((associativity = atoi(optarg)) == 0)
            {
                break;
            }
            needed_argument++;
            break;
        case 'b':
            if ((block_bit = atoi(optarg)) == 0)
            {
                break;
            }
            needed_argument++;
            break;
        case 't':
            file_name = optarg;
            needed_argument++;
            break;
        default:
            printf("%s is a invalid flag", optarg);
            return 1;
        }
    }

    // 检查是否已经输入正确的arguments
    if (ch == '?' || needed_argument < 4)
    {
        printf("./csim-ref: Missing required command line argument");
        return 1;
    }

    // 构建一个cache simulator
    size_t set_size = 1 << set_bit;
    CACHE_ADDRESS **set = malloc(sizeof(CACHE_ADDRESS *) * set_size);
    for (size_t i = 0; i < set_size; i++)
    {
        set[i] = malloc(sizeof(CACHE_ADDRESS) * associativity);
        for (size_t j = 0; j < associativity; j++)
        {
            set[i][j].address = INVALID_ADDRESS;
        }
    }

    // 打开目标文件
    FILE *fs = fopen(file_name, "r");
    if (fs == NULL)
    {
        printf("%s: No such file or directory", file_name);
    }

    // 定义parsing中会用到的变量
    char access_type[2]; // L, S, M
    uint64_t address = 0;
    size_t accessed_byte_size = 0; // 总共访问多少bytes
    size_t current_time = 0;       // time从0开始计时
    int evict = 0, miss = 0, hit = 0;
    // 开始parsing目标文件，并且将对应的addresses加入到cache simulator中
    while (fscanf(fs, "%s %lx,%ld\n", access_type, &address, &accessed_byte_size) != EOF)
    {
        current_time++;
        if (access_type[0] == 'I')
            continue;
        // 开始检查以address为第一个的总共大小为size的地址是否在内存中
        // address + i是否在cahce中
        if (is_verbose)
            printf("%s %lx,%ld", access_type, address, accessed_byte_size);
        // 遍历中需要得到的信息
        char is_hit = 0;
        size_t empty_address_index = 0;
        char is_any_empty = 0;
        // 当前address的set index
        size_t set_index = parse_set_index(address, set_bit, block_bit);
        // 当前的set
        CACHE_ADDRESS *cur_set = set[set_index];
        // 遍历当前set中所有的lines
        for (size_t i = 0; i < associativity; i++)
        {
            uint64_t cache_address = cur_set[i].address;
            if (cache_address == INVALID_ADDRESS)
            {
                is_any_empty = 1;
                empty_address_index = i;
                break;
            }
            if (cmp_tag(address, cache_address, set_bit, block_bit))
            {
                is_hit = 1;
                cur_set[i].accessTime = current_time;
                break;
            }
        }
        // 判断是否hit
        if (is_hit == 1)
        {
            if (is_verbose)
                printf(" hit ");
            hit++;
        }
        else
        {
            if (is_any_empty == 1)
            {
                cur_set[empty_address_index].address = address;
                cur_set[empty_address_index].accessTime = current_time;
                if (is_verbose)
                    printf(" miss ");
                miss++;
            }
            else
            {
                size_t least_used_index = LRU_index(cur_set, associativity);
                cur_set[least_used_index].accessTime = current_time;
                cur_set[least_used_index].address = address;
                if (is_verbose)
                    printf(" miss eviction ");
                evict++;
                miss++;
            }
        }

        // 如果类型是M的话，需要有其他操作
        if (access_type[0] == 'M')
        {
            if (is_verbose)
                printf("hit ");
            hit++;
        }
        if (is_verbose)
            printf("\n");
    }
    printSummary(hit, miss, evict);
    fclose(fs);
    free(set);
    return 0;
}
size_t parse_set_index(uint64_t my_address, short set_bit, short block_bits)
{
    uint64_t bit_mask = ((1 << set_bit) - 1) << block_bits;
    return ((my_address & bit_mask) >> block_bits);
}

char cmp_tag(uint64_t source_address, uint64_t target_address, short set_bit, short block_bit)
{
    uint64_t bit_mask = ((1 << (64 - set_bit - block_bit)) - 1) << (set_bit + block_bit);
    uint64_t source_tag_bits = bit_mask & source_address;
    uint64_t target_tag_bits = bit_mask & target_address;
    return (target_tag_bits ^ source_tag_bits) == 0;
}

size_t LRU_index(CACHE_ADDRESS *set, size_t length)
{
    int64_t least_used_time = INTMAX_MAX;
    size_t least_used_index = 0;
    for (size_t i = 0; i < length; i++)
    {
        if (set[i].accessTime < least_used_time)
        {
            least_used_time = set[i].accessTime;
            least_used_index = i;
        }
    }
    return least_used_index;
};