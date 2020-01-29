
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

char ssid[] = "*****";
char pass[] = "*****";
IPAddress dest_ip(192, 168, 1, 107);

const int dest_port = 7000;
const int localPort = 7000; //used for Udp.begin() which listens

bool calibration = false;
int digital_initial_state[NUM_DIGITAL_PINS];
int analog_max[NUM_ANALOG_PINS];
int analog_min[NUM_ANALOG_PINS];
unsigned long calibration_end_time = -1;

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

  // Calibrate all digital pins
  if (calibration) {
    calibration_end_time = millis() + CALIBRATION_TIME;
    for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
      digital_initial_state[i] = digitalRead(i);
    }
  }
  
}


void loop() {
  
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
    }
  }

  
  OSCBundle bndl_out;
  OSCBundle bndl_in;
  char addr[12];
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
  for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
    sprintf(addr, "/digital/%d", i);
    val = digitalRead(i); 
    val = val ^ digital_initial_state[i]; //XOR the reading with the initial reading of the pin    
    bndl_out.add(addr).add(val);
  }
  for (int i = 0; i < NUM_ANALOG_PINS; i++) {
    sprintf(addr, "/analog/%d", i);
    val = analogRead(i);
    val = map(val, analog_min[i], analog_max[i], 0, PIN_MAX_VALUE); //Map the reading based on calibration
    bndl_out.add(addr).add(val);
  }
  if (Udp.beginPacket(dest_ip, dest_port) == 0) {
    Serial.println("ERROR: beginPacket failed");
  }
  bndl_out.send(Udp);
  Udp.endPacket();
  bndl_out.empty();
}
