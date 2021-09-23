//Code designed for the Olimex ESP32-POE-ISO variant.
//Code by Luke Dzwonczyk, Jeremy Wagner & Olimex


#include <Arduino.h>
#include <ETH.h>
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <WiFiUdp.h>
#include <SPI.h>
#include <OSCBundle.h>
#include <OSCBoards.h>
#include <LiquidCrystal_I2C.h>

#define ETH_CLK_MODE ETH_CLOCK_GPIO17_OUT
#define ETH_PHY_POWER 12
#define PIN_MAX_VALUE 4095

// pin lists
const int digital_pins[] = {3, 5};
const int analog_pins[] = {0, 2, 4, 39, 36, 35, 33, 32, 15, 14};
const int num_digital_pins = sizeof(digital_pins) / sizeof(int);
const int num_analog_pins = sizeof(analog_pins) / sizeof(int);
const int button_pin = 34;

// calibration vars
bool calibration = false;
int digital_initial_state[num_digital_pins];
int analog_max[num_analog_pins];
int analog_min[num_analog_pins];
const int calibration_time = 10000;  //in ms
unsigned long calibration_end_time = -1;

// the OSC addresses that will be used
char osc_addrs_digital[num_digital_pins][16];
char osc_addrs_analog[num_analog_pins][16];
char osc_addr_button[8] = "/button";

// display vars
int lcdColumns = 20;
int lcdRows = 4;
LiquidCrystal_I2C lcd(0x27, lcdColumns, lcdRows);

// networking vars
WiFiUDP Udp;
unsigned int local_port = 1750;
IPAddress dest_ip;
unsigned int dest_port = 1751;

// ip reporting vars
bool send_ip = false;
bool report_ip = false;

// debugging and latency
// remove before release
boolean debug = false;

void setup() {
  Serial.begin(115200);  
 
  // initialize LCD
  lcd.init();
  // turn on LCD backlight
  lcd.backlight();
  display_text("Stompbox 2.0", "");

  WiFi.onEvent(WiFiEvent);
  ETH.begin();
  Udp.begin(local_port);


  //initialize OSC address arrays and calibration arrays
  for (int i = 0; i < num_digital_pins; i++) {
    sprintf(osc_addrs_digital[i], "/digital/%d", i);
    digital_initial_state[i] = 0;  // this gets updated during calibration
    pinMode(digital_pins[i], INPUT);
  }
  for (int i = 0; i < num_analog_pins; i++) {
    sprintf(osc_addrs_analog[i], "/analog/%d", i);
    
    // since there's no calibration yet, assume that the
    // analog pedals will output the range 0 to PIN_MAX_VALUE
    analog_max[i] = PIN_MAX_VALUE;
    analog_min[i] = 0;
    pinMode(analog_pins[i], INPUT);
  }
  pinMode(button_pin, INPUT);
  

}


