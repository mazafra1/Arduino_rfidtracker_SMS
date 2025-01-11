#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <SoftwareSerial.h>

SoftwareSerial mySIM800(2, 3); //SIM800 TX & RX is connected to Arduino #2 & #3

#define BUTTON_PIN 8

#define RST_PIN	9    //Pin 9 para el reset del RC522
#define SS_PIN	10   //Pin 10 para el SS (SDA) del RC522
MFRC522 mfrc522(SS_PIN, RST_PIN); //Creamos el objeto para el RC522

byte ActualUID[4]; //almacenará el código del Tag leído
const byte Atleta1[4]= {0x73, 0x1D, 0xF2, 0xF6}; //código del atleta 1
const byte Atleta2[4]= {0x63, 0x8B, 0x67, 0xEE}; //código del atleta 2

const int MAX_RETRIES = 2; // Number of times to retry when a SMS is not successful
const String DST_PHONE = "+ZZxxxxxxxxx"; //Use +ZZ with country code and xxxxxxxxxxx with phone number to sms
const int LAP_LENGTH = 400; // Use 0 for unknown lap distance.

unsigned long LastStateChangeTime = 0;
unsigned long startTime = 0;
bool isStartTimeSet = false;
unsigned long totalDistance = 0;
bool isNetworkOK = false;

// It prints out in the USB serial monitor 
// everything sent or received into the SIM800 serial
void updateSerial()
{
  delay(500);
  while (Serial.available()) 
  {
    mySIM800.write(Serial.read());//Forward what Serial received to Software Serial Port
  }
  while(mySIM800.available()) 
  {
    Serial.write(mySIM800.read());//Forward what Software Serial received to Serial Port
  }
}

// Function to check network status
bool isNetworkRegistered() {
  String response = "";
  String status = "";

  // Send the AT+CREG? command
  mySIM800.println("AT+CREG?");
  delay(500); // Wait for the response
  //updateSerial();

  // Read the response
  while (mySIM800.available()) {
    char c = mySIM800.read();
    response += c;
  }

  // Parse the response
  int commaIndex = response.indexOf(',');
  if (commaIndex != -1) {
    status = response.substring(commaIndex + 1, commaIndex + 2); // Extract the <stat> value
    status.trim();
  }

  // Return true if status is 1 (home network) or 5 (roaming), false otherwise
  return (status == "1" || status == "5");
}

String getNetworktime(){
  String extractedTime = "";
  String simResponse = "";

  if (!isNetworkOK){
    return "00:00:00";
  }

  mySIM800.println("AT+CCLK?"); //Check whether it has registered in the network
  delay(500);
  if (mySIM800.available())
  {
    simResponse = mySIM800.readString(); // Read the response
    // Filter out echoed AT command
    if (simResponse.startsWith("AT")) {
      simResponse = simResponse.substring(simResponse.indexOf('+CCLK: ') + 1); // Skip the first line (echoed command)
    }
  }

  // Find the comma after the date
  int commaIndex = simResponse.indexOf(','); 
  if (commaIndex != -1) {
    // Extract the time part, which starts after the comma
    int timeEndIndex = simResponse.indexOf('+', commaIndex); // Locate where the timezone starts
    if (timeEndIndex == -1) {
      timeEndIndex = simResponse.length(); // Default to end of string if no timezone
    }
    extractedTime = simResponse.substring(commaIndex + 1, timeEndIndex); // Extract time
  }
  
  return extractedTime;
}

bool InitializeSIM800(){
  mySIM800.println("AT"); //Once the handshake test is successful, it will back to OK
  updateSerial();
  mySIM800.println("AT+CSQ"); //Signal quality test, value range is 0-31 , 31 is the best
  updateSerial();
  // mySIM800.println("AT+CCID"); //Read SIM information to confirm whether the SIM is plugged
  // updateSerial();
  // mySIM800.println("AT+CREG?"); //Check whether it has registered in the network
  // updateSerial();
  // mySIM800.println("AT+CCLK?"); //Check network time
  // updateSerial();

  return isNetworkRegistered();
}

