/**
 * @file cache_mng.c
 * @brief cache management functions
 *
 * @author Mirjana Stojilovic
 * @date 2018-19
 */

#include "cache_mng.h"
#include "error.h"
#include "lru.h"
#include "util.h"

#include <inttypes.h> // for PRIx macros

//=========================================================================
#define PRINT_CACHE_LINE(OUTFILE, TYPE, WAYS, LINE_INDEX, WAY, WORDS_PER_LINE)                 \
    do                                                                                         \
    {                                                                                          \
        fprintf(OUTFILE, "V: %1" PRIx8 ", AGE: %1" PRIx8 ", TAG: 0x%03" PRIx16 ", values: ( ", \
                cache_valid(TYPE, WAYS, LINE_INDEX, WAY),                                      \
                cache_age(TYPE, WAYS, LINE_INDEX, WAY),                                        \
                cache_tag(TYPE, WAYS, LINE_INDEX, WAY));                                       \
        for (int i_ = 0; i_ < WORDS_PER_LINE; i_++)                                            \
            fprintf(OUTFILE, "0x%08" PRIx32 " ",                                               \
                    cache_line(TYPE, WAYS, LINE_INDEX, WAY)[i_]);                              \
        fputs(")\n", OUTFILE);                                                                 \
    } while (0)

#define PRINT_INVALID_CACHE_LINE(OUTFILE, TYPE, WAYS, LINE_INDEX, WAY, WORDS_PER_LINE)                                    \
    do                                                                                                                    \
    {                                                                                                                     \
        fprintf(OUTFILE, "V: %1" PRIx8 ", AGE: -, TAG: -----, values: ( ---------- ---------- ---------- ---------- )\n", \
                cache_valid(TYPE, WAYS, LINE_INDEX, WAY));                                                                \
    } while (0)

#define DUMP_CACHE_TYPE(OUTFILE, TYPE, WAYS, LINES, WORDS_PER_LINE)                                  \
    do                                                                                               \
    {                                                                                                \
        for (uint16_t index = 0; index < LINES; index++)                                             \
        {                                                                                            \
            foreach_way(way, WAYS)                                                                   \
            {                                                                                        \
                fprintf(output, "%02" PRIx8 "/%04" PRIx16 ": ", way, index);                         \
                if (cache_valid(TYPE, WAYS, index, way))                                             \
                    PRINT_CACHE_LINE(OUTFILE, const TYPE, WAYS, index, way, WORDS_PER_LINE);         \
                else                                                                                 \
                    PRINT_INVALID_CACHE_LINE(OUTFILE, const TYPE, WAYS, index, way, WORDS_PER_LINE); \
            }                                                                                        \
        }                                                                                            \
    } while (0)

//=========================================================================
// see cache_mng.h
int cache_dump(FILE *output, const void *cache, cache_t cache_type)
{
    M_REQUIRE_NON_NULL(output);
    M_REQUIRE_NON_NULL(cache);

    fputs("WAY/LINE: V: AGE: TAG: WORDS\n", output);
    switch (cache_type)
    {
    case L1_ICACHE:
        DUMP_CACHE_TYPE(output, l1_icache_entry_t, L1_ICACHE_WAYS,
                        L1_ICACHE_LINES, L1_ICACHE_WORDS_PER_LINE);
        break;
    case L1_DCACHE:
        DUMP_CACHE_TYPE(output, l1_dcache_entry_t, L1_DCACHE_WAYS,
                        L1_DCACHE_LINES, L1_DCACHE_WORDS_PER_LINE);
        break;
    case L2_CACHE:
        DUMP_CACHE_TYPE(output, l2_cache_entry_t, L2_CACHE_WAYS,
                        L2_CACHE_LINES, L2_CACHE_WORDS_PER_LINE);
        break;
    default:
        debug_print("%d: unknown cache type", cache_type);
        return ERR_BAD_PARAMETER;
    }
    putc('\n', output);

    return ERR_NONE;
}

uint32_t paddr_to_uint32_t(const phy_addr_t *paddr)
{
    return (paddr->phy_page_num << PAGE_OFFSET) | paddr->page_offset;
}

