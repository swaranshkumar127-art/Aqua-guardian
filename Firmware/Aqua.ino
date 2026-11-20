
#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ==========================================
// AQUAGUARDIAN ROBOT
// ESP32 + 2 L298N + 3 SERVOS
// ==========================================

// WiFi Configuration
const char* ssid = "AquaGuardian";
const char* password = "aqua12345";

WebServer server(80);

// ==========================================
// MOTOR PIN CONFIGURATION
// ==========================================

// Left Propeller - L298N 1
#define LEFT_EN 25
#define LEFT_IN1 26
#define LEFT_IN2 27

// Right Propeller - L298N 2
#define RIGHT_EN 33
#define RIGHT_IN1 18
#define RIGHT_IN2 19

// Servo Pins
#define LEFT_GATE_PIN 13
#define RIGHT_GATE_PIN 32
#define FEEDER_PIN 23

// PWM settings
const int PWM_FREQ = 1000;
const int PWM_RES = 8;

// Servo objects
Servo leftGate;
Servo rightGate;
Servo feeder;

// Servo angles - calibrate mechanically
const int LEFT_OPEN = 90;
const int LEFT_CLOSE = 0;

const int RIGHT_OPEN = 90;
const int RIGHT_CLOSE = 0;

const int FEED_HOME = 0;
const int FEED_RELEASE = 90;

// Motor speed
int motorSpeed = 160;

// Automatic feeding interval
const unsigned long FEED_INTERVAL =
  6UL * 60UL * 60UL * 1000UL;

const unsigned long FEED_TIME = 1000;

unsigned long lastFeedTime = 0;
unsigned long feedStartTime = 0;

bool feeding = false;

// ==========================================
// MOTOR CONTROL
// ==========================================

void setMotor(int en, int in1, int in2, int speed) {

  speed = constrain(speed, -255, 255);

  if (speed > 0) {
    digitalWrite(in1, HIGH);
    digitalWrite(in2, LOW);
  }
  else if (speed < 0) {
    digitalWrite(in1, LOW);
    digitalWrite(in2, HIGH);
  }
  else {
    digitalWrite(in1, LOW);
    digitalWrite(in2, LOW);
  }

  analogWrite(en, abs(speed));
}

void stopMotors() {
  setMotor(LEFT_EN, LEFT_IN1, LEFT_IN2, 0);
  setMotor(RIGHT_EN, RIGHT_IN1, RIGHT_IN2, 0);
}

void forward() {
  setMotor(LEFT_EN, LEFT_IN1, LEFT_IN2, motorSpeed);
  setMotor(RIGHT_EN, RIGHT_IN1, RIGHT_IN2, motorSpeed);
}

void backward() {
  setMotor(LEFT_EN, LEFT_IN1, LEFT_IN2, -motorSpeed);
  setMotor(RIGHT_EN, RIGHT_IN1, RIGHT_IN2, -motorSpeed);
}

void turnLeft() {
  setMotor(LEFT_EN, LEFT_IN1, LEFT_IN2, -motorSpeed);
  setMotor(RIGHT_EN, RIGHT_IN1, RIGHT_IN2, motorSpeed);
}

void turnRight() {
  setMotor(LEFT_EN, LEFT_IN1, LEFT_IN2, motorSpeed);
  setMotor(RIGHT_EN, RIGHT_IN1, RIGHT_IN2, -motorSpeed);
}

// ==========================================
// GATE CONTROL
// ==========================================

void openGate() {
  leftGate.write(LEFT_OPEN);
  rightGate.write(RIGHT_OPEN);
}

void closeGate() {
  leftGate.write(LEFT_CLOSE);
  rightGate.write(RIGHT_CLOSE);
}

// ==========================================
// FISH FEEDER
// ==========================================

void startFeeding() {

  if (feeding) return;

  feeding = true;
  feedStartTime = millis();

  feeder.write(FEED_RELEASE);

  Serial.println("Fish feeding started");
}

void updateFeeding() {

  if (feeding &&
      millis() - feedStartTime >= FEED_TIME) {

    feeder.write(FEED_HOME);

    feeding = false;
    lastFeedTime = millis();

    Serial.println("Fish feeding completed");
  }
}

// ==========================================
// WEB PAGE
// ==========================================

const char webpage[] PROGMEM = R"rawliteral(

<!DOCTYPE html>
<html>

<head>

<meta name="viewport"
content="width=device-width, initial-scale=1">

<title>AquaGuardian</title>

<style>

body {
  background: #eaf6f8;
  font-family: Arial;
  text-align: center;
  padding: 15px;
}

h1 {
  color: #007c91;
}