//Escribe la distancia total recorrida
String getDistance(unsigned long distanceInMeters) {
  unsigned long kilometers = distanceInMeters / 1000;
  unsigned long meters = distanceInMeters % 1000;

  if (distanceInMeters == 0) return "DISTANCIA TOTAL: ?";

  // Serial.print("DISTANCIA: ");
  // Serial.print(kilometers);
  // Serial.print(",");
  // Serial.print(meters);
  // Serial.println(" km");
  String res = "DIST. TOTAL: " + String(kilometers) + "," + String(meters) + " km";

  return res;
}

// Lee los milisegundos pasados y los convierte en min/km para una vuelta con longitud LAP_LENGTH (400m)
String getLapPace(unsigned long msec){
  String res = "";
  // Serial.println(msec);

  if (LAP_LENGTH == 0) return "";

  unsigned long proporcion = 1000 / LAP_LENGTH; // 1000/400 = 2.5

  unsigned long msecPerKm = msec * proporcion; 
  unsigned long hours = msecPerKm / 3600000;
  unsigned long minutes = (msecPerKm % 3600000) / 60000;
  unsigned long seconds = (msecPerKm % 60000) / 1000;

  res = "Ritmo Vuelta: ";
  // Serial.print("R vuelta: ");
  // Serial.print(hours);
  // Serial.print(":");
  // if (minutes < 10) Serial.print("0");
  res = res + minutes + ":";
  // Serial.print(minutes);
  // Serial.print(":");
  if (seconds < 10) res = res + "0";//Serial.print("0");
  res = res + seconds + " min/km";
  // Serial.print(seconds);
  // Serial.println(" min/km");

  return res;
}

String getLaptimeAndTotaltime(bool includeLapInfo)
{  
  String sTextoCompleto = "";
  if (!isStartTimeSet) {
    return sTextoCompleto;
  }

  unsigned long currentTime = millis();  
  unsigned long lapTime = currentTime - LastStateChangeTime;

  unsigned long hours = lapTime / 3600000;
  unsigned long minutes = (lapTime % 3600000) / 60000;
  unsigned long seconds = (lapTime % 60000) / 1000;
  String sTiempoVuelta = "";
  if (includeLapInfo) {
    sTiempoVuelta = "Tiempo Vuelta: ";
    // Serial.print("  T vuelta: ");
    // Serial.print(hours);
    // Serial.print(":");
    // if (minutes < 10) res = res + "0"; //Serial.print("0");
    sTiempoVuelta = sTiempoVuelta + minutes;
    // Serial.print(minutes);
    // Serial.print("m");
    sTiempoVuelta = sTiempoVuelta + "m";
    if (seconds < 10) sTiempoVuelta = sTiempoVuelta + "0";// Serial.print("0");
    // Serial.print(seconds);
    sTiempoVuelta = sTiempoVuelta + seconds;
    // Serial.print("s. ");
    sTiempoVuelta = sTiempoVuelta + "s";

    String ritmoVuelta = getLapPace(lapTime);
    // lcd.clear();
    // printLCD(0,0,sTiempoVuelta);
    // printLCD(1,0,ritmoVuelta);
    // delay(2000);
    
    Serial.print(sTiempoVuelta);
    Serial.print(". ");
    Serial.println(ritmoVuelta);
    sTextoCompleto = sTiempoVuelta + "\n" + ritmoVuelta + "\n";
  }

  //TOTAL STATISTICS
  unsigned long totalTime = currentTime - startTime;
  hours = totalTime / 3600000;
  minutes = (totalTime % 3600000) / 60000;
  seconds = (totalTime % 60000) / 1000;
  
  // Serial.println("  ---");
  String sTiempoTotal = "TIEMPO TOTAL: ";
  // Serial.print("T TOTAL: ");
  if (hours >0){
    // Serial.print(hours);
    // Serial.print("h");
    sTiempoTotal = sTiempoTotal + hours + "h";
  }
  if (minutes < 10) sTiempoTotal = sTiempoTotal + "0"; //Serial.print("0");
  // Serial.print(minutes);
  // Serial.print("m");
  sTiempoTotal = sTiempoTotal + minutes + "m";
  if (seconds < 10) sTiempoTotal = sTiempoTotal + "0"; //Serial.print("0");
  // Serial.print(seconds);
  // Serial.print("s. ");
  sTiempoTotal = sTiempoTotal + seconds + "s";

  // lcd.clear();
  // printLCD(0,0,sTiempoTotal);  
  Serial.print(sTiempoTotal);
  
  String sDistanciaTotal = getDistance(totalDistance);
  // printLCD(1,0,sDistanciaTotal);
  Serial.print(". ");
  Serial.println(sDistanciaTotal);
  sTextoCompleto = sTextoCompleto + sTiempoTotal + "\n" + sDistanciaTotal;

  LastStateChangeTime = currentTime;
  delay(200);  // Debounce delay

  return sTextoCompleto;
}

