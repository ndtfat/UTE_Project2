#include <Wire.h>
#include <WiFi.h>
#include <ThreeWire.h>
#include <RtcDS1302.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <FirebaseESP32.h>
#include <LiquidCrystal_I2C.h>
#include <LiquidCrystal_I2C.h>

//-----------------------------------------------------------------------------------------
#define FIREBASE_HOST "https://project-2-98f64-default-rtdb.firebaseio.com/"
#define FIREBASE_AUTH "YHyZwQumvU73BpeTyhXTmD352U2GhQk1dPAKQO6Q"
#define WIFI_SSID "fat"
#define WIFI_PW "nguyenduongtienfat"

//-----------------------------------------------------------------------------------------
ThreeWire myWire(4, 5, 2);  // IO, SCLK, CE
RtcDS1302<ThreeWire> Rtc(myWire);

FirebaseData fbData;
LiquidCrystal_I2C lcd(0x27, 20, 4);
#define btn_1_hand 25
#define btn_2_hand 33
#define btn_select_bulb 26

#define btn_select_mode 13
#define btn_cursor 12
#define btn_plus 14
#define btn_minus 27

#define relay_1 18
#define relay_2 19

#define countof(a) (sizeof(a) / sizeof(a[0]))

//define global variables------------------------------------------------------------------
bool is_bulb_1 = true;
int cursor = 0;
String mode_1, mode_2;
String state_1, state_2;
String timeOn_1, dateOn_1;
String timeOn_2, dateOn_2;
String timeOff_1, dateOff_1;
String timeOff_2, dateOff_2;
//-----------------------------------------------------------------------------------------
void initFirebase() {
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.reconnectWiFi(true);
  if (!Firebase.beginStream(fbData, "/devices/1/mode")) {
    Serial.println("REASON: " + fbData.errorReason());
    Serial.println();
  }
  Serial.println("Connect firebase success");
  Serial.println(fbData.stringData());
}

//-----------------------------------------------------------------------------------------
void initRTC() {
  Rtc.Begin();
  RtcDateTime compiled = RtcDateTime(__DATE__, __TIME__);
  if (!Rtc.IsDateTimeValid()) {
    // Common Causes:
    //    1) first time you ran and the device wasn't running yet
    //    2) the battery on the device is low or even missing

    Serial.println("RTC lost confidence in the DateTime!");
    Rtc.SetDateTime(compiled);
  }

  if (Rtc.GetIsWriteProtected()) {
    Serial.println("RTC was write protected, enabling writing now");
    Rtc.SetIsWriteProtected(false);
  }

  if (!Rtc.GetIsRunning()) {
    Serial.println("RTC was not actively running, starting now");
    Rtc.SetIsRunning(true);
  }

  RtcDateTime now = Rtc.GetDateTime();
  if (now < compiled) {
    Serial.println("RTC is older than compile time!  (Updating DateTime)");
    Rtc.SetDateTime(compiled);
  } else if (now > compiled) {
    Serial.println("RTC is newer than compile time. (this is expected)");
  } else if (now == compiled) {
    Serial.println("RTC is the same as compile time! (not expected but all is fine)");
  }
}

//-----------------------------------------------------------------------------------------
void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  delay(200);
  //-------------------------------------
  pinMode(btn_1_hand, INPUT_PULLUP);
  pinMode(btn_2_hand, INPUT_PULLUP);
  pinMode(btn_select_bulb, INPUT_PULLUP);
  pinMode(btn_select_mode, INPUT_PULLUP);
  pinMode(btn_cursor, INPUT_PULLUP);
  pinMode(btn_plus, INPUT_PULLUP);
  pinMode(btn_minus, INPUT_PULLUP);

  pinMode(relay_1, OUTPUT);
  pinMode(relay_2, OUTPUT);
  digitalWrite(relay_1, HIGH);
  digitalWrite(relay_2, HIGH);
  //-------------------------------------
  Serial.println();
  Serial.print("Connecting to wifi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PW);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  //-------------------------------------
  initRTC();
  initFirebase();
  lcd.begin(20, 4);
  lcd.init();
  lcd.backlight();
  Serial.println();
}

