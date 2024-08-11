#include <ArduinoBLE.h>
#include <Arduino_USBHostMbed5.h>
#include <DigitalOut.h>
#include <FATFileSystem.h>
#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h> // Hardware-specific library for ILI9341

#define TFT_CS     10
#define TFT_RST    9
#define TFT_DC     8

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// BLE Peripheral addresses as byte arrays
const uint8_t peripheralAddresses[][6] = {
  {0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC},  // Peripheral 1 address (replace with actual)
  {0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD},  // Peripheral 2 address (replace with actual)
  {0x34, 0x56, 0x78, 0x9A, 0xBC, 0xDE},  // Peripheral 3 address (replace with actual)
  {0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF}   // Peripheral 4 address (replace with actual)
};

BLEDevice peripherals[4];

BLECharacteristic notifyChars[4][3]; // To store Notify characteristics
BLECharacteristic readChars[4][4];   // To store Read characteristics

unsigned long lastReadTime = 0;
const unsigned long readInterval = 60000; // 1 minute

// USB Host and Mass Storage
USBHostMSD msd;
mbed::FATFileSystem usb("usb");

int err;
int count;

void setup() {
  Serial.begin(115200);

  // Initialize TFT screen
  tft.begin();
  tft.setRotation(3); // Adjust rotation as needed
  tft.fillScreen(ILI9341_BLACK);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);

  pinMode(PA_15, OUTPUT);  // Enable the USB-A port
  digitalWrite(PA_15, HIGH);

  while (!Serial); // Stop program from executing until serial port opens

  msd.connect();

  while (!msd.connected()) {
    Serial.print("MSD not found.");
    tft.println("MSD not found.");
    delay(1000);
  }

  Serial.println("Mounting USB device...");
  tft.println("Mounting USB device...");

  err = usb.mount(&msd);
  if (err) {
    Serial.print("Error mounting USB device ");
    Serial.println(err);
    tft.print("Error mounting USB device ");
    tft.println(err);
    while (1);
  }

  Serial.println("USB device mounted successfully.");
  tft.println("USB device mounted successfully.");

  // Initialize BLE
  if (!BLE.begin()) {
    Serial.println("Starting BLE failed!");
    tft.println("Starting BLE failed!");
    while (1);
  }

  // Initialize BLE peripherals
  for (int i = 0; i < 4; i++) {
    peripherals[i] = BLEDevice();  // Initialize BLEDevice
    connectPeripheral(i);
  }

  // Start logging BLE data to file
  WriteToFile();
}

void loop() {
  // BLE notifications and reading will be handled in the WriteToFile function.
}

