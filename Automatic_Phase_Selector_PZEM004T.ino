#include <PZEM004Tv30.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

#if !defined(PZEM_RX_PIN) && !defined(PZEM_TX_PIN)
#define PZEM_RX_PIN 16
#define PZEM_TX_PIN 17
#endif

#if !defined(PZEM_SERIAL)
#define PZEM_SERIAL Serial2
#endif

#define NUM_PZEMS 3

PZEM004Tv30 pzems[NUM_PZEMS];

#if defined(USE_SOFTWARE_SERIAL) && defined(ESP32)
#error "Can not use SoftwareSerial with ESP32"
#elif defined(USE_SOFTWARE_SERIAL)

SoftwareSerial pzemSWSerial(PZEM_RX_PIN, PZEM_TX_PIN);
#endif


long Sensorloopint = 300;
long Phaseloopint = 400;
unsigned long Sensorloop = 0;
unsigned long R_Loop = 0;
unsigned long Y_Loop = 0;
unsigned long B_Loop = 0;

unsigned long RVK_Ondelay_milli = 0;
unsigned long YVK_Ondelay_milli = 0;
unsigned long BVK_Ondelay_milli = 0;

const int RVK = 25;
const int YVK = 26;
const int BVK = 27;
const int UV_POT_PIN = 36;
const int OV_POT_PIN = 39;
const int VT_POT_PIN = 34;
const int BYP_PIN = 32;
const int KBYP = 33;
const int OK_LED_PIN = 15;
const int FAULT_LED_PIN = 4;
const int BUZZER_PIN = 2;
const int Fan = 12;
const int OLEDButtonPin = 23;  
#define DHTPIN 13 // DHT22 sensor pin

bool BYPState = HIGH;
bool KBYPState = LOW;
bool BuzzerState = LOW;

int UnderVolt = 0;
int OverVolt= 0;
int VTD = 0;

int RvoltS = 0;
int YvoltS = 0;
int BvoltS = 0;
int Rvolt = 0;
int Yvolt = 0;
int Bvolt = 0;

unsigned long LastButtonPressTime = 0;
unsigned long DebounceDelay = 50; // Debounce time in milliseconds
int CurrentPage = 1;

bool faultLedState = false;
unsigned long lastFaultLedToggleTime = 0;
unsigned long lastBuzzerToggleTime = 0;

unsigned long Templooptime = 0;
long Temploopint = 2000;
int MCUTemp;
int temperature;

//ESP Core Temp
#ifdef __cplusplus
extern "C" {
#endif

uint8_t temprature_sens_read();

#ifdef __cplusplus
}
#endif

uint8_t temprature_sens_read();

#define DHTTYPE DHT22 // DHT22 sensor type
DHT dht(DHTPIN, DHTTYPE);
bool FanState = LOW;

