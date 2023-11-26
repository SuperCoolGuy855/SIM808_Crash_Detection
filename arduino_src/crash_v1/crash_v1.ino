#include <Wire.h>  // Wire library - used for I2C communication
#include <SoftwareSerial.h>
#include <SIM808.h>
#include <DFRobot_SIM808.h>

#if defined(__AVR__)
#include <SoftwareSerial.h>
#define SIM_SERIAL_TYPE SoftwareSerial              ///< Type of variable that holds the Serial communication with SIM808
#define SIM_SERIAL SIM_SERIAL_TYPE(SIM_TX, SIM_RX)  ///< Definition of the instance that holds the Serial communication with SIM808

#define STRLCPY_P(s1, s2) strlcpy_P(s1, s2, BUFFER_SIZE)
#else
#include <HardwareSerial.h>
#define SIM_SERIAL_TYPE HardwareSerial  ///< Type of variable that holds the Serial communication with SIM808
#define SIM_SERIAL SIM_SERIAL_TYPE(2)   ///< Definition of the instance that holds the Serial communication with SIM808

#define STRLCPY_P(s1, s2) strlcpy(s1, s2, BUFFER_SIZE)
#endif

#define SIM_RST 9  ///< SIM808 RESET
#define SIM_RX 3   ///< SIM808 RXD
#define SIM_TX 2   ///< SIM808 TXD


#define SIM808_BAUDRATE 9600   ///< Control the baudrate use to communicate with the SIM808 module
#define SERIAL_BAUDRATE 9600   ///< Controls the serial baudrate between the arduino and the computer
#define NETWORK_DELAY 5000     ///< Delay between each GPS read
#define NO_FIX_GPS_DELAY 1000  ///< Delay between each GPS read when no fix is acquired
#define FIX_GPS_DELAY 10000

#define GPRS_APN "m-wap"  ///< Your provider Access Point Name
#define GPRS_USER "mms"   ///< Your provider APN user (usually not needed)
#define GPRS_PASS "mms"   ///< Your provider APN password (usually not needed)

#define BUFFER_SIZE 512  ///< Side of the response buffer
#define POSITION_SIZE 128
#define NL "\n"

#if defined(__AVR__)
typedef __FlashStringHelper* __StrPtr;
#else
typedef const char* __StrPtr;
#endif

//SoftwareSerial simSerial(SIM_TX, SIM_RX);
SIM_SERIAL_TYPE simSerial = SIM_SERIAL;
SIM808 sim808 = SIM808(SIM_RST);
DFRobot_SIM808 df_sim808(&simSerial);
bool done = false;
char buffer[BUFFER_SIZE];
//char position[POSITION_SIZE];
//char parsed[BUFFER_SIZE];

int ADXL345 = 0x53;  // The ADXL345 sensor I2C address

//float X_out, Y_out, Z_out;  // Outputs

float xaxis = 0, yaxis = 0, zaxis = 0;
float deltx = 0, delty = 0, deltz = 0;

float magnitude = 0;
int sensitivity = 59;
int counter = 2;
int dcounter = 75;

double angle;
boolean impact_detected = false;

unsigned long alert_delay = 3000;

byte update_flag;

unsigned long time1;
unsigned long impact_time;

float lat, lon;
uint16_t year;
uint8_t month, day, hour, minute, second;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);
  Serial.begin(9600);
  simSerial.begin(9600);

  // Log.begin(LOG_LEVEL_NOTICE, &Serial);
  // Log.notice(S_F("Initializing..." NL));
  // sim808.begin(simSerial);
  // sim808.init();
  digitalWrite(LED_BUILTIN, HIGH);
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);
  // sim808.setEcho(SIM808Echo::Off);
  Log.notice(S_F("Initialized..." NL));

  impact_setup();

  // gprs_setup();

  gps_setup();
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop() {
  impact_detection();
}

void gps_setup() {

  //************* Turn on the GPS power************
  if (df_sim808.attachGPS())
    Serial.println("Open the GPS power success");
  else
    Serial.println("Open the GPS power failure");
}

void gprs_setup() {
  bool enabled = false;
  while (true) {
    simSerial.write("AT+SAPBR=0,1\r\n");
    SIM808NetworkRegistrationState status = sim808.getNetworkRegistrationStatus();
    SIM808SignalQualityReport report = sim808.getSignalQuality();

    bool isAvailable = static_cast<int8_t>(status) & (static_cast<int8_t>(SIM808NetworkRegistrationState::Registered) | static_cast<int8_t>(SIM808NetworkRegistrationState::Roaming)) != 0;

    if (!isAvailable) {
      Log.notice(S_F("No network yet..." NL));
      delay(NETWORK_DELAY);
      continue;
    }

    Log.notice(S_F("Network is ready." NL));
    Log.notice(S_F("Attenuation : %d dBm, Estimated quality : %d" NL), report.attenuation, report.rssi);

    Log.notice(S_F("Powering on SIM808's GPRS..." NL));
    enabled = sim808.enableGprs(GPRS_APN, GPRS_USER, GPRS_PASS);
    if (!enabled) {
      delay(NETWORK_DELAY);
      continue;
    }
    Log.notice(S_F("GPRS is ready." NL));
    break;
  }
}

