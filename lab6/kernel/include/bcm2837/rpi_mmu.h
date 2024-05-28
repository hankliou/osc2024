#ifndef _RPI_MMU_H_
#define _RPI_MMU_H_

#define PHYS2VIRT(x)    (x + 0xffff000000000000)
#define VIRT2PHYS(x)    (x - 0xffff000000000000)
#define ENTRY_ADDR_MASK 0xfffffffff000L

#endif /*_RPI_MMU_H_ */