word_t *get_line_from_mem_space(const void *mem_space, uint32_t physical_address,
                                size_t lines)
{
    return (((word_t *)mem_space) + (physical_address - (physical_address % lines)) / sizeof(word_t));
}

#define check_well_aligned(TYPE, PADDR, CACHE_LINE) \
    M_REQUIRE(PADDR % CACHE_LINE == 0, ERR_BAD_PARAMETER, "is not well aligned %c", ' ');

#define entry_init(TYPE, REMAINING_BITS, CACHE_LINE, WORDS_PER_LINE)                        \
    {                                                                                       \
        check_well_aligned(TYPE, physical_address, CACHE_LINE);                             \
        TYPE *cache_init = (TYPE *)cache_entry;                                             \
        cache_init->v = 1;                                                                  \
        cache_init->age = 0;                                                                \
        cache_init->tag = physical_address >> REMAINING_BITS;                               \
        word_t *address = get_line_from_mem_space(mem_space, physical_address, CACHE_LINE); \
        for (int i = 0; i < L1_ICACHE_WORDS_PER_LINE; i++)                                  \
        {                                                                                   \
            cache_init->line[i] = address[i];                                               \
        }                                                                                   \
    }

int cache_entry_init(const void *mem_space,
                     const phy_addr_t *paddr,
                     void *cache_entry,
                     cache_t cache_type)
{
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(cache_entry);

    uint32_t physical_address = paddr_to_uint32_t(paddr);

    switch (cache_type)
    {
    case L1_ICACHE:
        entry_init(l1_icache_entry_t, L1_ICACHE_TAG_REMAINING_BITS, L1_ICACHE_LINE, L1_ICACHE_WORDS_PER_LINE);
        break;
    case L1_DCACHE:
        entry_init(l1_dcache_entry_t, L1_DCACHE_TAG_REMAINING_BITS, L1_DCACHE_LINE, L1_DCACHE_WORDS_PER_LINE);
        break;
    case L2_CACHE:
        entry_init(l2_cache_entry_t, L2_CACHE_TAG_REMAINING_BITS, L2_CACHE_LINE, L2_CACHE_WORDS_PER_LINE);
        break;
        break;
    default:
        return ERR_BAD_PARAMETER;
    }

    return ERR_NONE;
}
//=========================================================================

#define cache_flush_process(TYPE, CACHE_LINES, CACHE_WAYS, CACHE_WORDS_PER_LINE) \
    {                                                                            \
        int entries = CACHE_LINES * CACHE_WAYS;                                  \
        TYPE *cache_flushed = (TYPE *)cache;                                     \
        for (int i = 0; i < entries; ++i)                                        \
        {                                                                        \
            cache_flushed[i].tag = 0;                                            \
            cache_flushed[i].age = 0;                                            \
            cache_flushed[i].v = 0;                                              \
            for (int j = 0; j < CACHE_WORDS_PER_LINE; j++)                       \
            {                                                                    \
                cache_flushed[i].line[j] = 0;                                    \
            }                                                                    \
        }                                                                        \
    }

int cache_flush(void *cache, cache_t cache_type)
{
    M_REQUIRE_NON_NULL(cache);
    switch (cache_type)
    {
    case L1_ICACHE:
        cache_flush_process(l1_icache_entry_t, L1_ICACHE_LINES, L1_ICACHE_WAYS, L1_ICACHE_WORDS_PER_LINE);
        break;
    case L1_DCACHE:
        cache_flush_process(l1_dcache_entry_t, L1_DCACHE_LINES, L1_DCACHE_WAYS, L1_DCACHE_WORDS_PER_LINE);
        break;
    case L2_CACHE:
        cache_flush_process(l2_cache_entry_t, L2_CACHE_LINES, L2_CACHE_WAYS, L2_CACHE_WORDS_PER_LINE);
        break;
    default:
        return ERR_BAD_PARAMETER;
    }
    return ERR_NONE;
}

//=========================================================================

