/**
 * @memory.c
 * @brief memory management functions (dump, init from file, etc.)
 *
 * @author Jean-Cédric Chappelier
 * @date 2018-19
 */

#if defined _WIN32  || defined _WIN64
#define __USE_MINGW_ANSI_STDIO 1
#endif

#include "memory.h"
#include "page_walk.h"
#include "addr_mng.h"
#include "util.h" // for SIZE_T_FMT
#include "error.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h> // for memset()
#include <inttypes.h> // for SCNx macros
#include <assert.h>
#include <ctype.h>

// ======================================================================
/**
 * @brief Tool function to print an address.
 *
 * @param show_addr the format how to display addresses; see addr_fmt_t type in memory.h
 * @param reference the reference address; i.e. the top of the main memory
 * @param addr the address to be displayed
 * @param sep a separator to print after the address (and its colon, printed anyway)
 *
 */
static void address_print(addr_fmt_t show_addr, const void* reference,
                          const void* addr, const char* sep)
{
    switch (show_addr) {
    case POINTER:
        (void)printf("%p", addr);
        break;
    case OFFSET:
        (void)printf("%zX", (const char*)addr - (const char*)reference);
        break;
    case OFFSET_U:
        (void)printf(SIZE_T_FMT, (const char*)addr - (const char*)reference);
        break;
    default:
        // do nothing
        return;
    }
    (void)printf(":%s", sep);
}

// ======================================================================
/**
 * @brief Tool function to print the content of a memory area
 *
 * @param reference the reference address; i.e. the top of the main memory
 * @param from first address to print
 * @param to first address NOT to print; if less that `from`, nothing is printed;
 * @param show_addr the format how to display addresses; see addr_fmt_t type in memory.h
 * @param line_size how many memory byted to print per stdout line
 * @param sep a separator to print after the address and between bytes
 *
 */
static void mem_dump_with_options(const void* reference, const void* from, const void* to,
                                  addr_fmt_t show_addr, size_t line_size, const char* sep)
{
    assert(line_size != 0);
    size_t nb_to_print = line_size;
    for (const uint8_t* addr = from; addr < (const uint8_t*) to; ++addr) {
        if (nb_to_print == line_size) {
            address_print(show_addr, reference, addr, sep);
        }
        (void)printf("%02"PRIX8"%s", *addr, sep);
        if (--nb_to_print == 0) {
            nb_to_print = line_size;
            putchar('\n');
        }
    }
    if (nb_to_print != line_size) putchar('\n');
}

// ======================================================================
// See memory.h for description
int vmem_page_dump_with_options(const void *mem_space, const virt_addr_t* from,
                                addr_fmt_t show_addr, size_t line_size, const char* sep)
{
#ifdef DEBUG
    debug_print("mem_space=%p\n", mem_space);
    (void)fprintf(stderr, __FILE__ ":%d:%s(): virt. addr.=", __LINE__, __func__);
    print_virtual_address(stderr, from);
    (void)fputc('\n', stderr);
#endif
    phy_addr_t paddr;
    zero_init_var(paddr);

    M_EXIT_IF_ERR(page_walk(mem_space, from, &paddr),
                  "calling page_walk() from vmem_page_dump_with_options()");
#ifdef DEBUG
    (void)fprintf(stderr, __FILE__ ":%d:%s(): phys. addr.=", __LINE__, __func__);
    print_physical_address(stderr, &paddr);
    (void)fputc('\n', stderr);
#endif

    const uint32_t paddr_offset = ((uint32_t) paddr.phy_page_num << PAGE_OFFSET);
    const char * const page_start = (const char *)mem_space + paddr_offset;
    const char * const start = page_start + paddr.page_offset;
    const char * const end_line = start + (line_size - paddr.page_offset % line_size);
    const char * const end   = page_start + PAGE_SIZE;
    debug_print("start=%p (offset=%zX)\n", (const void*) start, start - (const char *)mem_space);
    debug_print("end  =%p (offset=%zX)\n", (const void*) end, end   - (const char *)mem_space) ;
    mem_dump_with_options(mem_space, page_start, start, show_addr, line_size, sep);
    const size_t indent = paddr.page_offset % line_size;
    if (indent == 0) putchar('\n');
    address_print(show_addr, mem_space, start, sep);
    for (size_t i = 1; i <= indent; ++i) printf("  %s", sep);
    mem_dump_with_options(mem_space, start, end_line, NONE, line_size, sep);
    mem_dump_with_options(mem_space, end_line, end, show_addr, line_size, sep);
    return ERR_NONE;
}

