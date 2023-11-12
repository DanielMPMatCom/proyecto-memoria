#include "bnb_manager.h"
#include "../memory.h"

#include "stdio.h"

#define MAX_PROCS_COUNT 20
int *bnb_pids;
addr_t *bnb_base;
addr_t *bnb_heap;
addr_t *bnb_stack;
addr_t *bnb_free_list;
int bnb_index_curr_pid;
addr_t bnb_bound;

addr_t bnb_pa(addr_t va)
{
  return bnb_base[bnb_index_curr_pid] + va;
}
int bnb_check_addr(addr_t va)
{
  return va >= bnb_bound;
}

// Esta función se llama cuando se inicializa un caso de prueba
void m_bnb_init(int argc, char **argv)
{
  bnb_pids = (int *)malloc(MAX_PROCS_COUNT * sizeof(int));
  bnb_base = (addr_t *)malloc(MAX_PROCS_COUNT * sizeof(addr_t));
  bnb_heap = (addr_t *)malloc(MAX_PROCS_COUNT * sizeof(addr_t));
  bnb_stack = (addr_t *)malloc(MAX_PROCS_COUNT * sizeof(addr_t));
  bnb_free_list = (addr_t *)malloc(m_size() * sizeof(addr_t));

  bnb_index_curr_pid = -1;
  bnb_bound = 1024;
  // bnb_bound = m_size() / MAX_PROCS_COUNT;

  for (int i = 0; i < MAX_PROCS_COUNT; i++)
  {
    bnb_pids[i] = -1;
    bnb_base[i] = i * bnb_bound;
    bnb_stack[i] = bnb_bound;
    bnb_heap[i] = 0;
  }
  for (size_t i = 0; i < m_size(); i++)
  {
    bnb_free_list[i] = -1;
  }
}

// Reserva un espacio en el heap de tamaño 'size' y establece un puntero al
// inicio del espacio reservado.
int m_bnb_malloc(size_t size, ptr_t *out)
{
  for (addr_t i = bnb_pa(0); i < bnb_stack[bnb_index_curr_pid] + bnb_pa(0); i++)
  {
    if (bnb_free_list[i] == (addr_t)bnb_index_curr_pid)
    {
      bnb_heap[bnb_index_curr_pid] = i + 1;
    }
    else
    {
      bnb_heap[bnb_index_curr_pid] = 0;
    }
  }

  if (bnb_heap[bnb_index_curr_pid] + size <= bnb_stack[bnb_index_curr_pid])
  {
    out->addr = bnb_heap[bnb_index_curr_pid];
    out->size = size;

    if (((int)out->addr < 0 && out->addr > bnb_bound) && ((int)out->addr + (int)size < 0 && out->addr + size > bnb_bound))
    {
      for (addr_t i = bnb_pa(out->addr); i < size + bnb_pa(out->addr); i++)
      {
        bnb_free_list[i] = bnb_pids[bnb_index_curr_pid];
      }
    }
    bnb_heap[bnb_index_curr_pid] += size;
    return 0;
  }
  return 1;
}

// Libera un espacio de memoria dado un puntero.
int m_bnb_free(ptr_t ptr)
{

  if (((int)ptr.addr < 0 || ptr.addr > bnb_bound) || ((int)bnb_free_list[bnb_pa(ptr.addr)] < 0))
  {
    return 1;
  }
  if (ptr.size > 0 || ptr.size < bnb_bound)
  {
    for (addr_t i = ptr.addr; i < ptr.size + ptr.addr; i++)
    {
      bnb_free_list[i] = -1;
    }
  }
  for (addr_t i = bnb_pa(0); i < bnb_stack[bnb_index_curr_pid] + bnb_pa(0); i++)
  {
    if ((int)bnb_free_list[i] == bnb_index_curr_pid)
    {
      bnb_heap[bnb_index_curr_pid] = i + 1;
    }
    else
    {
      bnb_heap[bnb_index_curr_pid] = 0;
    }
  }
  return 0;
}

// Agrega un elemento al stack
int m_bnb_push(byte val, ptr_t *out)
{
  if ((int)bnb_heap[bnb_index_curr_pid] < 0 || bnb_stack[bnb_index_curr_pid] - 1 > bnb_bound)
  {
    return 1;
  }
  bnb_stack[bnb_index_curr_pid]--;
  m_write(bnb_pa(bnb_stack[bnb_index_curr_pid]), val);
  out->addr = bnb_stack[bnb_index_curr_pid];
  out->size = 1;
  return 0;
}

