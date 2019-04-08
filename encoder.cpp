#include <Wire.h>
SYSTEM_THREAD(ENABLED)
#define stepsPIN D6
#define progPIN D3
#define knobPIN A4
#define setPosPIN D4

#define progRunSpeed 30

bool stepState, laststepState = 0;
bool revState, lastrevState = 0;
bool progState, lastprogState = 0;
bool runState, lastrunState = 0;
bool lastgoState = 0;
bool setPosState, lastsetPosState = 0;
bool programMode = false, runMode = false, stopped = false, positionsRead = false;
int prevMillis = 0, prevMillis1 = 0, secondsCount = 0, prevMillis3 = 0, prevMillis4 = 0;
short direction = 0;
int pos = 0, nextPos = 0, knobPos = 0, knobCount = 0, knobBuffer = 0, knobPosTemp = 0;
short countPos = 0, nextPosID = 0, counting = 2;
char input;

bool nextPosID1 = 0, nextPosID2 = 0, nextPosID3 = 0, stop = 0, go = 0;
bool slaveID1 = 0, slaveID2 = 0, slaveID3 = 0;
bool posID1 = 0, posID2 = 0, posID3 = 0, moving = false, lastMoving = false;
char localData[12] = "00000000000";
char datain2[10] = "0";
char datain3[6] = "0";
bool callStopping = false;
void updateSteps(void);

struct MyObject {
    uint8_t version;
    int pos1, pos2, pos3, pos4, pos5, pos6;
};
MyObject savedPos;
MyObject defaultObj = { 0, 0, 0, 0, 0, 0, 0 };

short knobHome = 0;
void setup() {
    Wire.begin();
    Wire.beginTransmission(0x20);
    // Select IODIR register
    Wire.write(0x00);
    // All pins are configured as outputs
    Wire.write(0x00);
    // Stop I2C transmission
    Wire.endTransmission();
    delay(150);
    
    Serial.begin(115200);
    Serial1.begin(9600);
    pinMode(stepsPIN, INPUT_PULLDOWN);
    pinMode(progPIN, INPUT_PULLDOWN);
    pinMode(knobPIN, INPUT);
    pinMode(setPosPIN, INPUT_PULLDOWN);
    attachInterrupt(stepsPIN, updateSteps, RISING);
    selectExternalMeshAntenna();
    Mesh.subscribe("toMasterControl", subHandler);
    
    EEPROM.get(0, savedPos);
    delay(5);
    if(savedPos.version != 0) {
        savedPos = defaultObj;
    }

}

void loop() {
    buttonInputs();
    updateBools();
    if(runMode == true && positionsRead == true) {
        
    }

    if(programMode == true) {
        getKnobPos();
        move();
        setPos();
    }
    
    int counttemp = 0;
    if(Serial.available() > 0){
        while(Serial.available() > 0) {
            datain2[counttemp] = Serial.read();
            counttemp++;
        }
        Mesh.publish("Next-Position", datain2);
        Serial.printf("Sent %s\n", datain2);
    }
    
    if(lastMoving != moving) {
        if(moving == true) {
            Wire.beginTransmission(0x20);
            Wire.write(0x09);
            Wire.write(0x04);
            Wire.endTransmission();
        }else{
            Wire.beginTransmission(0x20);
            Wire.write(0x09);
            Wire.write(0x00);
            Wire.endTransmission();
        }
    }
    lastMoving = moving;
    stopFunc();
}


void sendPosIDs(){
    localData[8] = posID1 + 48;
    localData[9] = posID2 + 48;
    localData[10] = posID3 + 48;
    Mesh.publish("toSlaveControl", localData);
}

/****************************************************/
/*  subHandler                                      */
/****************************************************/
void subHandler(const char *event, const char *data)
{
    for(int i = 0; i < 8; i++) {
        localData[i] = data[i];
    }
    nextPosID1 = localData[0] - 48;
    nextPosID2 = localData[1] - 48;
    nextPosID3 = localData[2] - 48;
    stop = localData[3] - 48;
    go = localData[4] - 48;
    slaveID1 = localData[5] - 48;
    slaveID2 = localData[6] - 48;
    slaveID3 = localData[7] - 48;
    Serial.printf("Master Received: %s\n", localData);
    nextPosID = (nextPosID1 * 4) + (nextPosID2 * 2) + nextPosID3;
    switch(nextPosID) {
        case 1: nextPos = savedPos.pos1;
            break;
        case 2: nextPos = savedPos.pos2;
            break;
        case 3: nextPos = savedPos.pos3;
            break;
        case 4: nextPos = savedPos.pos4;
            break;
        case 5: nextPos = savedPos.pos5;
            break;
        case 6: nextPos = savedPos.pos6;
            break;
        default: break;
    }
}

void setPos() {
    setPosState = digitalRead(setPosPIN);
    if(lastsetPosState != setPosState) {
        if(setPosState == 1) {    // ACTIVE high
            countPos += 1;
            switch(countPos) {
                case 1: {
                            pos = 0;
                            savedPos.pos1 = pos;
                            Serial.printf("Pos %d Set/n", countPos);
                            break;
                        }
                case 2: {
                            savedPos.pos2 = pos;
                            Serial.printf("Pos %d Set/n", countPos);
                            break;
                        }
                case 3: {
                            savedPos.pos3 = pos;
                            Serial.printf("Pos %d Set/n", countPos);
                            break;
                        }
                case 4: {
                            savedPos.pos4 = pos;
                            Serial.printf("Pos %d Set/n", countPos);
                            break;
                        }
                case 5: {
                            savedPos.pos5 = pos;
                            Serial.printf("Pos %d Set/n", countPos);
                            break;
                        }
                case 6: {
                            savedPos.pos6 = pos;
                            Serial.printf("Pos %d Set/n", countPos);
                            break;
                        }
                default: break;
            }
        }
    }
    lastsetPosState = setPosState;
}

