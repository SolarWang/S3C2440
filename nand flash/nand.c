#include "s3c2440_soc.h"
#include "my_type.h"

/*
*HCLK为100MHZ，每个机器周期为10ns
*/
#define TACLS 0
#define TWRPH0 2
#define TWRPH1 1

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

void nand_read_id(void)
{
    nand_enable_ce();

    nand_disable_ce();
}

void nand_wait_ready(void)
{
    while (!(NFSTAT & 0X01))
        ;
}

void nand_init(void)
{
    NFCONF = ((TACLS << 12) | (TWRPH0 << 8) | (TWRPH1 << 4));
    NFCONT = (1 << 4) | (1 << 1) | (1 << 0);
}

void nand_read_id(void)
{
    int i = 0;
    unsigned char maker_code, device_code;
    nand_enable_ce();
    nand_cmd(0x90);
    nand_addr(0x00);
    for (i = 0; i < 10; i++)
        ;
    maker_code = nand_read_data();
    device_code = nand_read_data();
    nand_disable_ce();
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
void nand_read(u32 _addr, u32 len, u8 *data)
{
    int i = 0;
    int col = _addr % 2048;
    /*
    *这里为什么除以2048，而不是4096呢？
    *举个例子说明问题。假设我要读取地址为2048的数据，
    *注意，2048指的是nand flash main区的地址，spare区(OOB)不参与直接的寻址。
    *它处于nand flash的(page1,col0),如果把这个坐标代入32bit地址中看，地址为
    *4096,与2048矛盾，在main区寻址时正确的做法是要忽略A11。nand flash寻址要
    *把A12-A28当成一个数看，A11-A0当成一个数看。
    */
    int page = _addr / 2048;

    while (i < len)
    {
        nand_enable_ce();
        nand_cmd(0);
        nand_col(col);
        nand_page(page);
        nand_cmd(0x30);
        nand_wait_ready();

        for (; (i < len) && (col < 2048); col++)
        {
            data[i++] = nand_write_reg_data();
        }
        if (i >= len)
            break;
        col = 0;
        page++;
        nand_disable_ce();
    }
}

