// Dual-Mode (Iambic Paddle or Straight Key) code practice device.
// This version uses a POTENTIOMETER (A0) for variable WPM speed 
// and outputs decoded characters to the Serial Monitor.

#include <Arduino.h>  // Includes core Arduino definitions (String, pinMode, etc.)
#include <string.h>   // Required for strcmp()

// =========================================================================
// !!! KEYER CONFIGURATION SWITCH !!!
// Set to 1 to activate the mode, 0 to disable. 
// Only ONE mode should be 1 at any time.
// =========================================================================
#define IAMBIC_MODE      0
#define STRAIGHT_KEY_MODE 1 

// --- Keyer Mode Configuration ---
enum KeyerMode {
  MODE_A, // Iambic Mode A (No Squeeze Memory)
  MODE_B  // Iambic Mode B (Squeeze Memory)
};
KeyerMode currentIambicMode = MODE_// =========================================================================
// WPM SPEED CONTROL CONFIGURATION (VARIABLE SPEED)
// =========================================================================
const int POT_PIN = A0;      // Analog pin for WPM Potentiometer
int currentWPM = 15;         // Starting WPM
const int MIN_WPM = 5;       // Minimum allowed WPM
const int MAX_WPM = 40;      // Maximum allowed WPM
const int TONE_FREQ = 650; // Frequency of the tone in Hertz.

// --- Morse Code Timing Parameters (now dynamic global variables) ---
// These are updated continuously by updateWPM()
unsigned int DOT_DURATION = 1200 / currentWPM; // Duration of a dot in milliseconds
unsigned int DASH_DURATION = 3 * DOT_DURATION;
unsigned int ELEMENT_GAP = DOT_DURATION; // Gap between elements 
unsigned int CHARACTER_GAP = 3 * DOT_DURATION; // Gap between characters 
unsigned int WORD_GAP = 7 * DOT_DURATION;      // Gap between words 

// --- Universal Pin Definitions ---
const int LED_PIN = 13;   // Digital pin for the LED.
const int BUZZER_PIN = 8; // Digital pin for the buzzer/speaker.

// --- Universal State Variables ---
unsigned long keyReleaseTime = 0;    // Time of the last transmitted element's release or tone stop
String morseSequence = "";           // Stores the sequence of dots and dashes for a letter.

// =========================================================================
// Iambic Keyer Variables & Pin Definitions
// =========================================================================
const int DOT_PIN = 2;    // Digital pin for the DOT paddle (connect to GND)
const int DASH_PIN = 3;   // Digital pin for the DASH paddle (connect to GND)

// Iambic State Variables
bool dotPaddleState = false;
bool dashPaddleState = false;
bool isKeying = false;               
bool iambicBuffer = false;           
unsigned long elementStopTime = 0;   
unsigned long nextElementTime = 0;   


// =========================================================================
// Straight Key Variables & Pin Definitions
// =========================================================================
const int STRAIGHT_KEY_PIN = 4; // Digital pin connected to the straight key (connect to GND) 

// Straight Key State Variables
unsigned long keyPressStartTime = 0;
bool keyWasPressed = false;


// --- Morse Code Lookup Table ---
const char* MORSE_ALPHABET[] = {
  ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
  "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
  "..-", "...-", ".--", "-..-", "-.--", "--..",
  "-----", ".----", "..---", "...--", "....-", ".....", "-....", "--...",
  "---..", "----."
};

// --- Function Prototypes ---
void startElement(unsigned int duration, char element);
void sendDot();
void sendDash();
void handleKeyerOutput();
void handleKeyPress();
void handleKeyRelease();
void updateWPM(); // New function prototype

// =========================================================================
// WPM Update Function
// =========================================================================
/**
 * @brief Reads the analog input and recalculates all Morse timing variables.
 */
