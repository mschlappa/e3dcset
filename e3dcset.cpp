#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include "RscpProtocol.h"
#include "RscpTags.h"
#include "SocketConnection.h"
#include "AES.h"

#define AES_KEY_SIZE        32
#define AES_BLOCK_SIZE      32

#define SERVER_IP           "192.168.xxx.yyy"
#define SERVER_PORT         5033

#define E3DC_USER           "deine@email.de"
#define E3DC_PASSWORD       "passwort e3dc online account"
#define AES_PASSWORD        "passwort s10 rsc"

static int iSocket = -1;
static int iAuthenticated = 0;
static AES aesEncrypter;
static AES aesDecrypter;
static uint8_t ucEncryptionIV[AES_BLOCK_SIZE];
static uint8_t ucDecryptionIV[AES_BLOCK_SIZE];

static uint32_t MAXLEISTUNG = 3000; // Speicher kann mit diesem Wert in Watt Be- und Entladen werden
static uint32_t MIN_MANUELLE_LADUNG_ENERGIE = 100; // Mindestens 100Wh (0,1kWh) laden 
static uint32_t MAX_MANUELLE_LADUNG_ENERGIE = 8000; // Maximal 8000Wh (8kWh) laden 

static bool automatischLeistungEinstellen = false;
static uint32_t maxLadeLeistung = 0;
static uint32_t maxEntladeLeistung = 0;

static bool manuelleSpeicherladung = false;
static uint32_t manuelleLadeEnergie = 0;

int createRequestExample(SRscpFrameBuffer * frameBuffer) {
    RscpProtocol protocol;
    SRscpValue rootValue;
    // The root container is create with the TAG ID 0 which is not used by any device.
    protocol.createContainerValue(&rootValue, 0);

    //---------------------------------------------------------------------------------------------------------
    // Create a request frame
    //---------------------------------------------------------------------------------------------------------
    if(iAuthenticated == 0)
    {
        printf("Request authentication\n");
        // authentication request
        SRscpValue authenContainer;
        protocol.createContainerValue(&authenContainer, TAG_RSCP_REQ_AUTHENTICATION);
        protocol.appendValue(&authenContainer, TAG_RSCP_AUTHENTICATION_USER, E3DC_USER);
        protocol.appendValue(&authenContainer, TAG_RSCP_AUTHENTICATION_PASSWORD, E3DC_PASSWORD);
        // append sub-container to root container
        protocol.appendValue(&rootValue, authenContainer);
        // free memory of sub-container as it is now copied to rootValue
        protocol.destroyValueData(authenContainer);
    }
    else
    {
        SRscpValue PMContainer;
        protocol.createContainerValue(&PMContainer, TAG_EMS_REQ_SET_POWER_SETTINGS);
        
	if (manuelleSpeicherladung){
          protocol.appendValue(&rootValue, TAG_EMS_REQ_START_MANUAL_CHARGE,manuelleLadeEnergie);       
          protocol.appendValue(&rootValue, TAG_EMS_START_MANUAL_CHARGE,true);
        }
        if (automatischLeistungEinstellen){
          printf("Setze automatischLeistungEinstellen aktiv\n");
          protocol.appendValue(&PMContainer, TAG_EMS_POWER_LIMITS_USED,false);

        }else{
          printf("Setze maxLadeLeistung=%iW maxEntladeLeistung=%iW\n",maxLadeLeistung,maxEntladeLeistung);
          protocol.appendValue(&PMContainer, TAG_EMS_POWER_LIMITS_USED,true);
          protocol.appendValue(&PMContainer, TAG_EMS_MAX_DISCHARGE_POWER,maxEntladeLeistung);
          protocol.appendValue(&PMContainer, TAG_EMS_MAX_CHARGE_POWER,maxLadeLeistung); 
        }
       
      	// append sub-container to root container
        protocol.appendValue(&rootValue, PMContainer);
        // free memory of sub-container as it is now copied to rootValue
        protocol.destroyValueData(PMContainer);

    }

    // create buffer frame to send data to the S10
    protocol.createFrameAsBuffer(frameBuffer, rootValue.data, rootValue.length, true); // true to calculate CRC on for transfer
    // the root value object should be destroyed after the data is copied into the frameBuffer and is not needed anymore
    protocol.destroyValueData(rootValue);

    return 0;
}