#define cache_insert_process(TYPE, CACHE_WAYS, CACHE_LINES, CACHE_WORDS_PER_LINE)                            \
    {                                                                                                        \
        M_REQUIRE(cache_way < CACHE_WAYS && cache_line_index < CACHE_LINES, ERR_BAD_PARAMETER, "%c", ' ');   \
        cache_tag(TYPE, CACHE_WAYS, cache_line_index, cache_way) = ((TYPE *)cache_line_in)->tag;             \
        cache_valid(TYPE, CACHE_WAYS, cache_line_index, cache_way) = ((TYPE *)cache_line_in)->v;             \
        cache_age(TYPE, CACHE_WAYS, cache_line_index, cache_way) = ((TYPE *)cache_line_in)->age;             \
        for (int i = 0; i < CACHE_WORDS_PER_LINE; i++)                                                       \
        {                                                                                                    \
            cache_line(TYPE, CACHE_WAYS, cache_line_index, cache_way)[i] = ((TYPE *)cache_line_in)->line[i]; \
        }                                                                                                    \
    }

int cache_insert(uint16_t cache_line_index,
                 uint8_t cache_way,
                 const void *cache_line_in,
                 void *cache,
                 cache_t cache_type)
{

    M_REQUIRE_NON_NULL(cache_line_in);
    M_REQUIRE_NON_NULL(cache);

    switch (cache_type)
    {
    case L1_ICACHE:
        cache_insert_process(l1_icache_entry_t, L1_ICACHE_WAYS, L1_ICACHE_LINES, L1_ICACHE_WORDS_PER_LINE);
        break;
    case L1_DCACHE:
        cache_insert_process(l1_dcache_entry_t, L1_DCACHE_WAYS, L1_DCACHE_LINES, L1_DCACHE_WORDS_PER_LINE);
        break;
    case L2_CACHE:
        cache_insert_process(l2_cache_entry_t, L2_CACHE_WAYS, L2_CACHE_LINES, L2_CACHE_WORDS_PER_LINE);
        break;
    default:
        return ERR_BAD_PARAMETER;
    }

    return ERR_NONE;
}

//=========================================================================

#define cache_hit_process(TYPE, CACHE_LINE, CACHE_LINES, REMAINING_BITS, CACHE_WAYS) \
    {                                                                                \
                                                                                     \
        uint32_t index_line = (paddr_converted / CACHE_LINE) % CACHE_LINES;          \
        uint32_t tag = paddr_converted >> REMAINING_BITS;                            \
                                                                                     \
        foreach_way(way, CACHE_WAYS)                                                 \
        {                                                                            \
            TYPE *entry = cache_entry(TYPE, CACHE_WAYS, index_line, way);            \
            if (entry->v == 0)                                                       \
            {                                                                        \
                return ERR_NONE;                                                     \
            }                                                                        \
            else if (entry->tag == tag)                                              \
            {                                                                        \
                *hit_way = way;                                                      \
                *hit_index = index_line;                                             \
                *p_line = cache_line(TYPE, CACHE_WAYS, index_line, way);             \
                LRU_age_update(TYPE, CACHE_WAYS, way, index_line);                   \
                return ERR_NONE;                                                     \
            }                                                                        \
        }                                                                            \
    }

int cache_hit(const void *mem_space,
              void *cache,
              phy_addr_t *paddr,
              const uint32_t **p_line,
              uint8_t *hit_way,
              uint16_t *hit_index,
              cache_t cache_type)
{

    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(cache);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(p_line);
    M_REQUIRE_NON_NULL(hit_way);
    M_REQUIRE_NON_NULL(hit_index);

    uint32_t paddr_converted = paddr_to_uint32_t(paddr);

    *hit_way = HIT_WAY_MISS;
    *hit_index = HIT_INDEX_MISS;
    switch (cache_type)
    {
    case L1_ICACHE:
        cache_hit_process(l1_icache_entry_t, L1_ICACHE_LINE, L1_ICACHE_LINES, L1_ICACHE_TAG_REMAINING_BITS, L1_ICACHE_WAYS);
        break;
    case L1_DCACHE:
        cache_hit_process(l1_dcache_entry_t, L1_DCACHE_LINE, L1_DCACHE_LINES, L1_DCACHE_TAG_REMAINING_BITS, L1_DCACHE_WAYS);
        break;
    case L2_CACHE:
        cache_hit_process(l2_cache_entry_t, L2_CACHE_LINE, L2_CACHE_LINES, L2_CACHE_TAG_REMAINING_BITS, L2_CACHE_WAYS);
        break;
    default:
        return ERR_BAD_PARAMETER;
    }
    return ERR_NONE;
}

