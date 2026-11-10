#include "M5Unified.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>

const char* home_ssid = "Very Slow Wi-Fi";
const char* home_password = "CKMZ370CTTF868";

const char* school_ssid = "CCSecure";
const char* school_username = "tyang2896@conestogac.on.ca";
const char* school_password = "*}Dfw s`:6c\"iZJ/,}yk";

const char* mqtt_username = "delibe0152";
const char* mqtt_password = "0152";


const char *mqtt_broker = "p07da41d.ala.us-east-1.emqxsl.com";

const int mqtt_port = 8883;

const char* ca_cert = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDrzCCApegAwIBAgIQCDvgVpBCRrGhdWrJWZHHSjANBgkqhkiG9w0BAQUFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0wNjExMTAwMDAwMDBaFw0zMTExMTAwMDAwMDBaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IENBMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEA4jvhEXLeqKTTo1eqUKKPC3eQyaKl7hLOllsB
CSDMAZOnTjC3U/dDxGkAV53ijSLdhwZAAIEJzs4bg7/fzTtxRuLWZscFs3YnFo97
nh6Vfe63SKMI2tavegw5BmV/Sl0fvBf4q77uKNd0f3p4mVmFaG5cIzJLv07A6Fpt
43C/dxC//AH2hdmoRBBYMql1GNXRor5H4idq9Joz+EkIYIvUX7Q6hL+hqkpMfT7P
T19sdl6gSzeRntwi5m3OFBqOasv+zbMUZBfHWymeMr/y7vrTC0LUq7dBMtoM1O/4
gdW7jVg/tRvoSSiicNoxBN33shbyTApOB6jtSj1etX+jkMOvJwIDAQABo2MwYTAO
BgNVHQ8BAf8EBAMCAYYwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUA95QNVbR
TLtm8KPiGxvDl7I90VUwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUw
DQYJKoZIhvcNAQEFBQADggEBAMucN6pIExIK+t1EnE9SsPTfrgT1eXkIoyQY/Esr
hMAtudXH/vTBH1jLuG2cenTnmCmrEbXjcKChzUyImZOMkXDiqw8cvpOp/2PV5Adg
06O/nVsJ8dWO41P0jmP6P6fbtGbfYmbW0W5BjfIttep3Sp+dWOIrWcBAI+0tKIJF
PnlUkiaY4IBIqDfv8NZ5YBberOgOzW6sRBc4L0na4UU+Krk2U886UAb3LujEV0ls
YSEY1QSteDwsOoBrp+uvFRTp2InBuThs4pFsiv9kuXclVzDAGySj4dzp30d8tbQk
CAUw7C29C79Fv1C5qfPrmAESrciIxpg0X40KPMbp1ZWVbd4=
-----END CERTIFICATE-----
)EOF";

namespace {
constexpr char TOPIC_PREFIX[] = "SENG3030/Wednesday/delibe0152/";
constexpr char DISPLAY_COMMAND[] = "control/display/mode";
constexpr char ACQUISITION_COMMAND[] = "control/acquisition/send/";
constexpr uint32_t RETRY_INTERVAL_MS = 5000;
constexpr uint32_t HOLD_MS = 700;

struct Sensor {
    const char *name;
    const char *key;
};

constexpr Sensor SENSORS[] = {
    {"QMP", "qmp"},
    {"SHT30", "sht30"},
    {"IMU", "imu"},
};

WiFiClientSecure transport;
PubSubClient mqtt(transport);
size_t selectedSensor = 0;
String clientId;
uint32_t lastAttempt = 0;
bool attempted = false;
bool redraw = true;
const char *lastCommand = "";
const char *lastSensor = "";

enum class Connection { Unconfigured, WiFi, MQTT, Online };
Connection connection = Connection::Unconfigured;
#ifdef A4_SCHOOL_WIFI
const bool wifi_configured = school_ssid[0] != '\0' &&
                             school_username[0] != '\0' && school_password[0] != '\0';
constexpr char wifi_label[] = "Connecting: School";
#else
const bool wifi_configured = home_ssid[0] != '\0';
constexpr char wifi_label[] = "Connecting: Home";
#endif

const bool configured = wifi_configured &&
                        mqtt_username[0] != '\0' && mqtt_password[0] != '\0';

const char *connectionLabel() {
    switch (connection) {
        case Connection::Unconfigured: return "Set network_config.h";
        case Connection::WiFi: return wifi_label;
        case Connection::MQTT: return "Connecting to MQTT";
        case Connection::Online: return "MQTT online";
    }
    return "";
}

void setConnection(Connection next) {
    if (connection != next) {
        connection = next;
        redraw = true;
    }
}

void setup_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    clientId = "seng3030-control-" + WiFi.macAddress();

#ifdef A4_SCHOOL_WIFI
    Serial.println("Connecting to school Wi-Fi (PEAP)");
    WiFi.begin(school_ssid, WPA2_AUTH_PEAP, school_username, school_username, school_password);
#else
    Serial.println("Connecting to home Wi-Fi");
    WiFi.begin(home_ssid, home_password);
#endif