int handleResponseValue(RscpProtocol *protocol, SRscpValue *response) {
    // check if any of the response has the error flag set and react accordingly
    if(response->dataType == RSCP::eTypeError) {
        // handle error for example access denied errors
        uint32_t uiErrorCode = protocol->getValueAsUInt32(response);
        printf("Tag 0x%08X received error code %u.\n", response->tag, uiErrorCode);
        return -1;
    }

    // check the SRscpValue TAG to detect which response it is
    switch(response->tag){
    case TAG_RSCP_AUTHENTICATION: {
        // It is possible to check the response->dataType value to detect correct data type
        // and call the correct function. If data type is known,
        // the correct function can be called directly like in this case.
        uint8_t ucAccessLevel = protocol->getValueAsUChar8(response);
        if(ucAccessLevel > 0) {
            iAuthenticated = 1;
        }
        printf("RSCP authentitication level %i\n", ucAccessLevel);
        break;
    }
    case TAG_EMS_START_MANUAL_CHARGE: {
        if (protocol->getValueAsBool(response)){
          printf("MANUAL_CHARGE_STARTED\n");
        }
	break;
    } 
    case TAG_EMS_POWER_PV: {    // response for TAG_EMS_REQ_POWER_PV
        int32_t iPower = protocol->getValueAsInt32(response);
        printf("EMS PV power is %i W\n", iPower);
        break;
    }
    case TAG_EMS_POWER_BAT: {    // response for TAG_EMS_REQ_POWER_BAT
        int32_t iPower = protocol->getValueAsInt32(response);
        printf("EMS BAT power is %i W\n", iPower);
        break;
    }
    case TAG_EMS_POWER_HOME: {    // response for TAG_EMS_REQ_POWER_HOME
        int32_t iPower = protocol->getValueAsInt32(response);
        printf("EMS house power is %i W\n", iPower);
        break;
    }
    case TAG_EMS_POWER_GRID: {    // response for TAG_EMS_REQ_POWER_GRID
        int32_t iPower = protocol->getValueAsInt32(response);
        printf("EMS grid power is %i W\n", iPower);
        break;
    }
    case TAG_EMS_POWER_ADD: {    // response for TAG_EMS_REQ_POWER_ADD
        int32_t iPower = protocol->getValueAsInt32(response);
        printf("EMS add power meter power is %i W\n", iPower);
        break;
    }
    case TAG_BAT_DATA: {        // resposne for TAG_BAT_REQ_DATA
        uint8_t ucBatteryIndex = 0;
        std::vector<SRscpValue> batteryData = protocol->getValueAsContainer(response);
        for(size_t i = 0; i < batteryData.size(); ++i) {
            if(batteryData[i].dataType == RSCP::eTypeError) {
                // handle error for example access denied errors
                uint32_t uiErrorCode = protocol->getValueAsUInt32(&batteryData[i]);
                printf("Tag 0x%08X received error code %u.\n", batteryData[i].tag, uiErrorCode);
                return -1;
            }
            // check each battery sub tag
            switch(batteryData[i].tag) {
            case TAG_BAT_INDEX: {
                ucBatteryIndex = protocol->getValueAsUChar8(&batteryData[i]);
                break;
            }
            case TAG_BAT_RSOC: {              // response for TAG_BAT_REQ_RSOC
                float fSOC = protocol->getValueAsFloat32(&batteryData[i]);
                printf("Battery SOC is %0.1f %%\n", fSOC);
                break;
            }
            case TAG_BAT_MODULE_VOLTAGE: {    // response for TAG_BAT_REQ_MODULE_VOLTAGE
                float fVoltage = protocol->getValueAsFloat32(&batteryData[i]);
                printf("Battery total voltage is %0.1f V\n", fVoltage);
                break;
            }
            case TAG_BAT_CURRENT: {    // response for TAG_BAT_REQ_CURRENT
                float fVoltage = protocol->getValueAsFloat32(&batteryData[i]);
                printf("Battery current is %0.1f A\n", fVoltage);
                break;
            }
            case TAG_BAT_STATUS_CODE: {    // response for TAG_BAT_REQ_STATUS_CODE
                uint32_t uiErrorCode = protocol->getValueAsUInt32(&batteryData[i]);
                printf("Battery status code is 0x%08X\n", uiErrorCode);
                break;
            }
            case TAG_BAT_ERROR_CODE: {    // response for TAG_BAT_REQ_ERROR_CODE
                uint32_t uiErrorCode = protocol->getValueAsUInt32(&batteryData[i]);
                printf("Battery error code is 0x%08X\n", uiErrorCode);
                break;
            }
            // ...
            default:
                // default behaviour
                printf("Unknown battery tag %08X\n", response->tag);
                break;
            }
        }
        protocol->destroyValueData(batteryData);
        break;
       }
	//case TAG_EMS_GET_POWER_SETTINGS:         // resposne for TAG_PM_REQ_DATA
        case TAG_EMS_SET_POWER_SETTINGS: {        // resposne for TAG_PM_REQ_DATA
            uint8_t ucPMIndex = 0;
            std::vector<SRscpValue> PMData = protocol->getValueAsContainer(response);
            for(size_t i = 0; i < PMData.size(); ++i) {
                if(PMData[i].dataType == RSCP::eTypeError) {
                    // handle error for example access denied errors
                    uint32_t uiErrorCode = protocol->getValueAsUInt32(&PMData[i]);
                    printf("TAG_EMS_GET_POWER_SETTINGS 0x%08X received error code %u.\n", PMData[i].tag, uiErrorCode);
                    return -1;
                }
                // check each PM sub tag
                switch(PMData[i].tag) {
                    case TAG_PM_INDEX: {
                        ucPMIndex = protocol->getValueAsUChar8(&PMData[i]);
                        break;
                    }
                    case TAG_EMS_POWER_LIMITS_USED: {              // response for POWER_LIMITS_USED
                        if (protocol->getValueAsBool(&PMData[i])){
                            printf("POWER_LIMITS_USED\n");
                            }
                        break;
                    }
                    case TAG_EMS_MAX_CHARGE_POWER: {              // 101 response for TAG_EMS_MAX_CHARGE_POWER
                        uint32_t uPower = protocol->getValueAsUInt32(&PMData[i]);
                        printf("MAX_CHARGE_POWER %i W\n", uPower);
                        break;
                    }
                    case TAG_EMS_MAX_DISCHARGE_POWER: {              //102 response for TAG_EMS_MAX_DISCHARGE_POWER
                        uint32_t uPower = protocol->getValueAsUInt32(&PMData[i]);
                        printf("MAX_DISCHARGE_POWER %i W\n", uPower);
                        //iDischarge = uPower;
                        break;
                    }
                    case TAG_EMS_DISCHARGE_START_POWER:{              //103 response for TAG_EMS_DISCHARGE_START_POWER
                        uint32_t uPower = protocol->getValueAsUInt32(&PMData[i]);
                        printf("DISCHARGE_START_POWER %i W\n", uPower);
                        break;
                    }
                    case TAG_EMS_POWERSAVE_ENABLED: {              //104 response for TAG_EMS_POWERSAVE_ENABLED
                        if (protocol->getValueAsBool(&PMData[i])){
                            printf("POWERSAVE_ENABLED\n");
                        }
                        break;
                    }
                    case TAG_EMS_WEATHER_REGULATED_CHARGE_ENABLED: {//105 resp WEATHER_REGULATED_CHARGE_ENABLED
                        if (protocol->getValueAsBool(&PMData[i])){
                            printf("WEATHER_REGULATED_CHARGE_ENABLED\n");
                        }
                        break;
                    }
                        // ...
                    default:
                        // default behaviour
/*                        printf("Unkonwn GET_POWER_SETTINGS tag %08X", PMData[i].tag);
                        printf(" len %08X", PMData[i].length);
                        printf(" datatype %08X\n", PMData[i].dataType);
                        uint32_t uPower = protocol->getValueAsUInt32(&PMData[i]);
                        printf(" Value  %i\n", uPower);
*/                        break;
                }
            }
            protocol->destroyValueData(PMData);
//            sleep(10);
            break;

    }
    // ...
    default:
        // default behavior
        printf("Unknown tag %08X\n", response->tag);
        break;
    }
}