//=========================================================================

uint8_t get_index_word(uint32_t paddr, cache_t cache_type)
{
    switch (cache_type)
    {
    case L1_ICACHE:
        return (paddr >> 2) % L1_ICACHE_WORDS_PER_LINE;
    case L1_DCACHE:
        return (paddr >> 2) % L1_DCACHE_WORDS_PER_LINE;
    case L2_CACHE:
        return (paddr >> 2) % L2_CACHE_WORDS_PER_LINE;
    default:
        return ERR_BAD_PARAMETER;
    }
}

#define L1_INDEX_MASK 63
#define L2_INDEX_MASK 511
#define WORDS_AND_BYTE_BITS 4

#define search_empty_space_or_old_entry(TYPE, WAYS, cold_case, line_to_eject, age_max_found, way_to_insert, line_index, paddr_evicted, REMAINING_BITS) \
    {                                                                                                                                                  \
        *(cold_case) = 0;                                                                                                                              \
        *(age_max_found) = -1;                                                                                                                         \
        *(way_to_insert) = -1;                                                                                                                         \
        *(paddr_evicted) = -1;                                                                                                                         \
        foreach_way(index_of_way, WAYS)                                                                                                                \
        {                                                                                                                                              \
            if (cache_valid(TYPE, WAYS, line_index, index_of_way) == 0)                                                                                \
            {                                                                                                                                          \
                *(cold_case) = 1;                                                                                                                      \
                *(way_to_insert) = index_of_way;                                                                                                       \
            }                                                                                                                                          \
            uint8_t age_of_current_way = cache_age(TYPE, WAYS, line_index, index_of_way);                                                              \
            if (*(age_max_found) < age_of_current_way)                                                                                                 \
            {                                                                                                                                          \
                *(age_max_found) = age_of_current_way;                                                                                                 \
                *(way_to_insert) = index_of_way;                                                                                                       \
                *(line_to_eject) = cache_line(TYPE, WAYS, line_index, index_of_way);                                                                   \
                *(paddr_evicted) = (cache_tag(TYPE, WAYS, line_index, index_of_way) << REMAINING_BITS) | (line_index << WORDS_AND_BYTE_BITS);          \
            }                                                                                                                                          \
        }                                                                                                                                              \
    }

#define L2_NUMBER_OF_BITS_OF_INDEX_SELECT 9

#define deplacement_from_level2_to_level1(TYPE, REMAINING_BITS, WAYS, LINE, CACHE_TYPE, CACHE_WORDS_PER_LINE, HIT_INDEX) \
    /**lines infos */                                                                                                    \
    {                                                                                                                    \
        /**invalidate in cache 2 */                                                                                      \
        void *cache = l2_cache;                                                                                          \
        cache_valid(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way) = 0;                                            \
        uint32_t new_paddr = ((cache_tag(l2_cache_entry_t, L2_CACHE_WAYS, hit_index, hit_way)                            \
                               << L2_NUMBER_OF_BITS_OF_INDEX_SELECT) |                                                   \
                              HIT_INDEX)                                                                                 \
                             << WORDS_AND_BYTE_BITS;                                                                     \
                                                                                                                         \
        /*search the right place to insert in l1_cache */                                                                \
        find_place_in_l1(TYPE, REMAINING_BITS, WAYS, LINE, new_paddr, CACHE_TYPE, CACHE_WORDS_PER_LINE);                 \
    }

