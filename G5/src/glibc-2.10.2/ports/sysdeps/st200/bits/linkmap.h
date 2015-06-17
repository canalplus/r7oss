struct link_map_machine
  {
    size_t fptr_table_len;
    Elf32_Addr *fptr_table;
    Elf32_Addr plt; /* Address of first plt minentry (.plt + 0x20)  */
    Elf32_Addr gotplt; /* Address of first .gotplt entry (.got + 0x0c) */
  };
