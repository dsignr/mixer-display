/**************************************************************************
 This is an example for our Monochrome OLEDs based on SSD1306 drivers

 Pick one up today in the adafruit shop!
 ------> http://www.adafruit.com/category/63_98

 This example is for a 128x32 pixel display using I2C to communicate
 3 pins are required to interface (two I2C and one reset).

 Adafruit invests time and resources providing this open
 source code, please support Adafruit and open-source
 hardware by purchasing products from Adafruit!

 Written by Limor Fried/Ladyada for Adafruit Industries,
 with contributions from the open source community.
 BSD license, check license.txt for more information
 All text above, and the splash screen below must be
 included in any redistribution.
 **************************************************************************/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

// Declaration for an SSD1306 display connected to I2C (SDA, SCL pins)
// The pins for I2C are defined by the Wire-library. 
// On an arduino UNO:       A4(SDA), A5(SCL)
// On an arduino MEGA 2560: 20(SDA), 21(SCL)
// On an arduino LEONARDO:   2(SDA),  3(SCL), ...
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C ///< See datasheet for Address; 0x3D for 128x64, 0x3C for 128x32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define LOGO_HEIGHT   16
#define LOGO_WIDTH    16

#define ROT_A A1
#define ROT_B A2
#define ROT_BUTTON 2
#define R_IC 6  // current sense right
#define L_IC 10  // current sense  Example threshold for stall current
#define L_PWM 12
#define R_PWM 3
#define L_EN 11
// #define R_EN - direct 5v

int counter = 0; 
int currentDuty = 0;
int aState;
int aLastState;

int menuLevel = 0;
int selectedIndex = 0;
volatile bool buttonPressed = false;
unsigned long debounceDelay = 0;
bool overCurrent = false;
volatile bool stopRequested = false;  // Flag to signal stopping

// Menus
const char* mainMenu[] = {"Preset", "Manual"};
// const char* presetMenu[] = {"Almond Milk", "Nuts", "Coconut", "Spices", "Liquids", "Exit"};
const char* presetMenu[] = {"Almond Milk", "Smoothie", "Spices", "Exit"};
const char* manualMenu[] = {"Pulse", "Continuous", "Exit"};
const char* pulseMenu[] = {"5", "10", "30", "Exit"};
const char* continuousMenu[] = {"Run", "Exit"};

const char** currentMenu;
int menuSize;

char dispCharArray[10];

bool motorRunning = false;  

void setup() {

  pinMode(ROT_A, INPUT);
  pinMode(ROT_B, INPUT);
  pinMode(ROT_BUTTON, INPUT_PULLUP); 
  pinMode(R_IC, INPUT); 
  pinMode(L_IC, INPUT); 
  // pinMode(R_EN, INPUT); 
  pinMode(L_EN, OUTPUT); 
  pinMode(L_PWM, OUTPUT); 
  pinMode(R_PWM, OUTPUT);

  digitalWrite(L_EN, HIGH);  

  analogWrite(L_PWM, 0);   // Set L_PWM to LOW forever
  analogWrite(R_PWM, 0);   // Ensure motor is initially off

  
  // digitalWrite(L_PWM, HIGH);  

  attachInterrupt(digitalPinToInterrupt(ROT_BUTTON), handleButtonPress, FALLING);

  Serial.begin(9600);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    for (;;) ; // Don't proceed, loop forever
  }

  display.clearDisplay();

  aLastState = digitalRead(ROT_A);
  updateDisp("WELCOME", "NEYA"); 
  delay(2000);

  setMenu(mainMenu, 2);  // Initialize with the main menu
  showMenu();
}

void loop() { 
  aState = digitalRead(ROT_A);
  int btnState = digitalRead(ROT_BUTTON);

  if (aState != aLastState) {     
    if (digitalRead(ROT_B) != aState) { 
      selectedIndex = (selectedIndex + 1) % menuSize;
    } else {
      selectedIndex = (selectedIndex - 1 + menuSize) % menuSize;
    }
    showMenu();
  } 
  aLastState = aState;

  if (buttonPressed) {
    buttonPressed = false;
    if (motorRunning) {
      stopMotor();
    } else {
      handleSelection();
    }
  }
}

