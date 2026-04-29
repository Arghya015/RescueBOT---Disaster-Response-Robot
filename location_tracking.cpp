#include <WiFi.h>
#include <ESP_Mail_Client.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>


#define LED_GREEN_PIN   27    // Green  LED  (NORMAL)
#define LED_YELLOW_PIN  14    // Yellow LED  (WARNING)
#define LED_RED_PIN     25    // Red    LED  (ALERT + CRITICAL)
#define BUZZER_PIN      26    // Passive piezo buzzer

// GPS on UART1
#define GPS_RX_PIN      16
#define GPS_TX_PIN      17
#define GPS_BAUD        9600

//  WiFi / EMAIL

#define WIFI_SSID        "Redmi Note 11 Pro+ 5G"
#define WIFI_PASSWORD    "@polu1411P"

#define SMTP_HOST        "smtp.gmail.com"
#define SMTP_PORT        465
#define AUTHOR_EMAIL     "prolayjitbiswas14112004@gmail.com"
#define AUTHOR_PASSWORD  "pqra zufp tqcq thar"
#define RECIPIENT_EMAIL  "bubun15072006@gmail.com"


//  TIMING (seconds)

#define T_NORMAL    10
#define T_WARNING    5
#define T_ALERT      5
#define T_CRITICAL  15
#define T_TOTAL     (T_NORMAL + T_WARNING + T_ALERT + T_CRITICAL)  // 35 s

// ─────────────────────────────────────────────────────────────
//  BUZZER PWM
// ─────────────────────────────────────────────────────────────
#define BUZZER_FREQ   3500   // Hz  (3-4 kHz = loudest perceived)
#define BUZZER_RES       8   // 8-bit PWM
#define BUZZER_ON      128   // 50% duty = max power for passive piezo

// ─────────────────────────────────────────────────────────────
//  EMAIL REPEAT IN CRITICAL
// ─────────────────────────────────────────────────────────────
#define EMAIL_INTERVAL_MS  5000   // every 5 s while CRITICAL

// ─────────────────────────────────────────────────────────────
//  GLOBALS
// ─────────────────────────────────────────────────────────────
enum State { NORMAL, WARNING, ALERT, CRITICAL };
State         currentState  = NORMAL;
State         lastState     = NORMAL;

unsigned long cycleStart    = 0;
unsigned long lastEmailTime = 0;

TinyGPSPlus    gps;
HardwareSerial gpsSerial(1);

SMTPSession    smtp;
Session_Config smtpCfg;



void allLedsOff() {
  digitalWrite(LED_GREEN_PIN,  LOW);
  digitalWrite(LED_YELLOW_PIN, LOW);
  digitalWrite(LED_RED_PIN,    LOW);
}

void ledGreen() {
  digitalWrite(LED_GREEN_PIN,  HIGH);
  digitalWrite(LED_YELLOW_PIN, LOW);
  digitalWrite(LED_RED_PIN,    LOW);
}

void ledYellow() {
  digitalWrite(LED_GREEN_PIN,  LOW);
  digitalWrite(LED_YELLOW_PIN, HIGH);
  digitalWrite(LED_RED_PIN,    LOW);
}

void ledRed() {
  digitalWrite(LED_GREEN_PIN,  LOW);
  digitalWrite(LED_YELLOW_PIN, LOW);
  digitalWrite(LED_RED_PIN,    HIGH);
}

// Called every loop tick during CRITICAL — blinks the red LED
void ledCriticalBlink() {
  digitalWrite(LED_GREEN_PIN,  LOW);
  digitalWrite(LED_YELLOW_PIN, LOW);
  // 200 ms ON / 200 ms OFF
  digitalWrite(LED_RED_PIN, ((millis() / 200) % 2 == 0) ? HIGH : LOW);
}


void updateBuzzer(State s) {
  uint16_t t;
  uint8_t  duty = 0;

  switch (s) {
    case NORMAL:
      duty = 0;
      break;

    case WARNING:
      t = (uint16_t)(millis() % 1500);
      duty = (t < 100 || (t >= 200 && t < 300)) ? BUZZER_ON : 0;
      break;

    case ALERT:
      t = (uint16_t)(millis() % 300);
      duty = (t < 150) ? BUZZER_ON : 0;
      break;

    case CRITICAL:
      t = (uint16_t)(millis() % 900);
      duty = (t < 80 ||
             (t >= 140 && t < 220) ||
             (t >= 280 && t < 360)) ? BUZZER_ON : 0;
      break;
  }

  ledcWrite(BUZZER_PIN, duty);
}