#define find_place_in_l1(TYPE, REMAINING_BITS, WAYS, LINE, new_paddr, CACHE_TYPE, CACHE_WORDS_PER_LINE)                                                                                                                     \
    {                                                                                                                                                                                                                       \
        cache = l1_cache;                                                                                                                                                                                                   \
        uint32_t paddr_to_evict = -1;                                                                                                                                                                                       \
        word_t *line_of_old_entry = NULL;                                                                                                                                                                                   \
        find_place_in_cache(TYPE, REMAINING_BITS, WAYS, LINE, new_paddr, CACHE_TYPE, CACHE_WORDS_PER_LINE,                                                                                                                  \
                            L1_INDEX_MASK, &paddr_to_evict, &line_of_old_entry);                                                                                                                                            \
        cache = l2_cache;                                                                                                                                                                                                   \
        uint32_t paddr_evicted = -1;                                                                                                                                                                                        \
        word_t *line_of_old_entry_evicted = NULL;                                                                                                                                                                           \
        find_place_in_cache(l2_cache_entry_t, L2_CACHE_TAG_REMAINING_BITS, L2_CACHE_WAYS, line_of_old_entry, paddr_to_evict, L2_CACHE, L2_CACHE_WORDS_PER_LINE, L2_INDEX_MASK, &paddr_evicted, &line_of_old_entry_evicted); \
    }

#define find_place_in_cache(TYPE, REMAINING_BITS, WAYS, LINE, PADDR_CONVERTED, CACHE_TYPE, CACHE_WORDS_PER_LINE, INDEX_MASK, paddr_evicted, line_of_old_entry_evicted) \
    {                                                                                                                                                                  \
        uint8_t age_max = -1;                                                                                                                                          \
        uint8_t way_to_insert = -1;                                                                                                                                    \
        *paddr_evicted = -1;                                                                                                                                           \
        uint8_t cold_case = 0;                                                                                                                                         \
        uint16_t line_index = (PADDR_CONVERTED >> WORDS_AND_BYTE_BITS) & INDEX_MASK;                                                                                   \
        search_empty_space_or_old_entry(TYPE, WAYS, &cold_case,                                                                                                        \
                                        line_of_old_entry_evicted, &age_max, &way_to_insert, line_index, paddr_evicted, REMAINING_BITS);                               \
                                                                                                                                                                       \
        TYPE new_cache_entry;                                                                                                                                          \
        new_cache_entry.v = 1;                                                                                                                                         \
        new_cache_entry.tag = PADDR_CONVERTED >> REMAINING_BITS;                                                                                                       \
        new_cache_entry.age = 0;                                                                                                                                       \
        for (int i = 0; i < CACHE_WORDS_PER_LINE; ++i)                                                                                                                 \
        {                                                                                                                                                              \
            new_cache_entry.line[i] = LINE[i];                                                                                                                         \
        }                                                                                                                                                              \
        /*coldcase case*/                                                                                                                                              \
        if (cold_case == 1)                                                                                                                                            \
        {                                                                                                                                                              \
            M_REQUIRE_NO_ERRNO(cache_insert(line_index, way_to_insert, &new_cache_entry, l1_cache, CACHE_TYPE));                                                       \
            LRU_age_increase(TYPE, WAYS, way_to_insert, line_index);                                                                                                   \
            return ERR_NONE;                                                                                                                                           \
        }                                                                                                                                                              \
                                                                                                                                                                       \
        /**there was no cold start : eviction*/                                                                                                                        \
        M_REQUIRE_NO_ERRNO(cache_insert(line_index, way_to_insert, &new_cache_entry, l1_cache, CACHE_TYPE));                                                           \
        LRU_age_update(TYPE, WAYS, way_to_insert, line_index);                                                                                                         \
    }

/**
 * @brief Ask cache for a word of data.
 *  Exclusive policy (https://en.wikipedia.org/wiki/Cache_inclusion_policy)
 *      Consider the case when L2 is exclusive of L1. Suppose there is a
 *      processor read request for block X. If the block is found in L1 cache,
 *      then the data is read from L1 cache and returned to the processor. If
 *      the block is not found in the L1 cache, but present in the L2 cache,
 *      then the cache block is moved from the L2 cache to the L1 cache. If
 *      this causes a block to be evicted from L1, the evicted block is then
 *      placed into L2. This is the only way L2 gets populated. Here, L2
 *      behaves like a victim cache. If the block is not found neither in L1 nor
 *      in L2, then it is fetched from main memory and placed just in L1 and not
 *      in L2.
 *
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param access to distinguish between fetching instructions and reading/writing data
 * @param l1_cache pointer to the beginning of L1 CACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param word pointer to the word of data that is returned by cache
 * @param replace replacement policy
 * @return error code
 */
