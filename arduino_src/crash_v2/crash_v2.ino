#include <Wire.h>
#include <SoftwareSerial.h>
#include <DFRobot_SIM808.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

#define SIM808_BAUDRATE 9600
#define SERIAL_BAUDRATE 9600

#define SIM_RX 2
#define SIM_TX 3

#define GPRS_RETRY 5
#define HTTP_RETRY 5

#define SIM808_INIT_DELAY 1000
#define GPS_NOT_FIX_DELAY 500
#define GPRS_RETRY_DELAY 2000
#define ACCEL_POLL_DELAY 250

#define BUFFER_SIZE 512

#define GPRS_APN F("m-wap")
#define GPRS_USER F("mms")
#define GPRS_PASS F("mms")

#define URL F("codescore.ddns.net")

#define THRESHOLD 15

int ADXL345 = 0x53;  // The ADXL345 sensor I2C address
int oldx, oldy, oldz;

SoftwareSerial simSerial(SIM_RX, SIM_TX);
DFRobot_SIM808 df_sim808(&simSerial);

Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
unsigned long time1;

int year, month, day, hour, minute, second;
float lat, lon;

void error() {
  while (true) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(500);
    digitalWrite(LED_BUILTIN, LOW);
    delay(500);
  }
}

void sim808_init() {
  Serial.println(F("Initializing..."));

  while (!df_sim808.checkPowerUp()) delay(SIM808_INIT_DELAY);

  if (!df_sim808.init()) {
    Serial.println(F("SIM808 can't be initialized"));
    error();
  }

  // sim808_send_cmd("ATE0\r\n");

  digitalWrite(LED_BUILTIN, HIGH);  // Indicate the SIM808 initialized
  delay(500);
  digitalWrite(LED_BUILTIN, LOW);

  Serial.println(F("Initialized..."));
}

void accel_init() {
  if (!accel.begin()) {
    /* There was a problem detecting the ADXL345 ... check your connections */
    Serial.println(F("Ooops, no ADXL345 detected ... Check your wiring!"));
    error();
  }
  accel.setRange(ADXL345_RANGE_16_G);
}

void gps_init() {
  if (sim808_check_with_cmd("AT+CGPSPWR=1\r\n", "OK\r\n", CMD))
    Serial.println("Open the GPS power success");
  else {
    Serial.println("Open the GPS power failure");
    error();
  }
}

void gprs_init() {
  int tries;
  for (tries = 0; tries <= GPRS_RETRY; tries++) {
    // if (tries > 0) {
    //   delay(GPRS_RETRY_DELAY);
    // }
    sim808_send_cmd(F("AT+SAPBR=0,1\r\n"));
    sim808_send_cmd(F("AT+CIPSHUT\r\n"));
    delay(GPRS_RETRY_DELAY);
    if (!sim808_check_with_cmd(F("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\"\r\n"), "OK\r\n", CMD)) {
      Serial.println("GPRS set contype failed");
      continue;
    }

    sim808_send_cmd(F("AT+SAPBR=3,1,\"APN\",\""));
    sim808_send_cmd(GPRS_APN);
    if (!sim808_check_with_cmd(F("\"\r\n"), "OK\r\n", CMD)) {
      Serial.println("GPRS set APN failed");
      continue;
    }

    if (GPRS_USER) {
      sim808_send_cmd(F("AT+SAPBR=3,1,\"USER\",\""));
      sim808_send_cmd(GPRS_USER);
      if (!sim808_check_with_cmd(F("\"\r\n"), "OK\r\n", CMD)) {
        Serial.println("GPRS set USER failed");
        continue;
      }
    }

    if (GPRS_PASS) {
      sim808_send_cmd(F("AT+SAPBR=3,1,\"PWD\",\""));
      sim808_send_cmd(GPRS_PASS);
      if (!sim808_check_with_cmd(F("\"\r\n"), "OK\r\n", CMD)) {
        Serial.println("GPRS set PASS failed");
        continue;
      }
    }
    if (!sim808_check_with_cmd(F("AT+SAPBR=1,1\r\n"), "OK\r\n", CMD)) {
      Serial.println("GPRS init failed");
      continue;
    }
    Serial.println("GPRS initialized");
    break;
  }

  if (tries > GPRS_RETRY) {
    error();
  }
}