// Quita un elemento del stack
int m_bnb_pop(byte *out)
{
  if ((int)bnb_stack[bnb_index_curr_pid] + 1 < 0 || bnb_stack[bnb_index_curr_pid] + 1 > bnb_bound)
  {
    return 1;
  }
  *out = m_read(bnb_pa(bnb_stack[bnb_index_curr_pid]));
  bnb_stack[bnb_index_curr_pid]++;
  return 0;
}

// Carga el valor en una dirección determinada
int m_bnb_load(addr_t addr, byte *out)
{
  if (addr > 0 && addr < bnb_bound && (int)bnb_free_list[bnb_pa(addr)] >= 0)
  {
    *out = m_read(bnb_pa(addr));
    return 0;
  }
  return 1;
}

// Almacena un valor en una dirección determinada
int m_bnb_store(addr_t addr, byte val)
{
  if ((int)addr >= 0 && addr <= bnb_bound && (int)bnb_free_list[bnb_pa(addr)] >= 0)
  {
    m_write(bnb_pa(addr), val);
    return 0;
  }
  return 1;
}

// Notifica un cambio de contexto al proceso 'next_pid'
void m_bnb_on_ctx_switch(process_t process)
{
  for (int i = 0; i < MAX_PROCS_COUNT; i++)
  {
    if (bnb_pids[i] == process.pid)
    {
      bnb_index_curr_pid = i;
      set_curr_owner(process.pid);
      return;
    }
  }
  for (int i = 0; i < MAX_PROCS_COUNT; i++)
  {
    if (bnb_pids[i] == -1)
    {
      bnb_index_curr_pid = i;
      bnb_pids[i] = process.pid;
      set_curr_owner(process.pid);
      m_set_owner(bnb_pa(0), bnb_pa(bnb_bound - 1));
      return;
    }
  }
  set_curr_owner(bnb_pids[bnb_index_curr_pid]); 
}

// Notifica que un proceso ya terminó su ejecución
void m_bnb_on_end_process(process_t process)
{
  for (int i = 0; i < MAX_PROCS_COUNT; i++)
  {
    if (process.pid == bnb_pids[i])
    {
      bnb_pids[i] = -1;
      bnb_stack[i] = bnb_bound;
      bnb_heap[i] = 0;
      m_unset_owner(bnb_pa(0), bnb_pa(bnb_bound - 1));
      for (size_t j = bnb_base[i]; j < bnb_base[i] + bnb_bound; j++)
      {
        bnb_free_list[j] = -1;
      }
      break;
    }
  }
}






















// #include "bnb_manager.h"

// #include "stdio.h"

// #include "../memory.h"

// #ifndef VAL_CODE
// #define VAL_CODE printf("Hey there, I was here")
// #define fori(base, bound) for (size_t i = (size_t)base; i < (size_t)base + bound; i++)
// #define min(a, b) (a < b) ? a : b
// #define max(a, b) (a > b) ? a : b
// #define clamp(a, b, x) (x > b) ? b : (x < a) ? a : x                                              
// #endif

// int BNB_MAX_PROC_COUNT = 20;

// int bnb_cur_ipid;
// int *bnb_pids;
// int *bnb_free_list;

// size_t *bnb_heap;
// size_t *bnb_stack;
// size_t bnb_default_bound;

// addr_t *bnb_bases;

// // functions
// addr_t bnb_pa(addr_t va)
// {
//   return bnb_bases[bnb_cur_ipid] + va;
// }

// int bnb_find_pid(int pid)
// {
//   fori(0, BNB_MAX_PROC_COUNT) if (bnb_pids[i] == pid) return i;
//   return -1;
// }

// int bnb_free_pid()
// {
//   return bnb_find_pid(-1);
// }

// int bnb_find_free_slots(int count, int *ls)
// {
//   int free_count = 0;
//   fori(bnb_pa(0), bnb_stack[bnb_cur_ipid])
//   {
//     if (bnb_free_list[i] == -1)
//     {
//       *ls = i;
//       free_count++;
//     }
//     else
//       free_count = 0;

//     if (free_count == count)
//       return 0;
//   }
//   return 1;
// }

// void bnb_new_procs()
// {
//   m_set_owner(bnb_pa(0), bnb_pa(bnb_default_bound - 1));
// }

