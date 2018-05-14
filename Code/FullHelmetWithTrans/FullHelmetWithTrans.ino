/**
 * This is the complete arduino sketch which is really just putting all the
 * individual sketches together.
 *
 * Use and modify as you like!
 *
 * Author: Thomas Carney
 * Date: 04/04/2018
 * Last Update: 04/04/2018
 */

// Tie in necessary libraryies
#include <Adafruit_NeoPixel.h> // for NeoPixel.
#include <Servo.h>  // for servos.

// Specify the pins to which buttons are attached.
const int BUTTON_1_PIN = 4;
const int BUTTON_2_PIN = 2;
const int BUTTON_3_PIN = 3;

// Specify the pin for NeoPixel strip.
const int NEO_PIN = 11; // Data pin for the NeoPixels.

// Specify the analog pins used by the joystick potentiometers.
const int JOYSTICK_X_PIN = 0;
const int JOYSTICK_Y_PIN = 1;

// Specify pins used by each servo.
const int C_HEAD_PIN = 6;
const int R_HEAD_PIN = 7;
const int L_HEAD_PIN = 8;
const int R_FIN_PIN = 10; // Right fin servo.
const int L_FIN_PIN = 9; // Left pin servo.
const int HEAD_TRANSISTOR_PIN = 12; // Head Transistor Control Pin.
const int FIN_TRANSISTOR_PIN = 13; // Fin Transister Control Pin.

/*  Specify servo angles where the head appears "centered". This may
 *  vary due to multiple items, primarily hardware related to housing
 *  and mounting the servos onto the helmet, adjust as needed.
 */
const int C_HEAD_HOME = 90;
const int R_HEAD_HOME = 60;
const int L_HEAD_HOME = 131;

/*  Specify the max angles head servos can vary from center.
 *
 *  HEAD_TILT: How far servo C can tilt in either direction.
 *  HEAD_RISE: How far servo L and R can turn towards from the helmet body.
 *  HEAD_DIP: How far servo L and R can turn away from the helmet body.
 *
 *  Since our rig requires L and R move in lockstep and doesn't angle the
 *  head up or down, RISE and DIP should be the same. If your setup doesn't
 *  have said requirement, adjust as appropriate.
 */
const int TILT_LIMIT = 60;
const int RISE_LIMIT = 25;
const int DIP_LIMIT = 25;

/* Define key angles for fin servos.
 * HOME = Angle at which fins are collapsed (passive).
 * RANGE = Spread distance from HOME when expanded (aggressive)
 */
const int R_FIN_HOME = 30;
const int L_FIN_HOME = 150;
const int FIN_RANGE = 60;

// Define how moods are differentiated.
const int PASSIVE_MOOD = 0;
const int AGGRESSIVE_MOOD = 1;
int currentMood = 0;
volatile boolean moodChanged = true; // Keep volatile (Used by ISR).

/**
 * I don't deal with the button setup to change mode here but they are
 * required and you can manually change it for testing.
 */
const int JERKY_MODE = 0;
const int SMOOTH_MODE = 1;
volatile int currentMode = JERKY_MODE;

/*   numZones is really just a shortcut for determining each arrays length
 *   dynamically. For now however, keep in mind you will have to make other
 *   changes if you decide to alter the zone count in either direction.
 */
const int numZones = 10;

// Masks for whether a zone is active/inactive... 0 = inactive, 1 = active.
const int lateralJerkyZones[] = {1, 1, 1, 0, 0, 0, 0, 1, 1, 1}; // R to L
const int lateralSmoothZones[] = {1, 1, 1, 1, 0, 0, 1, 1, 1, 1}; // R to L
const int tiltJerkyZones[] = {1, 1, 0, 0, 0, 0, 0, 0, 1, 1}; // R to L (Up to Down on stick)
const int tiltSmoothZones[] = {1, 1, 1, 0, 0, 0, 0, 1, 1, 1}; //R to L (Up to Down on stick)

// Number of degrees servos will move in each zone (smooth mode only).
const int lateralMoveAmount[] = {4, 3, 2, 1, 0, 0, 1, 2, 3, 4};
const int tiltMoveAmount[] = {3, 2, 1, 0, 0, 0, 0, 1, 2, 3};

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