void setup()
{
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  dht.begin();

  // For each PZEM, initialize it
  for (int i = 0; i < NUM_PZEMS; i++) {

#if defined(USE_SOFTWARE_SERIAL)
    // Initialize the PZEMs with Software Serial
    pzems[i] = PZEM004Tv30(pzemSWSerial, 0x10 + i);
#elif defined(ESP32)
    // Initialize the PZEMs with Hardware Serial2 on RX/TX pins 16 and 17
    pzems[i] = PZEM004Tv30(PZEM_SERIAL, PZEM_RX_PIN, PZEM_TX_PIN, 0x10 + i);
#else
    // Initialize the PZEMs with Hardware Serial2 on the default pins

    /* Hardware Serial2 is only available on certain boards.
       For example the Arduino MEGA 2560
    */
    pzems[i] = PZEM004Tv30(PZEM_SERIAL, 0x10 + i);
#endif
  }
  
  pinMode(UV_POT_PIN, INPUT);
  pinMode(OV_POT_PIN, INPUT);
  pinMode(VT_POT_PIN, INPUT);
  pinMode(OLEDButtonPin, INPUT_PULLUP);
  pinMode(BYP_PIN, INPUT_PULLUP);
  pinMode(OK_LED_PIN, OUTPUT);
  pinMode(FAULT_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(RVK, OUTPUT);
  pinMode(YVK, OUTPUT);
  pinMode(BVK, OUTPUT);

  // Clear the buffer
  display.clearDisplay();
  // Display Text
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 28);
  display.println("Hello... Hashva!");
  display.display();
  delay(1500);
  display.clearDisplay();
}
void loop()
{
  unsigned long CurrentTime = millis();

  if (CurrentTime - Sensorloop > Sensorloopint)
  {
    Sensorloop = CurrentTime;

    RvoltS = pzems[0].voltage();
    YvoltS = pzems[1].voltage();
    BvoltS = pzems[2].voltage();

    if (isnan(RvoltS))
    {
      Rvolt = 0;
    }
    else
    {
      Rvolt = RvoltS;
    }

    if (isnan(YvoltS))
    {
      Yvolt = 0;
    }
    else
    {
      Yvolt = YvoltS;
    }

    if (isnan(BvoltS))
    {
      Bvolt = 0;
    }
    else
    {
      Bvolt = BvoltS;
    }
    
    int UVPOT = analogRead(UV_POT_PIN);
    UnderVolt = map(UVPOT, 0, 4095, 80, 200); // Map lower threshold potentiometer value to threshold range

    int OVPOT = analogRead(OV_POT_PIN);
    OverVolt = map(OVPOT, 0, 4095, 240, 290); // Map upper threshold potentiometer value to threshold range

    int VTPOT = analogRead(VT_POT_PIN);
    VTD = map(VTPOT, 0, 4095, 500, 10000); // Map sum of potentiometer values to interval range

    // Bypass Switch
    if (digitalRead(BYP_PIN) == LOW && BYPState == HIGH) {
      digitalWrite(KBYP, HIGH);
      BYPState = LOW;
      KBYPState = HIGH;   
    }
    if (digitalRead(BYP_PIN) == HIGH && BYPState == LOW) {
      digitalWrite(KBYP, LOW);
      BYPState = HIGH;
      KBYPState = LOW;   
    }

    //Buzzer, OK LED and Fault LED initiation
    if (Rvolt > UnderVolt && Rvolt < OverVolt && Yvolt > UnderVolt && Yvolt < OverVolt && Bvolt > UnderVolt && Bvolt < OverVolt) {
      BuzzerState = LOW;
    }

    if (Rvolt < UnderVolt || Rvolt > OverVolt || Yvolt < UnderVolt || Yvolt > OverVolt || Bvolt < UnderVolt || Bvolt > OverVolt || isnan(Rvolt) || isnan(Yvolt) || isnan(Bvolt)) {
      BuzzerState = HIGH;
    }

    if (BuzzerState == HIGH) {
      // If BuzzerState is HIGH, turn off OKLED and start blinking FaultLED every 500ms
      digitalWrite(OK_LED_PIN, LOW);
      if (millis() - lastFaultLedToggleTime >= 200) {
        faultLedState = !faultLedState;
        digitalWrite(FAULT_LED_PIN, faultLedState);
        lastFaultLedToggleTime = millis();
      }
      // Turn on Buzzer for 1 second every 5 seconds
      if ((millis() - lastBuzzerToggleTime) % 5000 < 1000) {
        digitalWrite(BUZZER_PIN, HIGH);
      } else {
        digitalWrite(BUZZER_PIN, LOW);
      }
    } else {
      // If BuzzerState is LOW, turn on OKLED and turn off FaultLED and Buzzer
      digitalWrite(OK_LED_PIN, HIGH);
      digitalWrite(FAULT_LED_PIN, LOW);
      digitalWrite(BUZZER_PIN, LOW);
    }

    switch (CurrentPage) {
      case 1:
        displayPage1();
        break;
      case 2:
        displayPage2();
        break;
      case 3:
        displayPage3();
        break;
    }
  }
}
// R Phase relays turn on/off
if (CurrentTime - R_Loop > Phaseloopint)
{
    R_Loop = CurrentTime;
// R-Phase RVK turn on 
  if ((Rvolt > OverVolt || Rvolt < UnderVolt) && BYPState == HIGH )
  {
    if (RVK_Ondelay_milli == 0) 
    {
    RVK_Ondelay_milli = millis();  
    }
    if (millis() - RVK_Ondelay_milli >= VTD) 
    {
    digitalWrite(RVK, HIGH);
    RVK_Ondelay_milli = 0;
    }
  }
// R-Phase RVK turn off
  if ((Rvolt < OverVolt || Rvolt > UnderVolt) || BYPState == LOW )
  {
    if (RVK_Ondelay_milli == 0) 
    {
    RVK_Ondelay_milli = millis();
    }
    if (millis() - RVK_Ondelay_milli >= VTD) 
    {
    digitalWrite(RVK, LOW);
    RVK_Ondelay_milli = 0;
    }
  }
}
// Y Phase
if (CurrentTime - Y_Loop > Phaseloopint)
{
  Y_Loop = CurrentTime;
  // Y-Phase YVK turn on 
  if ((Yvolt > OverVolt || Yvolt < UnderVolt) && BYPState == HIGH )
  {
    if (YVK_Ondelay_milli == 0) 
    {
    YVK_Ondelay_milli = millis();  
    }
    if (millis() - YVK_Ondelay_milli >= VTD) 
    {
    digitalWrite(YVK, HIGH);
    YVK_Ondelay_milli = 0;
    }
  }
// Y-Phase YVK turn off
  if ((Yvolt < OverVolt || Yvolt > UnderVolt) || BYPState == LOW )
  {
    if (YVK_Ondelay_milli == 0) 
    {
    YVK_Ondelay_milli = millis();
    }
    if (millis() - YVK_Ondelay_milli >= VTD) 
    {
    digitalWrite(YVK, LOW);
    YVK_Ondelay_milli = 0;
    }
  }
}
// B Phase
if (CurrentTime - B_Loop > Phaseloopint)
{
  B_Loop = CurrentTime;
  if ((Bvolt > OverVolt || Bvolt < UnderVolt) && BYPState == HIGH )
  {
    if (BVK_Ondelay_milli == 0) 
    {
    BVK_Ondelay_milli = millis();  
    }
    if (millis() - BVK_Ondelay_milli >= VTD) 
    {
    digitalWrite(BVK, HIGH);
    BVK_Ondelay_milli = 0;
    }
  }
// B-Phase BVK turn off
  if ((Bvolt < OverVolt || Bvolt > UnderVolt) || BYPState == LOW )
  {
    if (BVK_Ondelay_milli == 0) 
    {
    BVK_Ondelay_milli = millis();
    }
    if (millis() - BVK_Ondelay_milli >= VTD) 
    {
    digitalWrite(BVK, LOW);
    BVK_Ondelay_milli = 0;
    }
  }
}
// Panel fan turn on/off
  if (CurrentTime - Templooptime > Temploopint)
  {
    MCUTemp = (temprature_sens_read() - 32) / 1.8;
    temperature = dht.readTemperature();
    if (isnan(temperature)) {
    } else {
      // Turn fan on or off based on temperature
      if (temperature > 40 || MCUTemp > 60) {
        digitalWrite(Fan, HIGH);
        FanState = HIGH;
      } else {
        digitalWrite(Fan, LOW);
        FanState = LOW;
      }
    }
    Templooptime = CurrentTime;
  }
