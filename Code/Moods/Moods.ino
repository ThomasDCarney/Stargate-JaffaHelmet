/**
 * This is the base arduino sketch for testing and setting up the 
 * eyes and ear fins... the elements that show mood. The eyes will 
 * change colors and fins will spread or retract depending on the 
 * current mood. These are triggered by the "changeMood" button  
 * which is handled via one of the external interrupt pins.
 * 
 * For a more detailed explanation, you can read the overview at... 
 * https://github.com/ThomasDCarney/Stargate-JaffaHelmet/blob/master/MoodOverview.md
 * 
 * To read more about adafruit NeoPixels or grab the library... 
 * https://learn.adafruit.com/adafruit-neopixel-uberguide/the-magic-of-neopixels
 * 
 * Use and modify as you like!
 * 
 * Author: Thomas Carney
 * Date: 04/05/2018
 * Last Update: 04/06/2018
 */

// Tie in necessary libraryies
#include <Adafruit_NeoPixel.h> // for NeoPixel.
#include <Servo.h>  // for servos.

// Specify pins used.
const int BUTTON_2_PIN = 2; // Change Mood Button.
const int NEO_PIN = 11; // Data pin for the NeoPixels.
const int R_FIN_PIN = 10; // Right fin servo.
const int L_FIN_PIN = 9; // Left pin servo.

// Define how to differentiate moods.
const int PASSIVE_MOOD = 0;
const int AGGRESSIVE_MOOD = 1;
int currentMood = 0;
volatile boolean moodChanged = true; // Keep volatile (Used by ISR).

/* Define key angles for each servo.
 * HOME = Angle at which fins are collapsed (passive).
 * RANGE = Spread distance from HOME when expanded (aggressive) 
 */
const int R_FIN_HOME = 30;
const int L_FIN_HOME = 150;
const int FIN_RANGE = 60;

/* The NeoPixel Strip object is used to represent/control the pixels.
 * Parameter 1 = Number of pixels in the strip.
 * Parameter 2 = Pin used for data input.
 * Parameter 3 = Pixel type flags... see documentation!
 */
const int NUM_PIXELS = 2; // Number of pixels on chain (One per eye).
Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_PIXELS, NEO_PIN, NEO_GRB);

// Will hold RGB values, 0 = off, 255 = max.
int red = 100;
int green = 100;
int blue = 100;

// Create our servo objects.
Servo finRightServo;
Servo finLeftServo;

void setup() {

  // Initialize buttons.
  pinMode(BUTTON_2_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(BUTTON_2_PIN), changeMood, RISING);

  // Init the NeoPixels strip.
  strip.begin();

  // Set some initial colors.
  for(int i = 0; i < NUM_PIXELS; i++) {

    strip.setPixelColor(i, red, green, blue);
    

  }

  // Send color instructions to the strip.
  strip.show();

  // Init the fin servos.
  finRightServo.attach(R_FIN_PIN);
  finLeftServo.attach(L_FIN_PIN);
  finRightServo.write(R_FIN_HOME);
  finLeftServo.write(L_FIN_HOME);

} // end setup


void loop() {

  updateMoodEffects();

} // end loop


/**
 * This function will handle all updates related to helmets "mood" 
 * and the outward effects.
 */
void updateMoodEffects() {

  boolean resetEyes = false;
  boolean resetFins = false;

  // Check if the change mood button was pressed.
  if(moodChanged) {

    // Trigger the reset of all mood indicators.
    resetEyes = true;
    resetFins = true;

    // Modify Globals.
    currentMood = (currentMood + 1) % 2; // Change mood.
    moodChanged = false; // Acknowledge/reset notification.
    
  }

  /* See if the color needs updating, returns a boolean true if values 
   * were changed (false otherwise). If no changes occured then no need 
   * to push the same values again.
   */
  if(updateColorValues(resetEyes)) {

    for(int i = 0; i < NUM_PIXELS; i++) {

      strip.setPixelColor(i, red, green, blue);
      strip.show();

    }

  }

  updateFinPositions(resetFins);
  
}


/**
 * This function will update fin positions. This may include resetting the fins 
 * between mood changes or random fluxuations done for effect.
 * 
 * @param resetFins - A boolean true if the fins should be reset, otherwise false. 
 * The exact position will depend current mood of the system.
 */
void updateFinPositions(boolean resetFins) {

  if(resetFins) {

    if(currentMood) {

      finRightServo.write(R_FIN_HOME);
      finLeftServo.write(L_FIN_HOME);
      
    } else {

      finRightServo.write(R_FIN_HOME + FIN_RANGE);
      finLeftServo.write(L_FIN_HOME - FIN_RANGE);
      
    }
    
  }
  
} // end updateFinPositions


/**
 * This function handles updating pixel color values. This may include 
 * resetting colors do to mood change, as part of the animation cycle or random 
 * fluxuations done for effect.
 * 
 * @param resetEyes - A boolean true if the eyes should reset, false otherwise. 
 * The colors will depend on current mood of the system.
 *
 * @return - A boolean true if colors were updated/changed, false otherwise.
 */
boolean updateColorValues(boolean resetEyes) {

  // Call the appropriate update function.
  if(currentMood) {

    return updatePassiveColors(resetEyes);

  } else {

    return updateAggressiveColors(resetEyes);

  }

} // end updateColorValues


/**
 * This method is used to update colors while in the angry/agressive state.
 * 
 * @param resetEyes - A boolean true if the update should start from reset, 
 * false otherwise.
 *
 * @return - A boolean true if colors were updated/changed, false otherwise.
 */