void loop() {
  // Receive data from Max
  int packetSize = Udp.parsePacket(); // Get the current header packet length
  if (packetSize) {                   // If data is available
    dest_ip = Udp.remoteIP();
    OSCBundle bundle_in;
    while (packetSize--) {
      bundle_in.fill(Udp.read());
    }
    if (!bundle_in.hasError()) {
      bundle_in.dispatch("/calibrate", calibrate);
      bundle_in.dispatch("/backlight", control_backlight);
      bundle_in.dispatch("/display", cue_display);
      bundle_in.dispatch("/identify", identify);
      bundle_in.dispatch("/getIP", set_send_ip);
      bundle_in.dispatch("/reportIP", set_report_ip);
    }
    if (debug) {
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
  }

  // Calibrate
  if (calibration) {
    update_calibration();
  }

  // Read pins
  OSCBundle bundle;
  int pin;
  int val;
  for (int i = 0; i < num_digital_pins; i++) {
    pin = digital_pins[i];
    val = digitalRead(pin);
    if (!calibration) {
      val = val ^ digital_initial_state[i]; //XOR the reading with the initial reading of the pin
    }
    if (debug) {
      Serial.print(pin);
      Serial.print(": Read: ");
      Serial.print(digitalRead(pin));
      Serial.print(" Init state: ");
      Serial.print(digital_initial_state[i]);
      Serial.print(" Final val: ");
      Serial.println(val);
    }
    bundle.add(osc_addrs_digital[i]).add(val);
  }
  for (int i = 0; i < num_analog_pins; i++) {
    pin = analog_pins[i];
    val = analogRead(pin);
    if (!calibration) {
      val = map(val, analog_min[i], analog_max[i], 0, PIN_MAX_VALUE); //Map the reading based on calibration
    }
    if (debug) {
      Serial.print("Addr: ");
      Serial.print(osc_addrs_analog[i]);
      Serial.print(" Pin: ");
      Serial.print(pin);
      Serial.print(" Read: ");
      Serial.print(analogRead(pin));
      Serial.print(" Min: ");
      Serial.print(digital_initial_state[i]);
      Serial.print(" Final val: ");
      Serial.println(val);
    }
    bundle.add(osc_addrs_analog[i]).add(val);
  }

  // read button
  bundle.add(osc_addr_button).add(analogRead(button_pin));

  
  if (send_ip || report_ip) {
    bundle.add("/ip").add(ETH.localIP().toString().c_str());
    bundle.add("/port").add(local_port);
    send_ip = false;
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


/* Expect OSC message to addr /backlight to be a 0 or 1 */
void control_backlight(OSCMessage &msg) {
  int val = msg.getInt(0);
  if (val) {
    lcd.backlight();
  } else {
    lcd.noBacklight();
  }
}


/* Called at the beginning of the calibration phase
  Expect OSC message to addr /calibrate to be a 0 or 1 */
void calibrate(OSCMessage &msg) {
  calibration = msg.getInt(0);
  if (calibration) {
    display_text("Calibrating", "");
    calibration_end_time = millis() + calibration_time;
    // Calibrate all digital pins
    for (int i = 0; i < num_digital_pins; i++) {
      digital_initial_state[i] = digitalRead(i);
    }
    for (int i = 0; i < num_analog_pins; i++) {
      analog_min[i] = PIN_MAX_VALUE;
      analog_max[i] = 0;
    }
  }
}


/* Called every loop during calibration phase */
void update_calibration() {
  unsigned long curr_time = millis();
  if (curr_time < calibration_end_time) {
    lcd.setCursor(12, 0);
    int time_left = (int)((calibration_end_time - curr_time) / 1000);
    lcd.print(time_left);
    for (int i = 0; i < num_analog_pins; i++) {
      int val = analogRead(analog_pins[i]);
      analog_min[i] = min(val, analog_min[i]);
      analog_max[i] = max(val, analog_max[i]);
    }
  } else {
    //Calibration is over
    calibration = false;
    display_ip();
  }
}

//method to find breaks for text wrapping on display
int findBreaks(String thing, int l){
  for(int i=l-1; i>0;i--){
    Serial.println(thing.charAt(i));
    if(thing.charAt(i)==' '){
      return i;
    }
  }
  return l;
}

/* Display the current IP on the I2C display
  You can index an IPAdress like an array
  example: access the first octet of 'ip' via ip[0] */
void display_ip() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Stompbox IP : Port");
  lcd.setCursor(0, 1);
  lcd.print(ETH.localIP());
  lcd.print(" : ");
  lcd.print(local_port);
}

void identify(OSCMessage &msg){
  display_ip();
}


/* Clear the display and show the given arguments */
void display_text(String line1, String line2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
}

void cue_display(OSCMessage &msg){
  lcd.clear();
  char text[96];
  int a = msg.getString(0,text,96);
  String t = String(text);
  if (t.length() > 80) {
    lcd.setCursor(0, 0);
    lcd.print("Error:");
    lcd.setCursor(0,2);
    lcd.print("Message too long");
  } else {
    
    int last = findBreaks(t,20)+1;
    int offset=(20-last) / 2;
    lcd.setCursor(offset, 0);
    lcd.print(t.substring(0, last));
    t.remove(0, last);
    
    last = findBreaks(t,20)+1;
    offset = (20-last) / 2;
    lcd.setCursor(offset, 1);
    lcd.print(t.substring(0, last));
    t.remove(0, last);

    last=findBreaks(t, 20)+1;
    offset = (20-last) / 2;
    lcd.setCursor(offset, 2);
    lcd.print(t.substring(0, last));
    t.remove(0, last);

    last=findBreaks(t, 20)+1;
    offset = (20-last) / 2;
    lcd.setCursor(offset, 3);
    lcd.print(t.substring(0, last));


  }
}

void configure_port(String addr, String val) {

}

void set_send_ip(OSCMessage &msg) {
  send_ip = true;
}

void set_report_ip(OSCMessage &msg) {
  report_ip = msg.getInt(0);
}

static bool eth_connected = false;

void WiFiEvent(WiFiEvent_t event)
{
  switch (event) {
    case SYSTEM_EVENT_ETH_START:
      Serial.println("ETH Started");
      display_text("Ethernet Starting", "Standby..");
      //set eth hostname here
      ETH.setHostname("Stompbox_001");
      break;
    case SYSTEM_EVENT_ETH_CONNECTED:
      Serial.println("ETH Connected");
      display_text("Ethernet Connected", "...Resolving IP...");
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
      display_ip();
      break;
    case SYSTEM_EVENT_ETH_DISCONNECTED:
      Serial.println("ETH Disconnected");
      display_text( "**Connection Lost**",
                    " ***Check Cable*** ");
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
