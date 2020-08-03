#include "srec.h"
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>

#define SREC_DELIMITER          'S'
#define RECORD_TYPE_S1_ASCII    '1'
#define RECORD_TYPE_S2_ASCII    '2'
#define RECORD_TYPE_S3_ASCII    '3'
#define RECORD_TYPE_S0          0U
#define RECORD_TYPE_S1          1U
#define RECORD_TYPE_S2          2U
#define RECORD_TYPE_S3          3U
#define RECORD_TYPE_S4          4U
#define RECORD_TYPE_S5          5U
#define RECORD_TYPE_S6          6U
#define RECORD_TYPE_S7          7U
#define RECORD_TYPE_S8          8U
#define RECORD_TYPE_S9          9U
#define MIN_BYTES               3U
#define MAX_BYTES               255U
#define DELIMITER_SIZE          1U
#define TYPE_SIZE               1U
#define COUNT_SIZE              2U
#define CHECKSUM_SIZE           2U
#define CARRIAGE_NEWLINE_SIZE   2U

/* State Machines with labels */
#define FSM_START() static void *pStates = NULL; if (pStates!=NULL) goto *pStates;
#define FSM_NEXT_STATE(state) ({pStates = &&state; return true;})
#define FSM_RESET() ({pStates = NULL; return false;})

/* Initialise the pointer to the record */ 
static srec_record* record = NULL;
static pFuncReadBytes pfunc_read_bytes = NULL;
static pFuncRecvRecord pfunc_recv_record = NULL;
static uint32_t record_address = 0x00000000;
static uint32_t file_size = 0;

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

static uint8_t get_hex_to_ascii(uint8_t value)
{
    if ((value >= 0) && (value <= 9))
    {
        return ('0' + value);
    }
    if((value >= 10) && (value <= 15))
    {
        return('A' + (value - 10));
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
        return 3;
    }
    else if (srec_type == RECORD_TYPE_S3)
    {
        return 4;
    }
    else
    {
        return 0;
    }
}

static inline uint8_t srec_record_type(uint8_t address_length)
{
    if(address_length == RECORD_TYPE_S1 * 2)
    {
        return RECORD_TYPE_S1_ASCII;
    }
    else if (address_length == ((RECORD_TYPE_S1 + 1) * 2))
    {
        return RECORD_TYPE_S2_ASCII;
    }
    else if (address_length == ((RECORD_TYPE_S1 + 2) * 2))
    {
        return RECORD_TYPE_S3_ASCII;
    }
    else
    {
        return 0;
    }
}

static bool srec_get_start()
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

static bool srec_get_recordtype()
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

static bool srec_get_bytecount()
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

static bool srec_get_address_data()
{
    bool got_address_data = true;
    
    /* Receive address and data */
    uint8_t* pBuffer = (*pfunc_read_bytes)(record->count);

    if(pBuffer == NULL)
    {
        return false;
    }

    record->pData = (uint8_t*)malloc((record->count/2) * sizeof(uint8_t));

    uint8_t index;
    uint8_t index_record = 0;
    /* Adding the count to checksum */
    uint16_t checksum = record->checksum;
    for (index = 0; index < record->count; index+=2)
    {
        record->pData[index_record] = (get_ascii_to_hex(pBuffer[index]) << 4) | get_ascii_to_hex(pBuffer[index + 1]);
        checksum += record->pData[index_record];
        printf("Data Bytes: %x \t", record->pData[index_record]);
        printf("Index: %x \t", index_record);
        printf("CheckSum: %x \n", checksum); 
        index_record++;
    }

    /* Removing most significant byte & added checksum to sum as well */
    checksum -= record->pData[index_record - 1];
    checksum = checksum & (0x00FF);

    /* Ones compliment */
    checksum = ~checksum;
    
    record->checksum = (uint8_t)checksum;

    /* Take checksum from last byte */
    if(record->checksum != record->pData[index_record -1])
    {
        printf("Checksum Exp: %x Calculated: %x \n", record->pData[index_record -1],record->checksum);
        got_address_data = false;
    }

    /* Free buffer */
    free(pBuffer);
    
    return got_address_data;
}

