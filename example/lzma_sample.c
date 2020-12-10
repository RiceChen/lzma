#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rtthread.h>
#include <dfs_posix.h>

#include "LzmaDec.h"
#include "LzmaEnc.h"

static void help(void)
{
    rt_kprintf("Usage:\n");
    rt_kprintf("lzma -c [file] [cmprs_file]          -compress \"file\" to \"cmprs_file\" \n");
    rt_kprintf("lzma -d [cmprs_file] [dcmprs_file]   -dcompress \"cmprs_file\" to \"dcmprs_file\" \n\n");
    
    rt_kprintf("lzmainfo [cmprs_file]                -show compress file infomation\n\n");
}

static void *lzma_alloc(ISzAllocPtr p, size_t size)
{
    void *ptr = rt_malloc(size);
    return ptr;
}

static void lzma_free(ISzAllocPtr p, void *address)
{
    rt_free(address);
}

static int lzma_compress(int fd_in, int fd_out)
{
    ISzAlloc allocator = {lzma_alloc, lzma_free};
    CLzmaEncProps props;
    Byte props_buff[LZMA_PROPS_SIZE];
    SizeT props_size = LZMA_PROPS_SIZE;
    size_t file_size = 0;
    Byte *src_stream = RT_NULL;
    Byte *dest_stream = RT_NULL;
    SizeT src_size = 0;
    SizeT dest_size = 0;
    SRes res = 0;
    
    file_size = lseek(fd_in, 0, SEEK_END);
    lseek(fd_in, 0, SEEK_SET);

    src_size = file_size;
    dest_size = file_size;

    src_stream = rt_malloc(src_size);
    dest_stream = rt_malloc(dest_size);

    if(read(fd_in, src_stream, file_size) != file_size)
    {
        goto _exit;
    }

    LzmaEncProps_Init(&props);
    props.level = 9;
    props.dictSize = 1 << 16;
    props.writeEndMark = 1;

    res = LzmaEncode(dest_stream + LZMA_PROPS_SIZE + sizeof(UInt64), &dest_size, 
                    src_stream, src_size,
                    &props, props_buff, &props_size, 1,
                    NULL, &allocator, &allocator);
    if(res != SZ_OK)
    {
        goto _exit;
    }

    rt_memcpy(dest_stream, props_buff, LZMA_PROPS_SIZE);

    for(int i = 0; i < 8; i++)
    {
        dest_stream[LZMA_PROPS_SIZE + i] = (Byte)(src_size >> (i * 8));
    }

    dest_size += LZMA_PROPS_SIZE + sizeof(UInt64);
    
    write(fd_out, dest_stream, dest_size);

_exit:
    if(src_stream != RT_NULL)
    {
        rt_free(src_stream);
    }
    if(dest_stream != RT_NULL)
    {
        rt_free(dest_stream);
    }

    return res;
}

static int lzma_decompress(int fd_in, int fd_out)
{
    ISzAlloc allocator = {lzma_alloc, lzma_free};
    ELzmaStatus state;
    size_t file_size = 0;
    SizeT dcmprs_size = 0;
    Byte *src_stream = RT_NULL;
    Byte *dest_stream = RT_NULL;
    SizeT src_size = 0;
    SizeT dest_size = 0;
    SRes res = 0;

    file_size = lseek(fd_in, 0, SEEK_END);
    lseek(fd_in, 0, SEEK_SET);

    src_size = file_size;
    src_stream = rt_malloc(src_size);
    read(fd_in, src_stream, file_size);


    for(int i = 0; i < 8; i++)
        dcmprs_size += (UInt64)src_stream[LZMA_PROPS_SIZE + i] << (i * 8);

    dest_size = src_size * 4;
    dest_size = ((dcmprs_size > dest_size) ? dest_size : dcmprs_size);

    dest_stream = rt_malloc(dest_size);   

    res = LzmaDecode(dest_stream, &dest_size,
                    src_stream + LZMA_PROPS_SIZE + sizeof(UInt64),
                    &src_size, src_stream, LZMA_PROPS_SIZE,
                    LZMA_FINISH_END, &state, &allocator);
    if(res != SZ_OK)
    {
        goto _exit;
    }

    write(fd_out, dest_stream, dest_size);
_exit:
    if(src_stream != RT_NULL)
    {
        rt_free(src_stream);
    }
    if(dest_stream != RT_NULL)
    {
        rt_free(dest_stream);
    }

    return res;
}