bool sendSMS(String textString){
  if (!isNetworkOK){
    return false;
  }
  String response = "";
  bool enviado = false;
  int i = 0;

  while (i<MAX_RETRIES and !enviado) {
    if (i>0){
      Serial.println("Esperamos 4s antes del reintento.");
      delay(4000);
      // Retry sending SMS
      Serial.println("Reintentando SMS...");
    }

    mySIM800.println("AT+CMGF=1"); // Configuring TEXT mode
    updateSerial();
    mySIM800.print("AT+CMGS=\"");
    mySIM800.print(DST_PHONE);
    mySIM800.println("\"");
    updateSerial();

    mySIM800.print(textString); //text content
    updateSerial();

    mySIM800.write(26); // End of text content
    updateSerial();
    delay(4000);
    Serial.println();
    
    // Read the response from the SIM800L
    while (mySIM800.available()) {
      char c = mySIM800.read();
      response += c;
    }

    // Check the response for success or failure
    if (response.indexOf("+CMGS:") != -1) {
      enviado = true;
      Serial.println("SMS enviado correctamente!");
    } else {
      Serial.println("Fallo en el envío SMS.");
      // Check if the SIM is registered on the network
      if (!isNetworkRegistered()) {
        Serial.println("ERROR: La SIM no está registrada en la red GSM.");
        // forceNetworkRegistration();                
      }
    }
    i++;
  }
  return enviado;
}

void handleButtonPressed(){
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!isStartTimeSet) { //Carrera aún no iniciada
      startTime = millis();
      LastStateChangeTime = startTime;
      isStartTimeSet = true;

      String startString = "Carrera INICIADA";

      if (isNetworkOK){
        String horaInicio = getNetworktime();
        startString = startString + " a las " + String(horaInicio);
      }
      startString = startString + "!";
      Serial.println(startString);      
      
      // lcd.clear();      
      // printLCD(0, 0, "Carrera iniciada");
      // printScreen(startString);
      
      if (isNetworkOK) sendSMS(startString);      
       
    } else { // Si se pulsa el botón cuando la carrera está iniciada, entonces la finalizamos
      unsigned long endTime = millis();
      String endString = "Carrera FINALIZADA";

      if (isNetworkOK){
        String horaFin = getNetworktime();
        endString = endString + " a las " + String(horaFin);
      }
      endString = endString + "!";

      Serial.println("===============================================");
      Serial.println(endString);
      String smsText = endString + "\n";
      String raceInfo = getLaptimeAndTotaltime(false);
      smsText = smsText + raceInfo;
      Serial.println("===============================================");
      // printScreen(smsText);

      if (isNetworkOK) sendSMS(smsText);      

      //Reset variables for a new race
      isStartTimeSet = false;
      LastStateChangeTime = 0;
      startTime = 0;
      totalDistance = 0;
    }
    delay(300);  // Debounce delay
  }
}