static int processReceiveBuffer(const unsigned char * ucBuffer, int iLength)
{
    RscpProtocol protocol;
    SRscpFrame frame;

    int iResult = protocol.parseFrame(ucBuffer, iLength, &frame);
    if(iResult < 0) {
        // check if frame length error occured
        // in that case the full frame length was not received yet
        // and the receive function must get more data
        if(iResult == RSCP::ERR_INVALID_FRAME_LENGTH) {
            return 0;
        }
        // otherwise a not recoverable error occured and the connection can be closed
        else {
            return iResult;
        }
    }

    int iProcessedBytes = iResult;

    // process each SRscpValue struct seperately
    for(unsigned int i; i < frame.data.size(); i++) {
        handleResponseValue(&protocol, &frame.data[i]);
    }

    // destroy frame data and free memory
    protocol.destroyFrameData(frame);

    // returned processed amount of bytes
    return iProcessedBytes;
}

static void receiveLoop(bool & bStopExecution)
{
    //--------------------------------------------------------------------------------------------------------------
    // RSCP Receive Frame Block Data
    //--------------------------------------------------------------------------------------------------------------
    // setup a static dynamic buffer which is dynamically expanded (re-allocated) on demand
    // the data inside this buffer is not released when this function is left
    static int iReceivedBytes = 0;
    static std::vector<uint8_t> vecDynamicBuffer;

    // check how many RSCP frames are received, must be at least 1
    // multiple frames can only occur in this example if one or more frames are received with a big time delay
    // this should usually not occur but handling this is shown in this example
    int iReceivedRscpFrames = 0;
    while(!bStopExecution && ((iReceivedBytes > 0) || iReceivedRscpFrames == 0))
    {
        // check and expand buffer
        if((vecDynamicBuffer.size() - iReceivedBytes) < 4096) {
            // check maximum size
            if(vecDynamicBuffer.size() > RSCP_MAX_FRAME_LENGTH) {
                // something went wrong and the size is more than possible by the RSCP protocol
                printf("Maximum buffer size exceeded %i\n", vecDynamicBuffer.size());
                bStopExecution = true;
                break;
            }
            // increase buffer size by 4096 bytes each time the remaining size is smaller than 4096
            vecDynamicBuffer.resize(vecDynamicBuffer.size() + 4096);
        }
        // receive data
        int iResult = SocketRecvData(iSocket, &vecDynamicBuffer[0] + iReceivedBytes, vecDynamicBuffer.size() - iReceivedBytes);
        if(iResult < 0)
        {
            // check errno for the error code to detect if this is a timeout or a socket error
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                // receive timed out -> continue with re-sending the initial block
                printf("Response receive timeout (retry)\n");
                break;
            }
            // socket error -> check errno for failure code if needed
            printf("Socket receive error. errno %i\n", errno);
            bStopExecution = true;
            break;
        }
        else if(iResult == 0)
        {
            // connection was closed regularly by peer
            // if this happens on startup each time the possible reason is
            // wrong AES password or wrong network subnet (adapt hosts.allow file required)
            printf("Connection closed by peer\n");
            bStopExecution = true;
            break;
        }
        // increment amount of received bytes
        iReceivedBytes += iResult;

        // process all received frames
        while (!bStopExecution)
        {
            // round down to a multiple of AES_BLOCK_SIZE
            int iLength = ROUNDDOWN(iReceivedBytes, AES_BLOCK_SIZE);
            // if not even 32 bytes were received then the frame is still incomplete
            if(iLength == 0) {
                break;
            }
            // resize temporary decryption buffer
            std::vector<uint8_t> decryptionBuffer;
            decryptionBuffer.resize(iLength);
            // initialize encryption sequence IV value with value of previous block
            aesDecrypter.SetIV(ucDecryptionIV, AES_BLOCK_SIZE);
            // decrypt data from vecDynamicBuffer to temporary decryptionBuffer
            aesDecrypter.Decrypt(&vecDynamicBuffer[0], &decryptionBuffer[0], iLength / AES_BLOCK_SIZE);

            // data was received, check if we received all data
            int iProcessedBytes = processReceiveBuffer(&decryptionBuffer[0], iLength);
            if(iProcessedBytes < 0) {
                // an error occured;
                printf("Error parsing RSCP frame: %i\n", iProcessedBytes);
                // stop execution as the data received is not RSCP data
                bStopExecution = true;
                break;

            }
            else if(iProcessedBytes > 0) {
                // round up the processed bytes as iProcessedBytes does not include the zero padding bytes
                iProcessedBytes = ROUNDUP(iProcessedBytes, AES_BLOCK_SIZE);
                // store the IV value from encrypted buffer for next block decryption
                memcpy(ucDecryptionIV, &vecDynamicBuffer[0] + iProcessedBytes - AES_BLOCK_SIZE, AES_BLOCK_SIZE);
                // move the encrypted data behind the current frame data (if any received) to the front
                memcpy(&vecDynamicBuffer[0], &vecDynamicBuffer[0] + iProcessedBytes, vecDynamicBuffer.size() - iProcessedBytes);
                // decrement the total received bytes by the amount of processed bytes
                iReceivedBytes -= iProcessedBytes;
                // increment a counter that a valid frame was received and
                // continue parsing process in case a 2nd valid frame is in the buffer as well
                iReceivedRscpFrames++;
            }
            else {
                // iProcessedBytes is 0
                // not enough data of the next frame received, go back to receive mode if iReceivedRscpFrames == 0
                // or transmit mode if iReceivedRscpFrames > 0
                break;
            }
        }
    }
}

