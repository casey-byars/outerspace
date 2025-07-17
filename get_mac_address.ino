/*
 * ESP32 MAC Address Finder
 * 
 * Simple utility to display the MAC address of your ESP32
 * Use this to get the MAC addresses needed for the multi-station setup
 * 
 * Instructions:
 * 1. Upload this sketch to each ESP32
 * 2. Open Serial Monitor
 * 3. Note down the MAC address
 * 4. Update the main code with these addresses
 */

#include <WiFi.h>

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP32 MAC Address Finder ===");
  
  // Initialize WiFi to get MAC address
  WiFi.mode(WIFI_MODE_STA);
  
  // Get and display MAC address
  String macAddress = WiFi.macAddress();
  
  Serial.println("MAC Address: " + macAddress);
  Serial.println("");
  
  // Display in different formats for easy copying
  Serial.println("For Arduino code (hex array format):");
  Serial.print("uint8_t mac[] = {");
  
  // Convert MAC address to hex array format
  for (int i = 0; i < 6; i++) {
    String hexByte = macAddress.substring(i * 3, i * 3 + 2);
    Serial.print("0x" + hexByte);
    if (i < 5) {
      Serial.print(", ");
    }
  }
  Serial.println("};");
  
  Serial.println("");
  Serial.println("Copy the hex array format above into your main code.");
  Serial.println("Replace the corresponding station MAC address with this value.");
  Serial.println("");
  Serial.println("=== Setup Complete ===");
}

void loop() {
  // Nothing to do in loop
  delay(5000);
  
  // Print reminder every 5 seconds
  Serial.println("MAC Address: " + WiFi.macAddress());
}