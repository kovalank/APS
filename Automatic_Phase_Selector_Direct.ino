#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DHT.h>

#define SCREEN_WIDTH 128  // OLED display width, in pixels
#define SCREEN_HEIGHT 64  // OLED display height, in pixels
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

long Sensorloopint = 200;
long Phaseloopint = 100;
unsigned long Sensorloop = 0;
unsigned long R_Loop = 0;
unsigned long Y_Loop = 0;
unsigned long B_Loop = 0;

unsigned long RVK_Ondelay_milli = 0;
unsigned long YVK_Ondelay_milli = 0;
unsigned long BVK_Ondelay_milli = 0;

const int Rvin = 36;
const int Yvin = 39;
const int Bvin = 34;
const int RVK = 25;
const int YVK = 26;
const int BVK = 27;
const int UV_POT_PIN = 33;
const int OV_POT_PIN = 32;
const int VT_POT_PIN = 35;
const int BYP_PIN = 19;
const int KBYP = 15;
const int OK_LED_PIN = 4;
const int FAULT_LED_PIN = 5;
const int BUZZER_PIN = 18;
const int Fan = 13;
const int OLEDButtonPin = 23;
#define DHTPIN 14  // DHT22 sensor pin

bool BYPState = HIGH;
bool KBYPState = LOW;
bool BuzzerState = LOW;

int UnderVolt = 0;
int OverVolt = 0;
int VTD = 0;

int Rvolt = 0;
int Yvolt = 0;
int Bvolt = 0;

unsigned long LastButtonPressTime = 0;
unsigned long DebounceDelay = 50;  // Debounce time in milliseconds
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

#define DHTTYPE DHT22  // DHT22 sensor type
DHT dht(DHTPIN, DHTTYPE);
bool FanState = LOW;