    setConnection(Connection::WiFi);
}

void drawScreen() {
    auto &display = M5.Display;
    display.fillScreen(TFT_BLACK);
    display.setCursor(4, 4);
    display.println("MQTT CONTROL");
    display.printf("Sensor: %s\n", SENSORS[selectedSensor].name);
    display.println(connectionLabel());
    display.println("A: Next   B: Show");
    display.println("Hold A: Send ON");
    display.println("Hold B: Send OFF");
    display.printf("%s %s\n", lastSensor, lastCommand);
    redraw = false;
}

void updateConnection() {
    if (!configured) {
        return;
    }
    if (WiFi.status() != WL_CONNECTED) {
        if (connection != Connection::WiFi) {
            transport.stop();
            attempted = false;
        }
        setConnection(Connection::WiFi);
        return;
    }
    if (mqtt.connected() && mqtt.loop()) {
        setConnection(Connection::Online);
        return;
    }

    setConnection(Connection::MQTT);
    if (attempted && millis() - lastAttempt < RETRY_INTERVAL_MS) {
        return;
    }

    // PubSubClient connects synchronously. Bound the wait and space out retries.
    if (mqtt.connect(clientId.c_str(), mqtt_username, mqtt_password)) {
        Serial.printf("MQTT connected as %s\n", mqtt_username);
        setConnection(Connection::Online);
    } else {
        Serial.printf("MQTT connection failed (state %d)\n", mqtt.state());
    }
    lastAttempt = millis();
    attempted = true;
}

void publishCommand(const String &suffix, const char *payload, bool retained,
                    const char *description) {
    lastSensor = SENSORS[selectedSensor].name;
    if (connection != Connection::Online) {
        lastCommand = "offline";
    } else {
        const String topic = String(TOPIC_PREFIX) + suffix;
        const bool sent = mqtt.publish(topic.c_str(), payload, retained);
        lastCommand = sent ? description : "send failed";
        Serial.printf("%s %s = %s (QoS 0, retain=%d)\n", sent ? "Wrote" : "Failed",
                      topic.c_str(), payload, retained);
    }
    redraw = true;
}

void handleButtons() {
    const Sensor &sensor = SENSORS[selectedSensor];
    if (M5.BtnA.wasClicked()) {
        selectedSensor = (selectedSensor + 1) % (sizeof(SENSORS) / sizeof(SENSORS[0]));
        redraw = true;
    } else if (M5.BtnA.wasHold()) {
        publishCommand(String(ACQUISITION_COMMAND) + sensor.key, "1", true, "ON sent");
    } else if (M5.BtnB.wasHold()) {
        publishCommand(String(ACQUISITION_COMMAND) + sensor.key, "0", true, "OFF sent");
    } else if (M5.BtnB.wasClicked()) {
        publishCommand(DISPLAY_COMMAND, sensor.key, false, "page sent");
    }
}
}  // namespace

void setup() {
    auto config = M5.config();
    M5.begin(config);
    Serial.begin(115200);
    M5.Display.setRotation(1);
    M5.Display.setTextSize(1);
    M5.BtnA.setHoldThresh(HOLD_MS);
    M5.BtnB.setHoldThresh(HOLD_MS);

    if (configured) {
        setup_wifi();

        transport.setCACert(ca_cert);
        transport.setTimeout(3);
        transport.setHandshakeTimeout(3);
        mqtt.setServer(mqtt_broker, mqtt_port);
        mqtt.setSocketTimeout(3);
    }
    drawScreen();
}

void loop() {
    M5.update();
    handleButtons();
    updateConnection();
    if (redraw) {
        drawScreen();
    }
}