int go_to_next_char(FILE* entree){
    char c = fgetc(entree);
    while(isspace(c)){
        M_REQUIRE(!feof(entree), ERR_BAD_PARAMETER, "the doc finishes to soo%c", 'n');
        c = fgetc(entree);
    }
    ungetc(c, entree);
    return ERR_NONE;
}

int fill_mem_at_address(void** memory, uint32_t phy_address, const char* filename){
  FILE* entree = fopen(filename, "rb");
  M_REQUIRE_NON_NULL_CUSTOM_ERR(entree, ERR_IO);

  fseek(entree, 0L, SEEK_END);
  size_t size_binary_file = (size_t) ftell(entree);
  rewind(entree);

  size_t nb_ok = fread((byte_t*) *memory + phy_address, 1, size_binary_file, entree);

  fclose(entree);

    if (nb_ok != size_binary_file) {
        return ERR_MEM;
    }

    return ERR_NONE;
}

int mem_init_from_dumpfile(const char* filename, void** memory, size_t* mem_capacity_in_bytes) {
    M_REQUIRE_NON_NULL(filename);
    M_REQUIRE_NON_NULL(memory);
    M_REQUIRE_NON_NULL(mem_capacity_in_bytes);

    FILE* entree = fopen(filename, "rb");
    M_REQUIRE_NON_NULL_CUSTOM_ERR(entree, ERR_IO);

    fseek(entree, 0L, SEEK_END);
    *mem_capacity_in_bytes = (size_t) ftell(entree);
    rewind(entree);

    *memory = calloc(*mem_capacity_in_bytes, 1);
    M_REQUIRE_NON_NULL_CUSTOM_ERR(*memory, ERR_MEM);

    size_t nb_ok = fread(*memory, 1, *mem_capacity_in_bytes, entree);

    fclose(entree);

    if (nb_ok != *mem_capacity_in_bytes) {
        return ERR_MEM;
    }

    return ERR_NONE;
}


