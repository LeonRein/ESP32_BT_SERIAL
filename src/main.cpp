// This example code is in the Public Domain (or CC0 licensed, at your option.)
// By Evandro Copercini - 2018
//
// This example creates a bridge between Serial and Classical Bluetooth (SPP)
// and also demonstrate that SerialBT have the same functionalities of a normal Serial
// Note: Pairing is authenticated automatically by this device

#include <BluetoothSerial.h>
#include "BLEBatteryTask.h"
#include "MenuCLI.h"
#include "ConfigManager.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

// Check if Bluetooth is available
#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to and enable it
#endif

// Check Serial Port Profile
#if !defined(CONFIG_BT_SPP_ENABLED)
#error Serial Port Profile for Bluetooth is not available or not enabled. It is only available for the ESP32 chip.
#endif

#define BUFFER_SIZE 256
#define LED_PIN 13

enum class SerialState
{
  Menu,
  Idle,
  SerialForward,
  SerialBTForward
};

SerialState serialState = SerialState::Idle;
unsigned long lastActivity = 0;
const unsigned long ownerTimeout = 2000; // 2 seconds

SemaphoreHandle_t stateMutex;

struct Config
{
  char bt_name[32] = "LC29HEA-BT";
  uint32_t serial_baud = 460800;
  uint32_t serial1_baud = 460800;
  uint32_t serial1_rx = 7;
  uint32_t serial1_tx = 8;
};

Config config;
ConfigManager<Config> configManager(0);

BluetoothSerial SerialBT;

MenuCLI menuCLI;
const char *magicWord = "menu";

void setState(SerialState state)
{
  if (stateMutex)
    xSemaphoreTake(stateMutex, portMAX_DELAY);
  serialState = state;
  if (stateMutex)
    xSemaphoreGive(stateMutex);
}

void checkOwnerTimeout()
{
  if (stateMutex)
    xSemaphoreTake(stateMutex, portMAX_DELAY);
  SerialState currentState = serialState;
  unsigned long last = lastActivity;
  if (stateMutex)
    xSemaphoreGive(stateMutex);

  if ((currentState == SerialState::SerialBTForward || currentState == SerialState::SerialForward) && millis() - last > ownerTimeout)
  {
    setState(SerialState::Idle);
  }
}

// Forward Serial1 data to Serial and SerialBT
void onSerial1Data(const uint8_t *buffer, size_t len)
{
  Serial.write(buffer, len);
  SerialBT.write(buffer, len);
}

void onSerialData(const uint8_t *buffer, size_t len)
{
  if (stateMutex)
    xSemaphoreTake(stateMutex, portMAX_DELAY);
  lastActivity = millis();
  SerialState currentState = serialState;
  if (stateMutex)
    xSemaphoreGive(stateMutex);

  static char firstLineBuf[4] = {0};
  static size_t firstLineLen = 0;

  switch (currentState)
  {
  case SerialState::Idle:
  {
    size_t processed = 0;
    while (firstLineLen < 4 && processed < len)
    {
      char c = buffer[processed++];
      firstLineBuf[firstLineLen++] = c;
      if (c != magicWord[firstLineLen - 1])
      {
        setState(SerialState::SerialForward);
        firstLineLen = 0;
        // Forward all data in SerialForward state
        onSerialData(buffer, len);
        return;
      }
      if (firstLineLen == 4)
      {
        Serial.println("\n[Menu mode entered]");
        menuCLI.begin();
        setState(SerialState::Menu);
        firstLineLen = 0;
        if (processed < len)
        {
          onSerialData(buffer + processed, len - processed);
        }
        return;
      }
    }
    // Not enough data yet to decide, wait for more
    return;
  }
  case SerialState::SerialForward:
  {
    SerialBT.write(buffer, len);
    Serial1.write(buffer, len);
    break;
  }
  case SerialState::Menu:
  {
    menuCLI.write(buffer, len);
    while (Serial.available())
    {
      menuCLI.write((uint8_t)Serial.read());
    }
    break;
  }
  case SerialState::SerialBTForward:
  {
    SerialBT.write(buffer, len);
    Serial.println("ERROR: Serial does not own Serial1.");
    break;
  }
  }
}

void onSerialBTData(const uint8_t *buffer, size_t len)
{
  if (stateMutex)
    xSemaphoreTake(stateMutex, portMAX_DELAY);
  lastActivity = millis();
  SerialState currentState = serialState;
  if (stateMutex)
    xSemaphoreGive(stateMutex);

  static char firstLineBuf[4] = {0};
  static size_t firstLineLen = 0;

  switch (currentState)
  {
  case SerialState::Idle:
  {
    size_t processed = 0;
    while (firstLineLen < 4 && processed < len)
    {
      char c = buffer[processed++];
      firstLineBuf[firstLineLen++] = c;
      if (c != magicWord[firstLineLen - 1])
      {
        setState(SerialState::SerialBTForward);
        firstLineLen = 0;
        // Forward all data in SerialBTForward state
        onSerialBTData(buffer, len);
        return;
      }
      if (firstLineLen == 4)
      {
        SerialBT.println("\n[Menu mode entered]");
        delay(10); // Give client time to process
        menuCLI.begin();
        setState(SerialState::Menu);
        firstLineLen = 0;
        if (processed < len)
        {
          onSerialBTData(buffer + processed, len - processed);
        }
        return;
      }
    }
    // Not enough data yet to decide, wait for more
    return;
  }
  case SerialState::SerialBTForward:
  {
    Serial.write(buffer, len);
    Serial1.write(buffer, len);
    break;
  }
  case SerialState::Menu:
  {
    menuCLI.write(buffer, len);
    while (SerialBT.available())
    {
      menuCLI.write((uint8_t)SerialBT.read());
    }
    break;
  }
  case SerialState::SerialForward:
  {
    Serial.write(buffer, len);
    SerialBT.println("ERROR: SerialBT does not own Serial1.");
    break;
  }
  }
}