.card {
  background: white;
  max-width: 400px;
  margin: 15px auto;
  padding: 20px;
  border-radius: 15px;
  box-shadow: 0 3px 10px #bbb;
}

button {
  padding: 15px;
  margin: 5px;
  border: none;
  border-radius: 10px;
  background: #008cba;
  color: white;
  font-size: 16px;
}

.stop {
  background: red;
}

.gate {
  background: #00897b;
}

.feed {
  background: orange;
}

input {
  width: 90%;
}

</style>

</head>

<body>

<h1>AquaGuardian</h1>

<p>Smart Water Cleaning Robot</p>

<div class="card">

<h2>Robot Movement</h2>

<button onclick="move('forward')">
Forward
</button>

<br>

<button onclick="move('left')">
Left
</button>

<button class="stop" onclick="move('stop')">
STOP
</button>

<button onclick="move('right')">
Right
</button>

<br>

<button onclick="move('backward')">
Backward
</button>

<h3>Motor Speed</h3>

<input type="range" min="0" max="255"
value="160" id="speed"
oninput="speedVal.innerText=this.value">

<p>Speed: <span id="speedVal">160</span></p>

</div>

<div class="card">

<h2>Waste Collection Basket</h2>

<button class="gate"
onclick="gate('open')">
Open Gate
</button>

<button class="gate"
onclick="gate('close')">
Close Gate
</button>

</div>

<div class="card">

<h2>Automatic Fish Feeder</h2>

<button class="feed" onclick="feed()">
Feed Fish Now
</button>

<p id="status">System Ready</p>

</div>

<script>

function move(direction) {

  let speed =
    document.getElementById("speed").value;

  fetch("/move?dir=" + direction +
        "&speed=" + speed);

}

function gate(state) {

  fetch("/gate?state=" + state)
  .then(r => r.text())
  .then(t => status.innerText = t);

}

function feed() {

  fetch("/feed")
  .then(r => r.text())
  .then(t => status.innerText = t);

}

</script>

</body>
</html>

)rawliteral";

// ==========================================
// WEB SERVER
// ==========================================

void setupWebServer() {

  server.on("/", HTTP_GET, []() {

    server.send_P(200, "text/html", webpage);

  });

  server.on("/move", HTTP_GET, []() {

    String direction = server.arg("dir");

    if (server.hasArg("speed")) {
      motorSpeed = constrain(
        server.arg("speed").toInt(), 0, 255
      );
    }

    if (direction == "forward") {
      forward();
    }
    else if (direction == "backward") {
      backward();
    }
    else if (direction == "left") {
      turnLeft();
    }
    else if (direction == "right") {
      turnRight();
    }
    else {
      stopMotors();
    }

    server.send(200, "text/plain", "Command received");

  });

  server.on("/gate", HTTP_GET, []() {

    String state = server.arg("state");

    if (state == "open") {
      openGate();
      server.send(200, "text/plain", "Gate opened");
    }
    else {
      closeGate();
      server.send(200, "text/plain", "Gate closed");
    }

  });

  server.on("/feed", HTTP_GET, []() {

    if (feeding) {
      server.send(200, "text/plain",
                  "Feeding already in progress");
    }
    else {
      startFeeding();
      server.send(200, "text/plain",
                  "Feeding started");
    }

  });

  server.begin();
}

// ==========================================
// SETUP
// ==========================================

void setup() {

  Serial.begin(115200);

  // Motor pins
  pinMode(LEFT_EN, OUTPUT);
  pinMode(LEFT_IN1, OUTPUT);
  pinMode(LEFT_IN2, OUTPUT);

  pinMode(RIGHT_EN, OUTPUT);
  pinMode(RIGHT_IN1, OUTPUT);
  pinMode(RIGHT_IN2, OUTPUT);

  stopMotors();

  // Servo setup
  leftGate.setPeriodHertz(50);
  rightGate.setPeriodHertz(50);
  feeder.setPeriodHertz(50);

  leftGate.attach(LEFT_GATE_PIN, 500, 2400);
  rightGate.attach(RIGHT_GATE_PIN, 500, 2400);
  feeder.attach(FEEDER_PIN, 500, 2400);

  closeGate();
  feeder.write(FEED_HOME);

  // WiFi Access Point
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  Serial.println("AquaGuardian Started");

  Serial.print("IP Address: ");
  Serial.println(WiFi.softAPIP());

  setupWebServer();

  lastFeedTime = millis();
}

// ==========================================
// MAIN LOOP
// ==========================================

void loop() {

  server.handleClient();

  updateFeeding();

  // Automatic fish feeding every 6 hours
  if (!feeding &&
      millis() - lastFeedTime >= FEED_INTERVAL) {

    startFeeding();
  }

}
