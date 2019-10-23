
#include <WiFi.h>
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <OSCData.h>

#define NUM_DIGITAL_PINS 12
#define NUM_ANALOG_PINS 12

char ssid[] = "DFO";
char pass[] = "payasparab";

IPAddress dest_ip(192, 168, 1, 110);
const int dest_port = 7000;
const int localPort = 7000; //used for Udp.begin() which I think listens

// A UDP instance to let us send and receive packets over UDP
WiFiUDP Udp;

void setup() {
  // put your setup code here, to run once:

  //Setup pins
//  for (int i = 0; i < 24; i++) {
//    //pin 13 has a built-in resistor on its LED which would require an additional resistor.  Lets leave it out
//    if (i != 12 && i != 13) { 
//      //all pins in pullup mode (inverts output but simplifies wiring)
//      pinMode(i, INPUT_PULLUP);
//    }
//  }

  //Setup WiFi
  Serial.begin(115200);
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, pass);

  int g=0;
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


char* digital_addrs[] = {"/digital/0", "/digital/1", "/digital/2", "/digital/3", "/digital/4", "/digital/5", "/digital/6", "/digital/7", "/digital/8", "/digital/9", "/digital/10", "/digital/11"};

void loop() {
  // put your main code here, to run repeatedly:
  OSCBundle bndl;
  for (int i = 0; i < NUM_DIGITAL_PINS; i++) {
    printf("Sending %s\n", digital_addrs[i]);
    int num = random(50);
    bndl.add(digital_addrs[i]).add(num);
  }
  if (Udp.beginPacket(dest_ip, dest_port) == 0) {
    Serial.println("ERROR: beginPacket failed");
  }
  bndl.send(Udp);
  Udp.endPacket();
  bndl.empty();
  
  delay(2000);
}