// ═══════════════════════════════════════════════════════════════
//  GPS HELPERS
// ═══════════════════════════════════════════════════════════════
void feedGPS() {
  while (gpsSerial.available()) gps.encode(gpsSerial.read());
}

String getMapURL() {
  if (gps.location.isValid())
    return "https://maps.google.com/?q=" +
           String(gps.location.lat(), 6) + "," +
           String(gps.location.lng(), 6);
  return "";
}

static String gpsTR(const String& lbl, const String& val) {
  return
    "<tr>"
    "<td style='padding:8px 6px;border-bottom:1px solid #eee;"
         "color:#7f8c8d;font-size:13px;width:145px'>" + lbl + "</td>"
    "<td style='padding:8px 6px;border-bottom:1px solid #eee;"
         "color:#2c3e50;font-size:13px;font-weight:600'>" + val + "</td>"
    "</tr>";
}

String buildGPSHtmlRows() {
  String rows = "";

  if (gps.location.isValid()) {
    rows += gpsTR("Latitude",  String(gps.location.lat(), 6) + " &deg;");
    rows += gpsTR("Longitude", String(gps.location.lng(), 6) + " &deg;");
  } else {
    rows +=
      "<tr><td colspan='2' style='padding:10px 6px;color:#c0392b;"
      "font-size:13px;font-style:italic'>"
      "GPS fix not yet acquired &mdash; location unavailable</td></tr>";
  }

  if (gps.altitude.isValid())
    rows += gpsTR("Altitude",  String(gps.altitude.meters(), 1) + " m");
  if (gps.speed.isValid())
    rows += gpsTR("Speed",     String(gps.speed.kmph(), 1) + " km/h");
  if (gps.course.isValid())
    rows += gpsTR("Heading",   String(gps.course.deg(), 1) + " &deg;");

  rows += gpsTR("Satellites", String(gps.satellites.value()));

  if (gps.hdop.isValid())
    rows += gpsTR("HDOP (accuracy)", String(gps.hdop.hdop(), 2));

  if (gps.date.isValid() && gps.time.isValid()) {
    char ts[28];
    snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d UTC",
             gps.date.year(), gps.date.month(), gps.date.day(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
    rows += gpsTR("GPS Timestamp", String(ts));
  }

  if (gps.location.isValid()) {
    unsigned long age = gps.location.age();
    String ageStr = (age < 2000)  ? "Live" :
                    (age < 10000) ? String(age / 1000) + " sec ago" :
                    "Stale (" + String(age / 1000) + "s)";
    rows += gpsTR("Fix age", ageStr);
  }

  return rows;
}

void printGPSSerial() {
  if (gps.location.isValid()) {
    Serial.printf("  GPS | Lat %.6f  Lng %.6f  Alt %.1fm"
                  "  Spd %.1fkm/h  Sats %u  Age %lums\n",
                  gps.location.lat(), gps.location.lng(),
                  gps.altitude.meters(), gps.speed.kmph(),
                  gps.satellites.value(), gps.location.age());
  } else {
    Serial.printf("  GPS | No fix  Sats %u  Chars %lu  Sentences %lu\n",
                  gps.satellites.value(),
                  gps.charsProcessed(),
                  gps.passedChecksum());
  }
}


// ═══════════════════════════════════════════════════════════════
//  EMAIL
//  Buzzer + LED kept alive every ~10 ms during blocking send.
// ═══════════════════════════════════════════════════════════════
void sendEmail(const String& level, const String& statusMsg) {

  Serial.println();
  Serial.println("  ╔═══════════════════════════════════════╗");
  Serial.println("  ║           SENDING EMAIL               ║");
  Serial.println("  ╠═══════════════════════════════════════╣");
  Serial.println("  ║  Level  : " + level);
  Serial.println("  ║  Msg    : " + statusMsg);
  printGPSSerial();

  String hdrBg, accentBg;
  if      (level == "WARNING")  { hdrBg = "#d68910"; accentBg = "#f39c12"; }
  else if (level == "ALERT")    { hdrBg = "#ba4a00"; accentBg = "#e67e22"; }
  else                          { hdrBg = "#922b21"; accentBg = "#e74c3c"; }

  String mapURL  = getMapURL();
  String gpsRows = buildGPSHtmlRows();
  String ip      = (WiFi.status() == WL_CONNECTED) ? WiFi.localIP().toString() : "Offline";
  String uptime  = String(millis() / 1000) + " s";

  // ── HTML email body ─────────────────────────────────────────
  String h = "";
  h += "<!DOCTYPE html><html lang='en'><head>"
       "<meta charset='UTF-8'>"
       "<meta name='viewport' content='width=device-width,initial-scale=1'>"
       "</head>"
       "<body style='margin:0;padding:0;background:#f2f3f5;"
       "font-family:Arial,Helvetica,sans-serif'>";

  h += "<div style='max-width:560px;margin:20px auto;"
       "border-radius:10px;overflow:hidden;border:1px solid #ddd'>";

  // Header
  h += "<div style='background:" + hdrBg + ";padding:24px 28px;text-align:center'>";
  h += "<p style='margin:0 0 6px;font-size:12px;color:#ffffffaa;"
       "letter-spacing:2px;text-transform:uppercase'>DISASTER ALERT SYSTEM</p>";
  h += "<h1 style='margin:0 0 8px;font-size:30px;font-weight:900;color:#fff'>"
       + level + "</h1>";
  h += "<p style='margin:0;font-size:14px;color:#ffffffdd'>" + statusMsg + "</p>";
  h += "</div>";

  // Body
  h += "<div style='background:#fff;padding:24px 28px'>";

  // Location
  h += "<h2 style='margin:0 0 10px;font-size:13px;font-weight:700;"
       "color:#2c3e50;text-transform:uppercase;letter-spacing:1px;"
       "border-left:4px solid " + accentBg + ";padding-left:10px'>Location</h2>";
  h += "<table style='width:100%;border-collapse:collapse;margin-bottom:18px'>";
  h += gpsRows;
  h += "</table>";

  if (mapURL != "") {
    h += "<div style='text-align:center;margin-bottom:22px'>";
    h += "<a href='" + mapURL + "' "
         "style='background:" + accentBg + ";color:#fff;padding:11px 26px;"
         "border-radius:6px;text-decoration:none;font-size:14px;font-weight:700'>"
         "Open in Google Maps</a>";
    h += "</div>";
  }

  // Device info
  h += "<h2 style='margin:0 0 10px;font-size:13px;font-weight:700;"
       "color:#2c3e50;text-transform:uppercase;letter-spacing:1px;"
       "border-left:4px solid " + accentBg + ";padding-left:10px'>Device</h2>";
  h += "<table style='width:100%;border-collapse:collapse;margin-bottom:18px'>";
  h += gpsTR("WiFi IP",  ip);
  h += gpsTR("SSID",     String(WIFI_SSID));
  h += gpsTR("RSSI",     String(WiFi.RSSI()) + " dBm");
  h += gpsTR("Uptime",   uptime);
  h += "</table>";

  // Notice
  h += "<div style='background:#fef9e7;border-left:4px solid #f39c12;"
       "padding:12px 14px;border-radius:0 6px 6px 0'>";
  h += "<p style='margin:0;font-size:13px;color:#7d6608;font-weight:600'>"
       "&#9888; Verify the situation immediately and take appropriate action.</p>";
  h += "</div>";

  h += "</div>";  // end body

  // Footer
  h += "<div style='background:#2c3e50;padding:10px 28px;text-align:center'>";
  h += "<p style='margin:0;font-size:11px;color:#95a5a6'>"
       "ESP32 Disaster Alert System &mdash; automated notification</p>";
  h += "</div>";

  h += "</div></body></html>";
  // ── END HTML ────────────────────────────────────────────────

  SMTP_Message msg;
  msg.sender.name  = "Disaster Alert System";
  msg.sender.email = AUTHOR_EMAIL;
  msg.subject      = "[" + level + "] " + statusMsg;
  msg.addRecipient("Emergency Contact", RECIPIENT_EMAIL);
  msg.html.content = h.c_str();
  msg.html.charSet = "UTF-8";

  smtpCfg.server.host_name  = SMTP_HOST;
  smtpCfg.server.port       = SMTP_PORT;
  smtpCfg.login.email       = AUTHOR_EMAIL;
  smtpCfg.login.password    = AUTHOR_PASSWORD;
  smtpCfg.login.user_domain = "";
  smtp.debug(0);

  // smtp.connect() blocks ~1-3 s — spin loop keeps red blinking + buzzer going
  bool connected = smtp.connect(&smtpCfg);
  unsigned long blockStart = millis();
  while (millis() - blockStart < 50) {   // small drain loop after connect
    updateBuzzer(CRITICAL);
    ledCriticalBlink();
    delay(5);
  }

  if (!connected) {
    Serial.println("  ║  SMTP CONNECT FAILED: " + smtp.errorReason());
    Serial.println("  ╚═══════════════════════════════════════╝");
    return;
  }

  // MailClient.sendMail() blocks ~1-2 s — keep blinking while it runs
  // We can't hook inside it, so pulse blink+buzzer immediately before and after
  for (int i = 0; i < 5; i++) { updateBuzzer(CRITICAL); ledCriticalBlink(); delay(10); }
  feedGPS();

  bool sent = MailClient.sendMail(&smtp, &msg);

  // Recover blink immediately after send — close the dark gap
  for (int i = 0; i < 5; i++) { updateBuzzer(CRITICAL); ledCriticalBlink(); delay(10); }

  if (sent) {
    Serial.println("  ║  OK  ->  " + String(RECIPIENT_EMAIL));
  } else {
    Serial.println("  ║  FAILED: " + smtp.errorReason());
  }

  smtp.closeSession();
  Serial.println("  ╚═══════════════════════════════════════╝");
}


// ═══════════════════════════════════════════════════════════════
//  SERIAL HELPERS
// ═══════════════════════════════════════════════════════════════
static String progressBar(int pct) {
  pct = constrain(pct, 0, 100);
  int filled = pct / 5;
  String bar = "[";
  for (int i = 0; i < 20; i++)
    bar += (i < filled) ? '=' : (i == filled ? '>' : ' ');
  return bar + "] " + String(pct) + "%";
}

void printStateChange(State s) {
  Serial.println();
  Serial.println("  ╔══════════════════════════════════════════╗");
  switch (s) {
    case NORMAL:
      Serial.println("  ║  STATE → NORMAL                          ║");
      Serial.println("  ║  LED : GREEN solid    Buzzer : Silent     ║");
      break;
    case WARNING:
      Serial.println("  ║  STATE → WARNING                         ║");
      Serial.println("  ║  LED : YELLOW solid   Buzzer : Dbl-beep  ║");
      break;
    case ALERT:
      Serial.println("  ║  STATE → ALERT                           ║");
      Serial.println("  ║  LED : RED solid      Buzzer : Fast beep ║");
      break;
    case CRITICAL:
      Serial.println("  ║  STATE → CRITICAL                        ║");
      Serial.println("  ║  LED : RED blink      Buzzer : SOS burst ║");
      Serial.println("  ║  → GPS fetch + email triggered            ║");
      break;
  }
  Serial.printf("  ║  Uptime : %lu s\n", millis() / 1000);
  printGPSSerial();
  Serial.println("  ╚══════════════════════════════════════════╝");
}

void printCycleProgress(unsigned long t) {
  static unsigned long lastPrint = 0;
  if (millis() - lastPrint < 5000) return;
  lastPrint = millis();

  int pct = (int)((t * 100UL) / T_TOTAL);

  int nextSec;
  const char* nextName;
  if      (t < T_NORMAL)                           { nextSec = T_NORMAL - t;                   nextName = "WARNING";  }
  else if (t < T_NORMAL + T_WARNING)               { nextSec = T_NORMAL + T_WARNING - t;       nextName = "ALERT";    }
  else if (t < T_NORMAL + T_WARNING + T_ALERT)     { nextSec = T_NORMAL+T_WARNING+T_ALERT - t; nextName = "CRITICAL"; }
  else                                              { nextSec = T_TOTAL - t;                    nextName = "RESET";    }

  Serial.printf("  Cycle %s | next: %-8s in %2ds | sats: %u\n",
                progressBar(pct).c_str(), nextName, nextSec,
                gps.satellites.value());
}


// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(300);

  Serial.println();
  Serial.println("  ╔══════════════════════════════════════════╗");
  Serial.println("  ║     ESP32  DISASTER  ALERT  SYSTEM       ║");
  Serial.println("  ║              Firmware  v5.0              ║");
  Serial.println("  ╠══════════════════════════════════════════╣");
  Serial.printf( "  ║  NORMAL %ds | WARNING %ds | ALERT %ds | CRIT %ds ║\n",
                 T_NORMAL, T_WARNING, T_ALERT, T_CRITICAL);
  Serial.println("  ╚══════════════════════════════════════════╝");
  Serial.println();

  // ── Individual LEDs ──────────────────────────────────────────
  pinMode(LED_GREEN_PIN,  OUTPUT);
  pinMode(LED_YELLOW_PIN, OUTPUT);
  pinMode(LED_RED_PIN,    OUTPUT);
  allLedsOff();
  Serial.printf("  [OK] LEDs  Green:GPIO%d  Yellow:GPIO%d  Red:GPIO%d\n",
                LED_GREEN_PIN, LED_YELLOW_PIN, LED_RED_PIN);

  // ── Buzzer PWM ───────────────────────────────────────────────
  ledcAttach(BUZZER_PIN, BUZZER_FREQ, BUZZER_RES);
  ledcWrite(BUZZER_PIN, 0);
  Serial.printf("  [OK] Buzzer  GPIO%d  %dHz  50%% duty=%d/255\n",
                BUZZER_PIN, BUZZER_FREQ, BUZZER_ON);

  // ── GPS ──────────────────────────────────────────────────────
  gpsSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Serial.printf("  [OK] GPS  RX:GPIO%d  TX:GPIO%d  %d baud\n",
                GPS_RX_PIN, GPS_TX_PIN, GPS_BAUD);

  // ── WiFi ─────────────────────────────────────────────────────
  Serial.print("  [..] WiFi connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long wStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wStart < 15000) {
    delay(400);
    Serial.print(".");
    // Blink green while connecting
    ((millis() / 400) % 2) ? ledGreen() : allLedsOff();
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("  [OK] WiFi  SSID:" + String(WIFI_SSID) +
                   "  IP:" + WiFi.localIP().toString() +
                   "  RSSI:" + String(WiFi.RSSI()) + "dBm");
  } else {
    Serial.println("  [!!] WiFi FAILED — email disabled");
    ledRed();
    delay(1500);
    allLedsOff();
  }

  // Boot confirmation — 3 quick green flashes
  for (int i = 0; i < 3; i++) {
    ledGreen(); delay(180);
    allLedsOff(); delay(120);
  }

  Serial.println();
  Serial.println("  System READY.  Automatic alert cycle starting...");
  Serial.println();

  cycleStart = millis();
}


