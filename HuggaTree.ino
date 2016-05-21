#include <Arduino.h>
#include <DueTimer.h>
#include <Breath.h>
#include <FIRFilter.h>
#include <CalibratedSensor.h>
#include <SensorArray.h>
#include <RGB.h>
#include <ARMLightStrip.h>
#include <BreathingColor.h>

#define STRIP_LENGTH 116
#define SIGN_STRIP_LENGTH 19
#define SIGN_STRIP_BUFFER_MULTIPLIER 40
#define SIGN_STRIP_HALF_BUFFER_LENGTH (SIGN_STRIP_LENGTH * SIGN_STRIP_BUFFER_MULTIPLIER)
#define STRIP_WRITE_INTERVAL 50
#define SENSOR_POLL_INTERVAL 100
#define SIGN_CYCLE_LENGTH 40
#define NUM_STRIPS 5
#define RAINBOW_STRIP_MULTIPLIER 3
#define MIN_SPARKLE_STRENGTH 0.7

static float stripAngles[NUM_STRIPS];
static ARMLightStrip<51> strip0;
static ARMLightStrip<53> strip1;
static ARMLightStrip<45> strip2;
static ARMLightStrip<43> strip3;
static ARMLightStrip<41> strip4;
static ARMLightStrip<37> signStrip;

static RGB writeBuffers[NUM_STRIPS][STRIP_LENGTH];
float hugStrength;

static RGB signBuffer[SIGN_STRIP_HALF_BUFFER_LENGTH * 2 + 1];
static RGB signWriteBuffer[SIGN_STRIP_HALF_BUFFER_LENGTH];

int signBufferPosition = SIGN_STRIP_HALF_BUFFER_LENGTH;
int bufferPosition = 0;

static ARMLightStripBase * strips[NUM_STRIPS] = {&strip0, &strip1, &strip2, &strip3, &strip4};
static RGB colorBuffer[STRIP_LENGTH];

static RGB bgColors[2] = { {0, 4, 2}, {0, 17, 7}};
static BreathingColor bgColor = BreathingColor(bgColors[0], bgColors[1], 4500, 0.65);
// static BreathingStrip breathingStrip = BreathingStrip(RGB(0, 2, 5), RGB(0, 4, 20), RGB(255, 50, 0), STRIP_LENGTH);

static RGB stripeColors[2] = { {100, 20, 0}, {100, 0, 60} };
static RGB stripeColorBuffer[STRIP_LENGTH * 2];

static RGB rainbowColorBuffer[STRIP_LENGTH * 2 * RAINBOW_STRIP_MULTIPLIER + 1];