void setup() {
  Serial.begin(115200);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  dht.begin();

  pinMode(Rvin, INPUT);
  pinMode(Yvin, INPUT);
  pinMode(Bvin, INPUT);
  pinMode(UV_POT_PIN, INPUT);
  pinMode(OV_POT_PIN, INPUT);
  pinMode(VT_POT_PIN, INPUT);
  pinMode(OLEDButtonPin, INPUT_PULLUP);
  pinMode(BYP_PIN, INPUT_PULLUP);
  pinMode(OK_LED_PIN, OUTPUT);
  pinMode(FAULT_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(Fan, OUTPUT);
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
// get maximum reading value R Phase
int Rmax() {
  int maxR = 0;
  for (int R = 0; R < 100; R++) {
    int Rphase = analogRead(Rvin);  // read from analog channel 0 (A0)
    if (maxR < Rphase) maxR = Rphase;
    delayMicroseconds(100);
  }
  return maxR;
}
// get maximum reading value Y Phase
int Ymax() {
  int maxY = 0;
  for (int Y = 0; Y < 100; Y++) {
    int Yphase = analogRead(Yvin);  // read from analog channel 1 (A1)
    if (maxY < Yphase) maxY = Yphase;
    delayMicroseconds(100);
  }
  return maxY;
}
// get maximum reading value B Phase
int Bmax() {
  int maxB = 0;
  for (int B = 0; B < 100; B++) {
    int Bphase = analogRead(Bvin);  // read from analog channel 2 (A2)
    if (maxB < Bphase) maxB = Bphase;
    delayMicroseconds(100);
  }
  return maxB;
}

void loop() {
  unsigned long CurrentTime = millis();

  if (CurrentTime - Sensorloop > Sensorloopint) {
    Sensorloop = CurrentTime;
    //R phase output
    char VoltR[10];
    // get amplitude (maximum - or peak value)
    uint32_t vrout = Rmax();
    // get actual voltage (ADC voltage reference = 1.1V)
    Rvolt = vrout * 1100 / 4095;
    // calculate the RMS value ( = peak/√2 )
    Rvolt /= sqrt(2);

    //Y phase output
    char VoltY[10];
    uint32_t vyout = Ymax();
    Yvolt = vyout * 1100 / 4095;
    Yvolt /= sqrt(2);

    //B phase output
    char VoltB[10];
    uint32_t vbout = Bmax();
    Bvolt = vbout * 1100 / 4095;
    Bvolt /= sqrt(2);

    int UVPOT = analogRead(UV_POT_PIN);
    UnderVolt = map(UVPOT, 0, 4095, 80, 200);  // Map lower threshold potentiometer value to threshold range

    int OVPOT = analogRead(OV_POT_PIN);
    OverVolt = map(OVPOT, 0, 4095, 240, 290);  // Map upper threshold potentiometer value to threshold range

    int VTPOT = analogRead(VT_POT_PIN);
    VTD = map(VTPOT, 0, 4095, 500, 10000);  // Map sum of potentiometer values to interval range

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
  // R Phase relays turn on/off
  if (CurrentTime - R_Loop > Phaseloopint) {
    R_Loop = CurrentTime;
    // R-Phase RVK turn on
    if ((Rvolt > OverVolt || Rvolt < UnderVolt) && BYPState == HIGH) {
      if (RVK_Ondelay_milli == 0) {
        RVK_Ondelay_milli = millis();
      }
      if (millis() - RVK_Ondelay_milli >= VTD) {
        digitalWrite(RVK, HIGH);
        RVK_Ondelay_milli = 0;
      }
    }
    // R-Phase RVK turn off
    if ((Rvolt < OverVolt || Rvolt > UnderVolt) || BYPState == LOW) {
      if (RVK_Ondelay_milli == 0) {
        RVK_Ondelay_milli = millis();
      }
      if (millis() - RVK_Ondelay_milli >= VTD) {
        digitalWrite(RVK, LOW);
        RVK_Ondelay_milli = 0;
      }
    }
  }
  // Y Phase
  if (CurrentTime - Y_Loop > Phaseloopint) {
    Y_Loop = CurrentTime;
    // Y-Phase YVK turn on
    if ((Yvolt > OverVolt || Yvolt < UnderVolt) && BYPState == HIGH) {
      if (YVK_Ondelay_milli == 0) {
        YVK_Ondelay_milli = millis();
      }
      if (millis() - YVK_Ondelay_milli >= VTD) {
        digitalWrite(YVK, HIGH);
        YVK_Ondelay_milli = 0;
      }
    }
    // Y-Phase YVK turn off
    if ((Yvolt < OverVolt || Yvolt > UnderVolt) || BYPState == LOW) {
      if (YVK_Ondelay_milli == 0) {
        YVK_Ondelay_milli = millis();
      }
      if (millis() - YVK_Ondelay_milli >= VTD) {
        digitalWrite(YVK, LOW);
        YVK_Ondelay_milli = 0;
      }
    }
  }
  // B Phase
  if (CurrentTime - B_Loop > Phaseloopint) {
    B_Loop = CurrentTime;
    if ((Bvolt > OverVolt || Bvolt < UnderVolt) && BYPState == HIGH) {
      if (BVK_Ondelay_milli == 0) {
        BVK_Ondelay_milli = millis();
      }
      if (millis() - BVK_Ondelay_milli >= VTD) {
        digitalWrite(BVK, HIGH);
        BVK_Ondelay_milli = 0;
      }
    }
    // B-Phase BVK turn off
    if ((Bvolt < OverVolt || Bvolt > UnderVolt) || BYPState == LOW) {
      if (BVK_Ondelay_milli == 0) {
        BVK_Ondelay_milli = millis();
      }
      if (millis() - BVK_Ondelay_milli >= VTD) {
        digitalWrite(BVK, LOW);
        BVK_Ondelay_milli = 0;
      }
    }
  }
  // Panel fan turn on/off
  if (CurrentTime - Templooptime > Temploopint) {
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
  } else {
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
  } else {
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
  display.println(OverVolt);
  display.setCursor(0, 35);
  display.println("V.DelayT:  ");
  display.setCursor(54, 35);
  display.println(VTD);
  display.setCursor(90, 35);
  display.println("~ms");
  display.display();
}