// ═══════════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {

  // 1. Feed GPS
  feedGPS();

  // 2. Elapsed time in current cycle (seconds)
  unsigned long t = (millis() - cycleStart) / 1000UL;

  // 3. Map time → state
  if      (t < T_NORMAL)                        currentState = NORMAL;
  else if (t < T_NORMAL + T_WARNING)            currentState = WARNING;
  else if (t < T_NORMAL + T_WARNING + T_ALERT)  currentState = ALERT;
  else if (t < T_TOTAL)                         currentState = CRITICAL;
  else {
    cycleStart   = millis();
    currentState = NORMAL;
    lastState    = CRITICAL;  // force state-change handler on next tick
    Serial.println("  [Cycle] Complete → resetting to NORMAL");
  }

  // 4. On state change: set LED + log + (CRITICAL) send email
  if (currentState != lastState) {
    lastState = currentState;   // set FIRST so blink handler is live immediately

    printStateChange(currentState);

    switch (currentState) {
      case NORMAL:   ledGreen();  break;
      case WARNING:  ledYellow(); break;
      case ALERT:    ledRed();    break;
      case CRITICAL:
        // Turn red ON immediately so there is no dark gap before blink starts
        ledRed();
        feedGPS();
        sendEmail("CRITICAL", "Critical Disaster Level Detected!");
        lastEmailTime = millis();
        break;
    }
  }

  // 5. CRITICAL: blink red LED every loop tick (millis-based, no blocking)
  if (currentState == CRITICAL) {
    ledCriticalBlink();
  }

  // 6. Buzzer pattern (every tick, ~10 ms resolution)
  updateBuzzer(currentState);

  // 7. Repeat email every 5 s while CRITICAL
  if (currentState == CRITICAL &&
      millis() - lastEmailTime >= EMAIL_INTERVAL_MS) {
    feedGPS();
    sendEmail("CRITICAL", "Critical Alert Still Active — Immediate action required!");
    lastEmailTime = millis();
  }

  // 8. WiFi watchdog — reconnect if dropped
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastRetry = 0;
    if (millis() - lastRetry > 30000) {
      Serial.println("  [!!] WiFi lost — reconnecting...");
      WiFi.reconnect();
      lastRetry = millis();
    }
  }

  // 9. Periodic cycle progress print every 5 s
  printCycleProgress(t);

  // 10. 10 ms yield — keeps buzzer/blink patterns smooth
  //     DO NOT increase: SOS bursts are 80 ms wide, need <10 ms granularity
  delay(10);
}