bool get_gps() {
  char temp[64];
  sim808_send_cmd("AT+CGPSSTATUS?\r\n");
  sim808_clean_buffer(temp, 64);
  sim808_read_buffer(temp, 64);

  if (strstr(temp, "3D Fix") == NULL) {
    // Serial.println(1);
    return false;
  }

  char loc[128];
  sim808_send_cmd("AT+CGPSINF=0\r\n");
  sim808_clean_buffer(loc, 128);
  sim808_read_buffer(loc, 128);

  // for (int i = 0; i < strlen(loc); i++) {
  //   Serial.print(loc[i], HEX);
  //   Serial.print(" ");
  // }

  // Serial.println(loc);
  char* tok = strtok(loc, ":");  // Skip +CGPSINF
  // Serial.println(tok);
  if (!tok) {
    // Serial.println(2);
    return false;
  }

  tok = strtok(NULL, ",");  // Skip mode
  // Serial.println(tok);
  if (!tok) {
    // Serial.println(3);
    return false;
  }

  char* lat = strtok(NULL, ",");  // Get lat
  // Serial.println(lat);
  if (!lat) {
    // Serial.println(4);
    return false;
  }

  char* lon = strtok(NULL, ",");  // Get long
  if (!lon) {
    // Serial.println(5);
    return false;
  }

  tok = strtok(NULL, ",");  // Skip alt
  if (!tok) {
    // Serial.println(6);
    return false;
  }

  char* time = strtok(NULL, ",");  // Get time
  if (!time) {
    // Serial.println(7);
    return false;
  }

  float latitude = atof(lat);
  float longitude = atof(lon);

  df_sim808.GPSdata.lat = (int)(latitude / 100) + (latitude - (int)(latitude / 100) * 100) / 60;

  // convert longitude from minutes to decimal
  df_sim808.GPSdata.lon = (int)(longitude / 100) + (longitude - (int)(longitude / 100) * 100) / 60;

  // Serial.println(df_sim808.GPSdata.lat, 6);
  // Serial.println(df_sim808.GPSdata.lon, 6);

  // Extract year
  char yearStr[5];
  strncpy(yearStr, time, 4);
  yearStr[4] = '\0';
  df_sim808.GPSdata.year = atoi(yearStr);

  // Extract month
  char monthStr[3];
  strncpy(monthStr, time + 4, 2);
  monthStr[2] = '\0';
  df_sim808.GPSdata.month = atoi(monthStr);

  // Extract day
  char dayStr[3];
  strncpy(dayStr, time + 6, 2);
  dayStr[2] = '\0';
  df_sim808.GPSdata.day = atoi(dayStr);

  // Extract hour
  char hourStr[3];
  strncpy(hourStr, time + 8, 2);
  hourStr[2] = '\0';
  df_sim808.GPSdata.hour = atoi(hourStr);

  // Extract minute
  char minuteStr[3];
  strncpy(minuteStr, time + 10, 2);
  minuteStr[2] = '\0';
  df_sim808.GPSdata.minute = atoi(minuteStr);

  // Extract second
  char secondStr[3];
  strncpy(secondStr, time + 12, 2);
  secondStr[2] = '\0';
  df_sim808.GPSdata.second = atoi(secondStr);

  return true;
}