int cache_read(const void *mem_space,
               phy_addr_t *paddr,
               mem_access_t access,
               void *l1_cache,
               void *l2_cache,
               uint32_t *word,
               cache_replace_t replace)
{

    M_REQUIRE(replace == LRU, ERR_POLICY, "not implemented this policy of remplacement%c", ' ');
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(l1_cache);
    M_REQUIRE_NON_NULL(l2_cache);
    M_REQUIRE_NON_NULL(word);

    //inclure la vérification de l'adresse fournie est bien alignée sur des adresses de mots : Comment ???

    //SEARCH IN FIRST LEVEL
    uint32_t paddr_converted = paddr_to_uint32_t(paddr);
    cache_t cache_type;
    if (access == INSTRUCTION)
    {
        check_well_aligned(l1_icache_entry_t, paddr_converted, L1_ICACHE_LINE);
        cache_type = L1_ICACHE;
    }
    else
    {
        check_well_aligned(l1_dcache_entry_t, paddr_converted, L1_DCACHE_LINE);
        cache_type = L1_DCACHE;
    }
    const uint32_t *line = NULL;
    uint8_t hit_way = HIT_WAY_MISS;
    uint16_t hit_index = HIT_INDEX_MISS;
    M_REQUIRE_NO_ERRNO(cache_hit(mem_space, l1_cache, paddr, &line, &hit_way, &hit_index, cache_type));

    //if hit at the first level
    if (hit_way != HIT_WAY_MISS)
    {
        M_REQUIRE_NON_NULL(line);
        // uint8_t index_word = get_index_word(paddr, cache_type);
        // *word = *(line + index_word);
        if (access == INSTRUCTION)
        {
            *word = line[get_index_word(paddr_converted, L1_ICACHE)];
        }
        else
        {
            *word = line[get_index_word(paddr_converted, L1_DCACHE)];
        }
        return ERR_NONE;
    }

    //SEARCH IN SECOND LEVEL

    line = NULL;
    M_REQUIRE_NO_ERRNO(cache_hit(mem_space, l2_cache, paddr, &line, &hit_way, &hit_index, cache_type));
    void *cache = l2_cache;
    if (hit_way == HIT_WAY_MISS)
    {
        l2_cache_entry_t the_new_cache_entry;
        M_REQUIRE_NO_ERRNO(cache_entry_init(mem_space, paddr, &the_new_cache_entry, L2_CACHE));
        line = the_new_cache_entry.line;

        if (cache_type == L1_ICACHE)
        {
            *word = line[get_index_word(paddr_converted, L1_ICACHE)];
            find_place_in_l1(l1_icache_entry_t, L1_ICACHE_TAG_REMAINING_BITS,
                             L1_ICACHE_WAYS, line, paddr_converted, cache_type, L1_ICACHE_WORDS_PER_LINE);
        }
        else
        {
            *word = line[get_index_word(paddr_converted, L1_DCACHE)];
            find_place_in_l1(l1_dcache_entry_t, L1_DCACHE_TAG_REMAINING_BITS,
                             L1_DCACHE_WAYS, line, paddr_converted, cache_type, L1_DCACHE_WORDS_PER_LINE);
        }
        return ERR_NONE;
    }

    //hit_way != HIT_WAY_MISS
    if (cache_type == L1_ICACHE)
    {
        *word = line[get_index_word(paddr_converted, L1_ICACHE)];
        deplacement_from_level2_to_level1(l1_icache_entry_t, L1_ICACHE_TAG_REMAINING_BITS,
                                          L1_ICACHE_WAYS, line, paddr_converted, cache_type, L1_ICACHE_WORDS_PER_LINE);
    }
    else
    {
        *word = line[get_index_word(paddr_converted, L1_DCACHE)];
        deplacement_from_level2_to_level1(l1_dcache_entry_t, L1_DCACHE_TAG_REMAINING_BITS,
                                          L1_DCACHE_WAYS, line, paddr_converted, cache_type, L1_DCACHE_WORDS_PER_LINE);
    }
    return ERR_NONE;
}