//-----------------------------------------------------------------------------------------
String formatTime(String time, String format) {
  if (format == "24") {
    int h = time.substring(0, time.indexOf(':')).toInt();
    if (time.indexOf("PM") > 0 && h != 12) {
      h += 12;
    } else if (time.indexOf("AM") > 0 && h == 12) {
      h = 0;
    }
    String formattedHour = (h < 10) ? "0" + String(h) : String(h);
    return formattedHour + time.substring(time.indexOf(':'), time.indexOf(' '));
  } else {
    int h = time.substring(0, 2).toInt();
    String period = (h >= 12) ? "PM" : "AM";
    if (h > 12) {
      h -= 12;
    } else if (h == 0) {
      h = 12;
    }
    String formattedHour = (h < 10) ? "0" + String(h) : String(h);
    return formattedHour + time.substring(2) + " " + period;
  }
}

//-----------------------------------------------------------------------------------------
void getFBData() {
  if (Firebase.getString(fbData, "/devices")) {
    String jsonString = fbData.stringData();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, jsonString);

    JsonObject bulb_1_obj = doc[0].as<JsonObject>();
    mode_1 = bulb_1_obj["mode"].as<String>();
    state_1 = bulb_1_obj["isOn"].as<String>();
    dateOn_1 = bulb_1_obj["onAt"]["date"].as<String>();
    timeOn_1 = formatTime(bulb_1_obj["onAt"]["time"].as<String>(), "24");
    dateOff_1 = bulb_1_obj["offAt"]["date"].as<String>();
    timeOff_1 = formatTime(bulb_1_obj["offAt"]["time"].as<String>(), "24");
    //------------------------------------------------------
    JsonObject bulb_2_obj = doc[1].as<JsonObject>();
    mode_2 = bulb_2_obj["mode"].as<String>();
    state_2 = bulb_2_obj["isOn"].as<String>();
    dateOn_2 = bulb_2_obj["onAt"]["date"].as<String>();
    timeOn_2 = formatTime(bulb_2_obj["onAt"]["time"].as<String>(), "24");
    dateOff_2 = bulb_2_obj["offAt"]["date"].as<String>();
    timeOff_2 = formatTime(bulb_2_obj["offAt"]["time"].as<String>(), "24");

    // Serial.println("Get data Success");
  } else
    Serial.println("Get data Fail");
}

//-----------------------------------------------------------------------------------------
void setFB(String path, String value) {
  if (WiFi.status() == WL_CONNECTED) {
    if (Firebase.setString(fbData, path, value)) {
      if (Firebase.setString(fbData, path, value))
        Serial.println("Update FB success");
      else {
        Serial.println("Set data Fail");
      }
    }
  }
}
//-----------------------------------------------------------------------------------------
void setObjectFB(String path, FirebaseJson json) {
  if (Firebase.setJSON(fbData, path, json))
    Serial.println("Update FB success");
  else {
    Serial.println("Set data Fail");
  }
}

//-----------------------------------------------------------------------------------------
void displayLCD(int bulb, String state, String mode, String time_on, String date_on, String time_off, String date_off, int cursor, const RtcDateTime& dt) {
  int pos = 4;
  String blink = "  ";
  bool isAppear = true;
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("B" + String(bulb));
  lcd.print(" - " + mode);
  lcd.print(state == "true" ? " - ON" : " - OFF");

  if (cursor == 0 || cursor == 5) {
    blink = "  ";
    pos = 4;
  } else if (cursor == 1 || cursor == 6)
    pos = 7;
  else if (cursor == 2 || cursor == 7)
    pos = 10;
  else if (cursor == 3 || cursor == 8)
    pos = 13;
  else if (cursor == 4 || cursor == 9) {
    blink = "    ";
    pos = 16;
  }
  
  lcd.setCursor(0, 2);
  lcd.print("ON  " + time_on + " " + date_on);
  lcd.setCursor(0, 3);
  lcd.print("OFF " + time_off + " " + date_off);
  lcd.setCursor(2, 1);
  lcd.print(date + " " + time);
  if (isAppear) {
    lcd.setCursor(pos, cursor > 4 ? 3 : 2);
    lcd.print(blink);
  }
  isAppear = !isAppear;
  delay(500);
}

