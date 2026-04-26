#ifndef S3_IPC_H
#define S3_IPC_H

#include <stdint.h>

#define S3_MSG_MAX   64
#define S3_MSG_PAYLOAD 16

typedef struct {
    uint16_t type;
    uint16_t sender;
    uint16_t receiver;
    uint16_t payload_size;
    uint8_t  payload[S3_MSG_PAYLOAD];
} S3Message;

typedef struct {
    S3Message msgs[S3_MSG_MAX];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} S3MsgQueue;

void     s3_ipc_init(void);
int16_t  s3_send(uint16_t target, S3Message* msg);
int16_t  s3_recv(S3Message* msg);
int16_t  s3_recv_nb(S3Message* msg);

#endif /* S3_IPC_H */