void registerMenuCommands(MenuCLI *cli)
{
  cli->registerCommand("get baud serial1", "Show Serial1 baudrate", [](const String &args, Stream &out)
                       {
        out.print("Serial1 baudrate: ");
        out.println(config.serial1_baud); });

  cli->registerCommand("set baud serial1", "Set Serial1 baudrate. Usage: set baud serial1 <baudrate>", [](const String &args, Stream &out)
                       {
        long baud = args.toInt();
        if (baud <= 0) {
            out.println("Invalid baudrate. Usage: set baud serial1 <baudrate>");
            return;
        }
        config.serial1_baud = baud;
        Serial1.updateBaudRate(baud);
        out.print("Serial1 baudrate set to: ");
        out.println(baud);
        configManager.save(); });

  cli->registerCommand("get baud serial", "Show Serial baudrate", [](const String &args, Stream &out)
                       {
        out.print("Serial baudrate: ");
        out.println(Serial.baudRate()); });

  cli->registerCommand("set baud serial", "Set Serial baudrate. Usage: set baud serial <baudrate>", [](const String &args, Stream &out)
                       {
        long baud = args.toInt();
        if (baud <= 0) {
            out.println("Invalid baudrate. Usage: set baud serial <baudrate>");
            return;
        }
        config.serial_baud = baud;
        Serial.updateBaudRate(baud);
        out.print("Serial baudrate set to: ");
        out.println(baud);
        configManager.save(); });

  cli->registerCommand("get bt_name", "Show Bluetooth device name", [](const String &args, Stream &out)
                       {
        out.print("Bluetooth device name: ");
        out.println(config.bt_name); });

  cli->registerCommand("set bt_name", "Set Bluetooth device name. Usage: set bt_name <name>", [](const String &args, Stream &out)
                       {
        String name = args;
        name.trim();
        if (name.length() == 0 || name.length() >= sizeof(config.bt_name)) {
            out.println("Invalid name. Usage: set bt_name <name>");
            return;
        }
        name.toCharArray(config.bt_name, sizeof(config.bt_name));
        out.print("Bluetooth device name set to: ");
        out.println(config.bt_name);
        configManager.save();
        out.println("Restarting ESP32 to apply new name...");
        delay(100); // Allow message to be sent
        ESP.restart(); });

  cli->registerCommand("echo on", "Enable echo mode", [](const String &args, Stream &out)
                       {
        menuCLI.setEcho(true);
        out.println("Echo mode enabled."); });

  cli->registerCommand("echo off", "Disable echo mode", [](const String &args, Stream &out)
                       {
        menuCLI.setEcho(false);
        out.println("Echo mode disabled."); });
}

void setup()
{
  bool loaded = configManager.begin(&config);
  if (loaded)
  {
    Serial.begin(config.serial_baud);
    Serial.setTimeout(10);
    Serial1.begin(config.serial1_baud, SERIAL_8N1, config.serial1_rx, config.serial1_tx);
    Serial1.setTimeout(10);

    if (!loaded)
    {
      Serial.println("Config CRC mismatch or uninitialized, using defaults.");
    }

    menuCLI.attachOutput(&Serial);
    menuCLI.attachOutput(&SerialBT);
    menuCLI.setOnExit([]()
                      { setState(SerialState::Idle); });

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Start BLE battery task
    startBLEBatteryTask(config.bt_name);

    registerMenuCommands(&menuCLI);

    stateMutex = xSemaphoreCreateMutex();

    // Inline lambdas for buffer handling
    Serial.onReceive([]()
                     {
        static uint8_t buffer[BUFFER_SIZE] = {0};
        while (Serial.available()) {
            digitalWrite(LED_PIN, HIGH); // Turn LED on
            size_t len = Serial.readBytes(buffer, BUFFER_SIZE);
            if (len > 0) {
                onSerialData(buffer, len); // updated here
            }
            digitalWrite(LED_PIN, LOW); // Turn LED off after processing
        } }, false);

    Serial1.onReceive([]()
                      {
        static uint8_t buffer[BUFFER_SIZE] = {0};
        while (Serial1.available()) {
            digitalWrite(LED_PIN, HIGH); // Turn LED on
            size_t len = Serial1.readBytes(buffer, BUFFER_SIZE);
            if (len > 0) {
                onSerial1Data(buffer, len);
            }
            digitalWrite(LED_PIN, LOW); // Turn LED off after processing
        } }, false);

    SerialBT.onData(onSerialBTData);

    SerialBT.begin(config.bt_name);
    Serial.printf("The device with name \"%s\" is started.\nNow you can pair it with Bluetooth!\n", config.bt_name);
  }
}

void loop()
{
  checkOwnerTimeout();
}