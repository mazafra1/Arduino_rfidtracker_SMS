#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <SoftwareSerial.h>
/**********************************************************/
LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display
/*********************************************************/
SoftwareSerial mySIM800(2, 3); //SIM800 TX & RX is connected to Arduino #2 & #3

#define BUTTON_PIN 8
#define REDPIN 7
#define GREENPIN 6

#define RST_PIN	9    //Pin 9 para el reset del RC522
#define SS_PIN	10   //Pin 10 para el SS (SDA) del RC522
MFRC522 mfrc522(SS_PIN, RST_PIN); //Creamos el objeto para el RC522

byte ActualUID[4]; //almacenará el código del Tag leído
byte Atleta1[4]= {0x73, 0x1D, 0xF2, 0xF6}; //código del atleta 1
byte Atleta2[4]= {0x63, 0x8B, 0x67, 0xEE}; //código del atleta 2

const int RETRY = 1; // Number of times to retry when a SMS is not successful
const String DST_PHONE = "+ZZxxxxxxxxx"; //Use +ZZ with country code and xxxxxxxxxxx with phone number to sms
const int LAP_LENGTH = 400; // Use 0 for unknown lap distance.
unsigned long LastStateChangeTime = 0;
unsigned long startTime = 0;
bool isStartTimeSet = false;
unsigned long totalDistance = 0;
bool isNetworkOK = false;

//Función para comparar dos vectores
 boolean compareArray(byte array1[],byte array2[])
{
  if(array1[0] != array2[0])return(false);
  if(array1[1] != array2[1])return(false);
  if(array1[2] != array2[2])return(false);
  if(array1[3] != array2[3])return(false);
  return(true);
}

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
  String res = "DISTANCIA TOTAL: " + String(kilometers) + "," + String(meters) + " km";

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

// Linea is 0 or 1. Cursor is between 0 and 15.
void printLCD(int linea, int cursor, String texto){
  lcd.setCursor(cursor, linea); // set the cursor to column 3, line 0
  lcd.print(texto);  // Print a message to the LCD
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

// Function to force network registration
void forceNetworkRegistration() {
  Serial.println("Intentando registrar en red GSM...");
  mySIM800.println("AT+COPS=0"); // Automatically select operator
  // updateSerial();
  delay(1000);                  // Wait for network registration attempt
}

String printLaptimeAndTotaltime()
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
  String sTiempoVuelta = "Tiempo Vuelta: ";  
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
  lcd.clear();
  printLCD(0,0,sTiempoTotal);
  Serial.print(sTiempoTotal);
  
  String sDistanciaTotal = getDistance(totalDistance);
  printLCD(1,0,sDistanciaTotal);
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

  while (i<RETRY and !enviado) {
    mySIM800.println("AT+CMGF=1"); // Configuring TEXT mode
    updateSerial();
    mySIM800.print("AT+CMGS=\"");
    mySIM800.print(DST_PHONE);
    mySIM800.println("\"");
    updateSerial();

    mySIM800.print(textString); //text content
    updateSerial();

    mySIM800.write(26);
    updateSerial();
    delay(4000);
    
    // Read the response from the SIM800L
    while (mySIM800.available()) {
      char c = mySIM800.read();
      response += c;
    }

    // Check the response for success or failure
    if (response.indexOf("+CMGS:") != -1) {
      enviado = true;
      Serial.println("SMS sent successfully!");
      return true;
    } else {
      Serial.println("Failed to send SMS.");
      // Check if the SIM is registered on the network
      if (!isNetworkRegistered()) {
        Serial.println("La SIM no está registrada en la red GSM.");
        forceNetworkRegistration();

        // Retry sending SMS after attempting to register
        Serial.println("Reintentando SMS...");
        i++;
      }
    }
  }
  return enviado;
}

