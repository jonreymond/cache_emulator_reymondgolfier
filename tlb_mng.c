#include "tlb_mng.h"


int tlb_entry_init( const virt_addr_t * vaddr,
    const phy_addr_t * paddr,
    tlb_entry_t * tlb_entry) {
    M_REQUIRE_NON_NULL(vaddr);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(tlb_entry);

    tlb_entry->tag = virt_addr_t_to_virtual_page_number(vaddr);
    tlb_entry->phy_page_num = paddr->phy_page_num;
    tlb_entry->v = 1;

    return ERR_NONE;
}

int tlb_flush(tlb_entry_t * tlb) {
    M_REQUIRE_NON_NULL(tlb);

    memset(tlb, 0, TLB_LINES * sizeof(tlb_entry_t));

    return ERR_NONE;
}

int tlb_hit(const virt_addr_t * vaddr,
            phy_addr_t * paddr,
            const tlb_entry_t * tlb,
            replacement_policy_t * replacement_policy) {
    M_REQUIRE_NON_NULL(vaddr);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(tlb);
    M_REQUIRE_NON_NULL(replacement_policy);

    uint64_t virt_page_num = virt_addr_t_to_virtual_page_number(vaddr);

    for_all_nodes_reverse(node, replacement_policy->ll) {
        if (tlb[node->value].tag == virt_page_num && tlb[node->value].v) {
            paddr->page_offset = vaddr->page_offset;
            paddr->phy_page_num = tlb[node->value].phy_page_num;
            replacement_policy->move_back(replacement_policy->ll, node);

            return 1;
        }
    }

    return 0;
}


int tlb_insert( uint32_t line_index,
                const tlb_entry_t * tlb_entry,
                tlb_entry_t * tlb) {
    M_REQUIRE_NON_NULL(tlb_entry);
    M_REQUIRE_NON_NULL(tlb);

    if (line_index >= TLB_LINES) {
        return ERR_BAD_PARAMETER; // index out of bounds
    }

    tlb[line_index].tag = tlb_entry->tag;
    tlb[line_index].phy_page_num = tlb_entry->phy_page_num;
    tlb[line_index].v = tlb_entry->v;

    return ERR_NONE;
}

int tlb_search( const void * mem_space,
                const virt_addr_t * vaddr,
                phy_addr_t * paddr,
                tlb_entry_t * tlb,
                replacement_policy_t * replacement_policy,
                int* hit_or_miss) {
    M_REQUIRE_NON_NULL(mem_space);
    M_REQUIRE_NON_NULL(vaddr);
    M_REQUIRE_NON_NULL(paddr);
    M_REQUIRE_NON_NULL(tlb);
    M_REQUIRE_NON_NULL(replacement_policy);
    M_REQUIRE_NON_NULL(hit_or_miss);

    *hit_or_miss = tlb_hit(vaddr, paddr, tlb, replacement_policy);
    if (!(*hit_or_miss)) {
        if (page_walk(mem_space, vaddr, paddr) == ERR_NONE) {
            tlb_entry_t tlb_entry;
            tlb_entry_init(vaddr, paddr, &tlb_entry);

            tlb[replacement_policy->ll->front->value] = tlb_entry;
            replacement_policy->move_back(replacement_policy->ll, replacement_policy->ll->front);
        }
    }

    return ERR_NONE;
}
