#include "almighty_ir.h"
#include "TV-B-Gone.h" // for checkIrTxPin()
#include "core/display.h"
#include "core/mykeyboard.h"
#include "core/sd_functions.h"
#include "core/settings.h"
#include "core/type_convertion.h"
#include "custom_ir.h"
#include <HTTPClient.h>
#include <IRutils.h>
#include <WiFi.h>
#include <WiFiUdp.h>

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Custom IR for daily use

static std::vector<IRCode *> codes;

void almightyResetCodesArray() {
    for (auto code : codes) { delete code; }
    codes.clear();
}

void bruceAlmighty() {
    if (bruceConfig.wifiAtStartup) {
        if (WiFi.status() != WL_CONNECTED) {
            displayTextLine("Connecting..");
            unsigned long timeout = millis() + 20000;
            while (WiFi.status() != WL_CONNECTED) {
                if (millis() > timeout) {
                    displayTextLine("No Wifi");
                    break;
                }
                delay(500);
            }
        }
        delay(1000);
    }
    checkIrTxPin();
    int total_codes = 0;
    String filepath;
    File databaseFile;
    FS *fs = setupSdCard() ? static_cast<FS *>(&SD) : static_cast<FS *>(&LittleFS);
    String line;
    String txt;
    String token;

    returnToMenu = true; // make sure menu is redrawn when quitting in any point

    while (true) {
        token = "";
        total_codes = 0;
        almightyResetCodesArray();
        // select a file to tx
        if (!(*fs).exists("/BruceIR")) (*fs).mkdir("/BruceIR/almighty");
        filepath = loopSD(*fs, true, "IR", "/BruceIR/almighty");
        if (filepath == "") return; //  cancelled

        // select mode
        bool exit = false;
        bool back = false;
        bool mode_cmd = true;

        databaseFile = fs->open(filepath, FILE_READ);
        drawMainBorder();

        if (!databaseFile) {
            Serial.println("Failed to open database file.");
            // displayError("Fail to open file");
            // delay(2000);
            return;
        }
        Serial.println("Opened database file.");

        pinMode(bruceConfigPins.irTx, OUTPUT);
        // digitalWrite(bruceConfig.irTx, LED_ON);

        // Mode to choose and send command by command limitted to 100 commands
        codes.push_back(new IRCode());
        Serial.println("Reading database file.");
        while (databaseFile.available() && total_codes < 100) {
            line = databaseFile.readStringUntil('\n');
            txt = line.substring(line.indexOf(":") + 1);
            txt.trim();
            if (line.startsWith("name:")) {
                // in case that the separation between codes are not made by "#" line
                if (codes[total_codes]->name != "") {
                    total_codes++;
                    codes.push_back(new IRCode());
                }
                // save signal name
                codes[total_codes]->name = txt;
                codes[total_codes]->filepath = txt + " " + filepath.substring(1 + filepath.lastIndexOf("/"));
            }
            if (line.startsWith("type:")) codes[total_codes]->type = txt;
            if (line.startsWith("protocol:")) codes[total_codes]->protocol = txt;
            if (line.startsWith("address:")) codes[total_codes]->address = txt;
            if (line.startsWith("frequency:") || line.startsWith("port:"))
                codes[total_codes]->frequency = txt.toInt();
            if (line.startsWith("bits:")) codes[total_codes]->bits = txt.toInt();
            if (line.startsWith("command:")) codes[total_codes]->command = txt;
            if (line.startsWith("token:")) token = txt;
            if (line.startsWith("data:") || line.startsWith("value:") || line.startsWith("state:")) {
                codes[total_codes]->data = txt;
            }
            // if there are a line with "#", and the code name isnt't "" (there are a signal saved), go to
            // next signal
            if (line.startsWith("#") && total_codes < codes.size() && codes[total_codes]->name != "") {
                total_codes++;
                codes.push_back(new IRCode());
            }
            // if(line.startsWith("duty_cycle:")) codes[total_codes]->duty_cycle = txt.toFloat();
        }
        Serial.println("Database file read.");
        options = {};
        for (auto code : codes) {
            if (code->name != "") {
                options.push_back({code->name.c_str(), [code, token]() {
                                       if (code->type.equalsIgnoreCase("POST")) {
                                           sendPostCommand(code, token);
                                       } else if (code->type.equalsIgnoreCase("GET")) {
                                           sendGetCommand(code);
                                       } else if (code->type.equalsIgnoreCase("UDP")) {
                                           sendUDPCommand(code);
                                       } else {
                                           sendIRCommand(code);
                                           //    addToRecentCodes(code);
                                       }
                                   }});
            }
        }
        options.push_back({"> Back", [&]() { back = true; }, true});
        options.push_back({"> Exit", [&]() { exit = true; }, true});
        databaseFile.close();

#ifdef USE_BQ25896 /// DISABLE 5V OUTPUT
        PPM.disableOTG();
#endif

        digitalWrite(bruceConfigPins.irTx, LED_OFF);
        int idx = 0;
        while (1) {
            idx = loopOptions(options, idx);
            if (check(EscPress) || exit || back) break;
        }
        options.clear();

        if (!back) { return; }
    }
} // end of otherIRcodes