void updateWPM() {
  // Read the potentiometer value (0 to 1023)
  int sensorValue = analogRead(POT_PIN);

  // Map the sensor value to the WPM range (MIN_WPM to MAX_WPM)
  int newWPM = map(sensorValue, 0, 1023, MIN_WPM, MAX_WPM);
  
  // Only update if the speed has changed
  if (newWPM != currentWPM) {
    currentWPM = newWPM;

    // Recalculate all timing variables based on the new WPM
    DOT_DURATION = 1200 / currentWPM; 
    DASH_DURATION = 3 * DOT_DURATION;
    ELEMENT_GAP = DOT_DURATION; 
    CHARACTER_GAP = 3 * DOT_DURATION; 
    WORD_GAP = 7 * DOT_DURATION;
    
    // Output the new speed to the Serial Monitor
    Serial.print("\nSpeed: ");
    Serial.print(currentWPM);
    Serial.print(" WPM | Dot: ");
    Serial.print(DOT_DURATION);
    Serial.println("ms");
  }
}

// =========================================================================
// UNIVERSAL HELPER FUNCTIONS
// =========================================================================

/**
 * @brief Looks up the morseSequence in the table and prints the character to Serial.
 */
void decodeAndPrintCharacter() {
  if (morseSequence.length() == 0) return;

  char decodedChar = '?';
  const char* sequence_cstr = morseSequence.c_str(); 

  // Loop through all 36 possible characters (26 Letters, 10 Numbers)
  for (int i = 0; i < 36; i++) {
    // strcmp is used for C-style string comparison
    if (strcmp(sequence_cstr, MORSE_ALPHABET[i]) == 0) {
      if (i < 26) {
        decodedChar = 'A' + i;
      } else {
        decodedChar = '0' + (i - 26);
      }
      break;
    }
  }

  Serial.print(decodedChar);
  morseSequence = "";
}

// =========================================================================
// IAMBIC KEYER FUNCTIONS
// =========================================================================

/**
 * @brief Starts a tone element (Dot or Dash) in a non-blocking way.
 */
void startElement(unsigned int duration, char element) {
  digitalWrite(LED_PIN, HIGH);
  tone(BUZZER_PIN, TONE_FREQ);
  
  morseSequence += element;
  elementStopTime = millis() + duration;
  nextElementTime = elementStopTime + ELEMENT_GAP;
  isKeying = true;
}

void sendDot() {
  startElement(DOT_DURATION, '.');
  
  if (currentIambicMode == MODE_B) {
    iambicBuffer = true; // Buffer for the next Dash
  }
}

void sendDash() {
  startElement(DASH_DURATION, '-');
  
  if (currentIambicMode == MODE_B) {
    iambicBuffer = false; // Buffer for the next Dot
  }
}

/**
 * @brief Manages the non-blocking tone output and timing.
 */
void handleKeyerOutput() {
  if (isKeying) {
    if (millis() >= elementStopTime) {
      noTone(BUZZER_PIN);
      digitalWrite(LED_PIN, LOW);
      isKeying = false;
      keyReleaseTime = millis();
    }
  }
}


// =========================================================================
// STRAIGHT KEY HELPER FUNCTIONS
// =========================================================================

void handleKeyPress() {
  keyPressStartTime = millis();
  keyWasPressed = true;
  digitalWrite(LED_PIN, HIGH);
  tone(BUZZER_PIN, TONE_FREQ);
}

void handleKeyRelease() {
  unsigned long keyPressDuration = millis() - keyPressStartTime;
  keyWasPressed = false;
  noTone(BUZZER_PIN);
  digitalWrite(LED_PIN, LOW);
  keyReleaseTime = millis();

  // Determine if the press was a dot or a dash based on dynamic timing ratios
  // The threshold is halfway between DOT_DURATION and DASH_DURATION (3*DOT_DURATION)
  if (keyPressDuration >= (DASH_DURATION - DOT_DURATION / 2)) {
    morseSequence += "-";
  } else if (keyPressDuration >= (DOT_DURATION - DOT_DURATION / 2)) {
    morseSequence += ".";
  }
}