// void bnb_delete_procs(int index)
// {
//   fori(bnb_pa(0), bnb_default_bound)
//       bnb_free_list[i] = -1;
//   m_unset_owner(bnb_pa(0), bnb_pa(bnb_default_bound - 1));
//   bnb_heap[index] = 0;
//   bnb_stack[index] = bnb_default_bound;
// }

// // 0 <= x < ls
// int bnb_is_valid(int x, int ls)
// {
//   return x < 0 || x > ls;
// }

// int bnb_bound_check(int va)
// {
//   return bnb_is_valid(va, bnb_default_bound);
// }

// // li <= x < ls, always current proc
// void bnb_resb(addr_t vli, int count)
// {
//   if (bnb_bound_check(vli) || bnb_bound_check(vli + count))
//   {
//     printf("RESB: Fuera de rango\n");
//     return;
//   }

//   fori(bnb_pa(vli), count)
//   {
//     if (bnb_free_list[i] != -1)
//       printf("RESB: Espacio ocupado 0x%zx\n", i);
//     bnb_free_list[i] = bnb_pids[bnb_cur_ipid];
//   }
// }

// void bnb_free_slots(size_t vli, int count)
// {
//   if (bnb_bound_check(vli) || bnb_bound_check(vli + count))
//   {
//     printf("FREE: Fuera de rango li:0x%zx -> %d  ls: 0x%zx -> %d\n", vli, bnb_bound_check(vli), vli + count, bnb_bound_check(vli + count));
//     return;
//   }
//   fori(bnb_pa(vli), count) bnb_free_list[i] = -1;
// }

// int bnb_mem_use(addr_t va)
// {
//   if (bnb_bound_check(va))
//   {
//     printf("USO DE MEMORIA : Fuera de rango va: 0x%zx Bounds: %ld\n", va, bnb_default_bound);
//     return 1;
//   }

//   if (bnb_free_list[bnb_pa(va)] < 0)
//   {
//     printf("USO DE MEMORIA : No alloced memory : va 0x%zx used: %d\n", va, bnb_free_list[bnb_pa(va)]);
//     return 1;
//   }

//   return 0;
// }

// void bnb_update_heap()
// {
//   bnb_heap[bnb_cur_ipid] = -1;
//   fori(bnb_pa(0), bnb_stack[bnb_cur_ipid]) bnb_heap[bnb_cur_ipid] = (bnb_free_list[i] == bnb_cur_ipid) ? i : bnb_heap[bnb_cur_ipid];
//   bnb_heap[bnb_cur_ipid]++;
// }

// // end functions

// // Esta función se llama cuando se inicializa un caso de prueba
// void m_bnb_init(int argc, char **argv)
// {
//   bnb_default_bound = 600;
//   // bnb_default_bound =  m_size() / 20;
//   // BNB_MAX_PROC_COUNT = m_size() / bnb_default_bound;

//   bnb_cur_ipid = -1;
//   bnb_pids = malloc(sizeof(int) * BNB_MAX_PROC_COUNT);
//   bnb_heap = malloc(sizeof(size_t) * BNB_MAX_PROC_COUNT);
//   bnb_stack = malloc(sizeof(size_t) * BNB_MAX_PROC_COUNT);
//   bnb_bases = malloc(sizeof(addr_t) * BNB_MAX_PROC_COUNT);

//   printf("MAX PROC COUNT: %d\n", BNB_MAX_PROC_COUNT);
//   printf("DEFAULT BOUND FOR Process: %ld\n", bnb_default_bound);

//   fori(0, BNB_MAX_PROC_COUNT)
//   {
//     bnb_pids[i] = -1;
//     bnb_heap[i] = 0;
//     bnb_stack[i] = bnb_default_bound;
//     bnb_bases[i] = bnb_default_bound * i;
//     // printf("BASE of i:%ld is %ld\n", i, bnb_bases[i]);
//   }

//   bnb_free_list = malloc(sizeof(int) * m_size());

//   fori(0, m_size())
//       bnb_free_list[i] = -1;
// }

// // Reserva un espacio en el heap de tamaño 'size' y establece un puntero al
// // inicio del espacio reservado.
// int m_bnb_malloc(size_t size, ptr_t *out)
// {
//   bnb_update_heap();
//   if (bnb_heap[bnb_cur_ipid] + size > bnb_stack[bnb_cur_ipid])
//   {
//     printf("MALLOC: HEAP OVERFLOW  Heap:%ld Stack: %ld DBound: %ld\n", bnb_heap[bnb_cur_ipid], bnb_stack[bnb_cur_ipid], bnb_default_bound);
//     return 1;
//   }
//   out->addr = bnb_heap[bnb_cur_ipid];
//   out->size = size;
//   bnb_heap[bnb_cur_ipid] += size;
//   bnb_resb(out->addr, size);
//   return 0;
// }

