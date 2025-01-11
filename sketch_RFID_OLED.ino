#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <SoftwareSerial.h>
#include <U8g2lib.h>

/**********************************************************/
// Initialize display with minimal memory mode
U8G2_SH1106_128X64_NONAME_1_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);
/*********************************************************/

SoftwareSerial mySIM800(2, 3); // SIM800 TX & RX connected to Arduino #2 & #3

#define BUTTON_PIN 8
//#define REDPIN 7
//#define GREENPIN 6

#define RST_PIN 9 // RC522 reset pin
#define SS_PIN 10 // RC522 SS (SDA) pin
MFRC522 mfrc522(SS_PIN, RST_PIN); // Create instance for RC522

byte ActualUID[4]; // Stores the Tag UID
const byte Atleta1[4] = {0x73, 0x1D, 0xF2, 0xF6}; // UID for athlete 1
const byte Atleta2[4] = {0x63, 0x8B, 0x67, 0xEE}; // UID for athlete 2

const int LAP_LENGTH = 400; // Use 0 for unknown lap distance.

// Function prototypes
void setupPins();
void initializeModules();
void handleRFID();
void handleDisplay(String texto);
void handleButtonPress();
String printLaptimeAndTotaltime(bool includeLapInfo);

unsigned long LastStateChangeTime = 0;
unsigned long startTime = 0;
bool isStartTimeSet = false;
unsigned long totalDistance = 0;

void setup() {
  setupPins();
  initializeModules();
  handleDisplay("TODO LISTO\nPulse el botón para\ncomenzar la carrera.");
}

void loop() {
  handleRFID();
  handleButtonPress();
}

void setupPins() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  // pinMode(REDPIN, OUTPUT);
  // pinMode(GREENPIN, OUTPUT);
  // digitalWrite(REDPIN, LOW);
  // digitalWrite(GREENPIN, LOW);
}

void initializeModules() {
  SPI.begin();
  mfrc522.PCD_Init();
  mySIM800.begin(9600);
  u8g2.begin();
  u8g2.setFont(u8g2_font_ncenB08_tr); // Use a memory-efficient font
}

void handleRFID() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) {
    return;
  }
  String resString = "";
  // Compare read UID with predefined UIDs
  if (memcmp(mfrc522.uid.uidByte, Atleta1, 4) == 0) {
    resString = "ATLETA 1";
    // handleDisplay("Athlete 1 Detected\nReady");    
    // digitalWrite(GREENPIN, HIGH);
    // delay(1000);
    // digitalWrite(GREENPIN, LOW);
  } else {
    handleDisplay("ATLETA Descnocido");
    // digitalWrite(REDPIN, HIGH);
    // delay(1000);
    // digitalWrite(REDPIN, LOW);
  }
  
  if (!isStartTimeSet){
    resString = "Carrera NO iniciada";    
    // printLCD(0, 0, resString);
    handleDisplay(resString);
  } else {
    String sVuelta = "";
    if (LAP_LENGTH > 0) {
      totalDistance = totalDistance + LAP_LENGTH;
      int n_vuelta = totalDistance / LAP_LENGTH;
      sVuelta = "Vuelta " + String(n_vuelta);
    } else {
      sVuelta = "Vuelta ?";
    }

    String smsText = resString + " - " + sVuelta + "\n";
    String raceInfo = printLaptimeAndTotaltime(true);
    smsText = smsText + raceInfo;
    // sendSMS(smsText);
    handleDisplay(smsText);
  }

  mfrc522.PICC_HaltA();
}

void handleDisplay(String texto){
  const char* message = texto.c_str();
  u8g2.firstPage();
  do {
    int y = 10; // Initial Y position
    const char* line = message;
    const char* nextLine;
    while ((nextLine = strchr(line, '\n')) != NULL) {
      u8g2.drawUTF8(0, y, String(line).substring(0, nextLine - line).c_str());
      y += 10; // Move to the next line
      line = nextLine + 1;
    }
    u8g2.drawStr(0, y, line); // Draw the last line
  } while (u8g2.nextPage());
}

void handleButtonPress() {
  if (digitalRead(BUTTON_PIN) == LOW) { // Adjusted for INPUT_PULLUP logic
    if (!isStartTimeSet) {
      startTime = millis();
      LastStateChangeTime = startTime;
      isStartTimeSet = true;
      handleDisplay("¡Carrera INICIADA!");      
    } else { // Si se pulsa el botón cuando la carrera está iniciada, entonces la finalizamos
      unsigned long endTime = millis();
      String endString = "Carrera FINALIZADA";

      // if (isNetworkOK){
      //   String horaFin = getNetworktime();
      //   endString = endString + " a las " + String(horaFin);
      // }
      endString = endString + "!";

      String smsText = endString + "\n";
      String raceInfo = printLaptimeAndTotaltime(false);
      smsText = smsText + "\n" + raceInfo;      
      handleDisplay(smsText.c_str());

      // if (isNetworkOK) sendSMS(smsText);
      

      //Reset variables for a new race
      isStartTimeSet = false;
      LastStateChangeTime = 0;
      startTime = 0;
      totalDistance = 0;
    }
    delay(200);  // Debounce delay
  }
}

String printLaptimeAndTotaltime(bool includeLapInfo)
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