void move() {
    if((millis() - prevMillis1) > 25) {
        int knobPos2 = knobPos;
        int speed;
        knobPos2 -= knobHome;
        speed = ((float) knobPos2 / knobHome) * progRunSpeed;
        char data[8];
        String format = (speed < 0) ? "(2%s)" : "(1%s)";
        if(speed < 0) { 
            speed *= -1;
            direction = 2; 
        }else if(speed > 0) {
            direction = 1;
        }
        if(speed != 0) { stopped = false; }
        int speed2 = speed % 100;
        char setting[5] = { ((int) speed / 100) + 48, ((int) speed2 / 10) + 48, ((int) speed2 % 10) + 48, '0' };
        sprintf(data, format, setting);
        char checkSum = 0;
        for(int i = 0; i < 8; i++){
            checkSum += data[i];
        }
        checkSum = checkSum & 255;
        char sum1 = (checkSum / 0x0F) + 0x30;
        char sum2 = (checkSum % 0x10) + 0x30;
        char *formatTwo = "%s%c%c";
        char finalData[10];
        sprintf(finalData, formatTwo, data, sum1, sum2);
        if( stopped == false) {
            Serial1.print(finalData);
            Serial.printf("Setting Data: %s\n", finalData);
        }
        
        if((millis() - prevMillis) > 1000) {
            if(speed == 0) {
                secondsCount++;
            }else {
                secondsCount = 0;
            }
            if(secondsCount > 10 && secondsCount < 13){
            callStopping = true;
            stopped = true;
            }
        prevMillis = millis();
        }
        
        
    Serial.printf("Saved Pos: %d, %d, %d, %d, %d, %d, %d       %d\n", savedPos.version, savedPos.pos1, 
        savedPos.pos2, savedPos.pos3, savedPos.pos4, savedPos.pos5, savedPos.pos6, pos);
        
    prevMillis1 = millis();
    }
    
    
    
}

void buttonInputs() {
    if(counting == 1){
        if((millis() - prevMillis3) > 500) {
            knobHome = analogRead(knobPIN);
            Serial.println("Knob Home Set");
            programMode = true;
            moving = true;
            counting = 2;
        }
    }else if(counting == 0){
        if((millis() - prevMillis3) > 100) {
            programMode = false;
            moving = false;
            callStopping = true;
            countPos = 0;
            counting = 2;
            EEPROM.put(0, savedPos);
            delay(5);
        }
    }
    
    // Program Button
    progState = digitalRead(progPIN);
    if(lastprogState != progState) {
        if(progState == 1) {
            counting = 1;
            prevMillis3 = millis();
        }else if(progState == 0){
            counting = 0;
            prevMillis3 = millis();
        }
    }
    lastprogState = progState;
    
    runState = digitalRead(runPIN);
    if(lastrunState != runState) {
        if(runState == 1 && programMode == 0) {
            runMode = true;
        }else if(runState == 0) {
            runMode = false;
        }
    }
    lastrunState =  runState;
    
    stepState = digitalRead(stepsPIN); 
    if(laststepState != stepState) {
        
    }
    laststepState = stepState;
}

void updateSteps() {
    if(direction == 1) {
        pos += 1;
    }else if(direction == 2) {
        pos -= 1;
    }
}

void getKnobPos() {
    knobPosTemp = analogRead(knobPIN);
    knobBuffer += knobPosTemp;
    knobCount++;
    if(knobCount == 25) {
        knobPos = knobBuffer / knobCount;
        knobCount = 0;
        knobBuffer = 0;
    }
}

void updateBools() {
    
    if(lastgoState != go) {
        if(go == 1) {
            char tempNextPosChar[15] = "";
            itoa(nextPos, tempNextPosChar, 10);
            Mesh.publish("Next-Position", tempNextPosChar);
            Serial.printf("Sent %s\n", tempNextPosChar);
            localData[4] = '0';
            go = 0;
        }
    }
    lastgoState = go;
}

void stopFunc() {
    if(callStopping == true) {
        if((millis() - prevMillis4) > 100) {
            Serial1.print("(3)84");
            Serial.println("Sent Stop Command");
            prevMillis4 = millis();
        }
        
        if(Serial1.available() > 4){
            
            for(int i = 0; i < 5; i++) {
                char tempchar2 = Serial1.read();
                if(tempchar2 == '('){
                    i = 0;
                }
                datain3[i] = tempchar2;
            }
            Serial.println(datain3);
            if(strcmp(datain3, "(3)84") == 0) {
                Serial.println("Recieved Stop Confirmation");
                callStopping = false;
            }
        }
    }
}

void selectExternalMeshAntenna() {

#if (PLATFORM_ID == PLATFORM_ARGON)
    digitalWrite(ANTSW1, 1);
    digitalWrite(ANTSW2, 0);
#elif (PLATFORM_ID == PLATFORM_BORON)
    digitalWrite(ANTSW1, 0);
#else
    digitalWrite(ANTSW1, 0);
    digitalWrite(ANTSW2, 1);
#endif
}

