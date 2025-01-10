#include <SPI.h>
#include <MFRC522.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
/**********************************************************/
LiquidCrystal_I2C lcd(0x27, 16, 2); // set the LCD address to 0x27 for a 16 chars and 2 line display
/*********************************************************/

#define STATE_STARTUP 0
#define STATE_WAITING 1

#define BUTTON_PIN 8
#define REDPIN 7
#define GREENPIN 6

#define RST_PIN	9    //Pin 9 para el reset del RC522
#define SS_PIN	10   //Pin 10 para el SS (SDA) del RC522
MFRC522 mfrc522(SS_PIN, RST_PIN); //Creamos el objeto para el RC522

byte ActualUID[4]; //almacenará el código del Tag leído
byte Atleta1[4]= {0x73, 0x1D, 0xF2, 0xF6}; //código del atleta 1
byte Atleta2[4]= {0x63, 0x8B, 0x67, 0xEE}; //código del atleta 2

const int LAP_LENGTH = 400; // 400 metros
byte currentState = STATE_STARTUP;
unsigned long LastStateChangeTime = 0;
unsigned long startTime = 0;
bool isStartTimeSet = false;
unsigned long totalDistance = 0;

//Función para comparar dos vectores
 boolean compareArray(byte array1[],byte array2[])
{
  if(array1[0] != array2[0])return(false);
  if(array1[1] != array2[1])return(false);
  if(array1[2] != array2[2])return(false);
  if(array1[3] != array2[3])return(false);
  return(true);
}

void setup() {
	Serial.begin(9600); //Iniciamos la comunicación  serial
	SPI.begin();        //Iniciamos el Bus SPI
	mfrc522.PCD_Init(); // Iniciamos  el MFRC522
  delay(4);
  
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

  Serial.println("===============================================");
  Serial.println("Todo listo. Pulse el botón para comenzar el reloj de carrera.");
}

//Escribe la distancia total recorrida
String getDistance(unsigned long distanceInMeters) {
  unsigned long kilometers = distanceInMeters / 1000;
  unsigned long meters = distanceInMeters % 1000;

  // Serial.print("DISTANCIA: ");
  // Serial.print(kilometers);
  // Serial.print(",");
  // Serial.print(meters);
  // Serial.println(" km");
  String res = "DIST: " + String(kilometers) + "," + String(meters) + " km";

  return res;
}

// Lee los milisegundos pasados y los convierte en min/km para una vuelta con longitud LAP_LENGTH (400m)
String getLapPace(unsigned long msec){
  String res = "";
  // Serial.println(msec);

  unsigned long msecPerKm = msec * 2.5; // 1000/400 = 2.5
  unsigned long hours = msecPerKm / 3600000;
  unsigned long minutes = (msecPerKm % 3600000) / 60000;
  unsigned long seconds = (msecPerKm % 60000) / 1000;

  res = "Ritmo ";
  // Serial.print("R vuelta: ");
  // Serial.print(hours);
  // Serial.print(":");
  // if (minutes < 10) Serial.print("0");
  res = res + minutes + ":";
  // Serial.print(minutes);
  // Serial.print(":");
  if (seconds < 10) res = res + "0";//Serial.print("0");
  res = res + seconds + " m/km";
  // Serial.print(seconds);
  // Serial.println(" min/km");

  return res;
}

// Linea is 0 or 1. Cursor is between 0 and 15.
void printLCD(int linea, int cursor, String texto){
  lcd.setCursor(cursor, linea); // set the cursor to column 3, line 0
  lcd.print(texto);  // Print a message to the LCD
}