int stripePointer = 150;
static int firCoefficients[10] = {1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
static FIRFilter<10, int> firFilter(firCoefficients);

typedef CalibratedSensor<30> CalSensor;
static CalSensor sensors[4] = {
  CalSensor(A0, 0),
  CalSensor(A1, 1),
  CalSensor(A2, 2),
  CalSensor(A3, 3)
};
static SensorArray<CalSensor> sensorArray = SensorArray<CalSensor>(sensors, 4);

static Breath breath;

void setup() {
  Serial.begin(115200);
  Timer.getAvailable().attachInterrupt(makeDisplay).start(STRIP_WRITE_INTERVAL * 1000);
  Timer.getAvailable().attachInterrupt(readSensor).start(SENSOR_POLL_INTERVAL * 1000);

  for (int i = 0; i < NUM_STRIPS; i++) stripAngles[i] = (float)i / (float)NUM_STRIPS;
  setUpRainbowColorBuffer(rainbowColorBuffer, STRIP_LENGTH * RAINBOW_STRIP_MULTIPLIER, 1.0);
  extendBufferWithCopy(rainbowColorBuffer, STRIP_LENGTH * RAINBOW_STRIP_MULTIPLIER);

  setUpRainbowColorBuffer(signBuffer, SIGN_STRIP_LENGTH * SIGN_STRIP_BUFFER_MULTIPLIER, 0.23);
  extendBufferWithCopy(signBuffer, SIGN_STRIP_LENGTH * SIGN_STRIP_BUFFER_MULTIPLIER);
  Serial.println("Begin signal filtering");
  analogReadResolution(12);
}

void setUpRainbowColorBuffer(RGB *bfr, int bulbCount, float strength) {
  for (int i = 0; i < bulbCount; i++) {
    float hue = (float)i / (float)bulbCount;
    RGB color = RGB(hue, strength);
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

const float hugThresholds[2] = { 0.1, 0.3 };
bool hugMetThreshold = false;
int hugCount = 0;
int numHugsRequired = 3;
bool newHug = true;

void readSensor() {
  firFilter.push(sensorArray.readMax(2));
  hugStrength = min(77000, firFilter.read()) / 77000.0;
}

void loop() {
  delay(12348123);
}

typedef void (*stripSetupFunction) (float s);
const int numSetupFunctions = 4;
static stripSetupFunction stripSetupFunctions[numSetupFunctions] = {
  setUpBreathBubble,
  setUpBreathBubble,
  setUpRainbowSpiral,
  setUpRainbowSpiral,
};

void makeDisplay() {
  determineHugCount();
  if (hugStrength < 0.1) setUpBreathingColor(hugStrength * 5, false);
  else {
    stripSetupFunctions[getStripSetupFunctionIndex(hugStrength)](hugStrength);
  }

  writeSignStrip();
  writeStrips();
}

int currentBreathBubbleIndex = 0;
const int numberBreathBubbleColors = 4;
int breathBubbleLength = 10;
static RGB breathBubbleColors[numberBreathBubbleColors] = {
  {255, 20, 0},
  {255, 80, 0},
  {255, 0, 30},
  {255, 120, 0}
};

bool functionIndexSwitched = false;
void determineHugCount() {
  if (functionIndexSwitched) newHug = false;
  if (hugStrength > hugThresholds[1]) hugMetThreshold = true;
  else if (hugMetThreshold && hugStrength < hugThresholds[0]) {
    hugCount++;
    hugMetThreshold = false;
    newHug = true;
    functionIndexSwitched = false;
  }
  if (hugCount >= numHugsRequired) {
    numHugsRequired = random(5);
    hugCount = 0;
  }
  if (hugStrength < hugThresholds[0]) {
    currentBreathBubbleIndex = random(numberBreathBubbleColors);
    breathBubbleLength = random(30) + 5;
  }
}

int setUpFunctionPointer = 0;
int getStripSetupFunctionIndex(float strength) {
  if (newHug && hugCount == 0) {
     setUpFunctionPointer = (setUpFunctionPointer + 1) % numSetupFunctions;
     functionIndexSwitched = true;
  }
  return setUpFunctionPointer;
}

void writeStrips() {
  for (int i = 0; i < NUM_STRIPS; i++) {
    strips[i]->write(writeBuffers[i], STRIP_LENGTH);
  }
}

void setUpBreathBubble(float strength) {
  RGB currentBreathBubbleColor = breathBubbleColors[currentBreathBubbleIndex] * strength;
  setUpBreathingColor(strength, true); // Write one strip only
  int start = min(max(0, strength * 1.2 * STRIP_LENGTH - breathBubbleLength), STRIP_LENGTH - breathBubbleLength);
  int finish = start + breathBubbleLength - 1;
  writeBuffers[0][start] = writeBuffers[0][finish] = writeBuffers[0][start].interpolate(currentBreathBubbleColor, 0.2);
  writeBuffers[0][start + 1] = writeBuffers[0][finish - 1] = writeBuffers[0][start + 1].interpolate(currentBreathBubbleColor, 0.5);
  for (int i = start + 2 ; i < start + breathBubbleLength - 2; i++) writeBuffers[0][i] = currentBreathBubbleColor;
  for (int i = 1; i < NUM_STRIPS; i++) {
    for (int j = 0; j < STRIP_LENGTH; j++) {
      writeBuffers[i][j] = writeBuffers[0][j];
    }
  }
  if (strength > 0.7) addSparkles(2, 0);
}

const int spiralSpeedFactor = 12;
void setUpRainbowSpiral(float strength) {
  int spiralOffsetPerSpoke = 20 + strength * 10;
  RGB color;
  float power = pow(strength, 2);
  bufferPosition = (bufferPosition - (int)(spiralSpeedFactor * strength) + STRIP_LENGTH * RAINBOW_STRIP_MULTIPLIER) % (STRIP_LENGTH * RAINBOW_STRIP_MULTIPLIER);
  unsigned int positions[NUM_STRIPS];
  for (int i = 0; i < NUM_STRIPS; i++) positions[i] = (bufferPosition + STRIP_LENGTH * RAINBOW_STRIP_MULTIPLIER - i * spiralOffsetPerSpoke) % (STRIP_LENGTH * RAINBOW_STRIP_MULTIPLIER);
  for (int i = 0; i < NUM_STRIPS; i++) {
    for (int j = 0; j < STRIP_LENGTH; j++) writeBuffers[i][j] = rainbowColorBuffer[positions[i]++] * power;
  }

  if (strength > MIN_SPARKLE_STRENGTH) addSparkles(3, strength);
}

void addSparkles(int sparkleLength, float strength) {
  int sparkleBrightness = 64 + (strength - MIN_SPARKLE_STRENGTH) * 191 * (1 - MIN_SPARKLE_STRENGTH);
  for (int i = 0; i < NUM_STRIPS; i++) {
    int sparkleBase = max(random(STRIP_LENGTH - sparkleLength), 0);
    for (int j = 0; j < sparkleLength; j++) {
      writeBuffers[i][sparkleBase++] = {sparkleBrightness, sparkleBrightness, sparkleBrightness};
    }
  }
}

void writeSignStrip() {
  signBufferPosition = (SIGN_STRIP_HALF_BUFFER_LENGTH + signBufferPosition - 1) % SIGN_STRIP_HALF_BUFFER_LENGTH;
  for (int i = 0; i < SIGN_STRIP_LENGTH; i++) {
    RGB color = signBuffer[signBufferPosition + i * SIGN_STRIP_BUFFER_MULTIPLIER / 2];
    signWriteBuffer[i] = color;
  }
  signStrip.write(signWriteBuffer, SIGN_STRIP_LENGTH);
}

void writeStripeColors() {
  stripePointer -= 2;
  if (stripePointer < 0) stripePointer += 150;
  strip0.write(stripeColorBuffer + stripePointer, 150);
}

void setUpBreathingColor(float hugStrength, bool oneStripOnly) {
  bgColor.breathe(50 + hugStrength * 150);
  RGB color = bgColor.color();
  for (int j = 0; j < STRIP_LENGTH; j++) {
    int numStrips = oneStripOnly ? 1 : NUM_STRIPS;
    for (int i = 0; i < numStrips; i++) writeBuffers[i][j] = color;
  }
}

// void writeBreathingStrip() {
//   breath.advance(50);
//   float fullness = breath.fullness() * (0.7 + hugStrength * 0.3);
//   breath.setExcitement(hugStrength);
//   breathingStrip.setExcitement(hugStrength);
//   for (int i = 0; i < 150; i++) {
//     RGB color = breathingStrip.value(fullness, i);
//     colorBuffer[i] = color;
//   }
//   for (int i = 0; i < NUM_STRIPS; i++) strips[i]->write(colorBuffer, 150);
// }
