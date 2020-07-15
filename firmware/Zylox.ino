//-----------------------------------------------------------------
// ZYLOX
// Firmware for Retail Locker Controller
// Use Arduino Mega 2560 Board at 16Mhz
// Can be set as a MASTER or SLAVE for extented lockers
// Created by : Permana
// Last Modified : July 14, 2020
// Company : Gerai Cerdas Internasional
// Country : Indonesia
// Email   : permana@geraicerdas.com
// 
//-----------------------------------------------------------------

// FOR SERIAL DEBUGGING
// Activate when it needed
//#define DEBUG 1

// Please take a look your solenoid datasheet
// DO NOT POWER YOUR SOLENOID MORE THAN 12 SECONDS.
int limitOn = 500;
String hasilBacaSerial;
String hasilBacaSerialMS;
boolean immediately = true;

// Definition
#define MAX_KEY 22
#define ditutup 1
#define tertutup 1
#define terbuka 0
#define SerialMS Serial3

// PLEASE TAKE A LOOK TO THE SCHEMATIC FOR THIS SETTING
// This is how the Arduino connected to the inputs and outputs
///                       1 2 3  4  5  6  7  8  9 10 11 12 13 14 15 16 17  18  19 20 21 22 
int boxKey[MAX_KEY] = {16,3,52,9,49,11,44,46,18,20,35,37,39,31,24,26,A12,A14,A6,A8,A0,A2 };
int boxIrd[MAX_KEY] = {6,7,51,50,13,48,43,42,38,41,34,33,29,28,23,22,A11,A10,A5,A4,4,5 };
int boxLsw[MAX_KEY] = {17,2,53,8,12,10,45,47,19,21,36,40,30,32,25,27,A13,A15,A7,A9,A1,A3 };

int onOff[MAX_KEY] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
long lastActivated[MAX_KEY] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
char statusKey[MAX_KEY] = {'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z', 'z' };
byte statusIrd[MAX_KEY] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
byte statusLsw[MAX_KEY] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };

