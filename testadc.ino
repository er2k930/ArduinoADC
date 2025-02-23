// Copyright 2025 Dmitry Ivanov
// v.1.0

// Flag for processing timer interrupts
volatile bool oneSecFlag = false;

// Compare value for Timer1
constexpr uint16_t CMP_VAL_T1 = 16000000 / 1024 * 1 - 1;   // (16MHz / 1024 prescaler) * 1s - 1

// String to store recieved data from USART
String incomingString = "";

// Control commands
String CMD_START_EXT  = "STE";
String CMD_START_INT  = "STI";
String CMD_STOP       = "OFF";
String CMD_CALIBRATE  = "CAL";

// USART baudrate
#define BAUDRATE 9600

// Flags to determine current state
bool isActive            = false;
bool isInCalibrationMode = false;

// Precise values of external and internal Uref
#define UREF_EXT_VAL 4.980
#define UREF_INT_VAL 1.127

// Current Uref value
float urefValue = 0.0;

// Values of resistors in voltage divider
const int R1 = 9310;
const int R2 = 816;

// Calibration value
float calibValue = 1.0;

// Last measured voltage
float voltage = 0.0;

// Pin number measuring voltage
#define U_PIN A0


void setup() 
{
  Serial.begin(BAUDRATE);
  
  cli();

  // Configuring Timer1 for 1s
  TCCR1A = 0;   // clear control register A
  TCCR1B = 0;   // clear control register B
  TCNT1 = 0;    // zero-initialize counter value
  TCCR1B |= (1 << WGM12); // set CTC mode
  OCR1A = CMP_VAL_T1;     // set compare match register for 1s
  TCCR1B |= (1 << CS12) | (1 << CS10);  // set prescaler to 1024
  TIMSK1 |= (1 << OCIE1A);              // enable compare interrupt

  sei();

  printHelpUSART();
}

// Print initial information in the USART
void printHelpUSART()
{
  Serial.println("[OK] Ready! List of available commands:");
  Serial.println("STE -> Start measuring with external Uref");
  Serial.println("STI -> Start measuring with internal Uref");
  Serial.println("OFF -> Stop measuring");
  Serial.println("CAL -> Perform calibration");
}

// Recieve commands from the USART
void recieveCommandUSART()
{
  while (Serial.available() > 0) {
    incomingString = Serial.readStringUntil('\n');
  }
  
  if (incomingString == CMD_START_EXT) {
    Serial.println("[OK] Starting with EXTERNAL Uref...");
    isActive = true;
    isInCalibrationMode = false;
    delay(1000);
    analogReference(DEFAULT);
    delay(1000);
    urefValue = UREF_EXT_VAL;
  } else if (incomingString == CMD_START_INT) {
    Serial.println("[OK] Starting with INTERNAL Uref...");
    isActive = true;
    isInCalibrationMode = false;
    delay(1000);
    analogReference(INTERNAL);
    delay(1000);
    urefValue = UREF_INT_VAL;
  } else if (incomingString == CMD_STOP) {
    isActive = false;
    isInCalibrationMode = false;
    Serial.println("[OK] Stopped");
    printHelpUSART();
  } else if (incomingString == CMD_CALIBRATE) {
    isActive = false;
    isInCalibrationMode = true;
    Serial.println("[OK] Starting calibration mode...");
    performCalibration();
  }
  else if (incomingString != "") {
    Serial.println("[WARNING] Invalid command");
  }
  
  incomingString = "";                
}

// Print voltage values in USART
void printADCValUSART()
{
  if (isActive && !isInCalibrationMode) {
    Serial.print("U = ");
    Serial.println(voltage);
  }
}

// Get and set new calibration value
void performCalibration()
{
  calibValue = 1.0;
  Serial.println("[CAL] Enter new calibration value (in %)");
  Serial.println("[CAL] Use '.' as a decimal separator");
  Serial.println("[CAL] >>> ");
  while (isInCalibrationMode) {
    if (Serial.available() > 0) {
      float cal = Serial.parseFloat();
      calibValue = calibValue * (1 + cal/100.0);
      Serial.print("[OK] New calibration value set: ");
      Serial.print(cal);
      Serial.println("%");
      printHelpUSART();
      isInCalibrationMode = false;
    }
  }
}

void loop() 
{
  if (isActive && !isInCalibrationMode) {
    voltage = (float)analogRead(U_PIN) * calibValue * urefValue * ((R1 + R2) / R2) / 1024.0;
  }

  if (oneSecFlag) {
    oneSecFlag = false;
    printADCValUSART();
  }

  recieveCommandUSART();
}

// Interrupt service routine for 1s
ISR(TIMER1_COMPA_vect) 
{
  oneSecFlag = true;
}