#define SELECT_BITS 2
#define ALIGNED_OF_WORDS_NUMBER 4
#define OCTET 8
#define BYTE_MASK 255

//=========================================================================
/**
 * @brief Ask cache for a byte of data. Endianess: LITTLE.
 *
 * @param mem_space pointer to the memory space
 * @param p_addr pointer to a physical address
 * @param access to distinguish between fetching instructions and reading/writing data
 * @param l1_cache pointer to the beginning of L1 CACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param byte pointer to the byte to be returned
 * @param replace replacement policy
 * @return error code
 */
int cache_read_byte(const void *mem_space,
                    phy_addr_t *p_paddr,
                    mem_access_t access,
                    void *l1_cache,
                    void *l2_cache,
                    uint8_t *p_byte,
                    cache_replace_t replace)
{
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(p_paddr);
    M_REQUIRE_NON_NULL(l1_cache);
    M_REQUIRE_NON_NULL(l2_cache);
    M_REQUIRE_NON_NULL(p_byte);

    check_well_aligned(l1_dcache_entry_t, paddr_to_uint32_t(p_paddr), L1_DCACHE_LINE);
    phy_addr_t paddr_aligned;
    uint8_t bit_select = p_paddr->page_offset % ALIGNED_OF_WORDS_NUMBER;
    paddr_aligned.page_offset = p_paddr->page_offset - bit_select;
    paddr_aligned.phy_page_num = p_paddr->phy_page_num;
    uint32_t word_of_result = -1;
    M_REQUIRE_NO_ERRNO(cache_read(mem_space, &paddr_aligned, access, l1_cache, l2_cache, &word_of_result, replace));
    *p_byte = (word_of_result >> (bit_select * OCTET)) & BYTE_MASK;
    return ERR_NONE;
}

#define get_index_line_from_paddr(PADDR, MASK_TYPE) \
    (((paddr_to_uint32_t(PADDR)) >> (2 + 2)) & MASK_TYPE)

void update_memory(void *mem_space, phy_addr_t *physical_address,
                   size_t lines, size_t words_per_line, const uint32_t *line_to_insert)
{
    word_t *address = get_line_from_mem_space(mem_space, paddr_to_uint32_t(physical_address), lines);
    for (int i = 0; i < words_per_line; i++)
    {
        address[i] = line_to_insert[i];
    }
}

//=========================================================================

#define WORD_INDEX_MASK 3

#define write_process(TYPE, TYPE_ENUM, INDEX_MASK, WAYS, LINES, WORDS_PER_LINE)                       \
    uint32_t line_index = get_index_line_from_paddr(paddr, INDEX_MASK);                               \
    cache_line(TYPE, WAYS, line_index, hit_way)[hit_index] = *word;                                   \
    M_REQUIRE_NO_ERRNO(cache_insert(line_index, hit_way,                                              \
                                    cache_entry(TYPE, WAYS, line_index, hit_way), cache, TYPE_ENUM)); \
    LRU_age_update(TYPE, WAYS, hit_way, line_index); /*check if good one */                           \
    update_memory(mem_space, paddr, LINES, WORDS_PER_LINE, line);                                     \
    M_REQUIRE_NO_ERRNO(cache_hit(mem_space, l1_cache, paddr, &line, &hit_way, &hit_index, TYPE_ENUM));
/**
 * @brief Change a word of data in the cache.
 *  Exclusive policy (see cache_read)
 *
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param l1_cache pointer to the beginning of L1 CACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param word const pointer to the word of data that is to be written to the cache
 * @param replace replacement policy
 * @return error code
 */
