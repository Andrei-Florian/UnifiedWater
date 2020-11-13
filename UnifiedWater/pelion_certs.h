#ifndef _PELION_CERTS_H
#define _PELION_CERTS_H

#include <MKRGSM.h>

int selectDF() {
    MODEM.noop();
    delay(100);

    String response;
    int retry = 5;

    while(!response.endsWith(",\"9000\"") && retry > 0) {
        MODEM.send("AT+CSIM=14,\"00A40000023F00\""); // MF MODE
        if (MODEM.waitForResponse(100, &response) != 1) {
            return 0;
        }
        retry--;
        delay(3000);
    }

    delay(3000);
    response = ""; retry = 5;

    while(!response.endsWith(",\"9000\"") && retry > 0) {
        MODEM.send("AT+CSIM=14,\"00A40000027F00\""); // SELECT DF
        if (MODEM.waitForResponse(100) != 1) {
            return 0;
        } 
        retry--;
        delay(3000);
    }

    return 1;
}

int readEF(int fileId, char* buffer) {
    String response;
    int retry = 5;
    char atCommand[30];
    int fileLength; // /!\ this will be the actual file length, i.e there will be twice as many HEX chars to read in total

    delay(3000);

    // select fileId
    while(!response.endsWith(",\"9000\"") && retry > 0) {
        sprintf(atCommand, (char*)F("AT+CSIM=14,\"00A4000002%04X\""), fileId); 
        MODEM.send(atCommand); 
        if (MODEM.waitForResponse(100) != 1) {
            return -1;
        } 
        retry--;
        delay(1000);
    }

    delay(4000);

    // read file length
    MODEM.send(F("AT+CSIM=10,\"00B0000002\""));
    if (MODEM.waitForResponse(500, &response) != 1) {
        return -1;
    } 
    int idx = response.indexOf(",\"");
    String fileLengthHex = response.substring(idx + 2, idx + 2 + 4);
    // parse length
    sscanf(fileLengthHex.c_str(), "%04X", &fileLength);
    Serial.print("File length: ");
    Serial.println(fileLength);

    String contents = "";

    char readCommand[30];
    for(int offset = 0 ; offset * 0xFF < fileLength ; offset ++) {
        delay(500);
        sprintf(readCommand, "AT+CSIM=10,\"00B0%04xFF\"", 2 + offset * 0xFF); 
        MODEM.send(readCommand);
        if (MODEM.waitForResponse(500, &response) != 1) {} // goto error_getting_certs;
        contents += response.substring(response.indexOf(",\"") + 2, response.lastIndexOf("9000\""));
        delay(200);
    }

    // trim contents to filelength*2
    contents = contents.substring(0, fileLength * 2);
    // convert hex representation to actual bytes
    const char* contents2 = contents.c_str();
    for(int i=0 ; i < fileLength ; i++) {
        char c;
        if(sscanf(&contents2[2*i], "%02x", &c) == 1) {
            buffer[i] = c;
        } else {
            Serial.println("err scanf");
            return -1;
        } 
    }

    return fileLength;

}



#endif
