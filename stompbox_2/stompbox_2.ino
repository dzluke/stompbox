
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <OSCData.h>
#include <LiquidCrystal_I2C.h>

#define NUM_DIGITAL_PINS 12
#define NUM_ANALOG_PINS 12

#define PIN_MAX_VALUE 4095

// Calibration time in ms 
#define CALIBRATION_TIME 10000

char ssid[] = "***";
char pass[] = "***";
IPAddress dest_ip(255, 255, 255, 255);

const int dest_port = 7000;
const int localPort = 7000; //used for Udp.begin() which listens

bool calibration = false;
int digital_initial_state[NUM_DIGITAL_PINS];
int analog_max[NUM_ANALOG_PINS];
int analog_min[NUM_ANALOG_PINS];
unsigned long calibration_end_time = -1;

// set the LCD number of columns and rows
int lcdColumns = 16;
int lcdRows = 2;

// set LCD address, number of columns and rows
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

void setup() {
  
  // initialize LCD
  lcd.init();
  // turn on LCD backlight
  lcd.backlight();
  lcd.setCursor(0, 0);

  //Setup WiFi
  Serial.begin(115200);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  int g = 0;
  Serial.print("WiFi Connecting...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if(g%2==0){
      lcd.setCursor(0,0);
      lcd.print("WiFi Connecting...");
    }else{
      lcd.clear();
    }
  }

  Serial.println("");

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
#ifdef ESP32
  Serial.println(localPort);
#else
  Serial.println(Udp.localPort());
#endif
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Port: ");
  Serial.println(localPort);

  //DISPLAY IP ADDRESS
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(WiFi.localIP());

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


void loop() {
  
  unsigned long curr_time = millis();
  
  if (calibration) {
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
      lcd.print(WiFi.localIP());m
    }
  }

  
  OSCBundle bndl_out;
  OSCBundle bndl_in;
  char addr[16];
  int bndl_in_size;

  if ((bndl_in_size = Udp.parsePacket()) > 0) {
    while (bndl_in_size--) {
      bndl_in.fill(Udp.read());
    }
    if (!bndl_in.hasError()) {
      bndl_in.dispatch("/calibrate", calibrate);
    }
  }
  
  int val;
  int last_octet = WiFi.localIP()[3];
  for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
    sprintf(addr, "/%d/digital/%d", last_octet, i);
    val = random(100);
    //val = digitalRead(i); 
    //val = val ^ digital_initial_state[i]; //XOR the reading with the initial reading of the pin    
    bndl_out.add(addr).add(val);
  }
  for (int i = 0; i < NUM_ANALOG_PINS; i++) {
    sprintf(addr, "/%d/analog/%d", last_octet, i);
    val = random(100);
    //val = analogRead(i);
    //val = map(val, analog_min[i], analog_max[i], 0, PIN_MAX_VALUE); //Map the reading based on calibration
    bndl_out.add(addr).add(val);
  }
  if (Udp.beginPacket(dest_ip, dest_port) == 0) {
    Serial.println("ERROR: beginPacket failed");
  }
  bndl_out.send(Udp);
  Udp.endPacket();
  bndl_out.empty();
}
