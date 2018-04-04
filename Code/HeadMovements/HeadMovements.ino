/**
 * This is the base arduino sketch for testing and setting up the head  
 * movements on the Stargate Jaffa helmet. In this, there are three servos 
 * which tilt the head and make it "look" left and right. It also has  
 * different behavior depending on the current mood and mode (thought how 
 * to toggle between moods and modes isn't handled here.
 * 
 * For a more detailed explanation, you can read the overview at... 
 * https://github.com/ThomasDCarney/Stargate-JaffaHelmet/blob/master/MovementOverview.md
 * 
 * Use and modify as you like!
 * 
 * Author: Thomas Carney
 * Date: 04/02/2018
 * Last Update: 04/04/2018
 */

// Include the header file for servo controls.
#include <Servo.h>

// Uncomment next line for debugging.
// #define DEBUGGING

// Button 1 is attached to digial pin 4.
const int BUTTON_1_PIN = 4;

// Specify the analog pins used by the joystick potentiometers.
const int JOYSTICK_X_PIN = 0;
const int JOYSTICK_Y_PIN = 1;

// Specify pins used by each servo.
const int C_HEAD_PIN = 6;
const int R_HEAD_PIN = 7;
const int L_HEAD_PIN = 8;

/*  Specify servo angles where the head appears "centered". This may 
 *  vary due to multiple items, primarily hardware related to housing 
 *  and mounting the servos onto the helmet, adjust as needed.
 */
const int C_HEAD_HOME = 80;
const int R_HEAD_HOME = 70;
const int L_HEAD_HOME = 120;

/*  Specify the max angles the servos can vary from center. 
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
const int RISE_LIMIT = 30;
const int DIP_LIMIT = 30;

/**
 * I don't deal with the button setup to change mode here but they are 
 * required and you can manually change it for testing.
 */
const int jerkyMode = 0;
const int smoothMode = 1;
int currentMode = jerkyMode;

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

// Create the servo objects which we'll manipulate.
Servo headCenterServo;
Servo headRightServo;
Servo headLeftServo;


void setup() {

  #ifdef DEBUGGING

    Serial.begin(9600);

  #endif

  // Attach each servo to its input pin.
  headCenterServo.attach(C_HEAD_PIN);
  headRightServo.attach(R_HEAD_PIN);
  headLeftServo.attach(L_HEAD_PIN);

  // Initially center the head.
  headCenterServo.write(C_HEAD_HOME);
  headRightServo.write(R_HEAD_HOME);
  headLeftServo.write(L_HEAD_HOME);

} // end setup


void loop() {

  int lateralZone = getZone(analogRead(JOYSTICK_X_PIN));
  int tiltZone = getZone(analogRead(JOYSTICK_Y_PIN));
  
  boolean lateralMovementRequested = isLateralMovementRequested(lateralZone);
  boolean tiltRequested = isTiltRequested(tiltZone);

  // Check for movement requests, if none, poll button.
  if(lateralMovementRequested || tiltRequested) {

    if(lateralMovementRequested) {

      updateLateralRotation(lateralZone);
      
    }

    if(tiltRequested) {

      updateTilt(tiltZone);
      
    }
    
  } else {

    if(digitalRead(BUTTON_1_PIN)) {

      recenterHead();
      
    }

  } // end outer IF-ELSE block

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
  if((currentMode == jerkyMode && lateralJerkyZones[zone - 1] == 1) ||
     (currentMode == smoothMode && lateralSmoothZones[zone - 1] == 1)) {

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

  if((currentMode == jerkyMode && tiltJerkyZones[zone - 1] == 1) ||
     (currentMode == smoothMode && tiltSmoothZones[zone - 1] == 1)) {

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

    if(currentMode == jerkyMode) {

      angleR = R_HEAD_HOME + RISE_LIMIT;
      
    } else { // Smooth mode

      angleR = min(angleR + lateralMoveAmount[zone - 1], R_HEAD_HOME + RISE_LIMIT);
      
    }

    headRightServo.write(angleR);
    
  }

  // Handle left servo, only change if not already at the limit.
  if(angleL < L_HEAD_HOME + DIP_LIMIT) {

    if(currentMode == jerkyMode) {

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

    if(currentMode == jerkyMode) {

      angleR = R_HEAD_HOME - DIP_LIMIT;
      
    } else { // Smooth Mode

      angleR = max(angleR - lateralMoveAmount[zone - 1], R_HEAD_HOME - DIP_LIMIT);
      
    }
    
    headRightServo.write(angleR);
    
  }

  // Handle left servo, only change if not already at the limit.
  if(angleL > L_HEAD_HOME - RISE_LIMIT) {

    if(currentMode == jerkyMode) {

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

    if(currentMode == jerkyMode) {

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

    if(currentMode == jerkyMode) {

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