static int lzma(int argc, char *argv[])
{
    int fd_in = -1, fd_out = -1;
    int ret = 0;

    if(argc != 4)
    {
        help();
        ret = -1;
        goto _exit;
    }

    fd_in = open(argv[2], O_RDONLY, 0);
    if(fd_in < 0)
    {
        rt_kprintf("[lzma] open the input file: %s erroe\n", argv[2]);
        ret = -1;
        goto _exit;
    }

    fd_out = open(argv[3], O_WRONLY | O_CREAT | O_TRUNC, 0);
    if(fd_out < 0)
    {
        rt_kprintf("[lzma] open the output file: %s erroe\n", argv[3]);
        ret = -1;
        goto _exit;
    }

    if(memcmp("-c", argv[1], strlen(argv[1])) == 0)
    {

        if(lzma_compress(fd_in, fd_out) != 0)
        {
            rt_kprintf("[lzma] lzma compress file error!\n");
        }

    }
    else if(memcmp("-d", argv[1], strlen(argv[1])) == 0)
    {

        if(lzma_decompress(fd_in, fd_out) != 0)
        {
            rt_kprintf("[lzma] lzma decompress file error!\n");
        }
    }
    else
    {
        help();
    }

_exit:
    if(fd_in >= 0)
    {
        close(fd_in);
    }

    if(fd_out >= 0)
    {
        close(fd_out);
    }

    return ret;
}

int log_2(UInt32 num)
{
    int count = 0;
    while(num)
    {
        num = num >> 1;
        if(num != 0)
        {
            count++;
        }
    }
    return count;
}

static int lzmainfo(int argc, char *argv[])
{
    CLzmaProps props;
    UInt32 dic_size;
    Byte props_buff[LZMA_PROPS_SIZE + sizeof(UInt64)];
    SizeT dcmprs_size = 0;
    Byte d;
    int fd_in = -1;
    int ret = 0;

    if(argc != 2)
    {
        help();
        ret = -1;
        goto _exit;
    }

    fd_in = open(argv[1], O_RDONLY, 0);
    if(fd_in < 0)
    {
        rt_kprintf("[lzma] open the lzma file: %s erroe\n", argv[1]);
        ret = -1;
        goto _exit;
    }

    read(fd_in, props_buff, LZMA_PROPS_SIZE + sizeof(UInt64));

    for(int i = 0; i < 8; i++)
        dcmprs_size += (UInt64)props_buff[LZMA_PROPS_SIZE + i] << (i * 8);

    dic_size = props_buff[1] | ((UInt32)props_buff[2] << 8) |
              ((UInt32)props_buff[3] << 16) | ((UInt32)props_buff[4] << 24);

    if(dic_size < (1 << 12))
    {
        dic_size = (1 << 12);
    }

    props.dicSize = dic_size; 

    d = props_buff[0];

    if(d >= (9 * 5 * 5))
    {
        rt_kprintf("lzmainfo: %s: Not a lzma file\n", argv[1]);
        goto _exit;
    }

    props.lc = (Byte)(d % 9);
    d /= 9;
    props.pb = (Byte)(d / 5);
    props.lp = (Byte)(d % 5);

    rt_kprintf("\n%s\n", argv[1]);
    rt_kprintf("Uncompressed size:              %d MB (%d bytes)\n", (UInt32)(dcmprs_size / 1024 / 1024), dcmprs_size);
    rt_kprintf("Dictionary size:                %d MB (2^%d bytes)\n", (UInt32)(props.dicSize / 1024 / 1024), log_2(props.dicSize));
    rt_kprintf("Literal context bits (lc):      %d\n", props.lc);
    rt_kprintf("Literal pos bits (lp):          %d\n", props.lp);
    rt_kprintf("Number of pos bits (pb):        %d\n\n", props.pb);

_exit:
    if(fd_in >= 0)
    {
        close(fd_in);
    }
    return 0;
}

#ifdef RT_USING_FINSH
#ifdef FINSH_USING_MSH

#include <finsh.h>

MSH_CMD_EXPORT(lzma, lzma compress and decompress test);
MSH_CMD_EXPORT(lzmainfo, Displays information about the LZMA compression package);
#endif
#endif