void setup() {
	Serial.begin(9600); //Iniciamos la comunicación  serial
	SPI.begin();        //Iniciamos el Bus SPI
	mfrc522.PCD_Init(); // Iniciamos  el MFRC522
  delay(4);

  //Begin serial communication with Arduino and SIM800
  mySIM800.begin(9600);
  Serial.println("Inicializando SIM800...");
  delay(1000);

  mySIM800.println("AT"); //Once the handshake test is successful, it will back to OK
  updateSerial();
  mySIM800.println("AT+CSQ"); //Signal quality test, value range is 0-31 , 31 is the best
  updateSerial();
  mySIM800.println("AT+CCID"); //Read SIM information to confirm whether the SIM is plugged
  updateSerial();
  // mySIM800.println("AT+CREG?"); //Check whether it has registered in the network
  // updateSerial();
  mySIM800.println("AT+CCLK?"); //Check network time
  updateSerial();
  isNetworkOK = isNetworkRegistered();
  if (!isNetworkOK) {
    Serial.println("ERROR. La SIM no está conectada a la red GSM.");
    forceNetworkRegistration();
    isNetworkOK = isNetworkRegistered();
    if (!isNetworkOK) Serial.println("Continuamos sin GSM.");
  }
    
  pinMode(BUTTON_PIN, INPUT_PULLUP); //Inicializamos el botón

  digitalWrite(GREENPIN, HIGH);
  digitalWrite(REDPIN, HIGH);
  delay(500);
  digitalWrite(GREENPIN, LOW);
  digitalWrite(REDPIN, LOW);

  lcd.init();  //initialize the lcd
  lcd.backlight();  //open the backlight

  printLCD(0,0, "Pulse el boton");
  printLCD(1,0, "para comenzar");
  delay(200);

  Serial.println("===============================================");
  Serial.println("Todo listo. Pulse el botón para comenzar el reloj de carrera.");
}

void loop() {

  // updateSerial();
  
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!isStartTimeSet) {
      startTime = millis();
      LastStateChangeTime = startTime;
      isStartTimeSet = true;

      String startString = "Carrera iniciada";

      if (isNetworkOK){
        String horaInicio = getNetworktime();
        startString = startString + " a las " + String(horaInicio) + ".";
      }
      Serial.println(startString);
      if (isNetworkOK) sendSMS(startString);
      
      // lcd.clear();      
      // printLCD(0, 0, "Carrera iniciada");
       
    } else { // Si se pulsa el botón cuando la carrera está iniciada, enteonces la finalizamos
      unsigned long endTime = millis();
      String endString = "Carrera finalizada";

      if (isNetworkOK){
        String horaFin = getNetworktime();
        endString = endString + " a las " + String(horaFin) + ".";
      }
      
      Serial.println(endString);
      String smsText = endString + "\n";
      String raceInfo = printLaptimeAndTotaltime();
      smsText = smsText + raceInfo;
      if (isNetworkOK) sendSMS(smsText);
            
      isStartTimeSet = false;
      LastStateChangeTime = 0;
      startTime = 0;
      totalDistance = 0;
    }
    delay(200);  // Debounce delay
  }
  
	// Revisamos si hay nuevas tarjetas  presentes
	if ( mfrc522.PICC_IsNewCardPresent() ) 
  {  
    //Seleccionamos una tarjeta
    if ( mfrc522.PICC_ReadCardSerial()) 
    {
      //Encendemos LED rojo para confirmación visual de la lectura
      digitalWrite(REDPIN, HIGH);
      delay(500);      
      digitalWrite(REDPIN, LOW);
      // Enviamos serialemente su UID
      // Serial.print("(Card UID:");
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        // Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
        // Serial.print(mfrc522.uid.uidByte[i], HEX);
        ActualUID[i]=mfrc522.uid.uidByte[i];
      } 
      //comparamos los UID para determinar si es uno de nuestros atletas
      String resString = "";
      if(compareArray(ActualUID,Atleta1))
        resString = "ATLETA 1";
      else if(compareArray(ActualUID,Atleta2))
        resString = "ATLETA 2";
      else
        resString = "Atleta desc.";
      
      String sVuelta = "";
      if (!isStartTimeSet){
        resString = "Carrera NO iniciada";
        Serial.println(resString);
        // printLCD(0, 0, resString);
      } else {
        if (LAP_LENGTH > 0) {
          totalDistance = totalDistance + LAP_LENGTH;
          int n_vuelta = totalDistance / LAP_LENGTH;
          sVuelta = "Vuelta " + String(n_vuelta);
        } else {
          sVuelta = "Vuelta ?";
        }

        // lcd.clear();
        // printLCD(0, 0, resString);                    
        Serial.print(resString);
        
        // Serial.print(" Vuelta ");
        // Serial.print(totalDistance / LAP_LENGTH);
        // Serial.println(":");
        Serial.print(" - ");
        Serial.println(sVuelta);
        // printLCD(1, 0, sVuelta);                    
        // delay(2000);

        String smsText = resString + " - " + sVuelta + "\n";
        String raceInfo = printLaptimeAndTotaltime();
        smsText = smsText + raceInfo;
        sendSMS(smsText);
      }
      
      // Terminamos la lectura de la tarjeta  actual
      mfrc522.PICC_HaltA();         
    }      
	}	
}