void handleRFID(){
  // Revisamos si hay nuevas tarjetas  presentes
	if ( mfrc522.PICC_IsNewCardPresent() ) 
  {  
    //Comprobamos qué tarjeta es
    if ( mfrc522.PICC_ReadCardSerial()) 
    {      
      String resString = "";
      String sVuelta = "";
      if (!isStartTimeSet){
        resString = "Carrera NO iniciada";
        Serial.println(resString);
        // printLCD(0, 0, resString);
        // printScreen(resString);
      } else {
        //comparamos los UID para determinar si es uno de nuestros atletas       
        // if(compareArray(ActualUID,Atleta1))
        if (memcmp(mfrc522.uid.uidByte, Atleta1, 4) == 0) 
          resString = "ATLETA 1";
        else if(memcmp(mfrc522.uid.uidByte, Atleta2, 4) == 0)
          resString = "ATLETA 2";
        else {
          Serial.println("Atleta desconocido. Lo ignoramos.");
          // Terminamos la lectura de la tarjeta  actual
          mfrc522.PICC_HaltA(); 
          return;
        }

        if (LAP_LENGTH > 0) {
          totalDistance = totalDistance + LAP_LENGTH;
          int n_vuelta = totalDistance / LAP_LENGTH;
          sVuelta = "Vuelta " + String(n_vuelta);
        } else {
          sVuelta = "Vuelta ?";
        }

        // lcd.clear();
        // printLCD(0, 0, resString);                    
        // printLCD(1, 0, sVuelta);                    
        // delay(2000);

        String smsText = resString + " - " + sVuelta;
        Serial.println(smsText);
        String raceInfo = getLaptimeAndTotaltime(true);
        smsText = smsText + "\n" + raceInfo;
        sendSMS(smsText);
        // printScreen(smsText);
      }
      
      // Terminamos la lectura de la tarjeta  actual
      mfrc522.PICC_HaltA();         
    }      
	}	  
}

// void printScreen(String texto) {
//   const int LINE_HEIGHT = 12;
//   const int cursor = 0;

//   const char* input = texto.c_str();

//   // Buffer to hold each line (maximum 10 lines of 64 characters each)
//   char lines[10][64];
//   int lineCount = 0;

//   // Create a mutable copy of the input string
//   char temp[strlen(input) + 1];
//   strcpy(temp, input);

//   // Split the input string by "\n" and store lines
//   char* token = strtok(temp, "\n");
//   while (token != nullptr && lineCount < 10) {
//     strncpy(lines[lineCount], token, 63);
//     lines[lineCount][63] = '\0';  // Ensure null termination
//     token = strtok(nullptr, "\n");
//     lineCount++;
//   }

//   // Serial.print("--- ");
//   // Serial.print(String(lineCount));
//   // Serial.print(" lines found ");
//   // Serial.println("---");
  
//   // Start page-based rendering
//   u8g2.firstPage();  
//   do {   
//     int y = LINE_HEIGHT; 
//     for (int i = 0; i < lineCount; i++) {
//       // u8g2_prepare();
//       u8g2.setFont(u8g2_font_6x10_tf);  // Set font
//       u8g2.drawUTF8(cursor, y, lines[i]);
      
//       y += LINE_HEIGHT;
//       // Stop if the y-coordinate exceeds the screen height
//       if (y > u8g2.getDisplayHeight()) {
//         break;
//       }
      
//     }
//   } while( u8g2.nextPage() );  
  
//   return;  
// }

// void clearScreen() {
//   u8g2.firstPage();  // Start the page-based rendering
//   do {
//     // Do nothing to draw an empty screen
//   } while (u8g2.nextPage());  // Continue until all pages are cleared
// }

void setup() {
  //Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(9600);
  Serial.println("---");

  //Begin serial communication between Arduino and SIM800
  mySIM800.begin(9600);
  
  Serial.println("Inicializando SIM800...");
  delay(1000);

  isNetworkOK = InitializeSIM800();
  if (!isNetworkOK) Serial.println("Continuamos sin GSM.");
  else Serial.println("SIM800 Inicializado.");

  // Initialize the OLED display
  // u8g2.begin();
  // clearScreen();
  // Serial.println("Display inicializado.");
  // delay(1000);

  SPI.begin();        //Iniciamos el Bus SPI
	mfrc522.PCD_Init(); // Iniciamos  el MFRC522
  delay(4);
  Serial.println("RFID inicializado.");

  pinMode(BUTTON_PIN, INPUT_PULLUP); //Inicializamos el botón
    
  // printScreen("TODO LISTO\nPulse el botón para\ncomenzar la carrera.");
  Serial.println("==============================================================");
  Serial.println("Todo listo. Pulse el botón para comenzar el reloj de carrera.");
  // clearScreen();
}

void loop() {
  handleButtonPressed();

  handleRFID();  
}
