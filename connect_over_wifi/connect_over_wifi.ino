
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <OSCData.h>

#define NUM_DIGITAL_PINS 12
#define NUM_ANALOG_PINS 12

#define PIN_MAX_VALUE 4095

char ssid[] = "DFO";
char pass[] = "payasparab";

IPAddress dest_ip(192, 168, 1, 110);
const int dest_port = 7000;
const int localPort = 7000; //used for Udp.begin() which I think listens

int POT_PIN = 34;
int pot_val = 0;

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


void loop() {
  // put your main code here, to run repeatedly:
  pot_val = analogRead(POT_PIN);
  Serial.println(pot_val);
  delay(1000);
  OSCBundle bndl;
  char addr[12];
  for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
    sprintf(addr, "/digital/%d", i);
    int num = pot_val;
    printf("Sending %s : %d\n", addr, num);
    bndl.add(addr).add(num);
  }
  for (int i = 0; i < NUM_ANALOG_PINS; i++) {
    sprintf(addr, "/analog/%d", i);
    int num = random(50);
    printf("Sending %s : %d\n", addr, num);
    bndl.add(addr).add(num);
  }
  if (Udp.beginPacket(dest_ip, dest_port) == 0) {
    Serial.println("ERROR: beginPacket failed");
  }
  bndl.send(Udp);
  Udp.endPacket();
  bndl.empty();

  delay(2000);
}
