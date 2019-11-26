#include "s3c2440_soc.h"
#include "my_type.h"
#include "my_printf.h"
#include "string_utils.h"

void nand_write_reg_cmd(unsigned char cmd)
{
    NFCMD = cmd;
}

void nand_write_reg_addr(unsigned char addr)
{
    NFADDR = addr;
}

void nand_write_reg_data(u8 data)
{
    NFDATA = data;
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

void clear_RnB(void)
{
    NFSTAT |= (1 << 2);
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
    nand_enable_ce();
    nand_write_reg_cmd(0xff);

    nand_wait_ready();
    nand_disable_ce();
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

u8 nand_check_bad(u32 addr)
{
    int oob_data[2];
    int block = addr / 0x20000;
    int page = block * 64;

    nand_read_oob(page, 1, oob_data);
    page++;
    nand_read_oob(page, 1, &(oob_data[1]));

    if ((oob_data[0] != 0xff) || (oob_data[1] != 0xff))
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

int nand_erase(u32 addr, u32 len)
{
    int page;
    if (addr & 0x1ffff)
    {
        printf("error!addr is not block aligned\r\n");
        return -1;
    }

    if (len & 0x1ffff)
    {
        printf("error!len is not block aligned\r\n");
        return -1;
    }

    page = addr / 2048;
    while (1)
    {
        nand_enable_ce();
        nand_write_reg_cmd(0x60);
        nand_page(page);
        nand_write_reg_cmd(0xD0);
        nand_wait_ready();
        nand_write_reg_cmd(0x70);
        if (nand_read_reg_data() & 0x01)
        {
            printf("it occur some errors during erasing the flash\r\n");
            return -1;
        }
        else
        {
            len -= 128 * 1024;
            if (len == 0)
                break;
        }

        nand_disable_ce();
    }
    return 0;
}

int nand_write(u32 addr, u8 *data, u32 len)
{
    int i = 0;
    int j = 0;
    u32 page = addr / 2048;
    u32 col = addr & 2047;

    NFCONT &= ~(1 << 12); //unlock nand flash
    nand_enable_ce();
    while (i < len)
    {

        if(addr & 0x1ffff)
        {
            if(nand_check_bad(addr) == 0)
            {
                addr += 0x20000;
                page += 64;
            }
        }

        clear_RnB();
        nand_write_reg_cmd(0x80);

        nand_col(col);
        nand_page(page);

        for (; (i < len) && (col < 2048); col++)
        {
            //if (i < len)
            {
                nand_write_reg_data(data[i++]);
                addr++;
            }
            // else
            // {
            //     nand_write_reg_data(0xff);
            // }
        }

        nand_write_reg_cmd(0x10);
        nand_wait_ready();
        nand_write_reg_cmd(0x70);
        if (nand_read_reg_data() & 0x1)
        {
            // nand_disable_ce();
            printf("page program operation fail!\r\n");
            break;
        }
        if (i == len)
        {
            break;
        }
        col = 0;
        page++;
        // nand_disable_ce();
    }
    nand_disable_ce();
    NFCONT |= (1 << 12); //lock nand flash
}

/*addr and len are both page-alignedly needed*/
void nand_write_with_ecc(u32 addr, u8 *data, u32 len)
{
    int i=0;
    int page, col;
    u8 ecc_buf[6];

    page = addr / 2048;
    col = addr & 2047;
    if (col)
    {
        printf("error!addr is not page aligned!\r\n");
        return;
    }

    if (col)
    {
        printf("error!len is not page aligned!\r\n");
        return;
    }

    nand_enable_ce();
    while (i<len)
    {
        NFCONT |= (1 << 4);               //init the ecc module
        NFCONT &= ~((1 << 5) | (1 << 6)); //unlock main ecc and spare ecc

        nand_write_reg_cmd(0x80);

        nand_col(col);
        nand_page(page);

        for (;  (col < 2048); col++)
        {
            nand_write_reg_data(data[i++]);
        }

        NFCONT |= (1 << 5); //lock main ecc

        ecc_buf[0] = NFMECC0 & 0XFF;
        ecc_buf[1] = (NFMECC0 >> 16) & 0XFF;
        ecc_buf[2] = NFMECC1 & 0XFF;
        ecc_buf[3] = (NFMECC1 >> 16) & 0XFF;

        nand_write_reg_data(ecc_buf[0]);
        nand_write_reg_data(ecc_buf[1]);
        nand_write_reg_data(ecc_buf[2]);
        nand_write_reg_data(ecc_buf[3]);

        NFCONT |= (1 << 6); //lock spare ecc

        ecc_buf[4] = NFSECC & 0XFF;
        ecc_buf[5] = (NFSECC >> 16) & 0XFF;

        nand_write_reg_data(ecc_buf[4]);
        nand_write_reg_data(ecc_buf[6]);
        printf("fuck ww fuck you!\r\n");

        nand_write_reg_cmd(0x10);
        nand_wait_ready();
        nand_write_reg_cmd(0x70);
        if (nand_read_reg_data() & 0x01)
        {

            printf("page program operation fail!\r\n");
            break;
        }
        if (i == len)
        {
            break;
        }
        col = 0;
        page++;
    }
    nand_disable_ce();
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

    if (len > 2048)
    {
        len = 2048;
        printf("\r\nthe max len to read is 2048!\r\n ");
    }

    nand_read(addr, len, data);

    printf("Data : \n\r");
    /* 长度固定为64 */
    for (i = 0; i < len / 16; i++)
    {
        /* 每行打印16个数据 */
        for (j = 0; j < 16; j++)
        {
            /* 先打印数值 */

            str[j] = data[i * 16 + j];
            printf("%02x ", str[j]);
        }

        printf("   ; ");

        for (j = 0; j < 16; j++)
        {
            /* 后打印字符 */
            if (str[j] < 0x20 || str[j] > 0x7e) /* 不可视字符 */
                putchar('.');
            else
                putchar(str[j]);
        }
        printf("\n\r");
    }
}

void do_nand_erase_test(void)
{
    unsigned int addr;
    u32 len;
    int i, j;
    unsigned char c;
    unsigned char str[16];
    u8 data[2048];

    while (1)
    {
        /* 获得地址 */
        printf("Enter the address to erase: ");
        addr = get_uint();
        if (addr & 2047)
        {
            printf("\r\nerror!addr should be page aligned!\r\n");
            continue;
        }
        else
        {
            break;
        }
    }

    // while (1)
    // {
    //     printf("Enter the length to read: ");
    //     len = get_uint();
    //     if (len & 2047)
    //     {
    //         printf("\r\nerror!len should be page aligned!\r\n");
    //         continue;
    //     }
    //     else
    //     {
    //         break;
    //     }
    // }

    printf("erasing..........");
    nand_erase(addr, 128 * 1024);
    printf("\r\nerase finish!\r\n");
}

void do_write_nand_flash(void)
{
    unsigned int addr;
    unsigned char str[100];
    int len = 0;
    int i = 0;
    unsigned int val;

    /* 获得地址 */
    printf("Enter the address of sector to write: ");
    addr = get_uint();

    printf("Enter the string to write: ");
    gets(str);

    printf("the string u inserted is : %s\r\n", str);

    // while (str[i] != '\0')
    // {
    //     len++;
    //     i++;
    // }
    // len++;
    len = strlen(str) + 1;
    printf("length of the string u inserted is %d\r\n", len);
    //len = strlen(str);

    printf("writing ...\n\r");

    /* str[0],str[1]==>16bit 
	 * str[2],str[3]==>16bit 
	 */
    nand_write(addr, str, len);
    printf("write finish! ...\n\r");
}

void do_nand_write_ecc_test(void)
{
    u8 data[2048];
    int addr;
    int i = 0;
    /* 获得地址 */
    printf("\r\nEnter the address of sector to write: ");
    addr = get_uint();

    if (addr & 2047)
    {
        printf("error!addr is not page aligned!\r\n");
    }

    for (i = 0; i < 2048; i++)
    {
        data[i] = i & 0xff;
    }
    printf("writing..........\r\n");
    nand_write_with_ecc(addr, data, 2048);
    printf("write finished!\r\n");
}

void do_nand_read_ecc_test(void)
{
    u8 data[64];
    int addr;
    int page;
    int len;
    int i = 0, j = 0;

    /* 获得地址 */
    printf("\r\nEnter the address of sector to write: ");
    addr = get_uint();
    printf("\r\nEnter the num of oob data u wanna get:");
    len = get_uint();
    if (len > 64)
        len = 64;

    page = addr / 2048;

    nand_read_oob(page, len, data);

    printf("data:\r\n");

    for (i = 0; i < len; i++)
    {
       // for (j = 0; j < 16; j++)
        {
            printf("%02x ", data[i]);
        }
        if(i!=0&&i%16==0)
        printf("\r\n");
    }
}

void nand_flash_test(void)
{
    char c;

    while (1)
    {
    A:
        /* 打印菜单, 供我们选择测试内容 */
        printf("[s] Scan nand flash\n\r");
        printf("[e] Erase nand flash\n\r");
        printf("[w] Write nand flash\n\r");
        printf("[r] Read nand flash\n\r");
        printf("[c] Enter ECC mode\n\r");
        printf("[q] quit\n\r");
        printf("Enter selection: ");

        c = getchar();
        //printf("fuck\r\n");
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
            do_nand_erase_test();
            //do_erase_nor_flash();
            break;

        case 'w':
        case 'W':
            do_write_nand_flash();
            //do_write_nor_flash();
            break;

        case 'r':
        case 'R':
            do_nand_read_test();
            //do_read_nor_flash();

        case 'c':
        case 'C':

            while (1)
            {
                printf("\r\n[w] Write nand flash\n\r");
                printf("[r] Read nand flash\n\r");
                printf("[q] quit\n\r");
                printf("Enter selection: ");
                c = getchar();
                printf("%c\r\n", c);

                switch (c)
                {
                case 'r':
                case 'R':
                    do_nand_read_ecc_test();
                    goto A;
                    break;

                case 'w':
                case 'W':
                    do_nand_write_ecc_test();
                    goto A;
                    break;

                case 'q':
                case 'Q':
                    goto A;
                    break;

                default:
                    goto A;
                    break;
                }
            }

            break;

        case 'i':
        case 'I':

            break;

        default:
            break;
        }
    }
}
