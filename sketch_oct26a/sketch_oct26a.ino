
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <DHT.h>


#define DHTPIN 11   // I used pin 11 on my esp32. Avoid 12, 9, 8 and 13
#define DHTTYPE DHT11

DHT dht(DHTPIN, DHTTYPE);

// ---------- Wi-Fi ----------
const char* ssid = "Kumene"; // I added my wi-fi  name it can be wusu or eduroam or BCC
const char* password = "********"; I need to add my wifi password. For future i will need to put it in an .env in my githut 

// ---------- ntfy ----------
const char* NTFY_TOPIC = "iwusu_sensor"; // I configured this name on the ntfy app when i registered

// At what point shoul I raise alarm
const float TEMP_LIMIT = 22.0;
const float RESET_LIMIT = 20.0;

// timer for our esp32 to perform some actions for reading after every 2 seconds and updating after 60 seconds
const unsigned long READ_INTERVAL = 2000UL;
const unsigned long UPDATE_INTERVAL = 60000UL;

unsigned long lastRead = 0;
unsigned long lastUpdate = 0;

bool alertSent = false;
bool firstReading = true;

float temperature = NAN;
float humidity = NAN;

WebServer server(80);

// ---------- Send ntfy notification  to the app ----------
bool sendNotification(const String& title,
                      const String& message,
                      const char* priority) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi disconnected; notification skipped.");
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  String url = String("https://ntfy.sh/") + NTFY_TOPIC;

  if (!https.begin(client, url)) {
    Serial.println("Could not start ntfy HTTPS request.");
    return false;
  }

  https.addHeader("Content-Type", "text/plain");
  
  https.addHeader("Content-Type", "text/plain");
  https.addHeader("Title", " Ekasi Temperature Alert");
  https.addHeader("Priority", "urgent");
  https.addHeader("Tags", "warning,thermometer");


  https.setConnectTimeout(5000);
  https.setTimeout(8000);

  int code = https.POST(message);

  Serial.print("ntfy HTTP status: ");
  Serial.println(code);

  https.end();

  return code >= 200 && code < 300;
}

// ---------- Web dashboard ----------
void handleRoot() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta http-equiv="refresh" content="3">
  <title>Environment Monitor</title>
  <style>
    /* Fire glow background animation */
    body {
      font-family: Arial, sans-serif;
      text-align: center;
      background: radial-gradient(circle at bottom, #721c24 0%, #1a0505 70%, #0d0202 100%);
      animation: fireFlicker 2s infinite alternate;
      padding: 20px;
      margin: 0;
      min-height: 100vh;
      color: #fff;
    }
    
    @keyframes fireFlicker {
      0% { background: radial-gradient(circle at bottom, #9c27b0 0%, #1a0505 70%, #0d0202 100%); filter: brightness(0.9); }
      25% { background: radial-gradient(circle at bottom, #b71c1c 0%, #210404 65%, #0d0202 100%); }
      50% { background: radial-gradient(circle at bottom, #e65100 0%, #1c0505 75%, #0d0202 100%); filter: brightness(1.1); }
      75% { background: radial-gradient(circle at bottom, #ff8f00 0%, #260505 70%, #0d0202 100%); }
      100% { background: radial-gradient(circle at bottom, #d84315 0%, #1a0505 65%, #0d0202 100%); filter: brightness(1); }
    }

    /* Adjusted card styling to look crisp against the dark fire theme */
    .card {
      background: rgba(255, 255, 255, 0.1);
      backdrop-filter: blur(10px);
      -webkit-backdrop-filter: blur(10px);
      border: 1px solid rgba(255, 255, 255, 0.2);
      padding: 24px;
      margin: 20px auto;
      max-width: 350px;
      border-radius: 18px;
      box-shadow: 0 8px 32px 0 rgba(0, 0, 0, 0.37);
    }
    
    h1 { 
      color: #ff9800; 
      text-shadow: 0 0 10px rgba(255,152,0,0.5);
    }
    
    .value {
      font-size: 34px;
      font-weight: bold;
      color: #ffeb3b;
      text-shadow: 0 0 8px rgba(255,235,59,0.4);
    }
    
    .label { 
      color: #e0e0e0; 
      font-weight: bold;
    }
    
    .alert-text {
      color: #ffc107;
    }
  </style>
</head>
<body>
  <h1>Ekasi Fire Monitor</h1>
  <div class="card">
)rawliteral";

  if (isnan(temperature) || isnan(humidity)) {
    html += "<p>Waiting for valid sensor readings...</p>";
  } else {
    html += "<p class='label'>Temperature</p><div class='value'>";
    html += String(temperature, 1) + " &deg;C</div>";

    html += "<p class='label'>Humidity</p><div class='value'>";
    html += String(humidity, 1) + " %</div>";
  }

  html += "<p class='alert-text'>Temperature alert: ";
  html += String(TEMP_LIMIT, 1);
  html += " &deg;C</p></div></body></html>";

  server.send(200, "text/html", html);
}


// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  delay(1000);

  dht.begin();

  Serial.println("Connecting to Wi-Fi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWi-Fi connected!");
  Serial.print("Dashboard: http://");
  Serial.println(WiFi.localIP());

  server.on("/", handleRoot);
  server.begin();

  Serial.println("Web server started.");

  // Reading sensor reading at startup
  temperature = dht.readTemperature();
  humidity = dht.readHumidity();

  // Startup notification is shown irrespective
  sendNotification(
    "Ekasi Monitor Online",
    "App working fine. Temperature monitoring started.",
    "high"
  );
}

// ---------- Main loop ----------
void loop() {
  server.handleClient();

  unsigned long now = millis();

  // Reading temp sensor every 2 seconds
  if (firstReading || now - lastRead >= READ_INTERVAL) {
    firstReading = false;
    lastRead = now;

    float t = dht.readTemperature();
    float h = dht.readHumidity();

    if (!isnan(t) && !isnan(h)) {
      temperature = t;
      humidity = h;

      Serial.print("Temperature: ");
      Serial.print(temperature);
      Serial.print(" C | Humidity: ");
      Serial.print(humidity);
      Serial.println(" %");

      // Alert when temperature reaches the programmed threshold
      if (temperature >= TEMP_LIMIT && !alertSent) {
        String message = "HIGH TEMPERATURE ALERT! Temperature: ";
        message += String(temperature, 1);
        message += " C. Humidity: ";
        message += String(humidity, 1);
        message += " %. Limit: ";
        message += String(TEMP_LIMIT, 1);
        message += " C.";

        bool sent = sendNotification(
          "URGENT: High Temperature!",
          message,
          "urgent"
        );

        // Retry on a later sensor reading if sending failed.
        if (sent) {
          alertSent = true;
        }
      }

      // Allow a new alert after temperature returns below 28 C.
      if (temperature < RESET_LIMIT) {
        alertSent = false;
      }
    } else {
      Serial.println("DHT11 read failed.");
    }
  }

  // Routine temperature/humidity update every 60 seconds.
  if (now - lastUpdate >= UPDATE_INTERVAL) {
    lastUpdate = now;

    if (!isnan(temperature) && !isnan(humidity)) {
      String message = "Temperature: ";
      message += String(temperature, 1);
      message += " C\nHumidity: ";
      message += String(humidity, 1);
      message += " %\nNext routine update in 60 seconds.";

      sendNotification(
        "Environment Monitor Update",
        message,
        "default"
      );
    }
  }
}
