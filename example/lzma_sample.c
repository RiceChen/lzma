#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <rtthread.h>
#include <dfs_posix.h>

#include "LzmaDec.h"
#include "LzmaEnc.h"
#include "7zFile.h"

static void help(void)
{
    rt_kprintf("Usage:\n");
    rt_kprintf("lzma -c [file] [cmprs_file]          -compress \"file\" to \"cmprs_file\" \n");
    rt_kprintf("lzma -d [cmprs_file] [dcmprs_file]   -dcompress \"cmprs_file\" to \"dcmprs_file\" \n\n");
    
    rt_kprintf("lzmainfo [cmprs_file]                -show compress file infomation\n\n");
}

static void *lzma_alloc(ISzAllocPtr p, size_t size)
{
    if(size == 0)
    {
        return RT_NULL;
    }
    return rt_malloc(size);
}

static void lzma_free(ISzAllocPtr p, void *address)
{
    if(address != RT_NULL)
    {
        rt_free(address);
    }
}

ISzAlloc allocator = {lzma_alloc, lzma_free};

static int lzma_compress(ISeqOutStream *outStream, ISeqInStream *inStream, UInt64 fileSize)
{
    CLzmaEncHandle enc;
    CLzmaEncProps props;
    SRes res;

    enc = LzmaEnc_Create(&allocator);
    if (enc == 0)
    {
        return SZ_ERROR_MEM;
    }

    LzmaEncProps_Init(&props);

    props.level = 9;
    props.dictSize = 1 << 12;
    props.writeEndMark = 1;
    res = LzmaEnc_SetProps(enc, &props);

    if (res == SZ_OK)
    {
        Byte header[LZMA_PROPS_SIZE + 8];
        size_t headerSize = LZMA_PROPS_SIZE;

        LzmaEnc_WriteProperties(enc, header, &headerSize);

        for (int i = 0; i < 8; i++)
        {
            header[headerSize++] = (Byte)(fileSize >> (8 * i));
        }

        if (outStream->Write(outStream, header, headerSize) != headerSize)
        {
            res = SZ_ERROR_WRITE;
        }
        else
        {
            res = LzmaEnc_Encode(enc, outStream, inStream, NULL, &allocator, &allocator);
        }
    }

    LzmaEnc_Destroy(enc, &allocator, &allocator);
    return res;
}

static int lzma_decompress(ISeqOutStream *outStream, ISeqInStream *inStream)
{
    CLzmaDec state;
    UInt64 unpack_size;
    Byte header[LZMA_PROPS_SIZE + 8];
    size_t headerSize = sizeof(header);
    Byte inBuf[4096];
    Byte outBuf[4096];
    size_t inPos = 0, inSize = 0, outPos = 0;
    SRes res;

    inStream->Read(inStream, header, &headerSize);

    for(int i = 0; i < 8; i++)
    {
        unpack_size += (UInt64)(header[LZMA_PROPS_SIZE + i] << (i * 8));
    }

    LzmaDec_Construct(&state);
    LzmaDec_Allocate(&state, header, LZMA_PROPS_SIZE, &allocator);
    LzmaDec_Init(&state);

    while(1)
    {
        if(inPos == inSize)
        {
            inSize = 4096;
            inPos = 0;
            inStream->Read(inStream, inBuf, &inSize);
        }
        {
            SizeT inProcessed = inSize - inPos;
            SizeT outProcessed = 4096 - outPos;
            ELzmaFinishMode finishMode = LZMA_FINISH_ANY;
            ELzmaStatus status;

            if (outProcessed > unpack_size)
            {
                outProcessed = (SizeT)unpack_size;
                finishMode = LZMA_FINISH_END;
            }

            res = LzmaDec_DecodeToBuf(&state, outBuf + outPos, &outProcessed,
                                      inBuf + inPos, &inProcessed, finishMode, &status);
            inPos += inProcessed;
            outPos += outProcessed;
            unpack_size -= outProcessed;

            if (outStream)
            {
                if (outStream->Write(outStream, outBuf, outPos) != outPos)
                {
                    return SZ_ERROR_WRITE;
                }
            }
            outPos = 0;

            if (res != SZ_OK)
            {
                return res;
            }

            if (inProcessed == 0 && outProcessed == 0)
            {
                if(status != LZMA_STATUS_FINISHED_WITH_MARK)
                {
                    return SZ_ERROR_DATA;
                }
                return res;
            }
        }
    }

    LzmaDec_Free(&state, &allocator);
    return res;
}

static int lzma(int argc, char *argv[])
{
    CFileSeqInStream inStream;
    CFileOutStream outStream;
    int ret = 0;

    FileSeqInStream_CreateVTable(&inStream);
    File_Construct(&inStream.file);

    FileOutStream_CreateVTable(&outStream);
    File_Construct(&outStream.file);

    if(argc != 4)
    {
        help();
        ret = -1;
        goto _err1;
    }

    if(InFile_Open(&inStream.file, argv[2]) != 0)
    {
        rt_kprintf("[lzma] open the input file: %s erroe\n", argv[2]);
        ret = -1;
        goto _err1;
    }

    if(OutFile_Open(&outStream.file, argv[3]) != 0)
    {
        rt_kprintf("[lzma] open the output file: %s erroe\n", argv[3]);
        ret = -1;
        goto _err2;
    }

    if(memcmp("-c", argv[1], strlen(argv[1])) == 0)
    {
        UInt64 fileSize;
        File_GetLength(&inStream.file, &fileSize);
        if(lzma_compress(&outStream.vt, &inStream.vt, fileSize) != SZ_OK)
        {
            rt_kprintf("[lzma] lzma compress file error!\n");
            goto _err3;
        }

    }
    else if(memcmp("-d", argv[1], strlen(argv[1])) == 0)
    {

        if(lzma_decompress(&outStream.vt, &inStream.vt)  != SZ_OK)
        {
            rt_kprintf("[lzma] lzma decompress file error!\n");
            goto _err3;
        }
    }
    else
    {
        help();
    }

_err3:
    File_Close(&outStream.file);

_err2:
    File_Close(&inStream.file);

_err1:
    return ret;
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
    rt_kprintf("Dictionary size:                %d MB (%d bytes)\n", (UInt32)(props.dicSize / 1024 / 1024), props.dicSize);
    rt_kprintf("Literal context bits (lc):      %d\n", props.lc);
    rt_kprintf("Literal pos bits (lp):          %d\n", props.lp);
    rt_kprintf("Number of pos bits (pb):        %d\n\n", props.pb);

_exit:
    if(fd_in >= 0)
    {
        close(fd_in);
    }
    return ret;
}

#ifdef RT_USING_FINSH
#ifdef FINSH_USING_MSH

#include <finsh.h>

MSH_CMD_EXPORT(lzma, lzma compress and decompress test);
MSH_CMD_EXPORT(lzmainfo, Displays information about the LZMA compression package);
#endif
#endif
