#include <Wire.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <PubSubClient.h>

String globalToken;


// WiFi credentials
const char* ssid = "xxxxxxxxxxx";
const char* password = "xxxxxxxxxxx";
const char* mqttServer = "192.168.xxxxxx.xxxxxxxx";
const int mqttPort = 1883;
const char* mqtt_user = "xxxxxxxx";
const char* mqtt_password = "xxxxxxxx";

String web_user = "xxxxxxxxx@xxxx.com";
String web_password = "xxxxxxxxxx";
String authsignature = "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx";
struct SensorConfig {
  String name;
  String valueTemplate;
  String unit;
};

const int numSensors = 5;
SensorConfig sensorConfigs[numSensors] = {
  {"ESP32 Battery Percentage", "{{ value_json.Battery }}", "%"},
  {"ESP32 Grid Consumption", "{{ value_json.GridCons }}", "W"},
  {"ESP32 House Consumption", "{{ value_json.HouseCons }}", "W"},
  {"ESP32 Battery Power", "{{ value_json.BatteryPwr }}", "W"},
  {"ESP32 Time", "{{ value_json.Time }}", ""}
};

WiFiClient espClient;
PubSubClient mqttClient(espClient);



struct SOCData {
  float soc;
  float gridConsumption;
  float houseConsumption;
  float battery;
  String createTime;
};

void displayRetryMessage() {
  String retryMessage = "Token not valid\nRetrying...";
  updateDisplay(retryMessage);
}

void displayToken() {
  String tokenMessage = "Token: \n" + String(globalToken);
  updateDisplay(tokenMessage);
}

void connectToMQTT() {
  while (!mqttClient.connected()) {
    updateDisplay("Connecting to MQTT...");

    if (mqttClient.connect("ESP32Client", mqtt_user, mqtt_password)) {
      updateDisplay("Connected to MQTT");
      mqttClient.setCallback(callback);
      mqttClient.subscribe("homeassistant/homebattery/command", 1);
      break; // Exit the loop once connected
    } else {
      String failMessage = "MQTT Connect Failed. State: " + String(mqttClient.state());
      updateDisplay(failMessage);
      delay(500); // Wait before retrying
    }
  }
}


void updateDisplay(const String &message) {
 
}

String getUniqueIdentifier() {
  uint8_t baseMac[6];
  // Get the base MAC address
  esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
  char baseMacChr[18] = {0};
  sprintf(baseMacChr, "%02X:%02X:%02X:%02X:%02X:%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);
  return String(baseMacChr);
}
void getToken() {
  bool validToken = false;
  while (!validToken) {
    HTTPClient http;
    String payload = "{\"username\": \"" + web_user + "\", \"password\": \"" + web_password + "\"}";
    String url = "https://monitor.byte-watt.com/api/Account/Login?authsignature=" + authsignature + "&authtimestamp=11111";

    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(payload);

    if (httpResponseCode > 0) {
      String response = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);
      if (response.length() > 20 && !doc["data"]["AccessToken"].isNull()) {
        globalToken = doc["data"]["AccessToken"].as<String>();
        validToken = true;
        displayToken();
      } else {
        displayRetryMessage();
        delay(5000); // Wait for 5 seconds
      }
    } else {
      displayRetryMessage();
      delay(5000); // Wait for 5 seconds
    }

    http.end();
  }
}

void setAuthorizationHeader(HTTPClient &http) {
  http.addHeader("Content-Type", "application/json");
  http.addHeader("authtimestamp", "11111");
  http.addHeader("authsignature", authsignature);

  if (globalToken.isEmpty()) {
    getToken();
  }
  String authHeader = "Bearer " + globalToken;
  http.addHeader("Authorization", authHeader);
}

