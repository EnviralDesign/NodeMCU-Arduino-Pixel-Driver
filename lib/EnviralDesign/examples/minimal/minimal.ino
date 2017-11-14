#include <DoubleResetDetector.h>

// Number of seconds after reset during which a 
// subseqent reset will be considered a double reset.
#define DRD_TIMEOUT 10

// RTC Memory Address for the DoubleResetDetector to use
#define DRD_ADDRESS 0

DoubleResetDetector drd(DRD_TIMEOUT, DRD_ADDRESS);

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  
  Serial.begin(9600);
  Serial.println();
  Serial.println("DoubleResetDetector Example Program");
  Serial.println("-----------------------------------");

  if (drd.detectDoubleReset()) {
    Serial.println("Double Reset Detected");
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    Serial.println("No Double Reset Detected");
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

void loop()
{
  // Call the double reset detector loop method every so often,
  // so that it can recognise when the timeout expires.
  // You can also call drd.stop() when you wish to no longer
  // consider the next reset as a double reset.
  drd.loop();
}
