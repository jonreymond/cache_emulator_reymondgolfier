
#include "addr.h"
#include "addr_mng.h"
#include "error.h"
#include <inttypes.h>
#include <stdio.h> // FILE

#define MAX_VALUE_9 511//(PD_ENTRIES - 1)
#define MAX_VALUE_12 4095//(PAGE_SIZE - 1)

int print_virtual_address(FILE* where, const virt_addr_t* vaddr) {
    M_REQUIRE_NON_NULL(where);
    M_REQUIRE_NON_NULL(vaddr);

    return fprintf(where, "PGD=0x%" PRIX16 "; PUD=0x%" PRIX16 "; PMD=0x%" PRIX16 "; PTE=0x%" PRIX16 "; offset=0x%" PRIX16,
    vaddr->pgd_entry, vaddr->pud_entry, vaddr->pmd_entry, vaddr->pte_entry, vaddr->page_offset);
}

// Makes sure the parameters entered for a virtual address are valid
int control_virt_addr_param(const virt_addr_t * vaddr, uint16_t pgd_entry,
               uint16_t pud_entry, uint16_t pmd_entry,
               uint16_t pte_entry, uint16_t page_offset) {

    M_REQUIRE_NON_NULL(vaddr);
    M_REQUIRE(pgd_entry <= MAX_VALUE_9, ERR_BAD_PARAMETER, "PGD is too high (require > 9 bits) %c", ' ');
    M_REQUIRE(pud_entry <= MAX_VALUE_9, ERR_BAD_PARAMETER, "PUD is too high (require > 9 bits) %c", ' ');
    M_REQUIRE(pmd_entry <= MAX_VALUE_9, ERR_BAD_PARAMETER, "PMD is too high (require > 9 bits) %c", ' ');
    M_REQUIRE(pte_entry <= MAX_VALUE_9, ERR_BAD_PARAMETER, "PTE is too high (require > 9 bits) %c", ' ');
    M_REQUIRE(page_offset <= MAX_VALUE_12, ERR_BAD_PARAMETER, "PAGE_OFFSET is too high (require > 12 bits) %c", ' ');

    return ERR_NONE;
}


int init_virt_addr(virt_addr_t * vaddr,
                   uint16_t pgd_entry,
                   uint16_t pud_entry, uint16_t pmd_entry,
                   uint16_t pte_entry, uint16_t page_offset) {
    int c = control_virt_addr_param(vaddr, pgd_entry, pud_entry, pmd_entry, pte_entry, page_offset);

    if(c == ERR_NONE) {
        vaddr->reserved = 0;
        vaddr->pgd_entry = pgd_entry;
        vaddr->pud_entry = pud_entry;
        vaddr->pmd_entry = pmd_entry;
        vaddr->pte_entry = pte_entry;
        vaddr->page_offset = page_offset;
    }

    return c;
}

// Makes sure the entries of the given virtual address are correct
int control_virt_addr(const virt_addr_t * vaddr) {
    return control_virt_addr_param(vaddr, vaddr->pgd_entry, vaddr->pud_entry, vaddr->pmd_entry, vaddr->pte_entry, vaddr->page_offset);
}

int init_virt_addr64(virt_addr_t * vaddr, uint64_t vaddr64) {
    M_REQUIRE_NON_NULL(vaddr);

    uint16_t pgd = ((vaddr64 >> (PAGE_OFFSET + PTE_ENTRY + PMD_ENTRY + PUD_ENTRY)) & MAX_VALUE_9);
    uint16_t pud = ((vaddr64 >> (PAGE_OFFSET + PTE_ENTRY + PMD_ENTRY)) & MAX_VALUE_9);
    uint16_t pmd = ((vaddr64 >> (PAGE_OFFSET + PTE_ENTRY)) & MAX_VALUE_9);
    uint16_t pte = ((vaddr64 >> PAGE_OFFSET) & MAX_VALUE_9);
    uint16_t offset = (vaddr64 & MAX_VALUE_12);

    return init_virt_addr(vaddr, pgd, pud, pmd, pte, offset);
}

uint64_t virt_addr_t_to_virtual_page_number(const virt_addr_t * vaddr) {
    int c = control_virt_addr(vaddr);

    if(c == ERR_NONE){
        uint64_t v = ((uint64_t) vaddr->pgd_entry << (PTE_ENTRY + PMD_ENTRY + PUD_ENTRY)) |
                    ((uint64_t) vaddr->pud_entry << (PTE_ENTRY + PMD_ENTRY)) |
                    ((uint64_t) vaddr->pmd_entry << PTE_ENTRY) |
                    (uint64_t) vaddr->pte_entry;

        return v;
    }

    return 0;
}

uint64_t virt_addr_t_to_uint64_t(const virt_addr_t * vaddr) {
    int c = control_virt_addr(vaddr);

    if(c == ERR_NONE) {
        return (virt_addr_t_to_virtual_page_number(vaddr) << PAGE_OFFSET) | vaddr->page_offset;
    }

    return 0;
}

int print_physical_address(FILE* where, const phy_addr_t* paddr) {
    M_REQUIRE_NON_NULL(where);
    M_REQUIRE_NON_NULL(paddr);

    return fprintf(where, "page num=0x%" PRIX32 "; offset=0x%" PRIX32, paddr->phy_page_num, paddr->page_offset);
}

int init_phy_addr(phy_addr_t* paddr, uint32_t page_begin, uint32_t page_offset) {
    M_REQUIRE(page_offset <= MAX_VALUE_12, ERR_BAD_PARAMETER, "PAGE_OFFSET is too high (require > 12 bits) %c", ' ');
    M_REQUIRE_NON_NULL(paddr);

    paddr->page_offset = page_offset;
    paddr->phy_page_num = page_begin >> PAGE_OFFSET & ((1 << (PHY_PAGE_NUM + 1)) - 1);

    return ERR_NONE;
}