//-----------------------------------------------------------------------------------------
void logSerial() {
  Serial.println("1. " + state_1 + ", mode-" + mode_1 + ", ON-" + timeOn_1 + " (" + dateOn_1 + "), OFF-" + timeOff_1 + " (" + dateOff_1 + ")");
  Serial.println("2. " + state_2 + ", mode-" + mode_2 + ", ON-" + timeOn_2 + " (" + dateOn_2 + "), OFF-" + timeOff_2 + " (" + dateOff_2 + ")");
  Serial.println("-------------------------------------------------------------------");
}

//-----------------------------------------------------------------------------------------
void check_state() {
  digitalWrite(relay_1, state_1 == "true");
  //--------------------------------------
  digitalWrite(relay_2, state_2 == "true");
}

//-----------------------------------------------------------------------------------------
void change_mode(int light) {
  if (light == 1) {
    mode_1 = (mode_1 == "Manual") ? "Timer" : "Manual";
    setFB("/devices/0/mode", mode_1);
  }
  if (light == 2) {
    mode_2 = (mode_2 == "Manual") ? "Timer" : "Manual";
    setFB("/devices/1/mode", mode_2);
  }
}

//-----------------------------------------------------------------------------------------
void adjust_time(int bulb, int cursor, bool isPlus) {
  int h_on, min_on, h_off, min_off;
  int d_on, m_on, y_on, d_off, m_off, y_off;

  //convert string to int-------------------
  h_on = (bulb == 1 ? timeOn_1.substring(0, 2) : timeOn_2.substring(0, 2)).toInt();
  min_on = (bulb == 1 ? timeOn_1.substring(3).toInt() : timeOn_2.substring(3).toInt());

  h_off = (bulb == 1 ? timeOff_1.substring(0, 2).toInt() : timeOff_2.substring(0, 2).toInt());
  min_off = (bulb == 1 ? timeOff_1.substring(3).toInt() : timeOff_2.substring(3).toInt());

  d_on = (bulb == 1 ? dateOn_1.substring(0, 2).toInt() : dateOn_2.substring(0, 2).toInt());
  m_on = (bulb == 1 ? dateOn_1.substring(3, 5).toInt() : dateOn_2.substring(3, 5).toInt());
  y_on = (bulb == 1 ? dateOn_1.substring(6).toInt() : dateOn_2.substring(6).toInt());

  d_off = (bulb == 1 ? dateOff_1.substring(0, 2).toInt() : dateOff_2.substring(0, 2).toInt());
  m_off = (bulb == 1 ? dateOff_1.substring(3, 5).toInt() : dateOff_2.substring(3, 5).toInt());
  y_off = (bulb == 1 ? dateOff_1.substring(6).toInt() : dateOff_2.substring(6).toInt());

  // check cursor to know what to adjust----
  Serial.println(String(d_on) + " " + String(m_on) + " " + String(y_on));

  switch (cursor) {
    case 0:
      h_on = isPlus ? ((h_on == 23) ? 0 : (h_on + 1)) : ((h_on == 0) ? 23 : (h_on - 1));
      break;
    case 1:
      min_on = isPlus ? ((min_on == 59) ? 0 : (min_on + 1)) : ((min_on == 0) ? 59 : (min_on - 1));
      break;
    case 2:
      d_on = isPlus ? ((d_on == 31) ? 1 : (d_on + 1)) : ((d_on == 1) ? 31 : (d_on - 1));
      Serial.println(d_on);
      break;
    case 3:
      m_on = isPlus ? ((m_on == 12) ? 1 : (m_on + 1)) : ((m_on == 1) ? 12 : (m_on - 1));
      Serial.println(m_on);
      break;
    case 4:
      y_on = isPlus ? (y_on + 1) : (y_on - 1);
      Serial.println(y_on);
      break;
    case 5:
      h_off = isPlus ? ((h_off == 23) ? 0 : (h_off + 1)) : ((h_off == 0) ? 23 : (h_off - 1));
      break;
    case 6:
      min_off = isPlus ? ((min_off == 59) ? 0 : (min_off + 1)) : ((min_off == 0) ? 59 : (min_off - 1));
      break;
    case 7:
      d_off = isPlus ? ((d_off == 31) ? 1 : (d_off + 1)) : ((d_off == 1) ? 31 : (d_off - 1));
      break;
    case 8:
      m_off = isPlus ? ((m_off == 12) ? 1 : (m_off + 1)) : ((m_off == 1) ? 12 : (m_off - 1));
      break;
    case 9:
      y_off = isPlus ? (y_off + 1) : (y_off - 1);
      break;
    default:
      Serial.println("Invalid cursor: " + String(cursor));
      break;
  }
  Serial.println(String(d_on) + " " + String(m_on) + " " + String(y_on));
  // convert back to string
  if (bulb == 1) {
    timeOn_1 = String(h_on < 10 ? "0" + String(h_on) : String(h_on)) + ":" + String(min_on < 10 ? "0" + String(min_on) : String(min_on));
    dateOn_1 = String(d_on < 10 ? "0" + String(d_on) : String(d_on)) + "/" + String(m_on < 10 ? "0" + String(m_on) : String(m_on)) + "/" + String(y_on);
    timeOff_1 = String(h_off < 10 ? "0" + String(h_off) : String(h_off)) + ":" + String(min_off < 10 ? "0" + String(min_off) : String(min_off));
    dateOff_1 = String(d_off < 10 ? "0" + String(d_off) : String(d_off)) + "/" + String(m_off < 10 ? "0" + String(m_off) : String(m_off)) + "/" + String(y_off);
  } else {
    timeOn_2 = String(h_on < 10 ? "0" + String(h_on) : String(h_on)) + ":" + String(min_on < 10 ? "0" + String(min_on) : String(min_on));
    dateOn_2 = String(d_on < 10 ? "0" + String(d_on) : String(d_on)) + "/" + String(m_on < 10 ? "0" + String(m_on) : String(m_on)) + "/" + String(y_on);
    timeOff_2 = String(h_off < 10 ? "0" + String(h_off) : String(h_off)) + ":" + String(min_off < 10 ? "0" + String(min_off) : String(min_off));
    dateOff_2 = String(d_off < 10 ? "0" + String(d_off) : String(d_off)) + "/" + String(m_off < 10 ? "0" + String(m_off) : String(m_off)) + "/" + String(y_off);
  }

  // set data on Firebase

  Serial.print(timeOn_1 + " | ");
  String timeOn__format12 = formatTime((bulb == 1) ? timeOn_1 : timeOn_2, "12");
  String timeOff__format12 = formatTime((bulb == 1) ? timeOff_1 : timeOff_2, "12");
  Serial.println(timeOn__format12);

  FirebaseJson json;
  json.set("isOn", (bulb == 1) ? state_1 : state_2);
  json.set("mode", (bulb == 1) ? mode_1 : mode_2);
  json.set("name", (bulb == 1) ? "Light Bulb 1" : "Light Bulb 2");
  json.set("onAt/date", (bulb == 1) ? dateOn_1 : dateOn_2);
  json.set("onAt/time", timeOn__format12);
  json.set("offAt/date", (bulb == 1) ? dateOff_1 : dateOff_2);
  json.set("offAt/time", timeOff__format12);
  setObjectFB((bulb == 1 ? "/devices/0" : "/devices/1"), json);

  // logSerial();
}

