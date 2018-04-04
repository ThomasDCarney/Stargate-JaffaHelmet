/**
 * This is the base arduino sketch for testing and setting up the change 
 * mood and mode buttons on the Stargate Jaffa helmet. In this, there are 
 * two buttons utilizing the two pins which allow external interrupts. 
 * 
 * For a more detailed explanation, you can read the overview at... 
 * https://github.com/ThomasDCarney/Stargate-JaffaHelmet/blob/master/ModeMoodButtonOverview.md
 * 
 * Use and modify as you like!
 * 
 * Author: Thomas Carney
 * Date: 04/02/2018
 */

// Uncomment next line for debugging.
#define DEBUGGING

// Buttons 2 and 3 are attached to digial pins 2 and 3.
const int button2Pin = 2;
const int button3Pin = 3;

/**
 * Since these variables are updated solely by the ISR's they should be 
 * declared volatile to ensure the compiler doesn't optimize them away.
 */
volatile int currentMood = 0; // 0 = Passive, 1 = Aggressive
volatile int currentMode = 0; // 0 = Jerky, 1 = Smooth

void setup() {

  #ifdef DEBUGGING 

    Serial.begin(9600);

  #endif

  pinMode(button2Pin, INPUT);
  pinMode(button3Pin, INPUT);

  // With the debounce circuit, pins will be low while buttons are open.
  attachInterrupt(digitalPinToInterrupt(button2Pin), changeMood, RISING);
  attachInterrupt(digitalPinToInterrupt(button3Pin), changeMode, RISING);

} // end setup


void loop() {
  
  // The loop is empty, all work for buttons 2 and 3 are done via ISR.

} // end loop


/**
 * Interrupt Service Routine for the change mood button.
 */
void changeMood() {

  // Simply toggling between 0 and 1.
  currentMood = (currentMood + 1) % 2;

  #ifdef DEBUGGING

    Serial.print("Current Mood: ");

    switch(currentMood) {
      case 0:
        Serial.println("Passive");
        break;
      case 1:
        Serial.println("Aggressive"); 
        break;
    }

  #endif

  // Notice, even with debugging, the ISR is/should be rather short.
  
} // end changeMood


/**
 * Interrupt Service Routine for the change mode button.
 */
void changeMode() {

  // Simply toggling between 0 and 1.
  currentMode = (currentMode + 1) % 2;

  #ifdef DEBUGGING

    Serial.print("Current Mode: ");

    switch(currentMode) {
      case 0: 
        Serial.println("Jerky");
        break;
      case 1:
        Serial.println("Smooth");
        break;
    }

  #endif

  // Notice, even with debugging, the ISR is/should be rather short.

} // end changeMode