// OLED Display page change
  // Check for button press with debounce
  if (digitalRead(OLEDButtonPin) == LOW && CurrentTime - LastButtonPressTime >= DebounceDelay) {
    LastButtonPressTime = CurrentTime;
    // Increment page number and wrap around if needed
    CurrentPage++;
    if (CurrentPage > 3) {
      CurrentPage = 1;
    }
    // Display the current page
    switch (CurrentPage) {
      case 1:
        displayPage1();
        break;
      case 2:
        displayPage2();
        break;
      case 3:
        displayPage3();
        break;
    }
  }
}

void displayPage1() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.drawCircle(8, 15, 8, WHITE);
  display.setCursor(6, 12);
  display.println("V");
  display.setCursor(0, 0);
  display.println("      R     Y     B");
  display.setCursor(29, 12);
  display.println(Rvolt);
  display.setCursor(65, 12);
  display.println(Yvolt);
  display.setCursor(101, 12);
  display.println(Bvolt);

  display.setCursor(0, 30);
  display.println("STS:");

  if (BuzzerState == LOW) {
    display.setCursor(35, 30);
    display.println(" --Normal-- ");
    display.display();
  }
  else {
    display.setCursor(35, 30);
    display.println(" ** Fault ** ");
    display.fillCircle(65, 52, 10, WHITE);
    display.display();
    display.fillCircle(65, 52, 10, BLACK);
    display.display();
}
}

void displayPage2() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 5);
  display.println("Temperature:");
  
  display.setCursor(0, 22);
  display.println("MCU Temp:      *C");
  display.setCursor(55, 22);
  display.println(MCUTemp);

  display.setCursor(0, 37);
  display.println("Panel Temp:     *C");
  display.setCursor(67, 37);
  display.println(temperature);
  display.setCursor(0, 55);
  display.println("Cooling Fan:");

  if (FanState == HIGH) {
    display.setCursor(70, 55);
    display.println(" ON ");
    display.display();
  }
  else {
    display.setCursor(70, 55);
    display.println(" OFF ");
    display.display();
  }
  display.display();
}

void displayPage3() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 5);
  display.println("Set:  UV   OV  ");
  display.setCursor(32, 17);
  display.println(UnderVolt);
  display.setCursor(65, 17);
  display.println( OverVolt);
  display.setCursor(0, 35);
  display.println("V.DelayT:  ");
  display.setCursor(54, 35);
  display.println(VTD);
  display.setCursor(90, 35);
  display.println("~ms");
  display.display();
}