void WriteToFile() {
  mbed::fs_file_t file;
  Serial.println("Opening file..");
  tft.println("Opening file..");
  FILE *f = fopen("/usb/ble_log.txt", "w+");

  if (!f) {
    Serial.println("Failed to open file for writing");
    tft.println("Failed to open file for writing");
    return;
  }

  for (int i = 0; i < 100; i++) {  // Example loop, modify as needed
    count += 1;
    for (int j = 0; j < 4; j++) {
      if (peripherals[j].connected()) {
        // Handle notifications
        for (int k = 0; k < 3; k++) {
          if (notifyChars[j][k].written()) {
            uint8_t buffer[20];
            int length = notifyChars[j][k].readValue(buffer, sizeof(buffer));
            String value = convertBufferToString(buffer, length);
            Serial.print("Peripheral ");
            Serial.print(j + 1);
            Serial.print(" - Notification ");
            Serial.print(k + 1);
            Serial.print(": ");
            Serial.println(value);

            tft.print("Peripheral ");
            tft.print(j + 1);
            tft.print(" - Notification ");
            tft.print(k + 1);
            tft.print(": ");
            tft.println(value);

            err = fprintf(f, "Peripheral %d - Notification %d: %s\n", j + 1, k + 1, value.c_str());
            if (err < 0) {
              Serial.println("Error writing to file");
              tft.println("Error writing to file");
              error("error: %s (%d)\n", strerror(errno), -errno);
            }
          }
        }

        // Handle periodic reads every 1 minute
        if (millis() - lastReadTime >= readInterval) {
          for (int k = 0; k < 4; k++) {
            if (readChars[j][k].canRead()) {
              uint8_t buffer[20];
              int length = readChars[j][k].readValue(buffer, sizeof(buffer));
              String value = convertBufferToString(buffer, length);
              Serial.print("Peripheral ");
              Serial.print(j + 1);
              Serial.print(" - Read ");
              Serial.print(k + 1);
              Serial.print(": ");
              Serial.println(value);

              tft.print("Peripheral ");
              tft.print(j + 1);
              tft.print(" - Read ");
              tft.print(k + 1);
              tft.print(": ");
              tft.println(value);

              err = fprintf(f, "Peripheral %d - Read %d: %s\n", j + 1, k + 1, value.c_str());
              if (err < 0) {
                Serial.println("Error writing to file");
                tft.println("Error writing to file");
                error("error: %s (%d)\n", strerror(errno), -errno);
              }
            }
          }
          lastReadTime = millis();
        }
      }
    }
    delay(10);  // Modify interval as needed
  }

  Serial.println("File closing");
  tft.println("File closing");
  err = fclose(f);

  if (err < 0) {
    Serial.print("fclose error:");
    Serial.print(strerror(errno));
    Serial.print(" (");
    Serial.print(-errno);
    Serial.print(")");

    tft.print("fclose error:");
    tft.print(strerror(errno));
    tft.print(" (");
    tft.print(-errno);
    tft.print(")");
  } else {
    Serial.println("File closed");
    tft.println("File closed");
  }
}

void connectPeripheral(int index) {
  BLEDevice peripheral = BLE.available();
  while (peripheral) {
    // Convert the BLE device address to a String
    String addressString = "";
    for (int i = 0; i < 6; i++) {
      if (peripheral.address()[i] < 0x10) {
        addressString += "0";
      }
      addressString += String(peripheral.address()[i], HEX);
      if (i < 5) {
        addressString += ":";
      }
    }

    // Convert the expected address to a String
    String expectedAddressString = "";
    for (int i = 0; i < 6; i++) {
      if (peripheralAddresses[index][i] < 0x10) {
        expectedAddressString += "0";
      }
      expectedAddressString += String(peripheralAddresses[index][i], HEX);
      if (i < 5) {
        expectedAddressString += ":";
      }
    }

    if (addressString.equalsIgnoreCase(expectedAddressString)) {
      if (peripheral.connect()) {
        peripherals[index] = peripheral;  // Store the connected peripheral
        Serial.print("Connected to Peripheral ");
        Serial.println(index + 1);

        tft.print("Connected to Peripheral ");
        tft.println(index + 1);

        BLEService service = peripherals[index].service("01FF0100-BA5E-F4EE-5CA1-EB1E5E4B1CE0");

        // Notify Characteristics
        notifyChars[index][0] = service.characteristic("01FF0100-BA5E-F4EE-5CA1-EB1E5E4B1CE0");
        notifyChars[index][1] = service.characteristic("01FF0101-BA5E-F4EE-5CA1-EB1E5E4B1CE0");
        notifyChars[index][2] = service.characteristic("01FF0102-BA5E-F4EE-5CA1-EB1E5E4B1CE0");

        // Read Characteristics
        BLEService lightService = peripherals[index].service("180A");
        readChars[index][0] = lightService.characteristic("2A29");
                readChars[index][1] = lightService.characteristic("2A24");
        readChars[index][2] = lightService.characteristic("2A25");
        readChars[index][3] = lightService.characteristic("2A26");

        // Start notifications
        for (int j = 0; j < 3; j++) {
          notifyChars[index][j].subscribe();
        }
        return;
      } else {
        Serial.print("Failed to connect to Peripheral ");
        Serial.println(index + 1);

        tft.print("Failed to connect to Peripheral ");
        tft.println(index + 1);
      }
    }
    peripheral = BLE.available();  // Check the next available peripheral
  }
}

String convertBufferToString(uint8_t* buffer, int length) {
  String result = "";
  for (int i = 0; i < length; i++) {
    result += String((char)buffer[i]);
  }
  return result;
}