// // Libera un espacio de memoria dado un puntero.
// int m_bnb_free(ptr_t ptr)
// {
//   if (bnb_mem_use(ptr.addr))
//   {
//     printf("FREE: error in memory use\n");
//     printf("En [0x%zx , 0x%zx)\n", ptr.addr, ptr.addr + ptr.size);
//     return 1;
//   }
//   bnb_free_slots(ptr.addr, ptr.size);
//   bnb_update_heap();
//   return 0;
// }

// // Agrega un elemento al stack
// int m_bnb_push(byte val, ptr_t *out)
// {
//   if (bnb_is_valid(bnb_heap[bnb_cur_ipid], bnb_stack[bnb_cur_ipid] - 1))
//   {
//     if (bnb_free_list[bnb_pa(bnb_stack[bnb_cur_ipid]) - 1] != -1)
//     {
//       printf("PUSH: Espacio ocupado error en heap update Heap: %ld Stack - 1 : %ld\n", bnb_heap[bnb_cur_ipid], bnb_stack[bnb_cur_ipid] - 1);
//       return 1;
//     }
//     printf("PUSH: STACKOVERFLOW, HEAP FULL %ld, Stack - 1 : %ld\n", bnb_heap[bnb_cur_ipid], bnb_stack[bnb_cur_ipid] - 1);
//     return 1;
//   }
//   bnb_stack[bnb_cur_ipid]--;
//   m_write(bnb_pa(bnb_stack[bnb_cur_ipid]), val);
//   out->addr = bnb_stack[bnb_cur_ipid];
//   out->size = 1;
//   return 0;
// }

// // Quita un elemento del stack
// int m_bnb_pop(byte *out)
// {
//   if (bnb_bound_check(bnb_stack[bnb_cur_ipid] + 1))
//   {
//     printf("POP: Fuera de rango 0%zx -> 0%zx Bound: %ld\n", bnb_stack[bnb_cur_ipid], bnb_stack[bnb_cur_ipid] + 1, bnb_default_bound);
//     return 1;
//   }
//   *out = m_read(bnb_pa(bnb_stack[bnb_cur_ipid]));
//   bnb_stack[bnb_cur_ipid]++;
//   return 0;
// }

// // Carga el valor en una dirección determinada
// int m_bnb_load(addr_t addr, byte *out)
// {
//   if (bnb_mem_use(addr))
//   {
//     printf("LOAD: Error de memoria\n");
//     return 1;
//   }

//   *out = m_read(bnb_pa(addr));
//   return 0;
// }

// // Almacena un valor en una dirección determinada
// int m_bnb_store(addr_t addr, byte val)
// {

//   if (bnb_mem_use(addr))
//   {
//     printf("STORE: Error de memoria\n");
//     return 1;
//   }

//   m_write(bnb_pa(addr), val);
//   return 0;
// }

// // Notifica un cambio de contexto al proceso 'next_pid'
// void m_bnb_on_ctx_switch(process_t process)
// {
//   int temp = bnb_cur_ipid;

//   int index = bnb_find_pid(process.pid);
//   set_curr_owner(process.pid);
//   bnb_cur_ipid = index;

//   if (index < 0)
//   {
//     index = bnb_free_pid();
//     if (index < 0)
//     {
//       printf("CAMBIO DE CONTEXTO: pid: %d No hay espacios libres\n", process.pid);
//       bnb_cur_ipid = temp;
//       set_curr_owner(bnb_pids[bnb_cur_ipid]);
//       return;
//     }
    
//     bnb_pids[index] = process.pid;
//     bnb_cur_ipid = index;
//     bnb_new_procs();
//   }
// }

// // Notifica que un proceso ya terminó su ejecución
// void m_bnb_on_end_process(process_t process)
// {
//   int index = bnb_find_pid(process.pid);
//   if (index < 0)
//   {
//     printf("END: (pid: %d) No existe el proceso\n", process.pid);
//     return;
//   }
//   bnb_delete_procs(index);
//   bnb_pids[index] = -1;
// }