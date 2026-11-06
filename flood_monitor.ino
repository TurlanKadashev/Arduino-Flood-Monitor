#include <Arduino.h>
#include <NewPing.h>
#include <SoftwareSerial.h>

// pins
#define TRIG_PIN    2
#define ECHO_PIN    4
#define LED_PIN     5
#define RAIN_PIN    3     // tipping bucket, needs to be an interrupt pin

#define BT_RX 10
#define BT_TX 11

// bucket / sensor
#define BUCKET_DEPTH_CM        29.0f
#define SENSOR_HEIGHT_FROM_RIM 5.0f    // not used for now

// alert thresholds
#define FLOOD_RATE_THRESHOLD   0.005f  // cm/s
#define HEAVY_RAIN_THRESHOLD   15.0f   // mm/h
#define SUSTAINED_RISE_COUNT   10

#define SAMPLE_INTERVAL_MS     3000
#define RATE_SAMPLES           10

#define MM_PER_TIP             0.279f
#define RAIN_INTERVAL_MS       60000
#define RAIN_BUCKETS           60

NewPing sonar(TRIG_PIN, ECHO_PIN, 300);
SoftwareSerial BT(BT_RX, BT_TX);

float distHist[RATE_SAMPLES];
unsigned long timeHist[RATE_SAMPLES];
int histIdx = 0;
bool bufFull = false;

volatile int rainTips = 0;
float rainHist[RAIN_BUCKETS];    // mm per minute
int rainIdx = 0;
unsigned long lastRainUpdate = 0;

int high_count = 0;
bool floodAlert = false;


// average of 5 pings
float getFilteredDistance() {
  float sum = 0;
  int n = 5;

  for (int i = 0; i < n; i++) {
    delay(50);
    float d = sonar.ping_cm();
    if (d > 0) sum += d;    // 0 means no echo
  }

  // TODO: median would be better, one bad ping drags the average down
  return sum / n;
}

float calcRisingRate() {
  if (!bufFull) return 0;

  int oldest = histIdx;    // slot we overwrite next = oldest sample
  int newest = (histIdx - 1 + RATE_SAMPLES) % RATE_SAMPLES;

  float dOld = distHist[oldest];
  float dNew = distHist[newest];

  float dt = (timeHist[newest] - timeHist[oldest]) / 1000.0f;
  if (dt <= 0) return 0;

  return (dOld - dNew) / dt;   // cm/s, positive = water going up
}

void rainISR() {
  rainTips++;
}

void updateRainfall() {
  if (millis() - lastRainUpdate >= RAIN_INTERVAL_MS) {
    lastRainUpdate = millis();

    float mm = rainTips * MM_PER_TIP;
    rainTips = 0;

    rainHist[rainIdx] = mm;
    rainIdx = (rainIdx + 1) % RAIN_BUCKETS;
  }
}

// sum of the last 60 one-minute buckets
float getHourlyRainfall() {
  float sum = 0;
  for (int i = 0; i < RAIN_BUCKETS; i++)
    sum += rainHist[i];
  return sum;
}

float predictHoursToOverflow(float currentDist, float rate) {
  if (rate <= 0) return -1;

  float waterLevel = BUCKET_DEPTH_CM - currentDist;
  float remaining = BUCKET_DEPTH_CM - waterLevel;   // ends up == currentDist, left as is

  return (remaining / rate) / 3600.0f;
}

void sendData(float dist, float rate, float rain1h, float timeToFlood, bool alert) {
  String out = "Distance ≈ " + String(dist, 2) + " cm, " +
               "Rise rate ≈ " + String(rate, 4) + " cm/s, " +
               "Rainfall ≈ " + String(rain1h, 2) + " mm/h, ";

  if (timeToFlood > 0) {
    out += "Possible overflow in ≈ " + String(timeToFlood, 2) + " hours, ";
  } else {
    out += "Overflow time ≈ unknown, ";
  }

  out += "Flood risk: ";
  out += alert ? "high chance" : "low chance";

  //Serial.print("raw rate: "); Serial.println(rate, 6);
  Serial.println(out);
  BT.println(out);
}


void setup() {
  Serial.begin(9600);
  BT.begin(9600);

  pinMode(LED_PIN, OUTPUT);
  pinMode(RAIN_PIN, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(RAIN_PIN), rainISR, FALLING);

  for (int i = 0; i < RATE_SAMPLES; i++) {
    distHist[i] = -1;
    timeHist[i] = 0;
  }

  for (int i = 0; i < RAIN_BUCKETS; i++) rainHist[i] = 0;
}

void loop() {
  static unsigned long lastSample = 0;

  updateRainfall();

  if (millis() - lastSample >= SAMPLE_INTERVAL_MS) {
    lastSample = millis();

    float currentDist = getFilteredDistance();
    if (currentDist <= 0) return;    // nothing read, skip this round

    distHist[histIdx] = currentDist;
    timeHist[histIdx] = millis();

    histIdx = (histIdx + 1) % RATE_SAMPLES;
    if (histIdx == 0) bufFull = true;

    float rate = calcRisingRate();
    float rain1h = getHourlyRainfall();
    float timeToFlood = predictHoursToOverflow(currentDist, rate);

    // alert only if it keeps rising AND it's raining hard, for N samples in a row
    if (bufFull && rate > FLOOD_RATE_THRESHOLD && rain1h >= HEAVY_RAIN_THRESHOLD) {
      high_count++;
    } else {
      high_count = 0;
    }

    floodAlert = (high_count >= SUSTAINED_RISE_COUNT);

    digitalWrite(LED_PIN, floodAlert ? HIGH : LOW);

    sendData(currentDist, rate, rain1h, timeToFlood, floodAlert);
  }
}
