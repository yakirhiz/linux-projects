
#include <stdint.h>

#define NO_MAPPING	(~0U)

uint32_t alloc_page_frame(void);
void* phys_to_virt(uint32_t phys_addr);

void page_table_update(uint32_t pt, uint32_t vpn, uint32_t ppn);
uint32_t page_table_query(uint32_t pt, uint32_t vpn);


