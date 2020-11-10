
#include <Arduino.h>
#include <ETH.h>
#include <WiFiUdp.h> 
#include <SPI.h>   
#include <OSCBundle.h>
#include <OSCBoards.h>
#include <LiquidCrystal_I2C.h>

#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#define ETH_PHY_POWER 12
#define NUM_DIGITAL_PINS 12
#define NUM_ANALOG_PINS 12
#define PIN_MAX_VALUE 4095
#define CALIBRATION_TIME 10000 //in ms

// this should be removed before release
int POT_PIN = 35;

// calibration vars
bool calibration = false;
int digital_initial_state[NUM_DIGITAL_PINS];
int analog_max[NUM_ANALOG_PINS];
int analog_min[NUM_ANALOG_PINS];
unsigned long calibration_end_time = -1;

// the OSC addresses that will be used
char osc_addrs_digital[NUM_DIGITAL_PINS][16];
char osc_addrs_analog[NUM_ANALOG_PINS][16];

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// set LCD address, number of columns and rows
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

// debugging and latency
boolean debug = false;


WiFiUDP Udp;  //Create UDP object
unsigned int local_port = 1750;
IPAddress dest_ip;
unsigned int dest_port = 1751;

void setup() {
  Serial.begin(115200);
  
  WiFi.onEvent(WiFiEvent);
  ETH.begin();
  Udp.begin(local_port);
  
  // initialize LCD
  lcd.init();
  // turn on LCD backlight
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Stompbox 2.0");
 
  // TODO: Display IP address
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("Connect");
  lcd.setCursor(1,0);
  lcd.print("Ethernet");

  //initialize OSC address arrays
  for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
    sprintf(osc_addrs_digital[i], "/digital/%d", i);
  }
  for (int i = 0; i < NUM_ANALOG_PINS; i++) {
    sprintf(osc_addrs_analog[i], "/analog/%d", i);
  }
}


void loop() {  
  // CALIBRATION 
  if (calibration) {
    update_calibration();
  }

  int packetSize = Udp.parsePacket(); // Get the current team header packet length
  if (packetSize)                     // If data is available
  {
    dest_ip = Udp.remoteIP();
    char buf[packetSize];
    Udp.read(buf, packetSize); // Read the current packet data
    Serial.println();
    Serial.print("Received: ");
    Serial.println(buf);
    Serial.print("From IP: ");
    Serial.println(Udp.remoteIP());
    Serial.print("From Port: ");
    Serial.println(Udp.remotePort());
  }

  // Read pins
  OSCBundle bundle;
  int val;
  for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
    val = random(100);
//    val = digitalRead(i); 
//    val = val ^ digital_initial_state[i]; //XOR the reading with the initial reading of the pin  
    if (debug) {
      Serial.println(osc_addrs_digital[i]);
      Serial.println(val);
    }
    bundle.add(osc_addrs_digital[i]).add(val);
  }
  for (int i = 0; i < NUM_ANALOG_PINS; i++) {
    val = random(100);
//    val = analogRead(i);
//    val = map(val, analog_min[i], analog_max[i], 0, PIN_MAX_VALUE); //Map the reading based on calibration
    if (debug) {
      Serial.println(osc_addrs_analog[i]);
      Serial.println(val);
    }
    bundle.add(osc_addrs_analog[i]).add(val);
  }

  // Send values to Max
  Udp.beginPacket(dest_ip, dest_port);
  bundle.send(Udp);
  Udp.endPacket();
  bundle.empty();

  if (debug) {
    delay(2000);
  }
}


/* Expect OSC message to addr /calibrate to be a 0 or 1 */
void calibrate(OSCMessage &msg) {
  calibration = msg.getInt(0);

  // Calibrate all digital pins
  if (calibration) {
    lcd.setCursor(0,1);
    lcd.print("Calibrating");
    
    calibration_end_time = millis() + CALIBRATION_TIME;
    for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
      digital_initial_state[i] = digitalRead(i);
    }
    for (int i = 0; i < NUM_ANALOG_PINS; i++) {
      analog_min[i] = PIN_MAX_VALUE;
      analog_max[i] = 0;
    }
  }
}

void update_calibration() {
  unsigned long curr_time = millis();
  if (curr_time < calibration_end_time) {
      lcd.setCursor(12,1);
      int time_left = (int)((calibration_end_time - curr_time) / 1000);
      lcd.print(time_left);
      for (int i = 0; i < NUM_ANALOG_PINS; i++) {
        int val = analogRead(i);
        analog_min[i] = min(val, analog_min[i]);
        analog_max[i] = max(val, analog_max[i]);
      }
    } else {
      //Calibration is over
      calibration = false;
      lcd.clear();
      lcd.setCursor(0,0);
      lcd.print(WiFi.localIP());
    }
}

bool eth_connected = false;

void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      //set eth hostname here
      ETH.setHostname("esp32-ethernet");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      break;
    case SYSTEM_EVENT_ETH_GOT_IP:
      Serial.print("ETH MAC: ");
      Serial.print(ETH.macAddress());
      Serial.print(", IPv4: ");
      Serial.print(ETH.localIP());
      if (ETH.fullDuplex()) {
        Serial.print(", FULL_DUPLEX");
      }
      Serial.print(", ");
      Serial.print(ETH.linkSpeed());
      Serial.println("Mbps");
      eth_connected = true;
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      eth_connected = false;
      break;
    case SYSTEM_EVENT_ETH_STOP:
      Serial.println("ETH Stopped");
      eth_connected = false;
      break;
    default:
      break;
  }
}