//-----------------------------------------------------------------------------------------
void listen_btns() {
  //select BULB------------------------------
  if (digitalRead(btn_select_bulb) == LOW) {
    is_bulb_1 = !is_bulb_1;
    Serial.println(is_bulb_1 ? "BULB 1" : "BULB 2");
    cursor = 0;
  }
  //control by hand--------------------------
  if (digitalRead(btn_1_hand) == LOW) {
    if (mode_1 == "Manual") {
      state_1 = state_1 == "true" ? "false" : "true";
      setFB("devices/0/isOn", state_1);
    } else Serial.println("BULB_1 in mode: " + mode_1);
  }
  if (digitalRead(btn_2_hand) == LOW) {
    if (mode_2 == "Manual") {
      state_2 = state_2 == "true" ? "false" : "true";
      setFB("devices/1/isOn", state_2);
    } else Serial.println("BULB_2 in mode: " + mode_2);
  }
  //change mode------------------------------
  if (digitalRead(btn_select_mode) == LOW) {
    change_mode(is_bulb_1 ? 1 : 2);
  }
  //control cursor---------------------------
  if (digitalRead(btn_cursor) == LOW) {
    cursor = (cursor == 9) ? 0 : (cursor + 1);
    Serial.println("Cursor: " + String(cursor));
  }
  //adjust time------------------------------
  // btn_4 --> (+)
  if (digitalRead(btn_plus) == LOW) {
    adjust_time(is_bulb_1 ? 1 : 2, cursor, true);
  }
  // // btn_5 --> (-)
  if (digitalRead(btn_minus) == LOW) {
    adjust_time(is_bulb_1 ? 1 : 2, cursor, false);
  }
}