static void mainLoop(void)
{
    RscpProtocol protocol;
    bool bStopExecution = false;
    int counter = 0;

    while(!bStopExecution)
    {
        //--------------------------------------------------------------------------------------------------------------
        // RSCP Transmit Frame Block Data
        //--------------------------------------------------------------------------------------------------------------
        SRscpFrameBuffer frameBuffer;
        memset(&frameBuffer, 0, sizeof(frameBuffer));

        // create an RSCP frame with requests to some example data
        createRequestExample(&frameBuffer);

        // check that frame data was created
        if(frameBuffer.dataLength > 0)
        {
            // resize temporary encryption buffer to a multiple of AES_BLOCK_SIZE
            std::vector<uint8_t> encryptionBuffer;
            encryptionBuffer.resize(ROUNDUP(frameBuffer.dataLength, AES_BLOCK_SIZE));
            // zero padding for data above the desired length
            memset(&encryptionBuffer[0] + frameBuffer.dataLength, 0, encryptionBuffer.size() - frameBuffer.dataLength);
            // copy desired data length
            memcpy(&encryptionBuffer[0], frameBuffer.data, frameBuffer.dataLength);
            // set continues encryption IV
            aesEncrypter.SetIV(ucEncryptionIV, AES_BLOCK_SIZE);
            // start encryption from encryptionBuffer to encryptionBuffer, blocks = encryptionBuffer.size() / AES_BLOCK_SIZE
            aesEncrypter.Encrypt(&encryptionBuffer[0], &encryptionBuffer[0], encryptionBuffer.size() / AES_BLOCK_SIZE);
            // save new IV for next encryption block
            memcpy(ucEncryptionIV, &encryptionBuffer[0] + encryptionBuffer.size() - AES_BLOCK_SIZE, AES_BLOCK_SIZE);

            // send data on socket
            int iResult = SocketSendData(iSocket, &encryptionBuffer[0], encryptionBuffer.size());
            if(iResult < 0) {
                printf("Socket send error %i. errno %i\n", iResult, errno);
                bStopExecution = true;
            }
            else {
                // go into receive loop and wait for response
                receiveLoop(bStopExecution);
		if (counter > 0) bStopExecution = true; // #MS# end program after first receive
            }
        }
        // free frame buffer memory
        protocol.destroyFrameData(&frameBuffer);

        // main loop sleep / cycle time before next request
        sleep(1);
	counter++;
    }
}