void updateDisp(String top, String bottom) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(top);

  display.setTextSize(2);
  display.println(bottom);
  display.display();
}

void handleSelection() {
  if (menuLevel == 0) {  // Main Menu
    if (selectedIndex == 0) {
      // Preset Menu Selected
      setMenu(presetMenu, 4);  // Move to Preset Menu
      menuLevel = 1;
    } else {
      // Manual Mode Selected
      setMenu(manualMenu, 3);  // Move to Manual Menu
      menuLevel = 2;
    }
  } 
  else if (menuLevel == 1) {  // Preset Menu
    if (selectedIndex < 3) {  // Almond Milk, Smoothie, or Spices Selected
      if (selectedIndex == 0) presetAlmondMilk();
      else if (selectedIndex == 1) presetSmoothie();
      else if (selectedIndex == 2) presetSpices();
    } else {
      // Exit Selected from Preset Menu
      setMenu(mainMenu, 2);  // Return to Main Menu
      menuLevel = 0;
    }
  } 
  else if (menuLevel == 2) {  // Manual Menu
    if (selectedIndex == 0) {
      // Pulse Mode Selected
      setMenu(pulseMenu, 4);  // Move to Pulse Options
      menuLevel = 3;
    } else if (selectedIndex == 1) {
      // Continuous Mode Selected
      setMenu(continuousMenu, 2);  // Move to Continuous Run Options
      menuLevel = 4;
    } else {
      // Exit Selected from Manual Menu
      setMenu(mainMenu, 2);  // Return to Main Menu
      menuLevel = 0;
    }
  } 
  else if (menuLevel == 3) {  // Pulse Options
    if (selectedIndex == 3) {
      // Exit Selected from Pulse Options
      setMenu(manualMenu, 3);  // Return to Manual Menu
      menuLevel = 2;
    } else {
      // Run Pulse Mode based on selected index
      handlePulseMode(selectedIndex);
    }
  } 
  else if (menuLevel == 4) {  // Continuous Run Options
    if (selectedIndex == 1) {
      // Exit Selected from Continuous Run Options
      setMenu(manualMenu, 3);  // Return to Manual Menu
      menuLevel = 2;
    } else {
      // Start Continuous Mode
      handleContinuousMode();
    }
  }

  showMenu();  // Display the current menu on OLED
}



void setMenu(const char** menu, int size) {
  currentMenu = menu;
  menuSize = size;
  selectedIndex = 0;
}

void showMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  for (int i = 0; i < menuSize; i++) {
    display.println((i == selectedIndex ? "> " : "  ") + String(currentMenu[i]));
  }
  display.display();
}

void handleButtonPress() {
  if (millis() - debounceDelay > 200) {  // Debounce the button
    Serial.println(F("Button Pressed"));
    buttonPressed = true;  // Flag that button was pressed

    // Toggle motor state only if running
    // if (motorRunning) {
    //   stopMotor();  // Stop motor if running
    //   setMenu(mainMenu, 2);
    // }
  }
  debounceDelay = millis();
}

bool checkOverCurrent() {
  // int faultState = digitalRead(L_IC);

  // if (faultState == HIGH) {  // Overcurrent detected
  //   Serial.println(F("Overcurrent detected!"));
  //   updateDisp("WARNING", "STALL");
  //   stopRequested = true;  // Set the flag to stop the loop
  //   stopMotor();           // Stop the motor
  //   return true;
  // }
  return false;
}


