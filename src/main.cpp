/*
 *  CatBox
 */
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// hardcoded wifi ssid, bssid and password
const char *ssid = "TestWifi";
const char *bssid = "11:11:11:11:11:11";
const char *password = "asdf";

// webserver
ESP8266WebServer server(80);

// pins for the pwm motor contoller (L298N)
const int motor_forward = D5;
const int motor_backward = D6;

const int cat_sensor = D7; // cat sensor pin
bool catPresent = false;   // cat sensor triggered flag

// pins for hall effect sensors (reversed to be able to check polarity)
const int hall_effect_home = D1;
const int hall_effect_dump = D2;
// bools for whether or not sensors are triggered
bool isHomeTriggered = false;
bool isDumpedTriggered = false;

// ---------- helper functions ----------
// non-blocking delay
void delay_ms(int ms) {
  unsigned long start = millis();
  while (millis() - start < ms)
    ;
}

// ---------- Section for the actual litter box control ----------

// interrupt for the home sensor
void IRAM_ATTR homeInterrupt()
{
  isHomeTriggered = digitalRead(hall_effect_home) == LOW;
}
// interrupt for the dump sensor
void IRAM_ATTR dumpInterrupt()
{
  isDumpedTriggered = digitalRead(hall_effect_dump) == LOW;
}

// interrupt for the cat presence
void IRAM_ATTR catPresenceInterrupt()
{
  catPresent = digitalRead(cat_sensor);
}

void setupPins()
{
  // setup pins for the motor controller
  pinMode(motor_forward, OUTPUT);
  pinMode(motor_backward, OUTPUT);

  // setup pin for cat sensor (just a switch)
  pinMode(cat_sensor, INPUT);
  attachInterrupt(digitalPinToInterrupt(cat_sensor), catPresenceInterrupt, CHANGE);

  // setup pins for the hall effect sensors
  pinMode(hall_effect_home, INPUT_PULLUP);
  pinMode(hall_effect_dump, INPUT_PULLUP);
  // setup hall effect interrupts
  attachInterrupt(digitalPinToInterrupt(hall_effect_home), homeInterrupt, CHANGE);
  attachInterrupt(digitalPinToInterrupt(hall_effect_dump), dumpInterrupt, CHANGE);
}

// handle calls to /empty that will empty the catbox by running the dc motor forward for 1 minute
void empty()
{
  Serial.println("Emptying the litter box...");

  // run the motor forward until the cat box is dumped
  digitalWrite(motor_forward, HIGH);
  while (!isDumpedTriggered)
  {
    delay(100);
  }
  digitalWrite(motor_forward, LOW); // stop the motor
  delay(1000);                      // wait for a second to make sure the motor has stopped

  // jiggle the motor a bit to make sure the cat box is empty
  digitalWrite(motor_backward, HIGH);
  delay(500);
  digitalWrite(motor_backward, LOW);
  digitalWrite(motor_forward, HIGH);
  delay(500);
  digitalWrite(motor_forward, LOW);
  digitalWrite(motor_backward, HIGH);
  delay(500);
  digitalWrite(motor_backward, LOW);
  digitalWrite(motor_forward, HIGH);
  delay(500);
  digitalWrite(motor_forward, LOW);
  digitalWrite(motor_backward, LOW);

  // reverse the motor until the home sensor is triggered
  digitalWrite(motor_backward, HIGH);
  while (!isHomeTriggered)
  {
    delay(100);
  }
  delay(6500);                       // keep going for another 6.5 seconds
  digitalWrite(motor_backward, LOW); // stop the motor
  delay(1000);                       // wait for a second to make sure the motor has stopped

  // go back to the home position
  digitalWrite(motor_forward, HIGH);
  while (!isHomeTriggered)
  {
    delay(100);
  }
  digitalWrite(motor_forward, LOW); // stop the motor

  Serial.println("Litter box is empty!");
}

// ------ Section for Webserver Stuff ------
void turnOnLed()
{
  digitalWrite(LED_BUILTIN, LOW);
}
void turnOffLed()
{
  digitalWrite(LED_BUILTIN, HIGH);
}

void startRequest()
{
  turnOnLed();
  Serial.println("Handling request...");
  Serial.println("Request method: " + (server.method() == HTTP_GET) ? "GET" : "POST");
  Serial.println("Request path: " + server.uri());
  Serial.println("Request params: " + server.args());
}
void endRequest()
{
  turnOffLed();
  Serial.println("Request handled!");
}

void handleEmpty()
{
  startRequest();
  server.send(200, "text/plain", "Running the empty procedure");
  empty();
  endRequest();
}

void handleStatus()
{
  startRequest();

  // get the status of the cat box

  server.send(200, "text/plain", "hello from esp8266!");
  endRequest();
}

void handleNotFound()
{
  startRequest();
  server.send(404, "text/plain", "Not found");
  endRequest();
}

void setupServer()
{
  // setup led pin
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH); // high turns off the led and low turns it on :)

  // setup multicast dns
  if (!MDNS.begin("catbox"))
  {
    Serial.println("Error setting up MDNS responder!");
    while (1)
      ;
  }
  Serial.println("mDNS responder started");

  // setup webserver
  server.on("/status", HTTP_GET, handleStatus);
  server.on("/empty", HTTP_GET, handleEmpty);
  server.onNotFound(handleNotFound);

  // start webserver
  server.begin();
  Serial.println("HTTP server started");
}

void connectToWifi()
{
  Serial.println("Connecting to wifi...");
  // Print out esp mac address
  Serial.println("MAC: " + WiFi.macAddress());

  // loop through available networks and find matching ssid
  int n = WiFi.scanNetworks();
  Serial.println("Found " + String(n) + " networks");
  // PRINT OUT ALL NETWORKS
  for (int i = 0; i < n; ++i)
  {
    Serial.println(WiFi.SSID(i) + " " + WiFi.RSSI(i) + " " + WiFi.channel(i) + " " + WiFi.BSSIDstr(i));
  }

  for (int i = 0; i < n; ++i)
  {
    if (WiFi.BSSIDstr(i) == bssid)
    {
      Serial.println("Found matching bssid, connecting...");
      WiFi.begin(ssid, password, WiFi.channel(i), WiFi.BSSID(i));
      Serial.println("Connected to " + String(ssid));

      // wait for connection
      Serial.println("Waiting for connection...");
      while (WiFi.status() != WL_CONNECTED)
      {
        delay(500);
        Serial.print("#");
      }
      Serial.println("");

      // print ip address
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());

      return;
    }
  }

  Serial.println("No matching BSSID found!!!");
}

void setup()
{
  Serial.begin(115200);

  setupPins();

  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  // connect to the wifi that is configured
  connectToWifi();

  // setup the server
  setupServer();

  Serial.println("Setup done");
}

void loop()
{
  // if at any point we disconnect from wifi, reconnect
  if (!WiFi.isConnected())
  {
    connectToWifi();
  }

  server.handleClient();

  if (catPresent)
  {
    Serial.println("Cat is present... waiting for cat to leave");

    // get current time so we can see how long cat stays in the box
    unsigned long startTime = millis();
    // wait for cat to leave the box
    while (catPresent)
    {
      delay(100);
    }
    // if cat stayed in the box for more than 10 seconds, empty the box
    if (millis() - startTime > 10000)
    {
      Serial.println("Cat stayed in the box for more than 10 seconds, emptying the box");
      empty();
    } else {
      Serial.println("Cat left the box too soon");
    }
  }
}
