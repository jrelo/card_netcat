#include <WiFi.h>
#include <M5Cardputer.h>
#include <FS.h>
#include <SPIFFS.h>

String wifi_ssid = "";
String wifi_password = "";

M5Canvas canvas(&M5Cardputer.Display);

void waitForInput(String& input);
void readFile(String path, String& content);
void writeFile(String path, String content);
bool fileExists(String path);
bool promptUseSavedCredentials(const String& message);
bool promptSaveCredentials(const String& message);
bool promptRetry(const String& message);
void connectionTest();

void setup() {
    Serial.begin(115200);
    Serial.println("Starting Setup");

    if (!SPIFFS.begin(true)) {
        Serial.println("SPIFFS Mount Failed");
        return;
    }

    auto cfg = M5.config();
    M5Cardputer.begin(cfg, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextSize(1);

    if (fileExists("/wifi_creds")) {
        if (promptUseSavedCredentials("Use saved WiFi credentials?")) {
            M5Cardputer.Display.print("Using saved WiFi credentials...\n");
            String wifiCredentials;
            readFile("/wifi_creds", wifiCredentials);
            Serial.println("Read WiFi credentials from file: " + wifiCredentials);
            int newlineIndex = wifiCredentials.indexOf('\n');
            wifi_ssid = wifiCredentials.substring(0, newlineIndex);
            wifi_password = wifiCredentials.substring(newlineIndex + 1);
            M5Cardputer.Display.printf("SSID: %s\nPassword: %s\n", wifi_ssid.c_str(), wifi_password.c_str());
        }
    }

    if (wifi_ssid == "" || wifi_password == "") {
        M5Cardputer.Display.print("SSID: ");
        waitForInput(wifi_ssid);
        M5Cardputer.Display.print("\nPassword: ");
        waitForInput(wifi_password);

        if (promptSaveCredentials("Save WiFi credentials?")) {
            SPIFFS.remove("/wifi_creds");
            writeFile("/wifi_creds", wifi_ssid + "\n" + wifi_password);
            Serial.println("Saved WiFi credentials: " + wifi_ssid + "\n" + wifi_password);
        }
    }

    while (true) {
        WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
        M5Cardputer.Display.print("Connecting to WiFi");
        int retryCount = 0;
        while (WiFi.status() != WL_CONNECTED && retryCount < 20) {
            delay(500);
            M5Cardputer.Display.print(".");
            retryCount++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            M5Cardputer.Display.printf("\nWiFi Connected, IP: %s\n", WiFi.localIP().toString().c_str());

            String macAddress = WiFi.macAddress();
            M5Cardputer.Display.printf("MAC Address: %s\n", macAddress.c_str());

            connectionTest();
            break;
        } else {
            M5Cardputer.Display.print("\nFailed to connect to WiFi.\n");
            if (!promptRetry("Retry WiFi connection?")) {
                ESP.restart();
            } else {
                M5Cardputer.Display.clear();
            }
        }
    }
}

void loop() {
    M5Cardputer.update();
}

void waitForInput(String& input) {
    unsigned long startTime = millis();
    unsigned long lastKeyPressMillis = 0;
    const unsigned long debounceDelay = 200;
    String currentInput = input;

    while (true) {
        M5Cardputer.update();
        if (M5Cardputer.Keyboard.isChange()) {
            Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

            if (status.del && currentInput.length() > 0) {
                // Handle backspace key
                currentInput.remove(currentInput.length() - 1);
                M5Cardputer.Display.setCursor(
                    M5Cardputer.Display.getCursorX() - 6,
                    M5Cardputer.Display.getCursorY());
                M5Cardputer.Display.print(" ");
                M5Cardputer.Display.setCursor(
                    M5Cardputer.Display.getCursorX() - 6,
                    M5Cardputer.Display.getCursorY());
                lastKeyPressMillis = millis();
            }

            for (auto i : status.word) {
                if (millis() - lastKeyPressMillis >= debounceDelay) {
                    currentInput += i;
                    M5Cardputer.Display.print(i);
                    lastKeyPressMillis = millis();
                }
            }

            if (status.enter) {
                M5Cardputer.Display.println();
                input = currentInput;
                break;
            }
        }

        if (millis() - startTime > 120000) {
            M5Cardputer.Display.println("\nInput timeout. Rebooting...");
            delay(1000);  
            ESP.restart();
        }
    }
}

void readFile(String path, String& content) {
    File file = SPIFFS.open(path, "r");
    if (!file) {
        Serial.println("Failed to open file for reading");
        return;
    }

    while (file.available()) {
        content += file.readString();
    }
    file.close();
}

void writeFile(String path, String content) {
    File file = SPIFFS.open(path, "w");
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    file.print(content);
    file.close();
}

bool fileExists(String path) {
    return SPIFFS.exists(path);
}

bool promptUseSavedCredentials(const String& message) {
    M5Cardputer.Display.print(message + " (y/n): ");
    String response;
    waitForInput(response);
    return response.equalsIgnoreCase("y");
}

bool promptSaveCredentials(const String& message) {
    M5Cardputer.Display.print(message + " (y/n): ");
    String response;
    waitForInput(response);
    return response.equalsIgnoreCase("y");
}

bool promptRetry(const String& message) {
    M5Cardputer.Display.print(message + " (y/n): ");
    String response;
    waitForInput(response);
    return response.equalsIgnoreCase("y");
}

void connectionTest() {
    while (true) {
        M5Cardputer.Display.print("IP: ");
        String ipAddressStr;
        waitForInput(ipAddressStr);
        IPAddress ipAddress;
        if (!ipAddress.fromString(ipAddressStr)) {
            M5Cardputer.Display.print("Invalid IP.\n");
            continue;
        }

        M5Cardputer.Display.print("Port: ");
        String portStr;
        waitForInput(portStr);
        int port = portStr.toInt();

        WiFiClient client;
        M5Cardputer.Display.printf("Connecting to %s:%d\n", ipAddress.toString().c_str(), port);
        if (client.connect(ipAddress, port)) {
            M5Cardputer.Display.printf("Connected to %s:%d\n", ipAddress.toString().c_str(), port);
            
            client.print("HELO\n");
            while (client.connected()) {
                while (client.available()) {
                    String line = client.readStringUntil('\n');
                    M5Cardputer.Display.println(line);
                }

                M5Cardputer.update();
                delay(100);

                if (M5Cardputer.BtnA.wasPressed()) {
                    client.stop();
                    break;
                }
            }

            if (!client.connected()) {
                M5Cardputer.Display.printf("Disconnected from %s:%d\n", ipAddress.toString().c_str(), port);
            }
        } else {
            M5Cardputer.Display.printf("Failed to connect to %s:%d\n", ipAddress.toString().c_str(), port);
        }

        if (!promptRetry("Restart?")) {
            ESP.restart();
        } else {
            M5Cardputer.Display.clear();
        }
    }
}