SOCData getSOC() {
  HTTPClient http;
  String url = "https://monitor.byte-watt.com/api/ESS/GetLastPowerDataBySN?sys_sn=All&noLoading=true";
  SOCData result;
  result.soc = -1.0; // Default failure value
  result.gridConsumption = 0.0;
  result.houseConsumption = 0.0;
  result.battery = 0.0;
  result.createTime = ""; // Default empty string

  bool tokenUpdated = false; // Flag to prevent infinite loop

  while (true) {
    http.begin(url);
    setAuthorizationHeader(http); // Set the headers with the current token

    int httpResponseCode = http.GET();

    if (httpResponseCode > 0) {
      String response = http.getString();
      DynamicJsonDocument doc(1024);
      deserializeJson(doc, response);

      if (response.indexOf("Unauthorized") >= 0 && !tokenUpdated) {
        globalToken = ""; // Reset the token
        getToken();
        tokenUpdated = true; // Prevent multiple token updates
      } else if (response.indexOf("Network exception") < 0) {
        // Parse JSON and get SOC and createTime
        result.soc = doc["data"]["soc"].as<float>();
        result.gridConsumption = doc["data"]["pmeter_l1"].as<float>();
        result.battery = doc["data"]["pbat"].as<float>();
        result.houseConsumption = result.gridConsumption + result.battery;
        result.createTime = doc["data"]["createtime"].as<String>();
        break; // Break the loop if successful
      }
    } else {
      Serial.print("Error on sending GET: ");
      Serial.println(httpResponseCode);
      break; // Break the loop if there's an HTTP error
    }

    http.end();
    delay(1000); // Wait a bit before retrying to avoid flooding the server
  }

  http.end();
  return result;
}

void displaySOCAndTime(SOCData data) {
  String displayMessage = "";
  displayMessage += "Time: " + String(data.createTime) + "\n";
  displayMessage += "Battery: " + String(data.soc) + "%\n";
  displayMessage += "Grid Cons: " + String(data.gridConsumption) + "W\n";
  displayMessage += "House Cons: " + String(data.houseConsumption) + "W\n";
  displayMessage += "Battery Pwr: " + String(data.battery) + "W";

  updateDisplay(displayMessage);
}


void ensureWiFiConnected() {
  if (WiFi.status() != WL_CONNECTED) {
    updateDisplay("Reconnecting WiFi...");

    WiFi.disconnect();
    WiFi.mode(WIFI_OFF); // Turn off WiFi
    delay(1000); // Wait for WiFi to turn off

    int reconnectAttempts = 0;
    const int maxReconnectAttempts = 10; // Maximum number of reconnection attempts

    while (WiFi.status() != WL_CONNECTED && reconnectAttempts < maxReconnectAttempts) {
      WiFi.mode(WIFI_STA); // Turn on WiFi in Station mode
      WiFi.begin(ssid, password); // Begin WiFi connection with your credentials

      int connectTimeout = 500 + (reconnectAttempts * 500); // Increase timeout each attempt
      delay(connectTimeout);
      Serial.print(".");
      reconnectAttempts++;
    }

    if (WiFi.status() == WL_CONNECTED) {
      updateDisplay("WiFi Reconnected");
    } else {
      updateDisplay("WiFi Reconnect Failed");
      // Additional failure handling code here
      // Consider restarting the ESP32
      ESP.restart(); // This restarts the ESP32
    }
  }
}



void setup() {
  Serial.begin(115200);


  // Start connecting to WiFi
  updateDisplay(F("Connecting to WiFi"));
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  // Display WiFi connected message
  String connectedMsg = "WiFi connected\nIP: " + WiFi.localIP().toString();
  updateDisplay(connectedMsg);

  getToken(); 
  updateDisplay("MQTT Setup");

  mqttClient.setServer(mqttServer, mqttPort);
  connectToMQTT();
  
  // Publish MQTT Discovery Messages
  String baseTopic = "homeassistant/sensor/homebattery/";
  String uniqueId = getUniqueIdentifier();

  for (int i = 0; i < numSensors; i++) {
      String sensorType = sensorConfigs[i].name;
      sensorType.toLowerCase(); // Convert sensorType to lowercase
      String configTopic = baseTopic + sensorType + "/config";
      
      String payload = "{\"name\": \"" + sensorConfigs[i].name + "\", \"state_topic\": \"homeassistant/sensor/homebattery\", \"value_template\": \"" + sensorConfigs[i].valueTemplate + "\", \"unit_of_measurement\": \"" + sensorConfigs[i].unit + "\", \"device\": {\"identifiers\": [\"" + uniqueId + "\"], \"name\": \"ESP32 Device\", \"model\": \"ESP32\", \"manufacturer\": \"Espressif\"}}";
      
      mqttClient.publish(configTopic.c_str(), payload.c_str());
  }
}

void callback(char* topic, byte* message, unsigned int length) {
  String displayMessage = "Message arrived on topic: \n";
  displayMessage += String(topic) + "\n";
  displayMessage += ". Message: \n";
  String transferMessage = "";
  for (int i = 0; i < length; i++) {
    displayMessage += (char)message[i];
    transferMessage += (char)message[i];
  }

  updateDisplay(displayMessage); // Update the display with new data
  setSettings(transferMessage);
}



