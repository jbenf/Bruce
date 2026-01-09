#include <Arduino.h>
#include <FS.h>
#include <IRremoteESP8266.h>
#include <IRsend.h>
#include <SD.h>
#include <globals.h>

// Forward Declaration
struct IRCode;

// // Custom IR
void sendPostCommand(IRCode *code, String token);
void sendGetCommand(IRCode *code);
void sendUDPCommand(IRCode *code);
void bruceAlmighty();
void bruceAlmightyM5Cc();