int cache_write(void *mem_space,
                phy_addr_t *paddr,
                void *l1_cache, //must be of type l1_dcache_entry_t, we do not write instructions
                void *l2_cache,
                const uint32_t *word,
                cache_replace_t replace)
{

    M_REQUIRE(replace == LRU, ERR_POLICY, "not implemented this policy of remplacement%c", ' ');
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(l1_cache);
    M_REQUIRE_NON_NULL(l2_cache);
    M_REQUIRE_NON_NULL(word);

    //inclure la vérification de l'adresse fournie est bien alignée sur des adresses de mots : Comment ???
    //search in first level

    const uint32_t *line = NULL;
    uint8_t hit_way = HIT_WAY_MISS;
    uint16_t hit_index = HIT_INDEX_MISS;
    if (hit_way != HIT_WAY_MISS)
    {
        void *cache = l1_cache;
        write_process(l1_dcache_entry_t, L1_DCACHE, L1_INDEX_MASK, L1_DCACHE_WAYS, L1_DCACHE_LINES, L1_DCACHE_WORDS_PER_LINE);
        return ERR_NONE;
    }

    //search in second level
    M_REQUIRE_NO_ERRNO(cache_hit(mem_space, l2_cache, paddr, &line, &hit_way, &hit_index, L2_CACHE));
    if (hit_way != HIT_WAY_MISS)
    {
        void *cache = l2_cache;
        write_process(l2_cache_entry_t, L2_CACHE, L2_INDEX_MASK, L2_CACHE_WAYS, L2_CACHE_LINES, L2_CACHE_WORDS_PER_LINE);
        return ERR_NONE;
    }
    //nothing was found
    word_t line_to_store[L1_DCACHE_WORDS_PER_LINE];
    uint32_t paddr_converted = paddr_to_uint32_t(paddr);
    word_t *address = get_line_from_mem_space(mem_space, paddr_converted, L1_DCACHE_LINE);
    for (int i = 0; i < L1_DCACHE_WORDS_PER_LINE; i++)
    {
        line_to_store[i] = address[i];
    }
    uint8_t word_index = (paddr_converted >> 2) & WORD_INDEX_MASK;
    line_to_store[word_index] = *word;
    update_memory(mem_space, paddr, L1_DCACHE_LINES, L1_DCACHE_WORDS_PER_LINE, line_to_store);

    void *cache = l1_cache;
    find_place_in_l1(l1_dcache_entry_t, L1_DCACHE_TAG_REMAINING_BITS,
                     L1_DCACHE_WAYS, line_to_store, paddr_converted, L1_DCACHE, L1_DCACHE_WORDS_PER_LINE);
    return ERR_NONE;
}

/**
 * @brief Write to cache a byte of data. Endianess: LITTLE.
 *
 * @param mem_space pointer to the memory space
 * @param paddr pointer to a physical address
 * @param l1_cache pointer to the beginning of L1 ICACHE
 * @param l2_cache pointer to the beginning of L2 CACHE
 * @param p_byte pointer to the byte to be returned
 * @param replace replacement policy
 * @return error code
 */
int cache_write_byte(void *mem_space,
                     phy_addr_t *paddr,
                     void *l1_cache,
                     void *l2_cache,
                     uint8_t p_byte,
                     cache_replace_t replace)
{

    M_REQUIRE(replace == LRU, ERR_POLICY, "not implemented this policy of remplacement%c", ' ');
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(l1_cache);
    M_REQUIRE_NON_NULL(l2_cache);

    phy_addr_t paddr_aligned;
    uint8_t bit_select = paddr->page_offset % ALIGNED_OF_WORDS_NUMBER;
    paddr_aligned.page_offset = paddr->page_offset - bit_select;
    paddr_aligned.phy_page_num = paddr->phy_page_num;
    uint32_t word = -1;
    M_REQUIRE_NO_ERRNO(cache_read(mem_space, &paddr_aligned, L1_DCACHE, l1_cache, l2_cache, &word, replace));
    switch (bit_select)
    {
    case 0:
        word = (word & 0xFFFFFF00) | p_byte;
        break;
    case 1:
        word = (word & 0xFFF00FF) | (p_byte << OCTET);
        break;
    case 2:
        word = (word & 0xFF00FFFF) | (p_byte << OCTET * 2);
        break;
    default:
        word = (word & 0x00FFFFFF) | (p_byte << OCTET * 3);
        break;
    }
    return cache_write(mem_space, &paddr_aligned, l1_cache, l2_cache, &word, replace);
}