// =========================================================================
// SETUP FUNCTION
// =========================================================================
void setup() {
  Serial.begin(9600);
  
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  // Initial call to set the default WPM and print the speed
  updateWPM(); 

  // --- Runtime Configuration Check and Setup ---
  if (IAMBIC_MODE == 1) {
    pinMode(DOT_PIN, INPUT_PULLUP);
    pinMode(DASH_PIN, INPUT_PULLUP);
    String modeName = (currentIambicMode == MODE_A) ? "Mode A (No Memory)" : "Mode B (Squeeze Memory)";
    Serial.println("Arduino Iambic Keyer Trainer Ready!");
    Serial.println("Current Mode: " + modeName);
  } 
  
  if (STRAIGHT_KEY_MODE == 1) {
    pinMode(STRAIGHT_KEY_PIN, INPUT_PULLUP);
    Serial.println("Arduino Straight Key Decoder Ready!");
  }

  // Final check for configuration error
  if (IAMBIC_MODE == 0 && STRAIGHT_KEY_MODE == 0) {
    Serial.println("ERROR: No Keyer Mode is Active. Set IAMBIC_MODE or STRAIGHT_KEY_MODE to 1.");
  }
  
  Serial.println("Start keying!");
}

// =========================================================================
// MAIN LOOP
// =========================================================================
void loop() {
  
  // 1. ALWAYS UPDATE SPEED FIRST
  updateWPM(); 
  
  // --- Iambic Keyer Logic (Controlled by runtime IF) ---
  if (IAMBIC_MODE == 1) {
    
    // 2. TONE MANAGEMENT: Must handle tone output first to maintain non-blocking timing.
    handleKeyerOutput();

    // 3. INPUT: Read the current paddle states (LOW means pressed, due to PULLUP)
    dotPaddleState = !digitalRead(DOT_PIN);
    dashPaddleState = !digitalRead(DASH_PIN);

    // 4. DECODE: Character/Word Detection (only check if we are NOT currently sending an element)
    if (!isKeying && morseSequence.length() > 0) {
      unsigned long timeSinceLastElement = millis() - keyReleaseTime;
      
      if (timeSinceLastElement > CHARACTER_GAP) {
        decodeAndPrintCharacter();
        if (timeSinceLastElement > WORD_GAP) {
          Serial.print(" ");
        }
      }
    }

    // 5. KEYER LOGIC: Only start a new element if timing is met (millis() >= nextElementTime)
    if (millis() >= nextElementTime) {
      
      // Check for alternating (both paddles pressed - SQUEEZE)
      if (dotPaddleState && dashPaddleState) {
        if (iambicBuffer) {
          sendDot();
        } else {
          sendDash();
        }
      } 
      // Check for single paddle press (Dot)
      else if (dotPaddleState) {
        sendDot();
      } 
      // Check for single paddle press (Dash)
      else if (dashPaddleState) {
        sendDash();
      } else {
        // If no paddles are pressed, reset the next start time to now 
        nextElementTime = millis();
        
        // In MODE A, reset the buffer when both paddles are released.
        if (currentIambicMode == MODE_A) {
            iambicBuffer = false; 
        }
      }
    }
  } // End IAMBIC_MODE

  // --- Straight Key Logic (Controlled by runtime IF) ---
  if (STRAIGHT_KEY_MODE == 1) {
    int keyState = digitalRead(STRAIGHT_KEY_PIN);

    if (keyState == LOW) {
      if (!keyWasPressed) {
        handleKeyPress();
      }
    } else {
      if (keyWasPressed) {
        handleKeyRelease();
      }
      
      // Character/Word Detection (Uses time since last release)
      unsigned long timeSinceLastRelease = millis() - keyReleaseTime;
      if (morseSequence.length() > 0) {
        if (timeSinceLastRelease > CHARACTER_GAP) {
          decodeAndPrintCharacter();
        }
        if (timeSinceLastRelease > WORD_GAP) {
          Serial.print(" ");
        }
      }
    }
  } // End STRAIGHT_KEY_MODE
}