void impact_setup() {
  //Serial.begin(9600); // Initiate serial communication for printing the results on the Serial monitor
  Wire.begin();  // Initiate the Wire library
  // Set ADXL345 in measuring mode
  Wire.beginTransmission(ADXL345);  // Start communicating with the device
  Wire.write(0x2D);                 // Access/ talk to POWER_CTL Register - 0x2D
  // Enable measurement
  Wire.write(8);  // (8dec -> 0000 1000 binary) Bit D3 High for measuring enable
  Wire.endTransmission();
  delay(10);

  time1 = micros();
  Wire.beginTransmission(ADXL345);
  Wire.write(0x32);  // Start with register 0x32 (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(ADXL345, 6, true);        // Read 6 registers total, each axis value is stored in 2 registers
  xaxis = (Wire.read() | Wire.read() << 8);  // X-axis value
  //xaxis = xaxis/256; //For a range of +-2g, we need to divide the raw values by 256, according to the datasheet
  yaxis = (Wire.read() | Wire.read() << 8);  // Y-axis value
  //yaxis = yaxis/256;
  zaxis = (Wire.read() | Wire.read() << 8);  // Z-axis value
  //zaxis = zaxis/256;
}

void impact_detection() {
  if (micros() - time1 > 1999)
    impact();
  if (update_flag > 0) {
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println("Impact detected!");
    Serial.println("Magnitude:");
    Serial.println(magnitude);
    while (!get_gps());
    http_post();
    update_flag = 0;
    impact_detected = true;
    impact_time = millis();
    digitalWrite(LED_BUILTIN, HIGH);
  }

  if (impact_detected) {
    if (millis() - impact_time >= alert_delay) {
      //delay(1000);
      impact_detected = false;
      impact_time = 0;
    }
  }
}

void impact() {
  time1 = micros();

  int oldx = xaxis;
  int oldy = yaxis;
  int oldz = zaxis;

  Wire.beginTransmission(ADXL345);
  Wire.write(0x32);  // Start with register 0x32 (ACCEL_XOUT_H)
  Wire.endTransmission(false);
  Wire.requestFrom(ADXL345, 6, true);        // Read 6 registers total, each axis value is stored in 2 registers
  xaxis = (Wire.read() | Wire.read() << 8);  // X-axis value
  //xaxis = xaxis/256; //For a range of +-2g, we need to divide the raw values by 256, according to the datasheet
  yaxis = (Wire.read() | Wire.read() << 8);  // Y-axis value
  //yaxis = yaxis/256;
  zaxis = (Wire.read() | Wire.read() << 8);  // Z-axis value
  //zaxis = zaxis/256;

  deltx = xaxis - oldx;
  delty = yaxis - oldy;
  deltz = zaxis - oldz;

  counter--;

  if (counter < 0) counter = 0;

  if (counter > 0) return;

  char abc[128];
  sprintf(abc, "x, y, z: %d, %d, %d; ox, oy, oz: %d, %d, %d", xaxis, yaxis, zaxis, oldx, oldy, oldz);
  Serial.println(abc);

  magnitude = sqrt(sq(deltx) + sq(delty) + sq(deltz));
  //Serial.println("Magnitude:");
  //Serial.println(magnitude);

  // if (magnitude >= sensitivity) {
  //   update_flag = 1;
  //   counter = dcounter;
  // } else {
  //   magnitude = 0;
  // }
  delay(500);
}


bool get_gps() {
  //gps_setup();
  bool gps = df_sim808.getGPS();
  if (gps) {
    year = df_sim808.GPSdata.year;
    Serial.print(year);
    Serial.print("/");
    month = df_sim808.GPSdata.month;
    Serial.print(df_sim808.GPSdata.month);
    Serial.print("/");
    day = df_sim808.GPSdata.day;
    Serial.print(df_sim808.GPSdata.day);
    Serial.print(" ");
    hour = df_sim808.GPSdata.hour;
    Serial.print(df_sim808.GPSdata.hour);
    Serial.print(":");
    minute = df_sim808.GPSdata.minute;
    Serial.print(df_sim808.GPSdata.minute);
    Serial.print(":");
    second = df_sim808.GPSdata.second;
    Serial.print(df_sim808.GPSdata.second);
    Serial.print(":");
    Serial.println(df_sim808.GPSdata.centisecond);

    Serial.print("latitude :");
    Serial.println(df_sim808.GPSdata.lat, 6);

    lat = df_sim808.GPSdata.lat;
    lon = df_sim808.GPSdata.lon;

    Serial.print("longitude :");
    Serial.println(df_sim808.GPSdata.lon, 6);

    //************* Turn off the GPS power ************
    //df_sim808.detachGPS();
  } else {
    delay(NO_FIX_GPS_DELAY);
  }
  return gps;
}

void http_post() {
  while (true) {
    char lat_str[15];
    char lon_str[15];

    dtostrf(lat, 0, 6, lat_str);
    dtostrf(lon, 0, 6, lon_str);

    Log.notice(S_F("Sending HTTP request..." NL));
    sprintf(buffer, "time=%02d%%2F%02d%%2F%02d%%20%02d%%3A%02d%%3A%02d&lat=%s&long=%s", year, month, day, hour, minute, second, lat_str, lon_str);

    //STRLCPY_P(buffer, PSTR("time=2023%2F11%2F14%2009%3A01%3A41&lat=10.123213&long=106.123123"));
    //notice that we're using the same buffer for both body and response
    uint16_t responseCode = sim808.httpPost("http://codescore.ddns.net/crash", S_F("application/x-www-form-urlencoded"), buffer, buffer, BUFFER_SIZE);

    Log.notice(S_F("Server responsed : %d" NL), responseCode);
    Log.notice(buffer, NL);

    if (responseCode == 0) gprs_setup();
    if (responseCode == 200) break;
  }
}