// Create the servo objects which we'll manipulate.
Servo headCenterServo;
Servo headRightServo;
Servo headLeftServo;
Servo finRightServo;
Servo finLeftServo;


void setup() {

  pinMode(BUTTON_1_PIN, INPUT);
  pinMode(BUTTON_2_PIN, INPUT);
  pinMode(BUTTON_3_PIN, INPUT);

  // With the debounce circuit, pins will be low while buttons are open.
  attachInterrupt(digitalPinToInterrupt(BUTTON_2_PIN), changeMood, RISING);
  attachInterrupt(digitalPinToInterrupt(BUTTON_3_PIN), changeMode, RISING);

  // Attach each servo to its input pin.
  headCenterServo.attach(C_HEAD_PIN);
  headRightServo.attach(R_HEAD_PIN);
  headLeftServo.attach(L_HEAD_PIN);
  finRightServo.attach(R_FIN_PIN);
  finLeftServo.attach(L_FIN_PIN);

  // Transistor setup.
  pinMode(HEAD_TRANSISTOR_PIN, OUTPUT);
  pinMode(FIN_TRANSISTOR_PIN, OUTPUT);
  digitalWrite(HEAD_TRANSISTOR_PIN, HIGH);
  digitalWrite(FIN_TRANSISTOR_PIN, HIGH);

  // Initially center the head.
  headCenterServo.write(C_HEAD_HOME);
  headRightServo.write(R_HEAD_HOME);
  headLeftServo.write(L_HEAD_HOME);
  finRightServo.write(R_FIN_HOME);
  finLeftServo.write(L_FIN_HOME);

  // Init the NeoPixels strip.
  strip.begin();

  // Set some initial colors.
  for(int i = 0; i < NUM_PIXELS; i++) {

    strip.setPixelColor(i, red, green, blue);


  }

  // Send color instructions to the strip.
  strip.show();

} // end setup


void loop() {

  static long lastHeadTransitionTime = 0;
  const long headTransitionDelay = 1000;

  int lateralZone = getZone(analogRead(JOYSTICK_X_PIN));
  int tiltZone = getZone(analogRead(JOYSTICK_Y_PIN));

  boolean lateralMovementRequested = isLateralMovementRequested(lateralZone);
  boolean tiltRequested = isTiltRequested(tiltZone);
  boolean recenterButtonPushed = digitalRead(BUTTON_1_PIN);

  updateMoodEffects();


  // Check for movement requests, if none, poll button.
  if(lateralMovementRequested || tiltRequested || recenterButtonPushed) {

    digitalWrite(HEAD_TRANSISTOR_PIN, HIGH);

    if(lateralMovementRequested || tiltRequested) {

      if(lateralMovementRequested) {

        updateLateralRotation(lateralZone);

      }

      if(tiltRequested) {

        updateTilt(tiltZone);

      }

    } else {

      recenterHead();

    }

    lastHeadTransitionTime = millis();

  }

  if(hasEnoughTimePassed(headTransitionDelay, lastHeadTransitionTime)) {

    digitalWrite(HEAD_TRANSISTOR_PIN, LOW);

  }

} // end loop


/**
 * This function takes the value (converted via A/D converter) present on an analog
 * pin and converts it to a its respective zone.
 *
 * @param value - The A/D converted value on a pin.
 */
int getZone(int value) {

  /*
   * Technically the analog to digital converter returns 0 to 1023 but
   * due to resistance along the circuit, the joystick issn't receiving
   * a full 5V. This means zones at the far ends never trigger. To
   * adjust, I'm reducing the max value closer to what I'm actually
   * seeing.
   */
  return map(value, 0, 950, 1, numZones);

} // end getZone


/**
 * This function takes a zone for lateral movement and determines whether or
 * not to turn the head.
 *
 * @param zone - The zone to check.
 */
boolean isLateralMovementRequested(int zone) {

  // Arrays start at 0, zones start at 1 so shift by 1.
  if((currentMode == JERKY_MODE && lateralJerkyZones[zone - 1] == 1) ||
     (currentMode == SMOOTH_MODE && lateralSmoothZones[zone - 1] == 1)) {

    return true;

  }

  return false;

} // end isLateralMovementRequested


/**
 * This function will take the zone for head tilt and determine whether or
 * not to tilt the head.
 *
 * @param zone - The zone to check.
 */
