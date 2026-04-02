/* YourDuinoStarter_SerialMonitor_SEND_RCVE<br> - WHAT IT DOES: 
   - Receives characters from Serial Monitor
   - Displays received character as Decimal, Hexadecimal and Character
   - Controls pin 13 LED from Keyboard
 - CONNECTIONS:
   - None: Pin 13 built-in LED
*/

/*-----( Declare Variables )-----*/
int ByteReceived;

/****** SETUP: RUNS ONCE ******/
void setup()   
{
  Serial.begin(9600);  //setup serial monitor
  // print heading to serial monitor
  Serial.println("--- Start Serial Monitor SEND_RCVE ---");
  Serial.println("Type 1 to turn on LED_BUILTIN");
  Serial.println("Type 9 to turn off LED_BUILTIN");  
  Serial.println(); 
  pinMode(LED_BUILTIN, OUTPUT);
}
//--(end setup )---

/****** LOOP: RUNS CONSTANTLY ******/
void loop()  
{
  if (Serial.available() > 0) //check if serial buffer had value
  {
    ByteReceived = Serial.read(); // read byte from serial
    Serial.print(ByteReceived);   //print byte ASCII Code
    Serial.print("        ");      //pring a "tab" space
    Serial.print(ByteReceived, HEX); //print byte ASCII Hex
    Serial.print("       ");     //pring a "tab" space
    Serial.print(char(ByteReceived)); //print charicter
    
    if(ByteReceived == '1') // Single Quote! This is a character.
    {
      digitalWrite(LED_BUILTIN,HIGH); //LED on
      Serial.print(" LED ON ");
    }
    
    if(ByteReceived == '0')
    {
      digitalWrite(LED_BUILTIN,LOW); //LED off
      Serial.print(" LED OFF");
    }
    
    Serial.println();    // End the line

  // END Serial Available
  }
}

//--(end main loop )---

/*********( THE END )***********/