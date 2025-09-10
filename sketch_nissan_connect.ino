#include <Wire.h>
#include <Adafruit_MCP4725.h>

// Создаем объекты для двух ЦАПов с разными адресами
Adafruit_MCP4725 dacA; // Адрес 0x60 (A0 подключен к GND - DEFAULT)
Adafruit_MCP4725 dacB; // Адрес 0x61 (A0 подключен к VCC)

// Пины для подключения аналоговых входов с руля
#define INPUT_A A0
#define INPUT_B A1

// ЦАП константы для выходов на магнитолу
const int outputPin_A = 5; // dacA - для канала A
const int outputPin_B = 6; // dacB - для канала B

// Напряжение питания ЦАП (для Nano обычно 5V)
const float DAC_VREF = 5.0;
const int DAC_MAX = 4095; // 12 бит

// Напряжение питания кнопок (3.47V)
const float TARGET_VOLTAGE = 3.47;

// Параметры обработки
const int DEADZONE = 30;
const int DEBOUNCE_DELAY = 100;
const int READ_DELAY = 90;

// Режим калибровки
bool calibrationMode = false; // true для калибровки, false для работы

struct Button {
  int adcMin;
  int adcMax;
  float emulateVoltage;
  int outputPin;
  const char* name;
  unsigned long lastPressTime;
};

// Кнопки с нужными выходами на каналы магнитолы
Button buttonsChannelA[] = {
  {0,   90,  2.28, outputPin_A,  "A: PHONE", 0},
  {91, 200,  2.00, outputPin_B,  "B: SEEK UP", 0},
  {201, 300,  1.00, outputPin_B,  "B: VOL UP", 0}
};
Button buttonsChannelB[] = {
  {0,   90,  0.00, outputPin_A,   "A: SOURCE", 0},
  {91, 200,  1.00, outputPin_A,  "A: SEEK DOWN", 0},
  {201, 300,  0.00, outputPin_B,  "B: VOL DOWN", 0}
};

const int numButtonsA = sizeof(buttonsChannelA) / sizeof(Button);
const int numButtonsB = sizeof(buttonsChannelB) / sizeof(Button);

void setup() {
  Serial.begin(9600);
  Serial.println("=== Steering Wheel Control ===");

  // Инициализация I2C
  Wire.begin();

  // Инициализация DAC для канала A (адрес 0x60)
  if (!dacA.begin(0x60)) {
    Serial.println("DAC A (0x60) not found!");
    while (1); // Стоп при ошибке
  }

  // Инициализация DAC для канала B (адрес 0x61)
  if (!dacB.begin(0x61)) {
    Serial.println("DAC B (0x61) not found!");
    while (1); // Стоп при ошибке
  }

  Serial.println("DACs initialized successfully!");

  // Изначально подаем на оба канала напряжение "кнопка не нажата"
  setVoltage(outputPin_A, TARGET_VOLTAGE, true);
  setVoltage(outputPin_B, TARGET_VOLTAGE, true);

  if (calibrationMode) {
    Serial.println("--- CALIBRATION MODE ---");
    Serial.println("Press buttons and note ADC values");
    Serial.println("A0 (Pin A) | A1 (Pin B)");
    Serial.println("-----------------------");
  } else {
    Serial.println("System ready for operation");
  }
}

void loop() {
  if (calibrationMode) {
    runCalibration();
  } else {
    runNormalOperation();
  }
}

void runCalibration() {
  int rawA = analogRead(INPUT_A);
  int rawB = analogRead(INPUT_B);

  // Преобразование ADC в напряжение
  float voltageA = (rawA * TARGET_VOLTAGE) / 1023.0;
  float voltageB = (rawB * TARGET_VOLTAGE) / 1023.0;

  Serial.print("A0: ");
  Serial.print(rawA);
  Serial.print(" (");
  Serial.print(voltageA, 2);
  Serial.print("V) | A1: ");
  Serial.print(rawB);
  Serial.print(" (");
  Serial.print(voltageB, 2);
  Serial.println("V)");

  delay(500);
}

void runNormalOperation() {
  // считываем напряжение с кнопок
  int rawA = analogRead(INPUT_A);
  int rawB = analogRead(INPUT_B);
  // обрабатываем нажатия
  int pressPinA = processChannel(rawA, buttonsChannelA, numButtonsA);
  int pressPinB = processChannel(rawB, buttonsChannelB, numButtonsB);

  delay(READ_DELAY);

  // сбрасываем напряжение на нужный канал через DAC
  if(pressPinA > 0)
  {
    setVoltage(pressPinA, TARGET_VOLTAGE, true);
  }
  if(pressPinB > 0)
  {
    setVoltage(pressPinB, TARGET_VOLTAGE, true);
  }
}

int processChannel(int adcValue, Button buttons[], int numButtons) {
  int newValue = TARGET_VOLTAGE;
  int outputPin = 0;

  for (int i = 0; i < numButtons; i++) {
    if (adcValue >= buttons[i].adcMin && adcValue <= buttons[i].adcMax) {
      if (millis() - buttons[i].lastPressTime > DEBOUNCE_DELAY) {
        buttons[i].lastPressTime = millis();

        newValue = buttons[i].emulateVoltage;
        outputPin = buttons[i].outputPin;

        Serial.print("Pressed: ");
        Serial.print(buttons[i].name);
        Serial.print(" | ADC: ");
        Serial.print(adcValue);
        Serial.print(" | Output: ");
        Serial.print(buttons[i].emulateVoltage);
        Serial.print("V");
        Serial.print(" | Output Pin: ");
        Serial.println(outputPin);
      }
      break;
    }
  }
  // Установим нужный вольтаж на необходимый канал магнитолы Nissan Connect 2
  if(outputPin == outputPin_A || outputPin == outputPin_B)
  {
    setVoltage(outputPin, newValue, false);
  }
  return outputPin;
}

// Функция для установки напряжения на указанном пине с помощью DAC MCP4725
void setVoltage(int pin, float voltage, bool saveVol) {
  int value = (voltage / DAC_VREF) * DAC_MAX;
  // Рассчитанное значение для напряжения покоя
  int targetValue = constrain(value, 0, DAC_MAX);

  if(pin == outputPin_A) {
    dacA.setVoltage(targetValue, saveVol);
  }
  if(pin == outputPin_B) {
    dacB.setVoltage(targetValue, saveVol);
  }

  // Блок для отладки через Serial Monitor
  if (calibrationMode) {
    Serial.print("Setting pin ");
    Serial.print(pin);
    Serial.print(" to ");
    Serial.print(voltage);
    Serial.print(" v (DAC = ");
    Serial.print(targetValue);
    Serial.println(")");
  }
}
