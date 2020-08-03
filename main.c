#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include "srec.h"
static FILE* fpSrecFile = NULL;
static FILE* fpBinFile = NULL;

void open_srec_file_to_read(FILE** fpSrecFile)
{
    *fpSrecFile = fopen("test.srec","r");
    if (NULL == *fpSrecFile)
    {
        printf("File Not Found");
        exit(1);
    }
}

void open_bin_file_to_write(FILE** fpBinFile)
{
    *fpBinFile = fopen("test.bin","w");
    if (NULL == *fpBinFile)
    {
        printf("File Not Created");
        exit(1);
    }
}

void open_bin_file_to_read(FILE** fpBinFile)
{
    *fpBinFile = fopen("test.bin","r");
    if (NULL == *fpBinFile)
    {
        printf("File Not Found");
        exit(1);
    }
}

void open_srec_file_to_write(FILE** fpSrecFile)
{
    *fpSrecFile = fopen("test_result.srec","w");
    if (NULL == *fpSrecFile)
    {
        printf("File Not Created");
        exit(1);
    }
}

uint8_t* read_srec_file(FILE* fpSrecFile,uint8_t size)
{
    static long position = 0;

    assert(fpSrecFile != NULL);
    
    uint8_t* pbuffer = (uint8_t*)malloc(size * sizeof(uint8_t));
    assert(pbuffer != NULL);

    uint8_t index = 0;
 
    while (size)
    {
        pbuffer[index] = fgetc(fpSrecFile);
        if (EOF == pbuffer[index])
        {
            return NULL;
        }
        position++;
        index++;
        size--;
    }
    return pbuffer;
}

uint8_t* read_bin_file(FILE* fpBinFile,uint8_t size)
{
    static long position = 0;

    assert(fpBinFile != NULL);

    if(size == 0xFF)
    {
        /* Calculating the size of the file */
        fseek(fpBinFile, 0L, SEEK_END);
        uint32_t file_size = ftell(fpBinFile);
        fseek(fpBinFile, 0L, SEEK_SET);
        uint8_t* pbuffer = (uint8_t*)malloc(4 * sizeof(uint8_t));
        assert(pbuffer != NULL);
        memcpy(pbuffer,&file_size,4);
        return pbuffer;
    }
    
    uint8_t* pbuffer = (uint8_t*)malloc(size + 1 * sizeof(uint8_t));
    assert(pbuffer != NULL);

    uint8_t index = 0;
    pbuffer[index] = size;
    index++;
 
    while (size)
    {
        pbuffer[index] = fgetc(fpBinFile);
        if (EOF == pbuffer[index])
        {
            /* populate new size less than the default length of 0x13 */
            pbuffer[0] -= size;
            break;
        }
        position++;
        index++;
        size--;
    }
    return pbuffer;
}

uint8_t* get_srec_data(uint8_t size)
{
    return(read_srec_file(fpSrecFile,size));
}

uint8_t* get_bin_data(uint8_t size)
{
    return(read_bin_file(fpBinFile,size));
}

bool write_bin_file(FILE* fpBinFile,uint8_t* pdata,uint8_t size)
{
    return (size == fwrite(pdata, sizeof(uint8_t),size, fpBinFile));
}

bool write_srec_data_to_bin(uint8_t* pdata,uint8_t size)
{
    return(write_bin_file(fpBinFile,pdata,size));
}

bool write_srec_file(FILE* fpSrecFile,uint8_t* pdata,uint8_t size)
{
    return (size == fwrite(pdata, sizeof(uint8_t),size, fpSrecFile));
}

bool write_bin_data_to_srec(uint8_t* pdata,uint8_t size)
{
    return(write_srec_file(fpSrecFile,pdata,size));
}

int main(int argc, char const *argv[])
{
    /* Read an srec file and binary file is generated */
    open_srec_file_to_read(&fpSrecFile);
    
    open_bin_file_to_write(&fpBinFile);
    
    srec_init(get_srec_data,write_srec_data_to_bin);

    bool fsm_stop = false;  
    
    do
    {
        fsm_stop = srec_fsm();
    } while (fsm_stop);

    fclose(fpSrecFile);
    fclose(fpBinFile);

    /* Read a binary file and a srec file is created */
    open_bin_file_to_read(&fpBinFile);
    open_srec_file_to_write(&fpSrecFile);

    
    srec_init(get_bin_data,write_bin_data_to_srec);

    bool fsm_stop = false;

    do
    {
        fsm_stop = srec_create_file_fsm();
    } while (fsm_stop);

    fclose(fpSrecFile);
    fclose(fpBinFile);

    return 0;
}