static bool srec_get_delimiter()
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

static void srec_print_record()
{
    printf("Record Type = %x\n",record->type);
    printf("Count = %x\n",(record->count/2));
    printf("Checksum = %x\n",record->checksum);    
}

bool srec_create_bin_fsm()
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
        recv_start = srec_get_start();
        /* EOF occured or unknown states */ 
        if(false == recv_start)
        {
            if(record != NULL)
            {
                free(record);
                record = NULL;
            }
            FSM_RESET();            
        }
        else
        {
            FSM_NEXT_STATE(GET_RECORD_TYPE);
        }

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
        FSM_NEXT_STATE(GET_START);

    return true;
}

static void calculate_checksum(uint8_t* pBuffer, uint8_t data_size,uint8_t* checksum)
{
    uint16_t calculated_checksum = *checksum;
    for (int i = 0; i < data_size; i++)
    {
        calculated_checksum += pBuffer[i];
    }
    uint8_t record_address_msb = ((record_address &  0xFF00)>>8);
    uint8_t record_address_lsb = (record_address &  0x00FF);
    calculated_checksum += record_address_msb + record_address_lsb;
    calculated_checksum = (uint8_t)(calculated_checksum & 0x00FF);
    *checksum = (uint8_t)(~calculated_checksum);
}

static uint8_t populate_address_srecord(uint8_t* pBuffer, uint8_t address_size)
{
    if (address_size == 4)
    {
        uint8_t address_msb_byte = (uint8_t)((record_address & 0x0000FF00)>>8);
        pBuffer[0] = get_hex_to_ascii((address_msb_byte & 0xF0)>>4);
        pBuffer[1] = get_hex_to_ascii(address_msb_byte & 0x0F);

        uint8_t address_lsb_byte = (uint8_t)(record_address & 0x000000FF);
        pBuffer[2] = get_hex_to_ascii((address_lsb_byte & 0xF0)>>4);
        pBuffer[3] = get_hex_to_ascii(address_lsb_byte & 0x0F);
        return address_size;
    }
    return 255;
}

static uint8_t populate_data_srecord(uint8_t* pBuffer, uint8_t* pData, uint8_t data_size)
{
    int buffer_index = 0;
    int data_index = 0;

    for (buffer_index = 0; buffer_index < data_size; buffer_index+=2)
    {
        pBuffer[buffer_index] = get_hex_to_ascii((pData[data_index] & 0xF0)>>4);
        pBuffer[buffer_index + 1] = get_hex_to_ascii(pData[data_index] & 0x0F);
        data_index++;
    }

    return buffer_index;
}

static uint8_t* populate_srec_record(uint8_t* pBuffer,uint8_t data_size,uint8_t address_size,uint8_t checksum,uint8_t* record_size)
{
    /* Overall size of record */
    uint8_t size_record = DELIMITER_SIZE + TYPE_SIZE + COUNT_SIZE + address_size + 
                          data_size + CHECKSUM_SIZE + CARRIAGE_NEWLINE_SIZE;
    
    /* Memory for record */
    uint8_t* pSrecRecord = (uint8_t*)malloc((size_record) * sizeof(uint8_t));
    assert(pSrecRecord != NULL);

    uint8_t record_index = 0;
    
    /* Creating record frame */
    pSrecRecord[record_index] = SREC_DELIMITER;
    record_index++;
    pSrecRecord[record_index] = srec_record_type(address_size/2);
    record_index++;
    
    /* Populating count of the record */
    uint8_t length_data_address = (address_size + data_size + CHECKSUM_SIZE)/2;
    pSrecRecord[record_index] = get_hex_to_ascii((length_data_address & 0xF0)>>4);
    record_index++;
    pSrecRecord[record_index] = get_hex_to_ascii(length_data_address & 0x0F);
    record_index++;

    /* Populating address value of record */
    uint8_t data_index = populate_address_srecord(pSrecRecord + record_index, address_size);
    record_index += data_index;

    /* Populating data value of the record */    
    uint8_t checksum_index = populate_data_srecord(pSrecRecord + record_index, pBuffer, data_size);
    record_address += data_size/2;
    record_index += checksum_index;

    /* Populating checksum */
    pSrecRecord[record_index] = get_hex_to_ascii((checksum & 0xF0)>>4);
    record_index++;
    pSrecRecord[record_index] = get_hex_to_ascii(checksum & 0x0F);
    record_index++;

    /* Populating carriage return and newline */
    pSrecRecord[record_index] = 0x0D;
    record_index++;
    pSrecRecord[record_index] = 0x0A;

    assert((size_record - 1) == record_index);
    *record_size = size_record;

    return pSrecRecord;
}

