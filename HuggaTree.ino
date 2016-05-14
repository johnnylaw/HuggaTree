#include <Arduino.h>
#include <DueTimer.h>
#include <Breath.h>
#include <FIRFilter.h>
#include <BreathingStrip.h>
#include <PressureSensor.h>
#include <RGB.h>
#include <ARMLightStrip.h>
#include <BreathingColor.h>

#define STRIP_LENGTH 150
#define SIGN_STRIP_LENGTH 19
#define STRIP_WRITE_INTERVAL 50
#define SENSOR_POLL_INTERVAL 100
#define SIGN_CYCLE_LENGTH 42
#define NUM_STRIPS 5 // maybe need this

static float stripAngles[NUM_STRIPS];
static ARMLightStrip<51> strip0;
static ARMLightStrip<53> strip1;
static ARMLightStrip<45> strip2;
static ARMLightStrip<43> strip3;
static ARMLightStrip<41> strip4;
static ARMLightStrip<37> signStrip;

static RGB writeBuffer[STRIP_LENGTH];

static RGB signBuffer[SIGN_STRIP_LENGTH * 4 + 1];
static RGB signWriteBuffer[SIGN_STRIP_LENGTH];

float signBufferPosition = SIGN_STRIP_LENGTH;

static ARMLightStripBase * strips[NUM_STRIPS] = {&strip0, &strip1, &strip2, &strip3, &strip4};
static RGB colorBuffer[STRIP_LENGTH];

//static RGB bgColors[2] = { {50, 0, 24}, {100, 0, 50}};  // log
//static RGB bgColors[2] = { {71, 0, 35}, {175, 0, 87}};  // log
//static RGB bgColors[2] = { {20, 0, 5}, {120, 0, 30}}; // linear 
static RGB bgColors[2] = { {50, 0, 12}, {110, 0, 40}}; // linear SLOW
static BreathingColor bgColor = BreathingColor(bgColors[0], bgColors[1], 4000, 0.57);
//static BreathingColor bgColor = BreathingColor(bgColors[0], bgColors[1], 1200, 0.9);

//static BreathingStrip breathingStrip = BreathingStrip(RGB(50, 0, 12), RGB(50, 255, 255), 150);
static BreathingStrip breathingStrip = BreathingStrip(RGB(6, 0, 2), RGB(30, 0, 8), RGB(15, 255, 255), 150);

static RGB stripeColors[2] = { {100, 20, 0}, {100, 0, 60} };
static RGB stripeColorBuffer[STRIP_LENGTH * 2];

static RGB rainbowColorBuffer[STRIP_LENGTH * 2 + 1];

int stripePointer = 150;
static float firCoefficients[] = {0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1};
static FIRFilter<10, int> firFilter(firCoefficients);
//static float firCoefficients[] = {1.0};
//static FIRFilter<1, int> firFilter(firCoefficients);

static PressureSensor<120> pressureSensor;

static Breath breath;

void setup() {
  Serial.begin(115200);
  Timer.getAvailable().attachInterrupt(writeStrips).start(STRIP_WRITE_INTERVAL * 1000);
  Timer.getAvailable().attachInterrupt(readSensor).start(SENSOR_POLL_INTERVAL * 1000);
//  setUpStripeColorBuffer();

  for (int i = 0; i < NUM_STRIPS; i++) stripAngles[i] = (float)i / (float)NUM_STRIPS;
//  setUpRainbowColorBuffer(rainbowColorBuffer, STRIP_LENGTH);
//  extendBufferWithCopy(rainbowColorBuffer, STRIP_LENGTH);
  
  setUpRainbowColorBuffer(signBuffer, SIGN_STRIP_LENGTH * 2);
  extendBufferWithCopy(signBuffer, SIGN_STRIP_LENGTH * 2);
  Serial.println("Begin signal filtering");
  analogReadResolution(12);
}

void setUpRainbowColorBuffer(RGB *bfr, int bulbCount) {
  for (int i = 0; i < bulbCount; i++) {
    float hue = (float)i / (float)bulbCount;
    RGB color = RGB(hue, 1.0);
    bfr[i] = color;
  }
}

void extendBufferWithCopy(RGB *bfr, int count) {
  for (int i = 0; i < count; i++) bfr[i + count] = bfr[i];
  bfr[2 * count] = bfr[0];
}

void setUpStripeColorBuffer() {
  float dither = 0.25;
  for (int i = 0; i < 4; i++) {
    RGB color = stripeColors[i % 2];
    RGB otherColor = stripeColors[(i + 1) % 2];
    int base = i * 75;
    for (int j = 0; j < 75; j++) {
      if (j <= 71) stripeColorBuffer[base + j] = color;
      else stripeColorBuffer[base + j] = color.interpolate(otherColor, dither * (j - 71));
    }
  }
}

void readSensor() {
  firFilter.push(pressureSensor.read(analogRead(A2)));
}

void loop() {
  delay(12348123);
}

void writeStrips() {
//  writeBreathingColor();
//  writeStripeColors();
  writeBreathingStrip();
  writeSignStrip();
}

void writeSignStrip() {
  signBufferPosition -= (SIGN_STRIP_LENGTH * 2) * STRIP_WRITE_INTERVAL / 1000.0 / SIGN_CYCLE_LENGTH; 
  signBufferPosition = fmod(signBufferPosition + SIGN_STRIP_LENGTH * 2, SIGN_STRIP_LENGTH * 2);
  unsigned int basePosition = floor(signBufferPosition);
  float fractionalPosition = fmod(signBufferPosition, 1.0);
  for (int i = 0; i < SIGN_STRIP_LENGTH; i++) {
    unsigned int pos = basePosition + i;
    RGB color = signBuffer[pos].interpolate(signBuffer[pos + 1], fractionalPosition);
    signWriteBuffer[i] = color;
  }
  signStrip.write(signWriteBuffer, SIGN_STRIP_LENGTH);
}

void writeStripeColors() {
  stripePointer -= 2;
  if (stripePointer < 0) stripePointer += 150;
  strip0.write(stripeColorBuffer + stripePointer, 150);
}

void writeBreathingColor() {
  bgColor.breathe(50);
  RGB color = bgColor.color();
  for (int i = 0; i < 150; i++) {
    colorBuffer[i] = color;
  }
  strip0.write(colorBuffer, 150);
}

void writeBreathingStrip() {
  breath.advance(50);
  float hugStrength = firFilter.read() / 4095.0;
  float fullness = breath.fullness() * (0.7 + hugStrength * 0.3);
  breath.setExcitement(hugStrength);
  breathingStrip.setExcitement(hugStrength);
  for (int i = 0; i < 150; i++) {
    RGB color = breathingStrip.value(fullness, i);
//    if (fullness >= 0.7 && i < 139 && i > 135) color.print();
    colorBuffer[i] = color;
  }
  strip1.write(colorBuffer, 150);
}