void startMotor(int duty, int runTime) {
  if (duty < 0) duty = 0;
  if (duty > 255) duty = 255;

  motorRunning = true;
  unsigned long startTime = millis();

  softStartMotor(duty);

  while (motorRunning) {
    if (checkOverCurrent()) return;  // Stop if overcurrent detected
    // if (buttonPressExitCheck()) return;

    if (runTime > 0 && (millis() - startTime >= runTime)) {
      stopMotor();
      return;
    }

    delay(10);  // Small delay to prevent fast loop execution
  }
}



void stopMotor() {
  digitalWrite(L_EN, LOW);  // Disable motor
  digitalWrite(L_PWM, LOW);  
  digitalWrite(R_PWM, LOW); 
  motorRunning = false;  // Reset motor state
}

void setPWMDutyCycle(int duty) {
  OCR2A = map(duty, 0, 100, 0, 255);
}

void softStartMotor(int targetDuty) {
  currentDuty = targetDuty;
  if (checkOverCurrent()) return;
  digitalWrite(L_EN, HIGH);
  
  // for (int duty = 0; duty <= targetDuty; duty++) {
  //   setPWMDutyCycle(duty);
  //   delay(50);
  // }
  for (int duty = 0; duty <= targetDuty; duty += 5) {
    analogWrite(R_PWM, duty);  // Increase PWM in small steps
    delay(30);  // Delay to smooth the start
  }
  // digitalWrite(R_PWM, HIGH);
  motorRunning = true;
}

void presetAlmondMilk() {
  stopRequested = false;  // Reset the flag before starting

  // Step 1: Pulsing
  for (int i = 0; i <= 15; i++) {
    if (stopRequested) break;  // Break if stop is requested
    String loop = "PULSE " + String(i);
    updateDisp(loop, "ALMOND MLK");
    startMotor(255, 2500);
    stopMotor();
    delay(1000);
  }

  if (stopRequested) return;  // Exit the function if stop requested

  // Step 2: Add Water
  for (int i = 0; i <= 10; i++) {
    if (stopRequested) break;
    String loop = "WATER " + String(i);
    updateDisp("ALMOND MLK", loop);
    delay(1000);
  }

  if (stopRequested) return;

  // Step 3: Final Blending
  for (int i = 0; i <= 10; i++) {
    if (stopRequested) break;
    String loop = "POWER " + String(i);
    updateDisp(loop, "ALMOND MLK");
    startMotor(255, 5000);
    stopMotor();
    delay(1000);
  }

  if (!stopRequested) {  // Only display "DONE" if not stopped
    updateDisp("ALMOND MLK", "DONE");
    delay(5000);
  }

  motorRunning = false;
  setMenu(mainMenu, 2);
  menuLevel = 0;
  showMenu();
}


void presetSmoothie() {
  stopRequested = false;  // Reset the flag

  // Step 1: Pulse to break ice and fruit chunks
  for (int i = 0; i <= 10; i++) {
    if (stopRequested) break;  // Exit loop if stop is requested
    String loop = "PULSE " + String(i);
    updateDisp(loop, "SMOOTHIE");
    startMotor(180, 2000);
    stopMotor();
    delay(1000);
  }

  if (stopRequested) return;

  // Step 2: Add Water
  for (int i = 0; i <= 10; i++) {
    if (stopRequested) break;
    String loop = "WATER " + String(i);
    updateDisp("SMOOTHIE", loop);
    delay(1000);
  }

  if (stopRequested) return;

  // Step 3: Blend at full speed
  for (int i = 0; i <= 5; i++) {
    if (stopRequested) break;
    String loop = "BLEND " + String(i);
    updateDisp(loop, "SMOOTHIE");
    startMotor(255, 4000);
    stopMotor();
    delay(1000);
  }

  if (stopRequested) return;

  // Step 4: Final touch with short pulses
  for (int i = 0; i <= 5; i++) {
    if (stopRequested) break;
    String loop = "FINAL " + String(i);
    updateDisp(loop, "SMOOTHIE");
    startMotor(200, 1000);
    stopMotor();
    delay(500);
  }

  if (!stopRequested) {
    updateDisp("SMOOTHIE", "DONE");
    delay(5000);
  }

  motorRunning = false;
  setMenu(mainMenu, 2);
  menuLevel = 0;
  showMenu();
}

