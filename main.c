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

void open_file_read(FILE** fpSrecFile)
{
    *fpSrecFile = fopen("test.srec","r");
    if (NULL == *fpSrecFile)
    {
        printf("File Not Found");
        exit(1);
    }
}

void open_file_write(FILE** fpBinFile)
{
    *fpBinFile = fopen("test.bin","w");
    if (NULL == *fpBinFile)
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

uint8_t* get_srec_data(uint8_t size)
{
    return(read_srec_file(fpSrecFile,size));
}

bool write_bin_file(FILE* fpBinFile,uint8_t* pdata,uint8_t size)
{
    return (size == fwrite(pdata, sizeof(uint8_t),size, fpBinFile));
}

bool write_srec_data_to_bin(uint8_t* pdata,uint8_t size)
{
    return(write_bin_file(fpBinFile,pdata,size));
}

int main(int argc, char const *argv[])
{
    open_file_read(&fpSrecFile);
    
    open_file_write(&fpBinFile);
    
    srec_init(get_srec_data,write_srec_data_to_bin);

    bool fsm_stop = false;  
    
    do
    {
        fsm_stop = srec_fsm();
    } while (fsm_stop);

    fclose(fpSrecFile);
    fclose(fpBinFile);
    return 0;
}