void setSettings(String x) {
    HTTPClient http;
    bool requestSuccessful = false;
    int maxRetries = 10;  // Set a maximum number of retries to avoid infinite loop
    int currentTry = 0;

    while (!requestSuccessful && currentTry < maxRetries) {
    http.begin("https://monitor.byte-watt.com/api/Account/CustomUseESSSetting"); // Specify the URL

    // Set request headers using the existing function
    setAuthorizationHeader(http);

    // JSON payload -- All of these values will overwrite what your settings are
    // They are ALL needed to make the request valid
    DynamicJsonDocument doc(1024);
    doc["grid_charge"] = 1;
    doc["ctr_dis"] = 1;
    doc["bat_use_cap"] = 6; // Stop discharging battery at Battery %
    doc["time_chaf1a"] = "10:00"; // Start Charging time
    doc["time_chae1a"] = "16:00"; // End Charging time
    doc["time_chaf2a"] = "00:00";
    doc["time_chae2a"] = "00:00";
    doc["time_disf1a"] = "16:00"; // Start Discharging time
      doc["time_dise1a"] = x; // End Discharging time - Variable placeholder
    doc["time_disf2a"] = "06:00"; // Start Discharging time - part 2
    doc["time_dise2a"] = "10:00"; // End Discharging time - part 2
    doc["bat_high_cap"] = "100";
    doc["time_cha_fwe1a"] = "00:00";
    doc["time_cha_ewe1a"] = "00:00";
    doc["time_cha_fwe2a"] = "00:00";
    doc["time_cha_ewe2a"] = "00:00";
    doc["time_dis_fwe1a"] = "00:00";
    doc["time_dis_ewe1a"] = "00:00";
    doc["time_dis_fwe2a"] = "00:00";
    doc["time_dis_ewe2a"] = "00:00";
    doc["peak_s1a"] = "00:00";
    doc["peak_e1a"] = "00:00";
    doc["peak_s2a"] = "00:00";
    doc["peak_e2a"] = "00:00";
    doc["fill_s1a"] = "00:00";
    doc["fill_e1a"] = "00:00";
    doc["fill_s2a"] = "00:00";
    doc["fill_e2a"] = "00:00";
    doc["pm_offset_s1a"] = "00:00";
    doc["pm_offset_e1a"] = "00:00";
    doc["pm_offset_s2a"] = "00:00";
    doc["pm_offset_e2a"] = "00:00";

    String payload;
    serializeJson(doc, payload);

    // Send the request
    int httpResponseCode = http.POST(payload);
    String displayMessage = "";

    if (httpResponseCode > 0) {
        String response = http.getString();
        displayMessage += "Response code: \n";
        displayMessage += String(httpResponseCode) + "\n";
        displayMessage += "Response: " + response + "\n";
        if (response.indexOf("Success") >= 0) {
            requestSuccessful = true; // Break the loop if "Success" is found in response
        }
    } else {
        displayMessage += "Error on sending POST: \n";
        displayMessage += String(httpResponseCode) + "\n";
        displayMessage += "x: " + String(x) + "\n";
    }

    updateDisplay(displayMessage);

    http.end();
        if (!requestSuccessful) {
            delay(400);
            updateDisplay("Retrying");
            delay(100);

            currentTry++;
        }
    }
}

void loop() {
  ensureWiFiConnected(); // Check and maintain WiFi connection

  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();

  static unsigned long lastUpdateTime = 0;
  if (millis() - lastUpdateTime > 30000) { // Update every 15 seconds
    SOCData data = getSOC();
    displaySOCAndTime(data);
    lastUpdateTime = millis();
    // Publish data to MQTT
    DynamicJsonDocument doc(1024);
    float targetSoc = round(data.soc*100.0)/100.0;
    if (targetSoc > 0)
    {
      doc["Time"] = data.createTime;
      doc["Battery"] = round(data.soc*100.0)/100.0;
      doc["GridCons"] = data.gridConsumption;
      doc["HouseCons"] = data.houseConsumption;
      doc["BatteryPwr"] = data.battery;

      String payload;
      serializeJson(doc, payload);
      mqttClient.publish("homeassistant/sensor/homebattery", payload.c_str());
    }
  }
  delay(1000);
}