bool http_post() {
  // Prepare data
  char lat_str[16];
  char lon_str[16];
  dtostrf(df_sim808.GPSdata.lat, 0, 6, lat_str);
  dtostrf(df_sim808.GPSdata.lon, 0, 6, lon_str);

  char body[128];
  sprintf(body, "time=%02d%%2F%02d%%2F%02d%%20%02d%%3A%02d%%3A%02d&lat=%s&long=%s",
          df_sim808.GPSdata.year,
          df_sim808.GPSdata.month,
          df_sim808.GPSdata.day,
          df_sim808.GPSdata.hour,
          df_sim808.GPSdata.minute,
          df_sim808.GPSdata.second,
          lat_str, lon_str);
  Serial.println(body);

  // Init HTTP stack
  if (!sim808_check_with_cmd(F("AT+HTTPINIT\r\n"), "OK\r\n", CMD)) {
    Serial.println(F("Failed to init HTTP"));
    return false;
  }

  // Setup HTTP parameters
  if (!sim808_check_with_cmd(F("AT+HTTPPARA=\"CONTENT\", \"application/x-www-form-urlencoded\"\r\n"), "OK\r\n", CMD)) {
    Serial.println(F("Failed to set content-type"));
    return false;
  }

  sim808_send_cmd(F("AT+HTTPPARA=\"URL\", \""));
  sim808_send_cmd(URL);
  if (!sim808_check_with_cmd(F("/crash\"\r\n"), "OK\r\n", CMD)) {
    Serial.println(F("Failed to set URL"));
    return false;
  }

  char cmd[25];
  sprintf(cmd, "AT+HTTPDATA=%d, 3000\r\n", strlen(body));

  char status[64];
  int tries;

  for (tries = 0; tries <= HTTP_RETRY; tries++) {
    sim808_send_cmd(cmd);
    delay(100);
    sim808_send_cmd(body);
    if (!sim808_check_with_cmd("\r\n", "OK\r\n", CMD)) {
      Serial.println(F("Failed to set data"));
      return false;
    }


    sim808_send_cmd(F("AT+HTTPACTION=1\r\n"));
    delay(5000);
    sim808_clean_buffer(status, 64);
    sim808_read_buffer(status, 64);

    Serial.println(status);
    if (strstr(status, "200") == NULL) {
      Serial.print("Retrying. Try: ");
      Serial.println(tries);
    } else {
      break;
    }
  }

  if (tries > HTTP_RETRY) {
    Serial.println("HTTP retry failed");
    return false;
  }

  sim808_send_cmd(F("AT+HTTPTERM\r\n"));

  return true;
}

void impact_detection() {
  time1 = micros();
  sensors_event_t event;
  accel.getEvent(&event);

  int x = event.acceleration.x, y = event.acceleration.y, z = event.acceleration.z;

  // int deltax = oldx - x;
  // int deltay = oldy - y;
  // int deltaz = oldz - z;

  // oldx = x;
  // oldy = y;
  // oldz = z;

  // double magnitude = sqrt(sq(deltax) + sq(deltay) + sq(deltaz));
  double magnitude = sqrt(sq(x) + sq(y) + sq(z));
  // Serial.println(magnitude);
  if (magnitude > THRESHOLD) {
    digitalWrite(LED_BUILTIN, LOW);
    Serial.println(F("Impact detected"));
    Serial.print(F("Magnitude: "));
    Serial.println(magnitude);

    while (!get_gps()) {
      delay(GPS_NOT_FIX_DELAY);
    }
    http_post();
    sim808_send_cmd(F("AT+HTTPTERM\r\n"));

    accel.getEvent(&event);
    x = event.acceleration.x, y = event.acceleration.y, z = event.acceleration.z;
    oldx = x;
    oldy = y;
    oldz = z;
    magnitude = 0;
    digitalWrite(LED_BUILTIN, HIGH);
  }
  delay(500);
}

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  simSerial.begin(9600);

  digitalWrite(LED_BUILTIN, LOW);  // Indicate the board is initializing

  accel_init();
  sim808_init();
  gps_init();
  // delay(2000);
  gprs_init();

  digitalWrite(LED_BUILTIN, HIGH);  // Indicate the board finished initializing
}

void loop() {
  if (micros() - time1 > 1999)
    impact_detection();
}