static bool srec_get_bin_data()
{
    bool read_write_successful = true;

    uint8_t checksum_length = 1;
    uint8_t address_length;

    if(record->type == '1')
    {
        address_length = 2;
    }
    else if(record->type == '2')
    {
        address_length = 3;
    }
    else
    {
        address_length = 4;
    }

    /* Default bytes read from the bin file */
    uint8_t default_length_read = 0x10;

    if(file_size == 0)
    {
        return false;
    }

    if(file_size >= default_length_read)
    {
        file_size -= default_length_read;
    }
    else
    {
        default_length_read = file_size;
        file_size = 0;
    }

    /* First byte will contain the size of data, followed by data */
    /* Byte[0] = 0x0F or numof bytes remaining in the bin file */
    uint8_t* pBuffer = (*pfunc_read_bytes)(default_length_read);
    
    /* Checksum calculation initialise */
    uint8_t checksum = 0;
    checksum += address_length + pBuffer[0] + (CHECKSUM_SIZE - 1);

    /* Checksum calculation */
    calculate_checksum(pBuffer + 1, pBuffer[0],&checksum);

    uint8_t record_size;

    /* Create a record to be passed to writing to memory or file */
    uint8_t* pRecord = populate_srec_record(pBuffer + 1,pBuffer[0]* 2, address_length*2,checksum,&record_size);
    assert(pRecord != NULL);
    
    free(pBuffer);

    /* Write the record to an interface */
    read_write_successful = (*pfunc_recv_record)(pRecord,record_size);

    return (read_write_successful);
}

static bool srec_get_bin_file_size(uint32_t* size)
{
    bool got_valid_address = false;
    
    uint8_t* pBuffer = (*pfunc_read_bytes)(0xff);
    *size = *((uint32_t*)pBuffer);
    uint32_t file_size = *size;

    if(file_size <= 0xFFFF)
    {
        record->type = '1';
    }
    else if((file_size > 0xFFFF) && (file_size <= 0x00FFFFFF))
    {
        record->type = '2';
    }
    else
    {
        record->type = '3';
    }

    if(record->type > 0)
    {
        got_valid_address = true;
    }
    
    printf("File size = %u",file_size);
    free(pBuffer);

    return got_valid_address;
}

bool srec_create_file_fsm()
{
    if(NULL == record)
    {
        record = (srec_record*)malloc(sizeof(srec_record));
    }

    bool valid_size = false;
    bool recv_bin_data = false;
    bool recv_recordtypte = false;
    bool recv_bytecount = false;
    bool recv_address_data = false;
    

    FSM_START();

    GET_FILE_SIZE_RECORD_TYPE:
        valid_size = srec_get_bin_file_size(&file_size);
        /* EOF occured or unknown states */ 
        if(false == valid_size)
        {
            if(record != NULL)
            {
                free(record);
                record = NULL;
            }
            FSM_RESET();            
        }
        else
        {
            FSM_NEXT_STATE(GET_RECORD_DATA);
        }

    GET_RECORD_DATA:
        recv_bin_data = srec_get_bin_data();
        (false == recv_bin_data)?(FSM_RESET()):FSM_NEXT_STATE(GET_RECORD_DATA);

    return true;
}

void srec_init(pFuncReadBytes pReadBytes,pFuncRecvRecord pRecvRecord)
{
    pfunc_read_bytes = pReadBytes;
    pfunc_recv_record = pRecvRecord;
}