boolean isTiltRequested(int zone) {

  if((currentMode == JERKY_MODE && tiltJerkyZones[zone - 1] == 1) ||
     (currentMode == SMOOTH_MODE && tiltSmoothZones[zone - 1] == 1)) {

    return true;

  }

  return false;

} // end isTiltRequested


/**
 * This function will handle any requests to update and turn the head left
 * or right.
 *
 * @param zone - The zone the joystick was in when the request was made.
 */
void updateLateralRotation(int zone) {

  // Used to log timing, don't change.
  static long lastUpdateTime = 0;

  /*  The servos can make changes faster than needed or wanted. To smooth
   *  movements out and avoid innundating them with rapid requests, alter
   *  this value until movments feel natural or the way you like.
   */
  int movementDelayInterval = 25;

  // Only change if the desired interval since last update has passed.
  if(hasEnoughTimePassed(movementDelayInterval, lastUpdateTime)) {

    // Zone arrays move R to L.
    if(zone <= numZones / 2) {

      turnRight(zone);

    } else {

      turnLeft(zone);

    }

    lastUpdateTime = millis();

  }

} // end updateHorizontal


/**
 * This function handles lateral turns to the right.
 *
 * @param zone - The zone the joystick was in when the request was made.
 */
void turnRight(int zone) {

  int angleR = headRightServo.read();
  int angleL = headLeftServo.read();

  /**
   * Turns to the right mean the angle on both R and L servos will
   * increase. R will turn towards the main helmet (RISE), L will turn
   * away (DIP). We also need to make sure they don't turn past their
   * safety limits.
   */

  // Handle right servo, only change if not already at the limit.
  if(angleR < R_HEAD_HOME + RISE_LIMIT) {

    if(currentMode == JERKY_MODE) {

      angleR = R_HEAD_HOME + RISE_LIMIT;

    } else { // Smooth mode

      angleR = min(angleR + lateralMoveAmount[zone - 1], R_HEAD_HOME + RISE_LIMIT);

    }

    headRightServo.write(angleR);

  }

  // Handle left servo, only change if not already at the limit.
  if(angleL < L_HEAD_HOME + DIP_LIMIT) {

    if(currentMode == JERKY_MODE) {

      angleL = L_HEAD_HOME + DIP_LIMIT;

    } else { // Smooth Mode

      angleL = min(angleL + lateralMoveAmount[zone - 1], L_HEAD_HOME + DIP_LIMIT);

    }

    headLeftServo.write(angleL);

  }

} // end turnRight


/**
 * This function handles lateral turns to the left.
 *
 * @param zone - The zone the joystick was in when the request was made.
 */
void turnLeft(int zone) {

  int angleR = headRightServo.read();
  int angleL = headLeftServo.read();

  /**
   * Turns to the left mean the angle on both R and L servos will
   * decrease. R will turn away from the main helmet (DIP), L will turn
   * towards (RISE). We also need to make sure they don't turn past their
   * safety limits.
   */

  // Handle right servo, only change if not already at the limit.
  if(angleR > R_HEAD_HOME - DIP_LIMIT) {

    if(currentMode == JERKY_MODE) {

      angleR = R_HEAD_HOME - DIP_LIMIT;

    } else { // Smooth Mode

      angleR = max(angleR - lateralMoveAmount[zone - 1], R_HEAD_HOME - DIP_LIMIT);

    }

    headRightServo.write(angleR);

  }

  // Handle left servo, only change if not already at the limit.
  if(angleL > L_HEAD_HOME - RISE_LIMIT) {

    if(currentMode == JERKY_MODE) {

      angleL = L_HEAD_HOME - RISE_LIMIT;

    } else { // Smooth mode

      angleL = max(angleL - lateralMoveAmount[zone - 1], L_HEAD_HOME - RISE_LIMIT);

    }

    headLeftServo.write(angleL);

  }

} // end turnLeft


/**
 * This function will handle any requests to update the heads tilt left
 * or right.
 *
 * @param zone - The zone the joystick was in when the request was made.
 */