void usage(void){
    fprintf(stderr, "\nUsage: e3dcset [-c maxLadeLeistung] [-d maxEntladeLeistung] [-e manuelleLadeEnergie] [-a]\n\n");
    exit(EXIT_FAILURE);
}

int main(int argc, char *argv[])
{

	// Argumente der Kommandozeile parsen
    
    	if (argc == 1){
	  usage();
	}
    
    int opt;

    while ((opt = getopt(argc, argv, "c:d:e:a")) != -1) {
        switch (opt) {
        case 'c': 
        	maxLadeLeistung = atoi(optarg);
        	break;
        case 'd': 
        	maxEntladeLeistung = atoi(optarg);
        	break;
        case 'e':
        	// Wenn der Speicher manuell geladen werden soll
        	// schalten wir auch 'automatischLeistungEinstellen' aktiv
        	manuelleSpeicherladung = true;
		manuelleLadeEnergie = atoi(optarg);
		//automatischLeistungEinstellen = true;
        	break;
        case 'a':
        	automatischLeistungEinstellen = true;
        	break;
        default:
          usage();
        }
    }

// Argumente der Kommandozeile plausibilisieren

    if (!automatischLeistungEinstellen && (maxLadeLeistung < 1 || maxLadeLeistung > MAXLEISTUNG)){
    	fprintf(stderr, "[-c maxLadeLeistung] muss zwischen 1 und %i liegen\n\n", MAXLEISTUNG);
    	exit(EXIT_FAILURE);
    }
    
    if (!automatischLeistungEinstellen && (maxEntladeLeistung < 1 || maxEntladeLeistung > MAXLEISTUNG)){
    	fprintf(stderr, "[-d maxEntladeLeistung] muss zwischen 1 und %i liegen\n\n", MAXLEISTUNG);
    	exit(EXIT_FAILURE);
    }
    
    if (automatischLeistungEinstellen && (maxEntladeLeistung > 0 || maxEntladeLeistung > 0)){
    	fprintf(stderr, "bei Lade/Entladeleistung Automatik [-a] duerfen [-c maxLadeLeistung] und [-d maxEntladeLeistung] nicht gesetzt sein\n\n");
    	exit(EXIT_FAILURE);
    }
    
    if (manuelleSpeicherladung && (manuelleLadeEnergie < MIN_MANUELLE_LADUNG_ENERGIE || manuelleLadeEnergie > MAX_MANUELLE_LADUNG_ENERGIE)){
    	fprintf(stderr, "Fuer die manuelle Speicherladung muss der angegebene Wert zwischen %iWh und %iWh liegen\n\n",MIN_MANUELLE_LADUNG_ENERGIE,MAX_MANUELLE_LADUNG_ENERGIE);
    	exit(EXIT_FAILURE);   	
    }


    // endless application which re-connections to server on connection lost
    
        // connect to server
        printf("Connecting to server %s:%i\n", SERVER_IP, SERVER_PORT);
        iSocket = SocketConnect(SERVER_IP, SERVER_PORT);
        if(iSocket < 0) {
            printf("Connection failed\n");
            sleep(1);
            //continue;
        }
        printf("Connected successfully\n");

        // reset authentication flag
        iAuthenticated = 0;

        // create AES key and set AES parameters
        {
            // initialize AES encryptor and decryptor IV
            memset(ucDecryptionIV, 0xff, AES_BLOCK_SIZE);
            memset(ucEncryptionIV, 0xff, AES_BLOCK_SIZE);

            // limit password length to AES_KEY_SIZE
            int iPasswordLength = strlen(AES_PASSWORD);
            if(iPasswordLength > AES_KEY_SIZE)
                iPasswordLength = AES_KEY_SIZE;

            // copy up to 32 bytes of AES key password
            uint8_t ucAesKey[AES_KEY_SIZE];
            memset(ucAesKey, 0xff, AES_KEY_SIZE);
            memcpy(ucAesKey, AES_PASSWORD, iPasswordLength);

            // set encryptor and decryptor parameters
            aesDecrypter.SetParameters(AES_KEY_SIZE * 8, AES_BLOCK_SIZE * 8);
            aesEncrypter.SetParameters(AES_KEY_SIZE * 8, AES_BLOCK_SIZE * 8);
            aesDecrypter.StartDecryption(ucAesKey);
            aesEncrypter.StartEncryption(ucAesKey);
        }

        // enter the main transmit / receive loop
        mainLoop();

        // close socket connection
        SocketClose(iSocket);
        iSocket = -1;
    
    printf("Done!\n\n");
    return 0;
}

