#include "s3c2440_soc.h"
#include "my_type.h"
#include "my_printf.h"

void nand_write_reg_cmd(unsigned char cmd)
{
    NFCMD = cmd;
}

void nand_write_reg_addr(unsigned char addr)
{
    NFADDR = addr;
}

unsigned int nand_read_reg_data(void)
{
    return NFDATA;
}

void nand_enable_ce(void)
{
    NFCONT &= ~(1 << 1);
}

void nand_disable_ce(void)
{
    NFCONT |= (1 << 1);
}

void nand_wait_ready(void)
{
    while (!(NFSTAT & 0X01))
        ;
}

void nand_init(void)
{
/*
    *HCLK为100MHZ，每个机器周期为10ns
    */
#define TACLS 0
#define TWRPH0 1
#define TWRPH1 0
    NFCONF = ((TACLS << 12) | (TWRPH0 << 8) | (TWRPH1 << 4));
    NFCONT = (1 << 4) | (1 << 1) | (1 << 0);
    //  nand_write_reg_cmd(0xff);
}

void nand_read_id(void)
{
    int i = 0;
    unsigned char maker_code, device_code;
    nand_enable_ce();
    nand_write_reg_cmd(0x90);
    nand_write_reg_addr(0x00);
    for (i = 0; i < 10; i++)
        ;
    maker_code = nand_read_reg_data();
    device_code = nand_read_reg_data();
    nand_disable_ce();

    printf("maker code is 0x%x\r\n", maker_code);
    printf("device code is 0x%x\r\n", device_code);
}

void nand_page(u32 page)
{
    int i = 0;

    nand_write_reg_addr(page & 0xff);
    for (i = 0; i < 10; i++)
        ;
    nand_write_reg_addr((page >> 8) & 0xff);
    for (i = 0; i < 10; i++)
        ;
    nand_write_reg_addr((page >> 16) & 0x1);
    for (i = 0; i < 10; i++)
        ;
}

void nand_col(u32 col)
{
    int i = 0;

    nand_write_reg_addr(col & 0xff);
    for (i = 0; i < 10; i++)
        ;
    nand_write_reg_addr((col >> 8) & 0xf);
    for (i = 0; i < 10; i++)
        ;
}

/*
 *nand flash的一次读操作是由指定地址一直读到page尾，详见ds p31
 *nand falsh的地址并不是完全连续，每行有2112个byte，前2048个byte为数据，
 *2049-2112为ECC内容，2112后有效数据的地址为行+1，列变为0
 *固当bit11为1时，读取的时ECC内容，当bit11为0时，读取的是存储的数据
 */
void nand_read(u32 addr, u32 len, u8 *data)
{
    int i = 0;
    int col = addr % 2048;
    /*
    *这里为什么除以2048，而不是4096呢？
    *举个例子说明问题。假设我要读取地址为2048的数据，
    *注意，2048指的是nand flash main区的地址，spare区(OOB)不参与直接的寻址。
    *它处于nand flash的(page1,col0),如果把这个坐标代入32bit地址中看，地址为
    *4096,与2048矛盾，在main区寻址时正确的做法是要忽略A11。nand flash寻址要
    *把A12-A28当成一个数看，A11-A0当成一个数看。
    */
    int page = addr / 2048;

    while (i < len)
    {
        nand_enable_ce();
        nand_write_reg_cmd(0);
        nand_col(col);
        nand_page(page);
        nand_write_reg_cmd(0x30);
        nand_wait_ready();

        for (; (i < len) && (col < 2048); col++)
        {
            data[i++] = nand_read_reg_data();
        }
        if (i >= len)
            break;
        col = 0;
        page++;
        nand_disable_ce();
    }
}

void nand_read_oob(u32 page, u32 len, u8 *data)
{
    int i = 0;
    int col = 2048;
    len = ((len > 64) ? 64 : len);

    nand_enable_ce();
    nand_write_reg_cmd(0);
    nand_col(col);
    nand_page(page);
    nand_write_reg_cmd(0x30);
    nand_wait_ready();

    for (; (i < len) && (col < 2112); col++)
    {
        data[i++] = nand_read_reg_data();
    }

    nand_disable_ce();
}

void nand_check(u32 addr)
{
    int oob_data[1];
    int block = addr / 0x8000000;
    int page = block * 64;

    nand_read_oob(page, 1, oob_data);
    page++;
    nand_read_oob(page, 1, oob_data);
}

void do_nand_read_test(void)
{
    unsigned int addr;
    u32 len;
	int i, j;
	unsigned char c;
	unsigned char str[16];
    u8 data[2048];
	
	/* 获得地址 */
	printf("Enter the address to read: ");
	addr = get_uint();

    printf("Enter the length to read: ");
	len = get_uint();

    if(len > 2048) 
    {
        len = 2048;
        printf("\r\nthe max len to read is 2048!\r\n ");
    }

    nand_read(addr,len,data);

	printf("Data : \n\r");
	/* 长度固定为64 */
	for (i = 0; i < len / 16; i++)
	{
		/* 每行打印16个数据 */
		for (j = 0; j < 16; j++)
		{
			/* 先打印数值 */
			
			str[j] = data[i*16+j];
			printf("%02x ", str[j]);
		}

		printf("   ; ");

		for (j = 0; j < 16; j++)
		{
			/* 后打印字符 */
			if (str[j] < 0x20 || str[j] > 0x7e)  /* 不可视字符 */
				putchar('.');
			else
				putchar(str[j]);
		}
		printf("\n\r");
	}
}

void nand_flash_test(void)
{
    char c;

    while (1)
    {
        /* 打印菜单, 供我们选择测试内容 */
        printf("[s] Scan nand flash\n\r");
        printf("[e] Erase nand flash\n\r");
        printf("[w] Write nand flash\n\r");
        printf("[r] Read nand flash\n\r");
        printf("[q] quit\n\r");
        printf("Enter selection: ");

        c = getchar();
        printf("fuck\r\n");
        printf("%c\n\r", c);
        

        /* 测试内容:
		 * 1. 识别nand flash
		 * 2. 擦除nand flash某个扇区
		 * 3. 编写某个地址
		 * 4. 读某个地址
		 */
        switch (c)
        {
        case 'q':
        case 'Q':
            return;
            break;

        case 's':
        case 'S':
        
            nand_read_id();
            //do_scan_nor_flash();
            break;

        case 'e':
        case 'E':
            //do_erase_nor_flash();
            break;

        case 'w':
        case 'W':
            //do_write_nor_flash();
            break;

        case 'r':
        case 'R':
            do_nand_read_test();
            //do_read_nor_flash();
            break;
        default:
            break;
        }
    }
}
