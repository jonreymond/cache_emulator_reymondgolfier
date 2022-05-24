# cache_emulator_reymondgolfier
Implementation of a cache emulator during the course "Projet programmation système" ([link][1]).


## Files content
- addr.h:
    virt_addr_t,
    phy_addr_t
- addr_mng.c:
  - init_virt_addr(), init_virt_addr64(), init_phy_addr()
  - virt_addr_t_to_uint64_t()
  - virt_addr_t_to_virtual_page_number()
  - print_virtual_address(), print_physical_address()
  
- commands.h:
    command_word_t, command_t, program_t
- commands.c:
  - program_print()
  - program_init()
  - program_add_command()
  - program_shrink()
  - program_read():
      get_next_nonspace_char()
      do_D(), do_R(), do_W()
      command_line_read()
  - program_free():
  
- memory.c:
  - mem_init_from_dumpfile():  
      ouverture fichier, allocation mémoire, lecture fichier, fermeture fichier
  - mem_init_from_description():
      ouverture fichier, allocation mémoire, lecture PGD page filename
      -page_file_read():  
      lecture PUDs, lecture pages data, fermeture fichier
      
- page_walk.c:
    - read_page_entry()
    - page_walk()
    
- list.c:
    - is_empty_list, init_list, clear_list
    - push_back, push_front, pop_back, pop_front, move_back
    - print_list, print_reverse_list
    
- tlb_mng.c:
    - tlb_entry_init()
    - tlb_flush()
    - tlb_insert()
    - tlb_hit()
    - tlb_search()
    
- tlb_hrchy_mng.c:
    - tlb_entry_init()
    - tlb_flush()
    - tlb_insert()
    - tlb_hit()
    - tlb_search()å

[1]:https://edu.epfl.ch/coursebook/fr/projet-programmation-systeme-CS-212 "link"
