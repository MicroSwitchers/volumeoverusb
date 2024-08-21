#include <HID-Project.h>

// Pin definitions
#define PIN_CLK 2
#define PIN_DT 3
#define PIN_SW 4
#define LED_PIN LED_BUILTIN  // Onboard LED

// State machine variables
int encoderPos = 0;
int lastEncoderPos = 0;
int lastState = 0;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 10;  // 10ms debounce time

bool isMuted = false;
unsigned long lastFlashTime = 0;
unsigned long lastVolumeChangeTime = 0;
const unsigned long volumeChangeInterval = 30;  // 30ms interval between volume changes

// Threshold to change volume only after a certain number of steps
const int stepThreshold = 1;  // Lower threshold for faster response
int stepCounter = 0;

void setup() {
  Serial.begin(9600);

  pinMode(PIN_CLK, INPUT_PULLUP);
  pinMode(PIN_DT, INPUT_PULLUP);
  pinMode(PIN_SW, INPUT_PULLUP);
  pinMode(LED_PIN, OUTPUT);

  Consumer.begin();
}

void loop() {
  // Read the current state of the CLK and DT pins
  int currentState = (digitalRead(PIN_CLK) << 1) | digitalRead(PIN_DT);

  // Check if state has changed and if sufficient time has passed for debounce
  if (currentState != lastState && (millis() - lastDebounceTime) > debounceDelay) {
    lastDebounceTime = millis();  // Update debounce timer

    // Determine rotation direction (reverse logic)
    if ((lastState == 0b00 && currentState == 0b01) || (lastState == 0b01 && currentState == 0b11) ||
        (lastState == 0b11 && currentState == 0b10) || (lastState == 0b10 && currentState == 0b00)) {
      encoderPos--;  // Counter-clockwise rotation (volume down)
      stepCounter++;
    } else if ((lastState == 0b00 && currentState == 0b10) || (lastState == 0b10 && currentState == 0b11) ||
               (lastState == 0b11 && currentState == 0b01) || (lastState == 0b01 && currentState == 0b00)) {
      encoderPos++;  // Clockwise rotation (volume up)
      stepCounter++;
    }

    lastState = currentState;  // Update last state

    // Handle volume control with step threshold and rate limiting
    if (stepCounter >= stepThreshold && (millis() - lastVolumeChangeTime > volumeChangeInterval)) {
      stepCounter = 0;  // Reset step counter

      if (encoderPos != lastEncoderPos) {
        if (isMuted) {
          // If muted, unmute and stop LED blinking
          isMuted = false;
          lastFlashTime = 0;  // Reset the flash timer
          Consumer.write(MEDIA_VOLUME_MUTE);  // Unmute
          Serial.println("Unmuted due to volume change");
        }

        if (encoderPos > lastEncoderPos) {
          for (int i = 0; i < 2; i++) {  // Increase volume by 2 steps per encoder step
            Consumer.write(MEDIA_VOLUME_UP);
          }
        } else if (encoderPos < lastEncoderPos) {
          for (int i = 0; i < 2; i++) {  // Decrease volume by 2 steps per encoder step
            Consumer.write(MEDIA_VOLUME_DOWN);
          }
        }
        lastEncoderPos = encoderPos;
        lastVolumeChangeTime = millis();  // Update last volume change time

        // Flash the LED to indicate volume change
        digitalWrite(LED_PIN, HIGH);
        delay(50);  // LED on for 50ms
        digitalWrite(LED_PIN, LOW);

        Serial.print("Encoder Position: ");
        Serial.println(encoderPos);
      }
    }
  }

  // Check for button press to toggle mute
  if (digitalRead(PIN_SW) == LOW) {  // Button pressed
    delay(50);  // Debounce delay
    if (digitalRead(PIN_SW) == LOW) {  // Confirm button still pressed
      if (!isMuted) {
        Consumer.write(MEDIA_VOLUME_MUTE);  // Mute
        isMuted = true;
        Serial.println("Muted");
      } else {
        Consumer.write(MEDIA_VOLUME_MUTE);  // Unmute
        isMuted = false;
        Serial.println("Unmuted");
        lastFlashTime = 0;  // Reset flash timer
      }
      delay(250);  // Prevent multiple toggles from a single press
    }
  }

  // Flash LED every second when muted
  if (isMuted && (millis() - lastFlashTime) >= 1000) {
    digitalWrite(LED_PIN, HIGH);
    delay(100);  // LED on for 100ms
    digitalWrite(LED_PIN, LOW);
    lastFlashTime = millis();
  }

  delay(10);  // Small delay for stability
}
