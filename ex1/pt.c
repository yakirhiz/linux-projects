#include "os.h"

void page_table_update(uint32_t pt, uint32_t vpn, uint32_t ppn){
	uint32_t upper_vpn = vpn >> 10;
	uint32_t lower_vpn = vpn & 0x3ff;
	
	uint32_t* table_one_base = (uint32_t*) phys_to_virt(pt << 12);
	
	uint32_t* table_one_pos = table_one_base + upper_vpn;
	
	uint32_t table_one_cont = *table_one_pos;
	
	if((table_one_cont & 1) == 0) {
		if(ppn == NO_MAPPING) {
			return;
		}
		
		*table_one_pos = (alloc_page_frame() << 12) | 1;
		
		table_one_cont = *table_one_pos;
	}
	
	uint32_t* table_two_base = (uint32_t*) phys_to_virt(table_one_cont & 0xfffffffe);
	
	uint32_t* table_two_pos = table_two_base + lower_vpn;
	
	if(ppn == NO_MAPPING){
		*table_two_pos = 0;
	} else {
		*table_two_pos = (ppn << 12) | 1;
	}
}

uint32_t page_table_query(uint32_t pt, uint32_t vpn){
	uint32_t upper_vpn = vpn >> 10;
	uint32_t lower_vpn = vpn & 0x3ff;
	
	uint32_t* table_one_base = (uint32_t*) phys_to_virt(pt << 12);
	
	uint32_t* table_one_pos = table_one_base + upper_vpn;
	
	uint32_t table_one_cont = *table_one_pos;
	
	if((table_one_cont & 1) == 0)
		return NO_MAPPING;
	
	uint32_t* table_two_base = (uint32_t*) phys_to_virt(table_one_cont & 0xfffffffe);
	
	uint32_t* table_two_pos = table_two_base + lower_vpn;
	
	uint32_t table_two_cont = *table_two_pos;
	
	if((table_two_cont & 1) == 0)
		return NO_MAPPING;
		
	return table_two_cont >> 12;
}