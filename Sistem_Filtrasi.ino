#include <WiFi.h>
#include <PubSubClient.h>
#define sensorPin 35
#define relayPin 32
#define ssid "Realme Note 50"
#define password ""
#define mqtt_server "broker.emqx.io"

WiFiClient espClient;
PubSubClient client(espClient);

unsigned long lastTime = 0;
unsigned long lastMsg = 0;
int sensorData[50];
float NTU;
float jernih, keruh, kotor;
int e = 0;
int thr = 95;

float AMembershipFunction(float x, float a, float b, float c)
{
  if(x < a) return 1;
  else if((x >= a) and (x <= b)) return 1;
  else if((x > b) and (x < c)) return (c - x) / (c - b);
  else return 0;
}

float BMembershipFunction(float x, float a, float b, float c)
{
  if((x > a) and (x <= b)) return (x - a) / (b - a);
  else if((x > b) and (x < c)) return (c - x) / (c - b);
  else return 0;
}

float CMembershipFunction(float x, float a, float b, float c)
{
  if((x > a) and (x < b)) return (x - a) / (b - a);
  else if((x >= b) and (x <= c)) return 1;
  else if(x > c) return 1;
  else return 0;
}

void fuzzify(float x, float& low, float& mid, float& high)
{
  low = AMembershipFunction(x, 0, 10, 100) * 100;
  mid = BMembershipFunction(x, 10, 100, 1000) * 100;
  high = CMembershipFunction(x, 100, 1000, 4000) * 100;
}

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
  // Serial.print("Message arrived [");
  // Serial.print(topic);
  // Serial.print("] ");

  String data;
  for (int i = 0; i < length; i++) {
    data += (char)payload[i];
  }

  data.trim();  

  if(String(topic) == "unsika/filtrasi/esp/switch") {
    if (data == "ON") {
      digitalWrite(relayPin, HIGH);  
      Serial.println("Relay ON");
    } else if (data == "OFF") {
      digitalWrite(relayPin, LOW);   
      Serial.println("Relay OFF");
    }
  }
  if(String(topic) == "unsika/filtrasi/esp/error") {
    e = data.toInt();
  }
  if(String(topic) == "unsika/filtrasi/esp/thershold") {
    thr = data.toInt();
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe("unsika/filtrasi/esp/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(9600);
  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  pinMode(sensorPin, INPUT);
  pinMode(relayPin, OUTPUT);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if((millis() - lastTime) > 30) {
    float averageValue = 0;
    sensorData[0] = analogRead(sensorPin);
    for(int i=49; i>0; i--) {
      sensorData[i] = sensorData[i-1];
      averageValue = averageValue + sensorData[i];
    }
    averageValue = averageValue/49;

    NTU = map(averageValue, 0, 4095, 1000, 0) + e;
    
    fuzzify(NTU, jernih, keruh, kotor);
    (jernih>thr) ? digitalWrite(relayPin, LOW): digitalWrite(relayPin, HIGH);

    Serial.print("Fuzzy: ");
    Serial.print("jernih: "+String(jernih)+"% " + "keruh: "+String(keruh)+"% " + "kotor: "+String(kotor)+"% ");
    Serial.print("Actual Value: ");
    Serial.print(analogRead(sensorPin));
    Serial.print(" Average Value: ");
    Serial.print(averageValue);
    Serial.print(" Turbidity (NTU): ");
    Serial.println(NTU);

    lastTime = millis();
  }

  if((millis() - lastMsg) > 2000) {
    // Convert the value to a char array
    char ntuString[8];
    dtostrf(NTU, 1, 2, ntuString);

    char jernihString[8], keruhString[8], kotorString[8];
    dtostrf(jernih, 1, 2, jernihString);
    dtostrf(keruh, 1, 2, keruhString);
    dtostrf(kotor, 1, 2, kotorString);

    client.publish("unsika/filtrasi/esp/turbidity", ntuString);

    client.publish("unsika/filtrasi/esp/jernih", jernihString);
    client.publish("unsika/filtrasi/esp/keruh", keruhString);
    client.publish("unsika/filtrasi/esp/kotor", kotorString);

    lastMsg = millis();
  }
}