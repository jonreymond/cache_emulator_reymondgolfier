#include "tlb_hrchy_mng.h"


//=========================================================================

#define init_entry(TYPE, LINES_BITS) \
    do { \
        ((TYPE *)tlb_entry)->tag = virt_addr_t_to_virtual_page_number(vaddr) >> LINES_BITS; /*we remove the index to keep only tag*/ \
        ((TYPE *)tlb_entry)->phy_page_num = paddr->phy_page_num; \
        ((TYPE *)tlb_entry)->v = 1; \
    } while(0)

int tlb_entry_init( const virt_addr_t * vaddr,
                    const phy_addr_t * paddr,
                    void * tlb_entry,
                    tlb_t tlb_type){

    M_REQUIRE_NON_NULL(vaddr);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(tlb_entry);

    switch (tlb_type) {
        case L1_ITLB:
            init_entry(l1_itlb_entry_t, L1_ITLB_LINES_BITS);
            break;
        case L1_DTLB:
            init_entry(l1_dtlb_entry_t, L1_DTLB_LINES_BITS);
            break;
        case L2_TLB:
            init_entry(l2_tlb_entry_t, L2_TLB_LINES_BITS);
            break;
        default :  return ERR_BAD_PARAMETER;
    }
    return ERR_NONE;
}

//=========================================================================

#define flush(TYPE, LINES) \
    do { \
        memset((TYPE *)tlb, 0, LINES * sizeof(TYPE)); \
    } while(0)

int tlb_flush(void *tlb, tlb_t tlb_type){
    M_REQUIRE_NON_NULL(tlb);

    switch (tlb_type) {
        case L1_ITLB:
            flush(l1_itlb_entry_t, L1_ITLB_LINES);
            break;
        case L1_DTLB:
            flush(l1_dtlb_entry_t, L1_DTLB_LINES);
            break;
        case L2_TLB:
            flush(l2_tlb_entry_t, L2_TLB_LINES);
            break;
        default :  return ERR_BAD_PARAMETER;
    }
    return ERR_NONE;
}

//=========================================================================

#define insert(TYPE, LINES) \
    do { \
        M_REQUIRE(line_index < LINES, ERR_BAD_PARAMETER, "insert : line index bigger than L1_ITLB_LINES %c", ' '); \
        ((TYPE *)tlb)[line_index].tag = ((TYPE *)tlb_entry)->tag; \
        ((TYPE *)tlb)[line_index].phy_page_num = ((TYPE *)tlb_entry)->phy_page_num; \
        ((TYPE *)tlb)[line_index].v = ((TYPE *)tlb_entry)->v; \
    } while(0)

int tlb_insert( uint32_t line_index,
                const void * tlb_entry,
                void * tlb,
                tlb_t tlb_type){

    M_REQUIRE_NON_NULL(tlb_entry);
    M_REQUIRE_NON_NULL(tlb);

    switch (tlb_type) {
        case L1_ITLB:
            insert(l1_itlb_entry_t, L1_ITLB_LINES);
            break;
        case L1_DTLB:
            insert(l1_dtlb_entry_t, L1_DTLB_LINES);
            break;
        case L2_TLB:
            insert(l2_tlb_entry_t, L2_TLB_LINES);
            break;
        default :  return ERR_BAD_PARAMETER;
    }
    return ERR_NONE;
}

//=========================================================================

#define hit(TYPE, LINES, LINES_BITS) \
    do { \
        line_index = virtual_page_number % LINES; \
        tag = virtual_page_number >> LINES_BITS; \
        if(((TYPE *)tlb)[line_index].v == 1 && ((TYPE *)tlb)[line_index].tag == tag){ \
            paddr->phy_page_num = ((TYPE *)tlb)[line_index].phy_page_num; \
            paddr->page_offset = vaddr->page_offset; \
            hit = 1; \
        } \
    } while(0)

int tlb_hit( const virt_addr_t * vaddr,
             phy_addr_t * paddr,
             const void  * tlb,
             tlb_t tlb_type){

    if(vaddr == NULL || paddr == NULL || tlb == NULL) return 0;

    uint32_t line_index = 0;
    uint32_t tag = 0;

    uint64_t virtual_page_number =  virt_addr_t_to_virtual_page_number(vaddr);
    int hit = 0;

    switch (tlb_type) {
        case L1_ITLB:
            hit(l1_itlb_entry_t, L1_ITLB_LINES, L1_ITLB_LINES_BITS);
            break;
        case L1_DTLB:
            hit(l1_dtlb_entry_t, L1_DTLB_LINES, L1_DTLB_LINES_BITS);
            break;
        case L2_TLB:
            hit(l2_tlb_entry_t, L2_TLB_LINES, L2_TLB_LINES_BITS);
            break;
        default :  hit = 0;
    }
    return hit;
}

