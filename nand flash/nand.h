#ifndef NAND_H
#define NAND_H
#include "my_type.h"
void nand_init(void);
void nand_flash_test(void);
void nand_read(u32 addr, u32 len, u8 *data);
#endif