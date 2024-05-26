#ifndef _RPI_MMU_H_
#define _RPI_MMU_H_

#define UADDR_TO_KADDR(x) (x + 0xffff000000000000)
#define KADDR_TO_UADDR(x) (x - 0xffff000000000000)
#define ENTRY_ADDR_MASK   0xfffffffff000L

#endif /*_RPI_MMU_H_ */
