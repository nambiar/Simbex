#include "srec.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#define SREC_DELIMITER 'S'
#define RECORD_TYPE_S0  0U
#define RECORD_TYPE_S1  1U
#define RECORD_TYPE_S2  2U
#define RECORD_TYPE_S3  3U
#define RECORD_TYPE_S4  4U
#define RECORD_TYPE_S5  5U
#define RECORD_TYPE_S6  6U
#define RECORD_TYPE_S7  7U
#define RECORD_TYPE_S8  8U
#define RECORD_TYPE_S9  9U
#define MIN_BYTES       3U
#define MAX_BYTES       255U

/* Macros for state machine */
#define FSM_START() static void *pStates = NULL; if (pStates!=NULL) goto *pStates;
#define FSM_NEXT_STATE(state) ({pStates = &&state; return true;})
#define FSM_RESET() ({pStates = NULL; return false;})

/* Initialise the pointer to the record */ 
static srec_record* record = NULL;
static pFuncReadBytes pfunc_read_bytes = NULL;
static pFuncRecvRecord pfunc_recv_record = NULL;

static uint8_t get_ascii_to_hex(uint8_t value)
{
    if ((value >= '0') && (value <= '9'))
    {
        return (value - '0');
    }
    if((value >= 'A') && (value <= 'F'))
    {
        return(10 + (value - 'A'));
    }
    /* srec record data contains valid hex digits */ 
    assert(!value);
    /* abnormal termination of the program */
    exit(1);
    
}

static inline uint8_t srec_address_length(uint8_t srec_type)
{
    if(srec_type == RECORD_TYPE_S1)
    {
        return 2;
    }
    else if (srec_type == RECORD_TYPE_S2)
    {
        return 4;
    }
    else if (srec_type == RECORD_TYPE_S3)
    {
        return 6;
    }
    else
    {
        return 0;
    }
}

static void calculate_crc(uint8_t* data, uint8_t size)
{
    /* Assume that byte count added to the checksum value before calling this function */
        /* Critical section: Calculate CRC */
    for (uint8_t i = 0; i < record->count; i++)
    {
        record->checksum += data[i];
    }
}

bool srec_get_start()
{
    bool got_start = false;
    
    uint8_t* pBuffer = (*pfunc_read_bytes)(1);
    
    if(SREC_DELIMITER == *pBuffer)
    {
        record->delimiter = SREC_DELIMITER;
        got_start = true;
    }
    
    free(pBuffer);
    
    return got_start;
}

bool srec_get_recordtype()
{
    bool got_recordtype = false;
    
    uint8_t* pBuffer = (*pfunc_read_bytes)(1);
    
    uint8_t record_type = get_ascii_to_hex(*pBuffer);
    
    free(pBuffer);
    
    assert((record_type - '0') <= 9);

    record->type = record_type;
    got_recordtype = true;
    
    return got_recordtype;
}

bool srec_get_bytecount()
{
    bool got_bytecount = false;
    
    uint8_t* pBuffer = (*pfunc_read_bytes)(2);
    
    uint8_t byte_msb = (get_ascii_to_hex(pBuffer[0]) << 4);
    uint8_t byte_lsb = get_ascii_to_hex(pBuffer[1]);
    uint8_t byte_count = byte_msb | byte_lsb;
    
    free(pBuffer);
    
    if((byte_count >= MIN_BYTES) &&  (byte_count <= MAX_BYTES))
    {
        /* digit pairs */
        record->count = byte_count * 2; 
        record->checksum = byte_count;
        got_bytecount = true;
    }
    
    return got_bytecount;
}

bool srec_get_address_data()
{
    bool got_address_data = false;
    
    /* Receive address and data */
    /* Adding support for s1 type record */
    uint8_t* pBuffer = (*pfunc_read_bytes)(record->count);

    if(pBuffer == NULL)
    {
        return false;
    }

    record->pData = (uint8_t*)malloc((record->count/2) * sizeof(uint8_t));

    uint8_t index;
    uint8_t index_record = 0;
    for (index = 0; index < record->count; index+=2)
    {
        record->pData[index_record] = (get_ascii_to_hex(pBuffer[index]) << 4) | get_ascii_to_hex(pBuffer[index + 1]);
        printf("Data Bytes: %x \t", record->pData[index_record]);
        printf("Index: %x \n", index_record);
        index_record++;
    }

    /* Take checksum from last 2 bytes */
    record->checksum = record->pData[index_record -1];
    printf("Checksum: %x \n", record->checksum);

    /* Free buffer */
    free(pBuffer);
    
    return true;
}

bool srec_get_delimiter()
{
    bool got_delimiter = false;
    
    uint8_t* pBuffer = (*pfunc_read_bytes)(2);
    if((pBuffer[0] == '\r')  && (pBuffer[1] == '\n'))
    {
        got_delimiter = true;
    }
    free(pBuffer);

    return got_delimiter;
}

void srec_print_record()
{
    printf("Record Type = %x\n",record->type);
    printf("Count = %x\n",(record->count/2));
    printf("Checksum = %x\n",record->checksum);    
}

bool srec_fsm()
{
    if(NULL == record)
    {
        record = (srec_record*)malloc(sizeof(srec_record));
    }

    bool recv_start = false;
    bool recv_delimiter = false;
    bool recv_recordtypte = false;
    bool recv_bytecount = false;
    bool recv_address_data = false;

    FSM_START();

    GET_START:
        while(!recv_start)
        {
            recv_start = srec_get_start();
        }
        FSM_NEXT_STATE(GET_RECORD_TYPE);

    GET_RECORD_TYPE:
        recv_recordtypte = srec_get_recordtype();
        (false == recv_recordtypte)?(FSM_RESET()):FSM_NEXT_STATE(GET_BYTE_COUNT);

    GET_BYTE_COUNT:
        recv_bytecount = srec_get_bytecount();
        (false == recv_bytecount)?(FSM_RESET()):FSM_NEXT_STATE(GET_ADDRESS_DATA);

    GET_ADDRESS_DATA:
        recv_address_data = srec_get_address_data();
        (false == recv_address_data)?(FSM_RESET()):FSM_NEXT_STATE(GET_DELIMITER);

    GET_DELIMITER:
        recv_address_data = srec_get_delimiter();
        (false == recv_address_data)?(FSM_RESET()):FSM_NEXT_STATE(PRINT_RECORD);

    PRINT_RECORD:
        srec_print_record();
        if((RECORD_TYPE_S1 == record->type) || (RECORD_TYPE_S2 == record->type) || (RECORD_TYPE_S3 == record->type))
        {
            uint8_t data_offset = srec_address_length(record->type);
            uint8_t size_data = (record->count/2) - 1 - data_offset;//checksum and address
            (*pfunc_recv_record)(record->pData + data_offset,size_data);
        }
        free(record->pData);
        free(record);
        record = NULL;
        FSM_RESET();
    
    return true;

}

void srec_init(pFuncReadBytes pReadBytes,pFuncRecvRecord pRecvRecord)
{
    pfunc_read_bytes = pReadBytes;
    pfunc_recv_record = pRecvRecord;
}

