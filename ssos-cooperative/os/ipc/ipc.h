#ifndef SS_IPC_H
#define SS_IPC_H

#include <stdint.h>

#define SS_MSG_MAX   64
#define SS_MSG_PAYLOAD 16

typedef struct {
    uint16_t type;
    uint16_t sender;
    uint16_t receiver;
    uint16_t payload_size;
    uint8_t  payload[SS_MSG_PAYLOAD];
} SSMessage;

typedef struct {
    SSMessage msgs[SS_MSG_MAX];
    volatile uint16_t head;
    volatile uint16_t tail;
    volatile uint16_t count;
} SSMsgQueue;

void     ss_ipc_init(void);
int16_t  ss_send(uint16_t target, SSMessage* msg);
int16_t  ss_recv(SSMessage* msg);
int16_t  ss_recv_nb(SSMessage* msg);

#endif /* SS_IPC_H */