boolean updateAggressiveColors(boolean resetEyes) {

  // Used to log timing.
  static long lastUpdateTime = 0;

  // Constants specific to the angry state.
  const int BASE_RED = 25;
  const int MID_RED = 100;
  const int OVERLOAD = 100; // Will pulse this far above mid levels.

  // Timing variables for each phase.
  const long PHASE_2_DELAY = 10;
  const long PHASE_3_DELAY = 7;

  // Booleans to control phases of the animation.
  static boolean phase2 = false;
  static boolean phase3 = false;

  // Return value, must be updated if values are changed.
  boolean valuesChanged = false;

  /* This outer IF-Else block work kind of like Keyframes or "phases" moving 
   * from one action to another. This is a fairly short example but add/remove 
   * phases until you reach the desired effect. 
   * 
   * Phase 1: Reset the eyes to a dim red.
   * Phase 2: Raise eye brightness to overloaded state.
   * Phase 3: Cool down eyes to standard level.
   * Stay Put!
   */
  if(resetEyes) {

    red = BASE_RED;
    green = 0;
    blue = 0;

    phase2 = true; // Trigger the next phase.
    valuesChanged = true;

  } else if(phase2) {

    // Change delay to speed up or slow down the overload speed.
    if(valuesChanged = hasEnoughTimePassed(PHASE_2_DELAY, lastUpdateTime)) {

      red++;

    }

    // Once overloaded, move to next phase.
    if(red == MID_RED + OVERLOAD) {

      phase2 = false; // Trigger the next phase.
      phase3 = true;

    }

  } else if(phase3) {

    // Change delay to speed up or slow the cool down.
    if(valuesChanged = hasEnoughTimePassed(PHASE_3_DELAY, lastUpdateTime)) {

      red--;

    }

    // Once eyes have cooled down, move past phase 3.
    if(red == MID_RED) {

      phase3 = false;

    }

  }

  if(valuesChanged) {

    lastUpdateTime = millis();
    
  }

  return valuesChanged;

} // end updateAngryColors


/**
 * This method is used to update colors while in the passive state.
 * 
 * @param resetEyes - A boolean true if the update should start from reset, 
 * false otherwise.
 *
 * @return - A boolean true if colors were updated/changed, false otherwise.
 */
boolean updatePassiveColors(boolean resetEyes) {

  // Used to log timing.
  static int lastUpdateTime = 0;

  // Costants specific to the passive state.
  const int BASE_RED = 25;
  const int BASE_GREEN = 25;
  const int BASE_BLUE = 25;
  const int MID_RED = 100;
  const int MID_GREEN = 100;
  const int MID_BLUE = 100;
  const int OVERLOAD = 100;

  // Timing variables for each phase.
  const long PHASE_2_DELAY = 10;
  const long PHASE_3_DELAY = 7;

  // Booleans to control phases of the animation.
  static boolean phase2 = false;
  static boolean phase3 = false;

  // Return value, update if values actually change.
  boolean valuesChanged = false;

  /* This outer IF-Else block work kind of like Keyframes or "phases" moving 
   * from one action to another. This is a fairly short example but add/remove 
   * phases until you reach the desired effect. 
   * 
   * Phase 1: Reset the eyes to a dim white.
   * Phase 2: Raise eye brightness to overloaded state.
   * Phase 3: Cool down eyes to standard level.
   * Stay Put!
   */
  if(resetEyes) {

    red = BASE_RED;
    green = BASE_GREEN;
    blue = BASE_BLUE;

    phase2 = true; // Trigger the next phase.
    valuesChanged = true;

  } else if(phase2) {

    // Change delay to speed up or slow the cool down.
    if(valuesChanged = hasEnoughTimePassed(PHASE_2_DELAY, lastUpdateTime)) {

      incrementPassiveColors();

    }

    // Once the eyes have reached max brightness.
    if(red == MID_RED + OVERLOAD) {

      phase2 = false; // This phase is over.
      phase3 = true; // Next phase begins.

    }

  } else if(phase3) {

    // Change delay to speed up or slow the cool down.
    if(valuesChanged = hasEnoughTimePassed(PHASE_3_DELAY, lastUpdateTime)) {

      decrementPassiveColors();

    }

    // Once settled, move to standard animation.
    if(red <= MID_RED) {

      phase3 = false; // This phase is over.

    }

  }

  if(valuesChanged) {

    lastUpdateTime = millis();
    
  }

  return valuesChanged;

} // end updatePassiveColors


/**
 * This function is used to increment the colors while in the passive state.
 */
void incrementPassiveColors() {

  red++;
  green++;
  blue++;

} // end incrementPassiveColors


/**
 * This function is used to decrement colors while in the passive state.
 */
void decrementPassiveColors() {

  red--;
  green--;
  blue--;

} // end decrementPassiveColors


/**
 * A helper method used to delay or prevent repetative actions without wasting 
 * processor time. 
 * 
 * @param delayInterval - The minimum time between identical actions.
 * 
 * @param lastUpdateTime - The last time the specific action was taken.
 */
boolean hasEnoughTimePassed(long delayInterval, long lastUpdateTime) {

  return (millis() - lastUpdateTime) >= delayInterval;
  
} // end hasEnoughTimePassed


/**
 * Interrupt Service Routine for the change mood button.
 */
void changeMood() {

      // Notice, this is slightly different than how it was done in MoodMode.ino

      /* There is a specific animation that occurs each time the mood flips which 
       * involvs the eyes and ear fins. We really shouldn't deal with that in this 
       * service routine because nothing else will get happen until it wraps up. 
       * So, we're going to flag that a change was requested and the main loop 
       * will handle in it's own way.
       */
      moodChanged = true; // Flag for outside methods to do something.

} // end moodButtonISR