//-- ONLY FOR MASTER CONTROLLER
byte perluGantiPort[MAX_KEY] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int portPengganti[MAX_KEY]= {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

int responseTime = 300;

struct responses {
  boolean isGetOk;
  String strResponse;
};

//-----------------------------------------------------------------
// SETUP and INIT
//-----------------------------------------------------------------
void setup() {
  // Port Initialization
  // Remember to consolidate with the schematic..!!!!
  for (int i = 0; i<MAX_KEY; i++) {
    pinMode(boxKey[i], OUTPUT);
    pinMode(boxIrd[i], INPUT);
    pinMode(boxLsw[i], INPUT);
  }
  
  // SERIAL INIT
  // set baud rate to 9600 bps or what ever you want
  Serial.begin(9600);
  SerialMS.begin(115200);
  #ifdef DEBUG
    Serial.println("Start debuging");
  #endif
  
}

//-----------------------------------------------------------------
// MAIN PROGRAM
//-----------------------------------------------------------------

void loop() {

  // Remember to Turn off the solenoids to help maximize it lifetime
  turnOffActiveKey(false);

  // Check the sensors
  checkAllSensors();


  // SERIAL HANDLE
  //
  // Read char
  // "ENTER" character or ( \n ) is end of the instructions set
  
  while (Serial.available()) {
    char charSerial = Serial.read();
    delay(1);
    if (charSerial == '\r') {}
    else if (charSerial == '\n') {
      #ifdef DEBUG
        Serial.print(hasilBacaSerial);
        Serial.println(" <-- Ini akhir instruksi");
      #endif
      
      // CHECK AND EXECUTE
      prosesData(hasilBacaSerial);
            
      // remove our last reading, because we already processed
      hasilBacaSerial = "";
    }
    // varchar concatenation
    else hasilBacaSerial += charSerial;
  }

  
  // Only for MASTER controller
  // remove this block if you set it as SLAVE controller
  while (SerialMS.available()) {
    char charDataSlave = SerialMS.read();
    delay(1);

    if (charDataSlave == '\r') {}
    else if (charDataSlave == '\n') {
      #ifdef DEBUG
        Serial.println(hasilBacaSerialMS);
        Serial.println("Ini akhir instruksi");
      #endif

      sendOk2Client(hasilBacaSerialMS);
                  
      int realLocker = hasilBacaSerialMS.substring(0,2).toInt();
            
      realLocker += MAX_KEY;

      String cmdForPc;
 
      cmdForPc = String(realLocker) + hasilBacaSerialMS.substring(2,4);

      SerialMS.flush();
      
      struct responses pcRespons = sendCmd2Pc(cmdForPc);
      
      // remove our last reading, because we already processed
      hasilBacaSerialMS = "";
    }
    // varchar concatenation
    else hasilBacaSerialMS += charDataSlave;
    
  }
   
}

//-----------------------------------------------------------------
// PROCEDURES and FUNCTIONS
//-----------------------------------------------------------------

//-----------------------------------------------------------------
// Function Name : prosesData
// Used for parsing string and execute if the command is in the list
// ACTION      : # (two digits locker number)(a-e)  
//                 example : 05a
//                 instruction for opening locker number 5 
//               # (two digits locker number)(z)  
//                 example : 05z
//                 instruction for turning off the solenoid number 5
// INFORMATION : # (two digits locker number)(s)
//                 example : 02s
//                 to get locker status information
//-----------------------------------------------------------------
void prosesData(String serialIn) {

  // only for MASTER controller
  if (serialIn=="RST") {
    sendOk2Pc("RST");
  }
  // only for MASTER controller
  else if (serialIn=="CHK") {

    
    struct responses clientRespons = sendCmd2Client("CHK");
    String responCommand;
    if (clientRespons.isGetOk) {
      responCommand = serialIn + "1";
    
      sendOk2Pc(responCommand);    
    }
  }
  
  // yang ini adalah kiriman yg dua karakter depannya adalah angka
  else {
    int idxBox = serialIn.substring(0,2).toInt();
    char command[2];
    serialIn.substring(2).toCharArray(command, 2);
    if (idxBox != 0) {
      // cek apakah port yg diakses ada penggantinya
      if (perluGantiPort[idxBox - 1]) {
        idxBox = portPengganti[idxBox - 1];
      }
    
      // jika nomor locker lebih besar dari MAX_KEY - 1
      // maka berarti akan mengakses port di slave
      if (idxBox > MAX_KEY) {
               
        int idxBoxForClient = idxBox - MAX_KEY;
        char cmdForClient[5];
        sprintf(cmdForClient, "%02d%s",idxBoxForClient,command );
        struct responses clientRespons = sendCmd2Client(cmdForClient);
        if (clientRespons.isGetOk) {
          char sensorStatus[3];
          clientRespons.strResponse.substring(2).toCharArray(sensorStatus,3);
          String cmdForPc = serialIn.substring(0,2) + sensorStatus;
          sendOk2Pc(cmdForPc);
        } 
      }
      
      // kalau nomor locker masih diantara 0 - MAX_KEY
      else {
        #ifdef DEBUG
        Serial.print("idxBox : ");
        Serial.println(idxBox);
        Serial.print("command : ");
        Serial.println(command[0]);
        Serial.println(command[1]);
        #endif

        // ini adalah instruksi untuk mengaktifkan solenoid
        if ( (strstr(command, "A") > 0) ||
             (strstr(command, "B") > 0) ||
             (strstr(command, "C") > 0) ||
             (strstr(command, "D") > 0) ||
             (strstr(command, "E") > 0) ) {

               // DUMMY, TAPI HARUSNYA BEKERJA, 5 DIGANTI 0
               if (idxBox == 0) {
                 
                 
               } else {
               
                                  
                    byte statusLocker = openLocker(idxBox);
                    String responCommand;
                    if (statusLocker == 0) {
                      responCommand = serialIn.substring(0,2) + "W0";
                    } else if (statusLocker == 1) {
                      responCommand = serialIn.substring(0,2) + "W1";
                    } else if (statusLocker == 2) {
                      responCommand = serialIn + "1";
                    } else if (statusLocker == 3) {
                      responCommand = serialIn.substring(0,2) + "W9";
                    }

                // update status box
                statusKey[idxBox-1] = command[0];
          
                // send respon to PC
                sendOk2Pc(responCommand);
          
               }
       }
       
       // command for turning off the solenoid
       else if (strstr(command, "Z") > 0) {
          digitalWrite(boxKey[idxBox-1], LOW);
          onOff[idxBox-1] = 0;
          lastActivated[idxBox-1] = 0;

          // update status box
          statusKey[idxBox-1] = command[0];
       }
       
       // instruction to get infrared sensor status
       else if (strstr(command, "S") > 0) {
         String responCommand;
         responCommand = serialIn + statusIrd[idxBox-1];
         sendOk2Pc(responCommand);
       }
       
       // instruction to get limit switch status
       else if (strstr(command, "T") > 0) {
         String responCommand;
         responCommand = serialIn + statusLsw[idxBox-1];
         sendOk2Pc(responCommand);
       } 
     }
    }
  }

}

//-----------------------------------------------------------------
// Function Name : turnOffActiveKey
// Parameter     : now
// digunakan untuk menonaktifkan solenoid yang aktif secara otomatis
// Ini untuk keamanan dan memaksimalkan life time dari solenoid
// Jika parameter now = true, berarti akan dimatikan saat ini juga
// tapi jika now = false, berarti akan dimatikan setelah limit
// yang didefinisikan di bagian atas sketch
//-----------------------------------------------------------------
void turnOffActiveKey(boolean now) {
  // scanning
  // Cek setiap key yang aktif
  // lalu bandingkan antara waktu saat ini dengan terakhir dia diakses
  for (int i=0; i<MAX_KEY; i++) {
    
    if (onOff[i] == 1) {
      #ifdef DEBUG
        Serial.println("ada yang ON nih!");
      #endif

      if (((millis() - lastActivated[i]) > limitOn) or (now)) {
        #ifdef DEBUG
          Serial.print("shutting down : ");
          Serial.println(i);
        #endif
        digitalWrite(boxKey[i], LOW);
        onOff[i] = 0;
        lastActivated[i] = 0;

        // Jangan lupa update status box!
        statusKey[i] = 'z';
        
      }
    }
  }
}


//-----------------------------------------------------------------
// Function Name : checkAllSensors
// digunakan untuk membaca seluruh sensor infrared dan
// sensor limit switch yang terpasang
// data hasil pembacaannya disimpan di array status
// masing-masing sensor
//-----------------------------------------------------------------
void checkAllSensors() {
  // Lakukan scanning
  // Cek seluruh sensor
  for (int i=0; i<MAX_KEY; i++) {
    
       int idxBox = i;
       int idxBoxGanti = idxBox;

    if (perluGantiPort[idxBox]) {
      idxBoxGanti = portPengganti[idxBox] - 1;
    }
    
    byte detectLsw;
    byte detectIrd;
    if (idxBoxGanti <= MAX_KEY) {
      
      detectLsw = digitalRead(boxLsw[idxBoxGanti]);
      detectIrd = digitalRead(boxIrd[idxBoxGanti]);
      delay(5);
    } else {
      char sendCommand[4];
      sprintf(sendCommand, "%02dT", idxBoxGanti - MAX_KEY);

      SerialMS.println(sendCommand);
    }
    

    if (statusIrd[idxBoxGanti] != detectIrd) {
      statusIrd[idxBoxGanti] = detectIrd;
    }
        
    if (statusLsw[idxBoxGanti] != detectLsw) {

      #ifdef DEBUG
        Serial.println("ada perubahan pintu locker ");
        #endif
      
      if (detectLsw == ditutup) {
        char responCommand[5];
        sprintf(responCommand, "%02dT%d", idxBox+1, statusIrd[idxBoxGanti]);
        #ifdef DEBUG
        Serial.print("ada locker ditutup : ");
        Serial.println(idxBox+1);
        #endif
      
        sendCmd2Pc(responCommand);
      } 
      
      // update last status
      statusLsw[idxBoxGanti] = detectLsw;
      
    }
    
  }
}

void sendInfoExistOrNot(byte noLocker) {
  if (noLocker<10) Serial.print("0");
  Serial.print(noLocker);
  Serial.print("T");
  Serial.println(statusIrd[noLocker]);  
}

void sendOk2Client(String hasilBacaSerial) {
  SerialMS.print(hasilBacaSerial);
  SerialMS.println(" OK");  
}

void sendOk2Pc(String cmd) {
  Serial.print(cmd);
  Serial.println(" OK");  
}

struct responses sendCmd2Pc(String cmd) {
  struct responses myPcResponse;
  boolean receivedResponse = false;
  boolean timeout = false;
  boolean firstAttempt = true;
  String strResponse = "";
  long startWaitingResponse = millis();
  byte totalAttempt = 0;
  while ( (!receivedResponse) && (!timeout) ) {
    if (((millis() - startWaitingResponse) > responseTime)  || firstAttempt ) {
      if (totalAttempt == 3) {
        timeout = true;
        // Send error message, cannot connect to PC
        Serial.println("00W3");
      }
      else {

        Serial.println(cmd);
        strResponse = "";
        firstAttempt = false;
        startWaitingResponse = millis(); 
        totalAttempt++;
      }
    }

    // baca serial untuk mendapatkan response nya
    while (Serial.available()) {
      char charSerial = Serial.read();
      if (charSerial == '\r') {}
      else if (charSerial == '\n') {
        #ifdef DEBUG
        Serial.println();
        #endif
   
        char desiredResponse[3];
        strResponse.substring(5).toCharArray(desiredResponse, 3);
        if (strstr(desiredResponse,"OK") > 0) receivedResponse = true;
        
      }
      // varchar concatenation
      else strResponse += charSerial;
       
    }
         
  }     
  
  myPcResponse.isGetOk = receivedResponse;  
  myPcResponse.strResponse = strResponse;
  return myPcResponse;
  
}

struct responses sendCmd2Client(String cmd) {  
  struct responses myClientResponse;
  
  boolean receivedResponse = false;
  boolean timeout = false;
  boolean firstAttempt = true;
  String strResponse = "";
  long startWaitingResponse = millis();
  byte totalAttempt = 0;
  while ( (!receivedResponse) && (!timeout) ) {
    if (((millis() - startWaitingResponse) > (responseTime + 3000))  || firstAttempt ) {
      
      if (totalAttempt == 1) {
        timeout = true;
        // send Error message
        Serial.println("00W2");
        //Serial.println("Cannot reach SLAVE");
      }
      else {

        SerialMS.println(cmd);
        strResponse = "";
        firstAttempt = false;
        startWaitingResponse = millis(); 
        totalAttempt++;
      }
    }

    // read serial
    while (SerialMS.available()) {
      char charSerial = SerialMS.read();
      delay(10);
      if (charSerial == '\r') {}
      
      else if (charSerial == '\n') {
        #ifdef DEBUG
        Serial.println();
        #endif
        
        char actualResponse[3];
        strResponse.substring(5).toCharArray(actualResponse, 3);

        #ifdef DEBUG
          Serial.print("Actual Respons : ");
          Serial.println(actualResponse);
        #endif

        if (strstr(actualResponse,"OK") > 0) receivedResponse = true;
        
      }
      // masih nampung data, belum ketemu "enter"
      else strResponse += charSerial;
       
    }
         
  }
  
  myClientResponse.isGetOk = receivedResponse;  
  myClientResponse.strResponse = strResponse;
  return myClientResponse;
  
}

byte openLocker(int noLocker) {

  // FOR SAFETY REASON
  // matikan key yang sudah aktif sebelumnya, SEKARANG JUGA!
  turnOffActiveKey(immediately);
     
  // Kalau kita bilang 1
  // maka di arraynya ada di posisi ke 0
  // jadi 1-1
     
  // Cek dulu posisi limit switch sebelum solenoidnya dibuka
  // karena ada beberapa kondisi yang ingin di cek
  // 1. Apa solenoidnya rusak : limit switch sebelum dan sesudah selalu terkunci
  // 2. Apa aktuator tidak terhubung : limit switch sebelum dan sesudah selalu terbuka
  //                   POSISI LIMIT SWITCH
  // Sebelum locker dibuka    Setelah locker dibuka   Artinya
  // ----------------------------------------------------------
  // 0 (terbuka)              0 (terbuka)             Konektor RJ45 tidak terhubung
  // 0 (terbuka)              1 (tertutup)            -
  // 1 (tertutup)             0 (terbuka)             OK
  // 1 (tertutup)             1 (tertutup)            W0 -> Solenoid /pin kontroler rusak
     
  byte posSwitchBefore = digitalRead(boxLsw[noLocker - 1]);
  //delay(100); // make sure
     
  digitalWrite(boxKey[noLocker-1], HIGH);
  onOff[noLocker-1] = 1;
  lastActivated[noLocker-1] = millis();
     
  delay(500); // make sure
     
  // Cek posisi sensor solenoid
  // kalau terbuka, kirim pesan OK
  // kalau tidak terbuka, kirim pesan warning
  byte posSwitchAfter = digitalRead(boxLsw[noLocker - 1]);
     
  #ifdef DEBUG
  Serial.print("sebelum : ");
  Serial.println(posSwitchBefore);
  Serial.print("sesudah : ");
  Serial.println(posSwitchAfter);
  #endif
  
  char command[7];
  if ( (posSwitchBefore==tertutup) && (posSwitchAfter==tertutup) ) {
    return 0; // W0
  } else if ( (posSwitchBefore==terbuka) && (posSwitchAfter==terbuka) ) {
    return 1; // W1
  } else if ( (posSwitchBefore==tertutup) && (posSwitchAfter==terbuka) ) {
    return 2; // OK
  } else {
    return 3;  // W9
  }
  
}
