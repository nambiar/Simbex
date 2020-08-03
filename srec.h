#ifndef SREC_H
#define SREC_H

#include <stdbool.h>

typedef unsigned char  uint8_t;
typedef struct 
{
    uint8_t  delimiter;
    uint8_t  type;
    uint8_t  count;
    uint8_t* pData;
    uint8_t  carriage_return;
    uint8_t  newline;
    uint8_t  checksum;
}srec_record;

typedef uint8_t* (*pFuncReadBytes)(uint8_t);
typedef bool (*pFuncRecvRecord)(uint8_t*,uint8_t);

/** Init the srec utility with a function which reads a block of data */
/** Decouples the srec lib to run on uC or on system */
/** Can use a file operator or uart to read a record or a single byte */ 
/** Also the buffer is provided by that function */
void srec_init(pFuncReadBytes pReadBytes, pFuncRecvRecord pRecvRecord);

/** FSM statemachine for creating a bin file from srec file */
bool srec_create_bin_fsm(void);

/** FSM statemachine for creating a bin file from srec file */
bool srec_create_file_fsm(void);
#endif