void updateTilt(int zone) {

  // Used to log timing, don't change.
  static long lastUpdateTime = 0;

  /*  Servos can make changes faster than needed or even wanted. To smooth
   *  movements out and avoid innundating them with rapid requests, alter
   *  this value until movments feel natural or just the way you like.
   */
  int tiltDelayInterval = 25;

  if(hasEnoughTimePassed(tiltDelayInterval, lastUpdateTime)){

    // Zone arrays for tilt move R to L.
    if(zone <= numZones / 2) {

      tiltRight(zone);

    } else {

      tiltLeft(zone);

    }

    lastUpdateTime = millis();

  }

} // end updateTilt


/**
 * This function handles tilting the head to the right.
 *
 * @param zone - The zone the joystick was in when the request was made.
 */
void tiltRight(int zone) {

  int angleC = headCenterServo.read();

  /**
   * Tilting to the right means the angle on servo C will increase. This is
   * simpler than lateral movements but we still need to make sure it doesn't
   * turn past a safe limit.
   */

  // Only change if not already at the limit.
  if(angleC < C_HEAD_HOME + TILT_LIMIT) {

    if(currentMode == JERKY_MODE) {

      angleC = C_HEAD_HOME + TILT_LIMIT;

    } else { // Smooth mode

      angleC = min(angleC + tiltMoveAmount[zone - 1], C_HEAD_HOME + TILT_LIMIT);

    }

    headCenterServo.write(angleC);

  }

} // end tiltRight


/**
 * This function handles tilting the head to the left.
 *
 * @param zone - The zone the joystick was in when the request was made.
 */
void tiltLeft(int zone) {

  int angleC = headCenterServo.read();

  /**
   * Tilting to the left means the angle on servo C will decrease. This is
   * simpler than lateral movements but we still need to make sure it doesn't
   * turn past a safe limit.
   */

  // Only change if not already at the limit.
  if(angleC > C_HEAD_HOME - TILT_LIMIT) {

    if(currentMode == JERKY_MODE) {

      angleC = C_HEAD_HOME - TILT_LIMIT;

    } else { // Smooth mode

      angleC = max(angleC - tiltMoveAmount[zone - 1], C_HEAD_HOME - TILT_LIMIT);

    }

    headCenterServo.write(angleC);

  }

} // end tiltLeft


/**
 * This function is used to recenter the head/mask.
 */
void recenterHead() {

  /* We don't want to trigger this repetatively if the button gets
   * held down for more than one loop so we'll check that a minimum
   * time has passed between performing this action.
   */
  static long lastUpdateTime = 0;
  long buttonDelayInterval = 250;

  if(hasEnoughTimePassed(buttonDelayInterval, lastUpdateTime)) {

    headCenterServo.write(C_HEAD_HOME);
    headRightServo.write(R_HEAD_HOME);
    headLeftServo.write(L_HEAD_HOME);
    lastUpdateTime = millis();

  }

} // end recenterHead



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

} // end updateFinPositions


/**
 * This function will update fin positions. This may include resetting the fins
 * between mood changes or random fluxuations done for effect.
 *
 * @param resetFins - A boolean true if the fins should be reset, otherwise false.
 * The exact position will depend current mood of the system.
 */
void updateFinPositions(boolean resetFins) {

  // The servos take a certain amount of time to open/close... the transistor
  // needs to stay high for that whole time.
  static long lastTransitionTime = 0;
  int transitionDelay = 1000;

  if(resetFins) {

    digitalWrite(FIN_TRANSISTOR_PIN, HIGH);

    if(currentMood) {

      finRightServo.write(R_FIN_HOME);
      finLeftServo.write(L_FIN_HOME);

    } else {

      finRightServo.write(R_FIN_HOME + FIN_RANGE);
      finLeftServo.write(L_FIN_HOME - FIN_RANGE);

    }

    lastTransitionTime = millis();

  }

  if(hasEnoughTimePassed(transitionDelay, lastTransitionTime)) {

    digitalWrite(FIN_TRANSISTOR_PIN, LOW);

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

  /* There is a specific animation that occurs each time the mood flips which
   * involvs the eyes and ear fins. We really shouldn't deal with that in this
   * service routine because nothing else will get happen until it wraps up.
   * So, we're going to flag that a change was requested and the main loop
   * will handle in it's own way.
   */
  moodChanged = true; // Flag for outside methods to do something.

} // end changeMood

/**
 * Interrupt Service Routine for the change mode button.
 */
void changeMode() {

  // Simply toggling between 0 and 1.
  currentMode = (currentMode + 1) % 2;

} // end changeMode