void sendPostCommand(IRCode *code, String token) {
    if (WiFi.status() == WL_CONNECTED) {
        displayTextLine("Sending..");
        HTTPClient http;

        http.begin(code->address);
        http.addHeader("Content-Type", "application/json");
        http.addHeader("Authorization", "Bearer " + token);

        int httpResponseCode = http.POST(code->data);

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Response:");
            Serial.println(response);
        } else {
            Serial.print("Error on sending POST: ");
            Serial.println(httpResponseCode);
        }

        http.end();
    } else {
        displayTextLine("No Wifi");
        delay(500);
    }
}

void bruceAlmightyM5Cc() {
    FS *fs = setupSdCard() ? static_cast<FS *>(&SD) : static_cast<FS *>(&LittleFS);
    resetTftDisplay();
    drawPNG(*fs, "/BruceIR/almighty/icons/bd.png", 0, 0, false);
    while (1) {}
}

void sendGetCommand(IRCode *code) {
    if (WiFi.status() == WL_CONNECTED) {
        displayTextLine("Sending..");
        HTTPClient http;

        http.begin(code->address);

        int httpResponseCode = http.GET();

        if (httpResponseCode > 0) {
            String response = http.getString();
            Serial.println("Response:");
            Serial.println(response);
        } else {
            Serial.print("Error on sending GET: ");
            Serial.println(httpResponseCode);
        }

        http.end();
    } else {
        displayTextLine("No Wifi");
        delay(500);
    }
}

void sendUDPCommand(IRCode *code) {
    if (WiFi.status() == WL_CONNECTED) {
        displayTextLine("Sending..");
        WiFiUDP udp;

        udp.beginPacket(code->address.c_str(), code->frequency);
        if (code->protocol.equalsIgnoreCase("HEX")) {
            const char *hexString = code->data.c_str();
            for (int i = 0; i < code->data.length() / 2; i++) {
                char highNibble = code->data.charAt(i * 2);
                char lowNibble = code->data.charAt(i * 2 + 1);

                // uint8_t data = strtol(code->data.substring(i, i + 1).c_str(), NULL, 16);

                byte high = (highNibble >= '0' && highNibble <= '9')   ? highNibble - '0'
                            : (highNibble >= 'A' && highNibble <= 'F') ? highNibble - 'A' + 10
                            : (highNibble >= 'a' && highNibble <= 'f') ? highNibble - 'a' + 10
                                                                       : 0;

                byte low = (lowNibble >= '0' && lowNibble <= '9')   ? lowNibble - '0'
                           : (lowNibble >= 'A' && lowNibble <= 'F') ? lowNibble - 'A' + 10
                           : (lowNibble >= 'a' && lowNibble <= 'f') ? lowNibble - 'a' + 10
                                                                    : 0;

                uint8_t data = (high << 4) | low;
                udp.write(&data, 1);
            }
        } else {
            const char *data = code->data.c_str();
            udp.write((const uint8_t *)data, sizeof(data));
        }
        udp.endPacket();
    } else {
        displayTextLine("No Wifi");
        delay(500);
    }
}
