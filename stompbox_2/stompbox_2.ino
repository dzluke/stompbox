
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

char ssid[] = "adkbb";
char pass[] = "adadcafe";
char dest_ip[] = "255.255.255.255";

const int dest_port = 7000;
const int localPort = 7000; //used for Udp.begin() which listens

// these should be removed before release
int POT_PIN = 35;
int pot_val = 0;

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

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

// debugging and latency
boolean debug = false;
const int RUNNING_AVG_N = 10;
int latency_list[RUNNING_AVG_N];
int latency_pos = 0; //keeps track of position in latency list


void setup() {
  // initialize LCD
  lcd.init();
  // turn on LCD backlight
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Stompbox 2.0");

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

  //initialize OSC address arrays
  int last_octet = WiFi.localIP()[3];
  for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
    sprintf(osc_addrs_digital[i], "/%d/digital/%d", last_octet, i);
  }
  for (int i = 0; i < NUM_ANALOG_PINS; i++) {
    sprintf(osc_addrs_analog[i], "/%d/analog/%d", last_octet, i);
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


//for debugging/latency purposes only
long sent_time = 0;
bool waiting = false;

void test_latency(OSCMessage &msg) {
  long recv_time = millis();
  long latency = recv_time - sent_time;
  latency_list[latency_pos] = latency;
  latency_pos++;
  latency_pos = latency_pos % RUNNING_AVG_N; // wrap around to beginning of list

  int sum = 0;
  for (int i = 0; i < RUNNING_AVG_N; i++) {
    sum += latency_list[i];
  }
  
//  Serial.print("Sent time: ");
//  Serial.println(sent_time);
//  Serial.print("Recv time: ");
//  Serial.println(recv_time);
  Serial.print("Two Way Latency: ");
  Serial.println(latency);
  Serial.print("Running Average: ");
  Serial.println(sum / RUNNING_AVG_N);
  Serial.println("------------");
//  delay(500);
  waiting = false;
  
}

void set_user_ip(OSCMessage &msg) {
  if (msg.isString(0)) {
    int len = msg.getDataLength(0);
    char user_ip[len];
    msg.getString(0, user_ip, len);
    strcpy(dest_ip, user_ip);
  } else {
    Serial.println("ERROR: expected user ip to be a string");
  }
}


void loop() {

  OSCBundle bndl_out;
  OSCBundle bndl_in;
  char addr[16];
  int bndl_in_size;
  
  unsigned long curr_time = millis();

  pot_val = analogRead(POT_PIN);

  // CALIBRATION 
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
      lcd.print(WiFi.localIP());
    }
  }

  // RECEIVE OSC FROM MAX
  if ((bndl_in_size = Udp.parsePacket()) > 0) {
    while (bndl_in_size--) {
      bndl_in.fill(Udp.read());
    }
    if (!bndl_in.hasError()) {
      bndl_in.dispatch("/calibrate", calibrate);
//      bndl_in.dispatch("/test_latency", test_latency);
      bndl_in.dispatch("/set_ip", set_user_ip);
    }
  }

  // READ FROM PINS
  int val;
  for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
//    val = random(100);
//    val = digitalRead(i); 
    //val = val ^ digital_initial_state[i]; //XOR the reading with the initial reading of the pin  
    val = pot_val;  
    if (debug) {
      Serial.println(osc_addrs_digital[i]);
      Serial.println(val);
    }
    bndl_out.add(osc_addrs_digital[i]).add(val);
  }
  for (int i = 0; i < NUM_ANALOG_PINS; i++) {
//    val = random(100);
//    val = analogRead(i);
    val = pot_val;
    //val = map(val, analog_min[i], analog_max[i], 0, PIN_MAX_VALUE); //Map the reading based on calibration
    if (debug) {
      Serial.println(osc_addrs_analog[i]);
      Serial.println(val);
    }
    bndl_out.add(osc_addrs_analog[i]).add(val);
  }
  
  // for testing latency only
//  if (!waiting) {
//    sent_time = millis();
//    bndl_out.add("/test_latency").add(3);
//    waiting = true;
//  }

  // SEND VALUES TO MAX
  if (Udp.beginPacket(dest_ip, dest_port) == 0) {
    Serial.println("ERROR: beginPacket failed");
  }
  bndl_out.send(Udp);
  Udp.endPacket();
  bndl_out.empty();

  if (debug) {
    delay(2000);
  }
}
