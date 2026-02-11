#ifndef RSCP_HANDLER_H
#define RSCP_HANDLER_H

#include <stdint.h>
#include "RscpProtocol.h"
#include "AES.h"

#define AES_KEY_SIZE    32
#define AES_BLOCK_SIZE  32

#define DEBUG(...)if(debug) {printf(__VA_ARGS__);}

// RSCP connection state (defined in rscp_handler.cpp)
extern int iSocket;
extern int iAuthenticated;
extern AES aesEncrypter;
extern AES aesDecrypter;
extern uint8_t ucEncryptionIV[AES_BLOCK_SIZE];
extern uint8_t ucDecryptionIV[AES_BLOCK_SIZE];

// RSCP Error Code Descriptions
const char* getErrorDescription(uint32_t errorCode);

// RSCP Request/Response handling
int sendRequestAndReceive(RscpProtocol* protocol, SRscpValue& rootValue);
int buildDCBRequest(RscpProtocol* protocol, SRscpFrameBuffer* frameBuffer, uint16_t batIndex, uint8_t dcbIndex);
int createRequestExample(SRscpFrameBuffer * frameBuffer);
int handleResponseValue(RscpProtocol *protocol, SRscpValue *response);
// Note: processReceiveBuffer, receiveLoop, and mainLoop are static in rscp_handler.cpp
void mainLoop(void);

#endif // RSCP_HANDLER_H
