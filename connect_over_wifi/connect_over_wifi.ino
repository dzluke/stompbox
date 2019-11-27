
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <OSCData.h>

#define NUM_DIGITAL_PINS 12
#define NUM_ANALOG_PINS 12

#define PIN_MAX_VALUE 4095

// Calibration time in ms 
#define CALIBRATION_TIME 10000

char ssid[] = "adkbb";
char pass[] = "******";

IPAddress dest_ip(192, 168, 99, 105);
const int dest_port = 7000;
const int localPort = 7000; //used for Udp.begin() which listens

int POT_PIN = 34;
int pot_val = 0;

bool calibration = false;
int digital_initial_state[NUM_DIGITAL_PINS];
int analog_max[NUM_ANALOG_PINS];
int analog_min[NUM_ANALOG_PINS];
unsigned long calibration_end_time = -1;

bool debug = true;

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

void setup() {
  // put your setup code here, to run once:

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
}


/* Expect OSC message to addr /calibrate to be a 0 or 1 */
void calibrate(OSCMessage &msg) {
  calibration = msg.getInt(0);
  Serial.print("Setting calibration to: ");
  Serial.println(calibration);

  // Calibrate all digital pins
  if (calibration) {
    calibration_end_time = millis() + CALIBRATION_TIME;
    for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
      digital_initial_state[i] = digitalRead(i);
    }
  }
  
}


void loop() {
//  potentiometer code:
//  pot_val = analogRead(POT_PIN);
//  Serial.println(pot_val);
//  delay(1000);

  unsigned long curr_time = millis();
  if (calibration) {
    if (curr_time < calibration_end_time) {
      for (int i = 0; i < NUM_ANALOG_PINS; i++) {
        int val = analogRead(i);
        analog_min[i] = min(val, analog_min[i]);
        analog_max[i] = max(val, analog_max[i]);
      }
    } else {
      //Calibration is over
      calibration = false;
      if (debug) {
        Serial.println("Calibration has ended. Min/max Values:");
        for (int i = 0; i < NUM_ANALOG_PINS; i++) {
          Serial.println(i);
          Serial.print("Min: ");
          Serial.println(analog_min[i]);
          Serial.print("Max: ");
          Serial.println(analog_max[i]);
        }
      }
    }
  }

  
  OSCBundle bndl_out;
  OSCBundle bndl_in;
  char addr[12];
  int bndl_in_size;

  if ((bndl_in_size = Udp.parsePacket()) > 0) {
    Serial.println("Received UDP packet");
    while (bndl_in_size--) {
      bndl_in.fill(Udp.read());
    }
    if (!bndl_in.hasError()) {
      bndl_in.dispatch("/calibrate", calibrate);
    } else {
      Serial.println("Packet has error");
    }
  }
  
  int val;
  for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
    sprintf(addr, "/digital/%d", i);
    val = digitalRead(i); 
    val = val ^ digital_initial_state[i]; //XOR the reading with the initial reading of the pin
    if (debug) {
      val = pot_val;
//      printf("Sending %s : %d\n", addr, val);
    }
    bndl_out.add(addr).add(val);
  }
  for (int i = 0; i < NUM_ANALOG_PINS; i++) {
    sprintf(addr, "/analog/%d", i);
    val = analogRead(i);
    val = map(val, analog_min[i], analog_max[i], 0, PIN_MAX_VALUE); //Map the reading based on calibration
    if (debug) {
      val = random(50);
//      printf("Sending %s : %d\n", addr, val);
    }
    bndl_out.add(addr).add(val);
  }
  if (Udp.beginPacket(dest_ip, dest_port) == 0) {
    Serial.println("ERROR: beginPacket failed");
  }
  bndl_out.send(Udp);
  Udp.endPacket();
  bndl_out.empty();

  delay(2000);
}