void printLaptimeAndTotaltime()
{  
  if (!isStartTimeSet) {
    return;
  }

  unsigned long currentTime = millis();
  
  unsigned long lapTime = currentTime - LastStateChangeTime;

  unsigned long hours = lapTime / 3600000;
  unsigned long minutes = (lapTime % 3600000) / 60000;
  unsigned long seconds = (lapTime % 60000) / 1000;
  String sTiempo = "Tiempo V ";  
  // Serial.print("  T vuelta: ");
  // Serial.print(hours);
  // Serial.print(":");
  // if (minutes < 10) res = res + "0"; //Serial.print("0");
  sTiempo = sTiempo + minutes;
  // Serial.print(minutes);
  // Serial.print("m");
  sTiempo = sTiempo + "m";
  if (seconds < 10) sTiempo = sTiempo + "0";// Serial.print("0");
  // Serial.print(seconds);
  sTiempo = sTiempo + seconds;
  // Serial.print("s. ");
  sTiempo = sTiempo + "s";

  String ritmoVuelta = getLapPace(lapTime);
  lcd.clear();
  printLCD(0,0,sTiempo);
  printLCD(1,0,ritmoVuelta);
  Serial.print(sTiempo);
  Serial.print(". ");
  Serial.println(ritmoVuelta);
  delay(2000);

  unsigned long totalTime = currentTime - startTime;
  hours = totalTime / 3600000;
  minutes = (totalTime % 3600000) / 60000;
  seconds = (totalTime % 60000) / 1000;
  
  // Serial.println("  ---");
  String sTotal = "T TOTAL: ";
  // Serial.print("T TOTAL: ");
  if (hours >0){
    // Serial.print(hours);
    // Serial.print("h");
    sTotal = sTotal + hours + "h";
  }
  if (minutes < 10) sTotal = sTotal + "0"; //Serial.print("0");
  // Serial.print(minutes);
  // Serial.print("m");
  sTotal = sTotal + minutes + "m";
  if (seconds < 10) sTotal = sTotal + "0"; //Serial.print("0");
  // Serial.print(seconds);
  // Serial.print("s. ");
  sTotal = sTotal + seconds + "s";
  lcd.clear();
  printLCD(0,0,sTotal);
  Serial.print(sTotal);

  String sDistancia = getDistance(totalDistance);
  printLCD(1,0,sDistancia);
  Serial.print(". ");
  Serial.println(sDistancia);

  LastStateChangeTime = currentTime;
  delay(200);  // Debounce delay
}

void loop() {
  
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (!isStartTimeSet) {
      startTime = millis();
      LastStateChangeTime = startTime;
      isStartTimeSet = true;

      // Serial.println();
      Serial.println("¡Carrera iniciada!");
      lcd.clear();
      printLCD(0, 0, "Carrera iniciada");
      delay(100);  // Debounce delay
    }
  }
  
	// Revisamos si hay nuevas tarjetas  presentes
	if ( mfrc522.PICC_IsNewCardPresent() ) 
        {  
  		//Seleccionamos una tarjeta
            if ( mfrc522.PICC_ReadCardSerial()) 
            {
                  // Enviamos serialemente su UID
                  // Serial.print("(Card UID:");
                  for (byte i = 0; i < mfrc522.uid.size; i++) {
                          // Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
                          // Serial.print(mfrc522.uid.uidByte[i], HEX);
                          ActualUID[i]=mfrc522.uid.uidByte[i];
                  } 
                  // Serial.print(") ");
                  //comparamos los UID para determinar si es uno de nuestros atletas
                  String resString = "";
                  if(compareArray(ActualUID,Atleta1))
                    resString = "ATLETA 1";
                    // Serial.print("ATLETA 1 - ");
                  else if(compareArray(ActualUID,Atleta2))
                    resString = "ATLETA 2";
                    // Serial.print("ATLETA 2 - ");
                  else
                    resString = "Atleta desc.";
                    // Serial.print("Atleta desconocido. ");
                  
                  if (!isStartTimeSet){
                    resString = "Carrera no iniciada";
                    Serial.println("Carrera no iniciada.");
                    printLCD(0, 0, resString);
                  } else {
                    totalDistance = totalDistance + LAP_LENGTH;
                    int n_vuelta = totalDistance / LAP_LENGTH;
                    String sVuelta = "Vuelta " + String(n_vuelta);

                    lcd.clear();
                    printLCD(0, 0, resString);                    
                    Serial.print(resString);
                    
                    // Serial.print(" Vuelta ");
                    // Serial.print(totalDistance / LAP_LENGTH);
                    // Serial.println(":");
                    Serial.print(". ");
                    Serial.println(sVuelta);
                    printLCD(1, 0, sVuelta);
                    
                    delay(2000);
                    printLaptimeAndTotaltime();
                  }
                  
                  // Terminamos la lectura de la tarjeta  actual
                  mfrc522.PICC_HaltA();         
            }      
	}	
}