void presetSpices() {
  stopRequested = false;  // Reset the flag

  // Step 1: Short pulses to break down whole spices
  for (int i = 0; i <= 10; i++) {
    if (stopRequested) break;
    String loop = "PULSE " + String(i);
    updateDisp(loop, "SPICES");
    startMotor(150, 1000);
    stopMotor();
    delay(500);
  }

  if (stopRequested) return;

  // Step 2: Grind continuously at high speed
  for (int i = 0; i <= 5; i++) {
    if (stopRequested) break;
    String loop = "GRIND " + String(i);
    updateDisp(loop, "SPICES");
    startMotor(255, 3000);
    stopMotor();
    delay(1000);
  }

  if (stopRequested) return;

  // Step 3: Final smooth grind with pulses
  for (int i = 0; i <= 5; i++) {
    if (stopRequested) break;
    String loop = "FINE " + String(i);
    updateDisp(loop, "SPICES");
    startMotor(200, 500);
    stopMotor();
    delay(300);
  }

  if (!stopRequested) {
    updateDisp("SPICES", "DONE");
    delay(5000);
  }

  motorRunning = false;
  setMenu(mainMenu, 2);
  menuLevel = 0;
  showMenu();
}



void handlePulseMode(int pulseIndex) {
  int pulseOnTime = 0;  // Pulse duration in milliseconds
  int pulseOffTime = 500;  // Time between pulses (in ms)
  int totalPulses = 5;  // Number of pulses to perform

  // Set the pulse duration based on the selected index
  switch (pulseIndex) {
    case 0:  // 5-second pulse
      pulseOnTime = 5000;
      break;
    case 1:  // 10-second pulse
      pulseOnTime = 10000;
      break;
    case 2:  // 20-second pulse
      pulseOnTime = 20000;
      break;
    case 3:  // Exit
      setMenu(manualMenu, 3);  // Return to Manual Menu
      menuLevel = 2;
      return;  // Exit the function
  }

  // Perform the pulses
  for (int i = 0; i < totalPulses; i++) {
    // if (buttonPressExitCheck()) return;  // Exit if button is pressed

    // Display pulse information
    String loop = "PULSE " + String(i + 1);
    updateDisp(loop, String(pulseOnTime / 1000) + " SECS");

    // Start the motor and wait for the pulse duration
    startMotor(255, pulseOnTime);
    stopMotor();

    // Display rest period message, if applicable
    if (i < totalPulses - 1) {
      updateDisp("500MS", "REST");
      delay(pulseOffTime);  // Wait between pulses
    }
  }

  // Display completion message
  updateDisp("PULSE", "DONE");
  delay(1000);  // Small delay before returning to the menu
}

void handleContinuousMode() {
  int maxRunTime = 30000;  // Maximum time to run the motor (30 seconds)
  int elapsedTime = 0;  // Track the elapsed time in milliseconds
  int checkInterval = 1000;  // Check every second (1000 ms)
  int counter = 0;  // Counter to display elapsed seconds

  updateDisp("CONTINUOUS", "RUNNING");

  startMotor(255, 20000);

  // Stop the motor after max run time
  stopMotor();

  updateDisp("CONTINUOUS", "DONE");
  delay(500);  // Small delay before returning to the menu

  // Return to the manual menu
  setMenu(manualMenu, 3);  
  menuLevel = 2;
  showMenu();  // Refresh the menu display
}


bool buttonPressExitCheck() {
  if (buttonPressed) {
    buttonPressed = false;  // Reset the flag
    stopRequested = true;   // Set the flag to stop the loop
    stopMotor();            // Stop the motor
    delay(500);             // Debounce
    setMenu(mainMenu, 2);   // Return to Main Menu
    menuLevel = 0;
    showMenu();
    return true;
  }
  return false;
}
