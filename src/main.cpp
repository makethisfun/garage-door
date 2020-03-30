/**
 * Hardware
 * ========
 * 
 * Board
 * -----
 *  
 * Parts
 * -----
 * 
 */

#include <Bounce2.h>
#include <Common.h>

// Communications
// TODO use the MDNS address of the pi. Need to look it up for the correct address.
char remote_url_config[][128] = {"Jeffreys-MacBook-Pro-v15.local", "80", "/garagedoor"};
char connection_config[][32] = {"http", "tcp", "home_garagedoor", "80"};

// Pushover settings
String pushoversite = "api.pushover.net";
String apitoken = "ai7px42xs7d1gmtvsa5bhqb4enp264";
String userkey = "uajuupjacn6foxvyi8okyffm8yt9bq";

// For ESP8266 boards (NodeMCU or Feather HUZZAH)
#define REED_PIN 12
#define RELAY_PIN 15

bool door_open = false;

#define DEBOUNCE_PERIOD 400 //ms
Bounce REED_BOUNCE_LISTENER = Bounce();

HTTPClient http;

void triggerDoor() {
  digitalWrite(RELAY_PIN, HIGH);
  delay(500);
  digitalWrite(RELAY_PIN, LOW);
  remote_send_quick(remote_url_config, "Triggered Door");
}

void pushover(String message) {
  if (WiFi.status() == WL_CONNECTED) {
    http.begin("http://" + pushoversite + "/1/messages.json");
    http.addHeader("Connection", "close");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int httpCode = http.POST("token=" + apitoken + "&user=" + userkey + "&message=" + message);
    String payload = http.getString();

    Serial.println(httpCode);
    Serial.println(payload);

    http.end();

    Serial.print("Sent Message: " + message);
  } else {
    Serial.println("Error in WiFi connection");
  }
}

void setup() {
  Serial.println("Setup....");
  setup_common(connection_config, remote_url_config);

  setup_server([door_open](ESP8266WebServer *server) {
    server->on(
        "/open", HTTP_POST, [server, door_open]() {
          StaticJsonDocument<200> doc;
          if (!door_open) {
            doc["state"] = "opening";
            triggerDoor();
          } else {
            doc["state"] = "open";
          }
          String payload;
          serializeJson(doc, payload);
          server->send(200, "application/json", payload);
        });

    server->on("/close", HTTP_POST, [server, door_open]() {
      StaticJsonDocument<200> doc;
      if (!door_open) {
        doc["state"] = "closed";
      } else {
        doc["state"] = "closing";
        triggerDoor();
      }
      String payload;
      serializeJson(doc, payload);
      server->send(200, "application/json", payload);
    });
  });

  digitalWrite(RELAY_PIN, LOW);
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);

  pinMode(REED_PIN, INPUT_PULLUP);

  REED_BOUNCE_LISTENER.attach(REED_PIN);
  REED_BOUNCE_LISTENER.interval(DEBOUNCE_PERIOD);

  door_open = REED_BOUNCE_LISTENER.read();

  remote_send_quick(remote_url_config, (door_open ? "Setup Complete, Door is Open" : "Setup Complete, Door is Closed"));

  pushover((door_open ? "Setup Complete, Door is Open" : "Setup Complete, Door is Closed"));
}

void loop() {

  loop_common();

  // Check serial input for board debugging
  // Allows us to simulate button presses via serial input
  // int serialInt = debug_serial_input();

  REED_BOUNCE_LISTENER.update();

  if (REED_BOUNCE_LISTENER.fell()) {
    pushover("Garage Door is closed");
    remote_send_quick(remote_url_config, "Garage closed");
    door_open = false;
  } else if (REED_BOUNCE_LISTENER.rose()) {
    pushover("Garage Door is open");
    remote_send_quick(remote_url_config, "Garage open");
    door_open = true;
  }
}
