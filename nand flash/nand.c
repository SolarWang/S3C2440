#include "s3c2440_soc.h"
#include "my_type.h"

/*
*HCLK为100MHZ，每个机器周期为10ns
*/
#define TACLS  0
#define TWRPH0 2
#define TWRPH1 1

void nand_write_cmd(unsigned char cmd)
{
    NFCMD = cmd;
}

void nand_write_addr(unsigned char addr)
{
    NFADDR = addr;
}

unsigned int nand_read_data(void)
{
    return NFDATA;
}


void nand_enable_ce(void)
{
    NFCONT &= ~(1<<1);
}

void nand_disable_ce(void)
{
    NFCONT |= (1<<1);
}

void nand_read_id(void)
{
    nand_enable_ce();

    nand_disable_ce();
}

void nand_wait_ready(void)
{
    while(!(NFSTAT&0X01));
}

void nand_init(void)
{
    NFCONF = ((TACLS<<12)|(TWRPH0<<8)|(TWRPH1<<4));
    NFCONT = (1<<4)|(1<<1)|(1<<0);
}

void nand_read_id(void)
{
	int i = 0;
    unsigned char maker_code,device_code;
	nand_enable_ce();
	nand_cmd(0x90);
	nand_addr(0x00);
	for(i = 0;i < 10;i++);
	maker_code = nand_read_data();
    device_code = nand_read_data();
	nand_disable_ce();
}

void nand_page(u32 addr)
{
    int i = 0;
    int temp = addr / 2048; 
    nand_addr(addr);
    for(i = 0;i < 10;i++);
    nand_addr(row_addr2);
    for(i = 0;i < 10;i++);
    nand_addr(row_addr3);
    for(i = 0;i < 10;i++);
}

#define BYTE1(X) *((volatile char *)(&X))
#define BYTE2(X) *((volatile char *)(&X)+1)
#define BYTE3(X) *((volatile char *)(&X)+2)
#define BYTE4(X) *((volatile char *)(&X)+3)

typedef struct 
{
    /* data */
    unsigned int col1 : 8;
    unsigned int col2 : 4;
    unsigned int row1 : 8;
    unsigned int row2 : 8;
    unsigned int row3 : 1;
}nand_addr_field_t;

typedef union
{
    int value;
    nand_addr_field_t field;
}nand_addr_t;

/*
 *nand flash的一次读操作是由指定地址一直读到page尾，详见ds p31
 *nand falsh的地址并不是完全连续，每行有2112个byte，前2048个byte为数据，
 *2049-2112为ECC内容，2112后有效数据的地址为行+1，列变为0
 *固当bit12为1时，读取的时ECC内容，当bit12为0时，读取的是存储的数据
 */
void nand_read(u32 _addr,u32 len,u8 *data)
{
    nand_addr_t addr;
    int i=0;
    int col = _addr % 2048;
    int row = _addr / 2048;

    // addr.value = _addr;
    
    // unsigned char col_addr1 = addr.field.col1;
    // unsigned char col_addr2 = addr.field.col2;
    // unsigned char row_addr1 = addr.field.row1;
    // unsigned char row_addr2 = addr.field.row2;
    // unsigned char row_addr3 = addr.field.row3; 

    while(i < len)
    {
        nand_enable_ce();
        
        i++;
        nand_disable_ce();
    }

    

}






#define PAGE_SIZE 2112
void nand_read(unsigned int _addr)
{
    nand_addr_t addr;
    unsigned char data[PAGE_SIZE];
    int i=0;



    addr.value = _addr;
    

    unsigned char col_addr1 = addr.field.col1;
    unsigned char col_addr2 = addr.field.col2;
    unsigned char row_addr1 = addr.field.row1;
    unsigned char row_addr2 = addr.field.row2;
    unsigned char row_addr3 = addr.field.row3;
    nand_cmd(0);
    nand_addr(col_addr1);
    nand_addr(col_addr2);
    nand_addr(row_addr1);
    nand_addr(row_addr2);
    nand_addr(row_addr3);
    nand_cmd(0x30);
    nand_wait_ready();
    for(i = 0;i < PAGE_SIZE;i++)
    {
        data[i] = nand_read_data();
    }

}

void nand_loop(void)
{
    
}