int mem_init_from_description(const char* master_filename, void** memory, size_t* mem_capacity_in_bytes){
    M_REQUIRE_NON_NULL(master_filename);
    M_REQUIRE_NON_NULL(memory);
    M_REQUIRE_NON_NULL(mem_capacity_in_bytes);

    FILE* entree = fopen(master_filename, "r");
    M_REQUIRE_NON_NULL_CUSTOM_ERR(entree, ERR_IO);

    size_t nb_ok = fscanf(entree, "%zu", mem_capacity_in_bytes);
    if(nb_ok == 0 || nb_ok ==EOF){
        fclose(entree);
        return ERR_IO;
    }

    *memory = calloc(*mem_capacity_in_bytes, 1);
    M_REQUIRE_NON_NULL_CUSTOM_ERR(entree, ERR_MEM);

    nb_ok = go_to_next_char(entree);
    if(nb_ok != ERR_NONE){
        free(*memory);
        fclose(entree);
        return nb_ok;
    }

    char* page_filename = calloc(50, 1);
    nb_ok = fscanf(entree, "%s", page_filename);
    if(nb_ok == 0 || nb_ok ==EOF){
        free(*memory);
        free(page_filename);
        fclose(entree);
        return ERR_IO;
    }

    nb_ok = fill_mem_at_address(memory, 0, page_filename);
    if (nb_ok != ERR_NONE) {
        free(*memory);
        free(page_filename);
        fclose(entree);
        return nb_ok;
    }

    nb_ok = go_to_next_char(entree);
    if(nb_ok != ERR_NONE){
        free(*memory);
        free(page_filename);
        fclose(entree);
        return nb_ok;
    }
    int number_of_pages = 0;
    nb_ok = fscanf(entree, "%d", &number_of_pages);
    if(nb_ok == 0 || nb_ok ==EOF){
        free(*memory);
        free(page_filename);
        fclose(entree);
        return ERR_IO;
    }

    uint32_t physical_address = 0;

    for(int i = 0; i < number_of_pages; i++){
      nb_ok = go_to_next_char(entree);
      if(nb_ok != ERR_NONE){
        free(*memory);
        free(page_filename);
        fclose(entree);
        return nb_ok;
    }
      nb_ok = fscanf(entree, "%x", &physical_address);
      if(nb_ok == 0 || nb_ok ==EOF){
          free(*memory);
          free(page_filename);
          fclose(entree);
          return ERR_IO;
        }
        nb_ok = go_to_next_char(entree);
        if(nb_ok != ERR_NONE){
        free(*memory);
        free(page_filename);
        fclose(entree);
        return nb_ok;
    }

        nb_ok = fscanf(entree, "%s", page_filename);
        if(nb_ok == 0 || nb_ok ==EOF){
            free(*memory);
            free(page_filename);
            fclose(entree);
            return ERR_IO;
        }

        nb_ok = fill_mem_at_address(memory, physical_address, page_filename);
        if (nb_ok != ERR_NONE) {
            free(*memory);
            free(page_filename);
            fclose(entree);
            return nb_ok;
        }
    }

  nb_ok = go_to_next_char(entree);
  if(nb_ok != ERR_NONE){
        free(*memory);
        free(page_filename);
        fclose(entree);
        return nb_ok;
    }

  uint64_t virtual_address = 0;
  uint32_t translated_physical_address = 0;
  while(!feof(entree)){
    nb_ok = fscanf(entree, "%lx", &virtual_address);
      if(nb_ok == 0 || nb_ok ==EOF){
        free(*memory);
        free(page_filename);
        fclose(entree);
        return ERR_IO;
    }
    nb_ok = go_to_next_char(entree);
     if(nb_ok != ERR_NONE){
        free(*memory);
        free(page_filename);
        fclose(entree);
        return nb_ok;
    }

    nb_ok = fscanf(entree, "%s", page_filename);
    if(nb_ok == 0 || nb_ok ==EOF){
        free(*memory);
        free(page_filename);
        fclose(entree);
        return ERR_IO;
    }
    nb_ok = go_to_next_char(entree);
    if(nb_ok != ERR_NONE){
        free(*memory);
        free(page_filename);
        fclose(entree);
        return nb_ok;
    }

    virt_addr_t vaddr;
    phy_addr_t  paddr;
    
    nb_ok = init_virt_addr64(&vaddr, virtual_address);
    if(nb_ok != ERR_NONE){
        free(*memory);
        free(page_filename);
        fclose(entree);
        return nb_ok;
    }

    nb_ok = page_walk(*memory, &vaddr, &paddr);
    if(nb_ok != ERR_NONE){
        free(*memory);
        free(page_filename);
        fclose(entree);
        return nb_ok;
    }

    translated_physical_address = (paddr.phy_page_num << PAGE_OFFSET) | paddr.page_offset;

    nb_ok = fill_mem_at_address(memory, translated_physical_address, page_filename);
    if (nb_ok != ERR_NONE) {
        free(*memory);
        free(page_filename);
        fclose(entree);
        return nb_ok;
    }

    nb_ok = go_to_next_char(entree);
    if (nb_ok != ERR_NONE) {
        free(*memory);
        free(page_filename);
        fclose(entree);
        return nb_ok;
    }
  }
  fclose(entree);
  free(page_filename);

  return ERR_NONE;
}
