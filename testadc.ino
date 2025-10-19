// Copyright 2025 Dmitry Ivanov
// v.1.1

// Flag for processing timer interrupts
volatile bool oneSecFlag = false;

// Compare value for Timer1
constexpr uint16_t CMP_VAL_T1 = 16000000 / 1024 * 1 - 1;   // (16MHz / 1024 prescaler) * 1s - 1

// String to store recieved data from UART
String incomingString = "";

// Control commands
String CMD_START_EXT  = "STE";
String CMD_START_INT  = "STI";
String CMD_STOP       = "OFF";
String CMD_CALIBRATE  = "CAL";
String CMD_FILTER     = "FIL";

// UART baudrate
#define BAUDRATE 9600

// Flags to determine current state
bool isActive            = false;
bool isInCalibrationMode = false;
bool filter              = true;

// Precise values of external and internal Uref
#define UREF_EXT_VAL 4.980
#define UREF_INT_VAL 1.127

// Current Uref value
float urefValue = 0.0;

// Values of resistors in voltage divider
const int R1 = 9310;
const int R2 = 816;

// Calibration values
float calibValueK = 1.0;
float calibValueB = 0.0;

// Last measured voltage
float measure = 0.0;
// Filtered voltage
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

  printHelpUART();
}

// Print initial information in the UART
void printHelpUART()
{
  Serial.println("[v] Готов. Список доступных команд:");
  Serial.println("  STE -> Запуск измерения с внешним Uоп");
  Serial.println("  STI -> Запуск измерения с внутренним Uоп");
  Serial.println("  OFF -> Остановка");
  Serial.println("  CAL -> Выполнение калибровки");
  Serial.println("  FIL -> Вкл/выкл цифровой фильтр");
}

// Receive commands from the UART
void receiveCommandUART()
{
  while (Serial.available() > 0) {
    incomingString = Serial.readStringUntil('\n');
  }
  
  if (incomingString == CMD_START_EXT) {
    Serial.println("[v] Запуск с ВНЕШНИМ Uоп...");
    isActive = true;
    isInCalibrationMode = false;
    delay(1000);
    analogReference(DEFAULT);
    delay(1000);
    urefValue = UREF_EXT_VAL;
  } else if (incomingString == CMD_START_INT) {
    Serial.println("[v] Запуск с ВНУТРЕННИМ Uоп...");
    Serial.println("[!] ЗАПРЕЩАЕТСЯ ПОДАВАТЬ НА АЦП БОЛЬШЕ 12 ВОЛЬТ!");
    isActive = true;
    isInCalibrationMode = false;
    delay(1000);
    analogReference(INTERNAL);
    delay(1000);
    urefValue = UREF_INT_VAL;
  } else if (incomingString == CMD_STOP) {
    isActive = false;
    isInCalibrationMode = false;
    Serial.println("[v] Остановлено");
    delay(1000);
    analogReference(DEFAULT);
    delay(1000);
    printHelpUART();
  } else if (incomingString == CMD_CALIBRATE) {
    isActive = false;
    isInCalibrationMode = true;
    Serial.println("[v] Запуск режима калибровки...");
    delay(1000);
    analogReference(DEFAULT);
    delay(1000);
    performCalibration();
  } else if (incomingString == CMD_FILTER) {
    filter = !filter;
    if (filter) {
      Serial.println("[v] Цифровой фильтр включен");
    } else {
      Serial.println("[v] Цифровой фильтр выключен");
    }
  } else if (incomingString != "") {
    Serial.println("[x] Неверная команда");
  }
  
  incomingString = "";                
}

// Print voltage values in UART
void printADCValUART()
{
  if (isActive && !isInCalibrationMode) {
    Serial.print("U = ");
    Serial.println(voltage, 3);
  }
}

// Get and set new calibration values
void performCalibration()
{
  Serial.setTimeout(60000);
  Serial.println("[!] Передаточная характеристика АЦП представима как уравнение вида:");
  Serial.println("    Uацп = k * Uфакт + b");
  Serial.println("Введите калибровочные коэффициенты k и b");
  Serial.println("Используйте '.' в качестве десятичного разделителя");
  Serial.println(">>> k = ");
  while (isInCalibrationMode) {
    if (Serial.available() > 0) {
      calibValueK = Serial.parseFloat();
      Serial.println(calibValueK);
      Serial.println(">>> b = ");
      while (isInCalibrationMode) {
        if (Serial.available() > 0) {
          calibValueB = Serial.parseFloat();
          Serial.println(calibValueB);
          Serial.println("[v] Новые калибровочные коэффициенты установлены");
          printHelpUART();
          isInCalibrationMode = false;
          Serial.setTimeout(1000);
        }
      }
    }
  }
}

void loop() 
{
  if (isActive && !isInCalibrationMode) {
    if (filter) {
      for (int i = 0; i < 100; ++i) {
        measure += (float)analogRead(U_PIN) * calibValueK * urefValue * ((R1 + R2) / R2) / 1024.0 + calibValueB;
      }
      voltage = measure / 100.0;
      measure = 0;
    } else {
      measure = (float)analogRead(U_PIN) * calibValueK * urefValue * ((R1 + R2) / R2) / 1024.0 + calibValueB;
      voltage = measure;
    }
  }

  if (oneSecFlag) {
    oneSecFlag = false;
    printADCValUART();
  }

  receiveCommandUART();
}

// Interrupt service routine for 1s
ISR(TIMER1_COMPA_vect) 
{
  oneSecFlag = true;
}