//-----------------------------------------------------------------------------------------
void checkTime(const RtcDateTime& dt) {
  char dateChar[11];
  char timeChar[6];
  char secondChar[4];

  snprintf_P(dateChar,
             countof(dateChar),
             PSTR("%02u/%02u/%04u"),
             dt.Day(),
             dt.Month(),
             dt.Year());

  snprintf_P(timeChar,
             countof(timeChar),
             PSTR("%02u:%02u"),
             dt.Hour(),
             dt.Minute());

  snprintf_P(secondChar,
             countof(secondChar),
             PSTR("%02u"),
             dt.Second());

  String date = String(dateChar);
  String time = String(timeChar);
  String second = String(secondChar);

  Serial.println(date + " " + time);

  if (dateOn_1 == date && timeOn_1 == time && second == "00") {
    state_1 = "true";
    setFB("devices/0/isOn", state_1);
    Serial.println("Time to BULB_1 ON");
  }
  if (dateOff_1 == date && timeOff_1 == time && second == "00") {
    state_1 = "false";
    setFB("devices/0/isOn", state_1);
    Serial.println("Time to BULB_1 OFF");
  }
  //--------------------------------------------
  if (dateOn_2 == date && timeOn_2 == time && second == "00") {
    state_2 = "true";
    setFB("devices/1/isOn", state_2);
    Serial.println("Time to BULB_2 ON");
  }
  if (dateOff_2 == date && timeOff_2 == time && second == "00") {
    state_2 = "false";
    setFB("devices/1/isOn", state_2);
    Serial.println("Time to BULB_2 OFF");
  }
}

//-----------------------------------------------------------------------------------------
void loop() {
  RtcDateTime now = Rtc.GetDateTime();
  listen_btns();
  checkTime(now);
  check_state();

  // check wifi-----------------------------------------------
  if (WiFi.status() == WL_CONNECTED)
    getFBData();

  //----------------------------------------------------
  if (is_bulb_1)
    displayLCD(1, state_1, mode_1, timeOn_1, dateOn_1, timeOff_1, dateOff_1, cursor, now);
  else
    displayLCD(2, state_2, mode_2, timeOn_2, dateOn_2, timeOff_2, dateOff_2, cursor, now);
}