//=========================================================================

#define pointer_and_type(POINTER, TYPE) \
    do { \
        tlb_pointer = POINTER; \
        tlb_type = TYPE; \
    } while(0)

#define index_and_tag(LINES, LINES_BITS) \
    do { \
        index_tlb1 = virtual_page_number % LINES; \
        tag_tlb1 = virtual_page_number >> LINES_BITS; \
    } while(0)

#define insert_in_tlb(TYPE, TAG, INDEX, POINTER, TYPE2) \
    do { \
        TYPE entry_casted; \
        entry_casted.phy_page_num = paddr->phy_page_num; \
        entry_casted.tag = TAG; \
        entry_casted.v = 1; \
        error_code = tlb_insert(INDEX, &entry_casted, POINTER, TYPE2); \
    } while(0)

int tlb_search( const void * mem_space,
                const virt_addr_t * vaddr,
                phy_addr_t * paddr,
                mem_access_t access,
                l1_itlb_entry_t * l1_itlb,
                l1_dtlb_entry_t * l1_dtlb,
                l2_tlb_entry_t * l2_tlb,
                int* hit_or_miss){

    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(vaddr);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(l1_itlb);
    M_REQUIRE_NON_NULL(l1_dtlb);
    M_REQUIRE_NON_NULL(l2_tlb);
    M_REQUIRE_NON_NULL(hit_or_miss);

    // first level: search in caches of instructions or data
    int hit = 0;
    void* tlb_pointer = NULL;
    tlb_t tlb_type = L1_ITLB;

    if(access == INSTRUCTION){
        pointer_and_type(l1_itlb, L1_ITLB);
    } else {
        pointer_and_type(l1_dtlb, L1_DTLB);
    }
    hit = tlb_hit(vaddr, paddr, tlb_pointer, tlb_type);
    if(hit == 1){
        *hit_or_miss = 1;
        return ERR_NONE;
    }

    // second level: search in l2_tlb
    int error_code = 0;
    uint64_t virtual_page_number =  virt_addr_t_to_virtual_page_number(vaddr);
    uint32_t index_tlb1 = 0;
    uint32_t tag_tlb1 = 0;

    if (access == INSTRUCTION) {
        index_and_tag(L1_ITLB_LINES, L1_ITLB_LINES_BITS);
    } else {
        index_and_tag(L1_DTLB_LINES, L1_DTLB_LINES_BITS);
    }

    hit = tlb_hit(vaddr, paddr, l2_tlb, L2_TLB);
    if(hit == 1){
        *hit_or_miss = 1;

        //insert in the right tlb1
        if(access == INSTRUCTION){
            insert_in_tlb(l1_itlb_entry_t, tag_tlb1, index_tlb1, tlb_pointer, tlb_type);
        }
        else {
            insert_in_tlb(l1_dtlb_entry_t, tag_tlb1, index_tlb1, tlb_pointer, tlb_type);
        }

        return error_code;
    }

    //no hit part

    //page walk
    *hit_or_miss = 0;
    error_code = page_walk(mem_space, vaddr, paddr);
    if(error_code != ERR_NONE) return error_code;

    //insert in tlb2
    uint32_t tag_tlb2 = virtual_page_number >> L2_TLB_LINES_BITS;
    uint32_t index_tlb2 = virtual_page_number % L2_TLB_LINES;
    insert_in_tlb(l2_tlb_entry_t, tag_tlb2, index_tlb2, l2_tlb, L2_TLB);

    if(error_code != ERR_NONE) return error_code;

    size_t mask_tlb1 = 3;                                           //to catch only the two LSB bits of the tlb1 tag
    size_t msb_index_tlb2 = index_tlb2 >> (L2_TLB_LINES_BITS - 2);  // to catch only the two MSB bits of the index on tlb2

    if(access == INSTRUCTION){
            //insert in tlb1
            insert_in_tlb(l1_itlb_entry_t, tag_tlb1, index_tlb1, tlb_pointer, tlb_type);

            //check if must invalidate in other tlb

            if((l1_dtlb[index_tlb1].tag & mask_tlb1) == msb_index_tlb2){
                l1_dtlb[index_tlb1].v = 0;
            }
        }
        else {
            //insert in tlb1
            insert_in_tlb(l1_dtlb_entry_t, tag_tlb1, index_tlb1, tlb_pointer, tlb_type);
            //check if must invalidate in other tlb
            if((l1_itlb[index_tlb1].tag & mask_tlb1) == msb_index_tlb2){
                l1_itlb[index_tlb1].v = 0;
            }
        }
    return error_code;
}
