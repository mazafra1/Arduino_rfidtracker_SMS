#include <SoftwareSerial.h>

// Weird enough, the pin PWX on the SIM800C needs to be hooked up to a GND to work.
// Some interesting commads:
// AT -> OK
// ATI -> Model and firmware version
// AT+CSQ -> Check signal strength (0-31). It should be at least >5 (higher is better).
// AT+CREG? -> Check if registered on the network
// AT+COPS? -> Connected to the network?
// AT+COPS=? -> Lists all networks
// AT+CBC -> Voltage and current
// AT+CMGF=1 -> Switch to text mode (valid only for the next command)
// AT+CMGL="ALL" -> List all messages
// AT+CMGD=1 -> Delete message number 1
// AT+CMGS="+34666111222" -> Destination tel number. This will bring the serial monitor to the write message status. 
// Once the message is written, a Ctrl+C character is needed (Alt+026).
// AT+CBAND? -> Checks which bands are enabled.

//Create software serial object to communicate with SIM800C
SoftwareSerial mySIM800(2, 3); //SIM800C TX & RX is connected to Arduino #2 & #3

#define BUTTON_PIN 8

void sendSMS(String textString){
  mySIM800.println("AT+CMGF=1"); // Configuring TEXT mode
  updateSerial();
  mySIM800.println("AT+CMGS=\"+ZZxxxxxxxxx\"");//change ZZ with country code and xxxxxxxxxxx with phone number to sms
  updateSerial();
  mySIM800.print(textString); //text content
  updateSerial();
  mySIM800.write(26);
  Serial.println();
}

void setup()
{
  pinMode(BUTTON_PIN, INPUT_PULLUP); //Inicializamos el botón
  //Begin serial communication with Arduino and Arduino IDE (Serial Monitor)
  Serial.begin(9600);
    
  //Begin serial communication with Arduino and SIM800C
  mySIM800.begin(9600);

  Serial.println("Initializing...");
  delay(1000);

  mySIM800.println("AT"); //Once the handshake test is successful, it will back to OK
  updateSerial();
  mySIM800.println("AT+CSQ"); //Signal quality test, value range is 0-31 , 31 is the best
  updateSerial();
  mySIM800.println("AT+CCID"); //Read SIM information to confirm whether the SIM is plugged
  updateSerial();
  mySIM800.println("AT+CREG?"); //Check whether it has registered in the network
  updateSerial();
  // mySIM800.println("AT+CCLK?"); //Check whether it has registered in the network
  // updateSerial();
  mySIM800.println("AT+CMGF=1"); // Configuring TEXT mode
  updateSerial();
  mySIM800.println("AT+CNMI=1,2,0,0,0"); // Decides how newly arrived SMS messages should be handled
  updateSerial();
}

void loop()
{
  updateSerial();

  if (digitalRead(BUTTON_PIN) == LOW) {
    Serial.println("Button pressed. Sending out SMS.");
    sendSMS("TEST");
    delay(1000);
  }
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

