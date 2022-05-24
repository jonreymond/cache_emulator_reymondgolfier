#include "page_walk.h"

#define WORD_SIZE 4

static inline pte_t read_page_entry(const pte_t * start,
                                    pte_t page_start,
                                    uint16_t index)
{
    const pte_t* address = (start + (page_start / sizeof(pte_t))) + index;
    return *address;
}

int page_walk(const void* mem_space, const virt_addr_t* vaddr, phy_addr_t* paddr){
    M_REQUIRE(mem_space != NULL, ERR_BAD_PARAMETER, "mem_space pointer is nul%c", 'l');
    M_REQUIRE(vaddr != NULL, ERR_BAD_PARAMETER, "vaddr pointer is nul%c", 'l');
    M_REQUIRE(paddr != NULL, ERR_BAD_PARAMETER, "paddr pointer is nul%c", 'l');

   pte_t pgd_content = read_page_entry(mem_space, 0,  vaddr->pgd_entry);
   pte_t pud_content = read_page_entry(mem_space, pgd_content, vaddr->pud_entry);
   pte_t pmd_content = read_page_entry(mem_space, pud_content, vaddr->pmd_entry);
   pte_t pte_content = read_page_entry(mem_space, pmd_content, vaddr->pte_entry);

    init_phy_addr(paddr, pte_content, vaddr->page_offset);

    return ERR_NONE;

}
