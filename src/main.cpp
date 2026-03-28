#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <PDIO.h>
#include "imagenes.h"

RTC_DS3231 rtc;
DateTime now;

#define PIN_SCL 9
#define PIN_SDA 8

// Pines de Encoder (cambiar si tu cableado difiere)
#define ENC_CLK 3
#define ENC_DT 2
#define ENC_SW 1

#define LED 20

#define ANCHO_PANTALLA 128
#define ALTO_PANTALLA 64
#define OLED_RESET -1
#define ADDRESS_SCREEN 0x3c

struct Imagen {
  char ID[4];
  int Alto;
  int Ancho;
  const unsigned char* BitMap;
};

const Imagen IMAGENES[] = {
  {"RLJ", Reloj_Height, Reloj_Width, Reloj},
  {"LUZ", Foco_Height, Foco_Width, Foco},
  {"CAL", Calendario_Height, Calendario_Width, Calendario},
  {"EVT", Evento_Alcanzado_Height, Evento_Alcanzado_Width, Evento_Alcanzado},
};

const size_t NUM_IMAGENES = sizeof(IMAGENES)/sizeof(IMAGENES[0]);

Adafruit_SSD1306 display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, OLED_RESET);

// --- Utilidades imagenes ---
const Imagen* BuscarImagenID(const char id[4]) {
  if(!id) return nullptr;
  for (size_t i = 0; i < NUM_IMAGENES; ++i) {
    if(strncmp(IMAGENES[i].ID, id, 3) == 0) return &IMAGENES[i];
  }
  return nullptr;
}

void MostrarImagenID(const char id[4], int tiempoSeg) {
  const Imagen* img = BuscarImagenID(id);
  if (!img) {
    Serial.println("Imagen no encontrada");
    digitalWrite(LED, HIGH);
    delay(3000);
    digitalWrite(LED, LOW);
    return;
  }
  display.clearDisplay();
  display.drawBitmap((display.width() - img->Ancho) / 2, (display.height() - img->Alto) / 2, img->BitMap, img->Ancho, img->Alto, SSD1306_WHITE);
  display.display();
  delay(tiempoSeg * 1000);
  display.clearDisplay();
  display.display();
}

// --- Estado del Menu ---
enum Pantalla {
  MENU,
  LUZ_TOGGLE,
  FECHA_HORA,
  MODO_PROG,
  CALENDARIO
};

// Encoder: sensibilidad ligeramente menor (5 transiciones por detent y 2ms entre lecturas)
volatile int8_t enc_delta = 0;
volatile uint32_t last_enc_ms = 0;
volatile uint8_t enc_state = 0; // bits: A en bit0, B en bit1
volatile int8_t enc_accum = 0;  // acumula +1/-1 por transición válida

// Botón: detección de corto/largo
volatile bool enc_btn_state = false;      // true = presionado (LOW)
volatile bool enc_btn_changed = false;    // hubo cambio
volatile uint32_t enc_btn_change_ms = 0;  // tiempo del cambio (ms)

int menu_index = 0; // 0..3
Pantalla pantalla = MENU;
bool luz_on = false;
bool modo_prog_activo = false; // LED encendido mientras esté activo
// Estado del calendario navegable
int cal_year = 0;
int cal_month = 0; // 1..12

// === Estado de edición Fecha/Hora ===
bool fh_edit_mode = false;
int  fh_sel_field = 0; // 0:HH,1:MM,2:SS,3:DD,4:MM,5:YYYY
int  fh_year = 0, fh_month = 0, fh_day = 0, fh_hour = 0, fh_min = 0, fh_sec = 0;

// --- Encoder ISRs ---
void IRAM_ATTR isrEncoderRot() {
  uint32_t nowMs = millis();
  if (nowMs - last_enc_ms < 2) return; // filtro mínimo aumentado a 2ms
  last_enc_ms = nowMs;

  uint8_t a = digitalRead(ENC_CLK) & 1;
  uint8_t b = digitalRead(ENC_DT) & 1;
  uint8_t new_state = (a) | (b << 1);

  // Tabla de movimientos (00->01->11->10->00 == +1)
  int8_t diff = ((enc_state & 1) ^ (new_state >> 1)) - ((enc_state >> 1) ^ (new_state & 1));
  enc_state = new_state;

  if (diff == 1) enc_accum++;
  else if (diff == -1) enc_accum--;

  // Emitir paso solo cuando se completan 5 transiciones (ligeramente menos sensible)
  if (enc_accum >= 5) { enc_accum = 0; enc_delta++; }
  else if (enc_accum <= -5) { enc_accum = 0; enc_delta--; }
}

void IRAM_ATTR isrEncoderBtn() {
  // Pulsador cableado a GND, con PULLUP => LOW = presionado
  bool pressed = (digitalRead(ENC_SW) == LOW);
  uint32_t nowMs = millis();
  static uint32_t lastChange = 0;
  if (nowMs - lastChange < 10) return; // debounce básico ISR
  lastChange = nowMs;
  enc_btn_state = pressed;
  enc_btn_changed = true;
  enc_btn_change_ms = nowMs;
}

// Dibuja un selector simple
void drawMenu(int index) {
  const char* items[4] = {
    "Luz ON/OFF",
    "Fecha y Hora",
    "Modo Programacion",
    "Calendario"
  };
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println("> MENU");

  for (int i = 0; i < 4; ++i) {
    int y = 14 + i*12;
    if (i == index) {
      display.fillRect(0, y-2, ANCHO_PANTALLA, 11, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(4, y);
    display.println(items[i]);
  }

  // Indicadores de estado pequeños
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 62);
  display.print("Luz:"); display.print(luz_on?"ON":"OFF");
  display.setCursor(64, 62);
  display.print("Prog:"); display.print(modo_prog_activo?"ON":"OFF");
  display.display();
}

int dayOfWeek(int y, int m, int d) { // 0=Domingo..6=Sabado
  // Zeller congruence (adjusted for Sunday=0)
  if (m < 3) { m += 12; y -= 1; }
  int K = y % 100;
  int J = y / 100;
  int h = (d + (13*(m+1))/5 + K + K/4 + J/4 + 5*J) % 7; // 0=Sabado
  int dow = (h + 6) % 7; // 0=Domingo
  return dow;
}

int daysInMonth(int y, int m) {
  static const int d[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
  if (m==2) {
    bool leap = ( (y%4==0 && y%100!=0) || (y%400==0) );
    return leap?29:28;
  }
  return d[m-1];
}

void drawFechaHoraView() {
  now = rtc.now();
  display.clearDisplay();

  const Imagen* iconoReloj = BuscarImagenID("RLJ");
  const Imagen* iconoCalendario = BuscarImagenID("CAL");

  const int ICON_W = 22, ICON_H = 22;
  if (iconoReloj) {
    int drawW = min(iconoReloj->Ancho, ICON_W);
    int drawH = min(iconoReloj->Alto, ICON_H);
    int offsetX = (ICON_W - drawW)/2;
    int offsetY = (ICON_H - drawH)/2;
    display.drawBitmap(offsetX, offsetY, iconoReloj->BitMap, drawW, drawH, SSD1306_WHITE);
  }
  if (iconoCalendario) {
    int drawW = min(iconoCalendario->Ancho, ICON_W);
    int drawH = min(iconoCalendario->Alto, ICON_H);
    int offsetX = (ICON_W - drawW)/2;
    int offsetY = (ICON_H - drawH)/2;
    display.drawBitmap(offsetX, offsetY + 32, iconoCalendario->BitMap, drawW, drawH, SSD1306_WHITE);
  }

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(ICON_W + 4, 4);
  char hora[9];
  snprintf(hora, sizeof(hora), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  display.println(hora);

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(ICON_W + 4, 36);
  char fecha[11];
  snprintf(fecha, sizeof(fecha), "%02d/%02d/%02d", now.day(), now.month(), now.year()%100);
  display.println(fecha);

  display.setTextSize(1);
  display.setCursor(0, 56);
  display.print("Pulsa para editar | Mantener: menu");
  display.display();
}

void drawFechaHoraEdit() {
  // Dibuja usando fh_* y resalta el campo activo
  display.clearDisplay();

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.print("Editar Fecha/Hora (24h)");

  // Hora HH:MM:SS
  display.setTextSize(2);
  int x = 0, y = 16;
  char buf[9];
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", fh_hour, fh_min, fh_sec);
  display.setCursor(x, y);
  display.print(buf);

  // Resaltado de campo activo (rectángulos detrás)
  // Ubicaciones aproximadas para fuente size=2 (cada char ~12px ancho)
  int hx = x + (fh_sel_field==0 ? 0 : fh_sel_field==1 ? 3*12 : 6*12); // posición de inicio del campo HH/MM/SS
  int hw = 2*12; // ancho de 2 dígitos
  if (fh_sel_field <= 2) { // Hora
    display.fillRect(hx, y-2, hw, 18, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(x, y);
    display.print(buf);
    display.setTextColor(SSD1306_WHITE);
  }

  // Fecha DD/MM/YYYY
  display.setTextSize(2);
  y = 38;
  char bufF[16];
  snprintf(bufF, sizeof(bufF), "%02d/%02d/%04d", fh_day, fh_month, fh_year);
  display.setCursor(x, y);
  display.print(bufF);

  // Resaltado de campo activo de fecha
  if (fh_sel_field >= 3) {
    int idx = fh_sel_field - 3; // 0:DD 1:MM 2:YYYY
    int fx = x + (idx==0 ? 0 : idx==1 ? 3*12 : 6*12); // posiciones aproximadas
    int fw = (idx==2 ? 4*12 : 2*12);
    display.fillRect(fx, y-2, fw, 18, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(x, y);
    display.print(bufF);
    display.setTextColor(SSD1306_WHITE);
  }

  // Ayuda
  display.setTextSize(1);
  display.setCursor(0, 58);
  display.print("Gira: cambia | Pulsa: sig | Mantener: cancelar");
  display.display();
}

// Calendario: lunes como primer dia, 6 filas completas con desborde de meses
void drawCalendario() {
  if (cal_year == 0 || cal_month == 0) {
    now = rtc.now();
    cal_year = now.year();
    cal_month = now.month();
  }
  int y = cal_year;
  int m = cal_month;

  DateTime hoy = rtc.now();
  int today_y = hoy.year();
  int today_m = hoy.month();
  int today_d = hoy.day();

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  static const char* meses[13] = {"","Ene","Feb","Mar","Abr","May","Jun","Jul","Ago","Sep","Oct","Nov","Dic"};
  display.setCursor(0,0);
  display.printf("%s %04d", meses[m], y);
  display.setCursor(80,0);
  display.print("< Mes >");

  const int colW = 18;
  const int rowH = 9;
  const int x0 = 1;
  const int yHdr = 10;
  const int y0 = 20;

  // Encabezado Lunes primero
  const char* hdr[7] = {"L","M","M","J","V","S","D"};
  for (int c=0; c<7; ++c) {
    int x = x0 + c*colW + 5;
    display.setCursor(x, yHdr);
    display.print(hdr[c]);
  }

  // 0=Lunes..6=Domingo
  int firstDowSunday = dayOfWeek(y, m, 1); // 0=Domingo..6=Sabado
  int firstDowMonday = (firstDowSunday + 6) % 7; // 0=Lunes..6=Domingo
  int dim = daysInMonth(y, m);

  // Mes anterior y siguiente
  int prevY = (m == 1) ? (y - 1) : y;
  int prevM = (m == 1) ? 12 : (m - 1);
  int nextY = (m == 12) ? (y + 1) : y;
  int nextM = (m == 12) ? 1 : (m + 1);

  int dimPrev = daysInMonth(prevY, prevM);
  int startPrevCount = firstDowMonday; // cuantos días previos van al inicio

  // Imprimir 6 filas x 7 columnas = 42 celdas
  int col = 0, row = 0;
  int dayCur = 1;
  int dayNext = 1;

  for (int cell = 0; cell < 42; ++cell) {
    int cellX = x0 + col * colW + 1;  // borde izquierdo celda
    int cellY = y0 + row * rowH - 1;  // borde superior celda
    int xx = cellX + 2;               // cursor X para número
    int yy = cellY + 2;               // cursor Y para número

    bool isCurrentMonthCell = false;
    int valueToPrint = 0;

    if (cell < startPrevCount) {
      // Días del mes anterior
      valueToPrint = dimPrev - startPrevCount + 1 + cell;
      isCurrentMonthCell = false;
    } else if (dayCur <= dim) {
      // Días del mes actual
      valueToPrint = dayCur++;
      isCurrentMonthCell = true;
    } else {
      // Días del mes siguiente
      valueToPrint = dayNext++;
      isCurrentMonthCell = false;
    }

    bool isToday = (isCurrentMonthCell &&
                    y == today_y && m == today_m && valueToPrint == today_d);

    // Recuadro para días fuera del mes
    if (!isCurrentMonthCell) {
      // dibuja un marco fino rodeando la celda
      display.drawRect(cellX, cellY, colW - 2, rowH, SSD1306_WHITE);
    }

    if (isToday) {
      // Resalta el día de hoy con fondo sólido
      display.fillRect(cellX, cellY, colW - 2, rowH, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }

    // Número del día
    display.setCursor(xx, yy);
    display.printf("%2d", valueToPrint);

    // Restaurar color si fue invertido
    if (isToday) display.setTextColor(SSD1306_WHITE);

    // Avance de columna/fila
    col++;
    if (col > 6) { col = 0; row++; }
  }

  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Wire.begin(PIN_SDA, PIN_SCL);
  pinMode(LED, OUTPUT);

  // Encoder
  pinMode(ENC_CLK, INPUT_PULLUP);
  pinMode(ENC_DT, INPUT_PULLUP);
  pinMode(ENC_SW, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(ENC_CLK), isrEncoderRot, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_DT),  isrEncoderRot, CHANGE);
  attachInterrupt(digitalPinToInterrupt(ENC_SW),  isrEncoderBtn,  CHANGE);

  if (!display.begin(SSD1306_SWITCHCAPVCC, ADDRESS_SCREEN)) {
    Serial.println("Error inicializando la pantalla OLED");
  }

  if (!rtc.begin()) {
    Serial.println("No se encontro el DS3231 en I2C");
  } else if (rtc.lostPower()) {
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  drawMenu(menu_index);
}

int scanI2C() {
  byte error, address; int nDevices = 0;
  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) nDevices++; 
  }
  return nDevices;
}

void ProcesarComando(String cmd) {
  if (cmd.startsWith("IMG")) {
    String IDImg = cmd.substring(4, 7);
    int tiempo = cmd.substring(7).toInt();
    MostrarImagenID(IDImg.c_str(), tiempo);
  }
}

static void fh_load_from_rtc() {
  DateTime t = rtc.now();
  fh_year = t.year();
  fh_month = t.month();
  fh_day = t.day();
  fh_hour = t.hour();
  fh_min = t.minute();
  fh_sec = t.second();
}

int daysInMonth(int y, int m); // forward (ya definida arriba)

static void fh_clamp_field() {
  // Asegura rangos válidos según campo
  if (fh_hour < 0) fh_hour = 0; if (fh_hour > 23) fh_hour = 23;
  if (fh_min  < 0) fh_min  = 0; if (fh_min  > 59) fh_min  = 59;
  if (fh_sec  < 0) fh_sec  = 0; if (fh_sec  > 59) fh_sec  = 59;
  if (fh_month < 1) fh_month = 1; if (fh_month > 12) fh_month = 12;
  if (fh_year < 2000) fh_year = 2000; if (fh_year > 2099) fh_year = 2099;
  int dim = daysInMonth(fh_year, fh_month);
  if (fh_day < 1) fh_day = 1; if (fh_day > dim) fh_day = dim;
}

static void fh_apply_delta(int8_t delta) {
  if (delta == 0) return;
  switch (fh_sel_field) {
    case 0: fh_hour += delta; break;
    case 1: fh_min  += delta; break;
    case 2: fh_sec  += delta; break;
    case 3: fh_day  += delta; break;
    case 4: fh_month += delta; break;
    case 5: fh_year += delta; break;
  }
  // Ajustes de rollover suaves
  if (fh_sel_field == 0) { if (fh_hour < 0) fh_hour = 23; if (fh_hour > 23) fh_hour = 0; }
  if (fh_sel_field == 1) { if (fh_min  < 0) fh_min  = 59; if (fh_min  > 59) fh_min  = 0; }
  if (fh_sel_field == 2) { if (fh_sec  < 0) fh_sec  = 59; if (fh_sec  > 59) fh_sec  = 0; }
  if (fh_sel_field == 4) {
    if (fh_month < 1) { fh_month = 12; fh_year--; }
    if (fh_month > 12) { fh_month = 1; fh_year++; }
  }
  fh_clamp_field();
}

void loop() {
  // Lectura del encoder y botón con eventos
  int8_t delta;
  bool btnChanged;
  bool btnState;
  uint32_t btnChangeAt;

  noInterrupts();
  delta = enc_delta; enc_delta = 0;
  btnChanged = enc_btn_changed; enc_btn_changed = false;
  btnState = enc_btn_state;
  btnChangeAt = enc_btn_change_ms;
  interrupts();

  // Detección corto/largo
  static bool btnDown = false;
  static uint32_t btnDownMs = 0;
  const uint32_t longPressMs = 700;
  const uint32_t shortPressMaxMs = 300;
  bool btnShort = false;
  bool btnLong = false;

  if (btnChanged) {
    if (btnState) { // presionado
      btnDown = true;
      btnDownMs = btnChangeAt;
    } else { // liberado
      if (btnDown) {
        uint32_t dur = millis() - btnDownMs;
        if (dur >= longPressMs) btnLong = true;
        else if (dur >= 30 && dur <= shortPressMaxMs) btnShort = true;
      }
      btnDown = false;
    }
  }

  // --- Máquina de estados ---
  if (pantalla == MENU) {
    if (delta != 0) {
      menu_index = (menu_index + (delta>0?1:-1)) % 4;
      if (menu_index < 0) menu_index += 4;
      drawMenu(menu_index);
    }
    if (btnShort) {
      switch (menu_index) {
        case 0: pantalla = LUZ_TOGGLE; break;
        case 1: pantalla = FECHA_HORA; fh_edit_mode = false; drawFechaHoraView(); break;
        case 2: pantalla = MODO_PROG;  break;
        case 3: pantalla = CALENDARIO; 
                now = rtc.now();
                cal_year = now.year();
                cal_month = now.month();
                break;
      }
    }
    // btnLong en MENU: sin acción
  } else {
    if (pantalla == LUZ_TOGGLE) {
      luz_on = !luz_on;
      digitalWrite(LED, luz_on ? HIGH : LOW);
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(10, 24);
      display.println(luz_on?"LUZ ON":"LUZ OFF");
      display.display();
      delay(500);
      pantalla = MENU;
      drawMenu(menu_index);
    } else if (pantalla == FECHA_HORA) {
      if (!fh_edit_mode) {
        drawFechaHoraView();
        if (btnShort) {
          // Entrar en edición
          fh_edit_mode = true;
          fh_sel_field = 0;
          fh_load_from_rtc();
          drawFechaHoraEdit();
        }
        if (btnLong) { pantalla = MENU; drawMenu(menu_index); }
        delay(100);
      } else {
        // En edición
        if (delta != 0) {
          fh_apply_delta(delta > 0 ? 1 : -1);
          drawFechaHoraEdit();
        }
        if (btnShort) {
          fh_sel_field++;
          if (fh_sel_field > 5) {
            // Confirmar y guardar
            rtc.adjust(DateTime((uint16_t)fh_year, (uint8_t)fh_month, (uint8_t)fh_day,
                                (uint8_t)fh_hour, (uint8_t)fh_min, (uint8_t)fh_sec));
            fh_edit_mode = false;
            drawFechaHoraView();
          } else {
            drawFechaHoraEdit();
          }
        }
        if (btnLong) {
          // cancelar edición y volver a vista sin guardar
          fh_edit_mode = false;
          drawFechaHoraView();
        }
      }
    } else if (pantalla == MODO_PROG) {
      static bool first = true;
      if (first) { first = false; modo_prog_activo = !modo_prog_activo; digitalWrite(LED, modo_prog_activo?HIGH:LOW); }
      display.clearDisplay();
      display.setTextSize(2);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 24);
      display.println("Modo Prog");
      display.setTextSize(1);
      display.setCursor(0, 50);
      display.print("Estado: "); display.println(modo_prog_activo?"ON":"OFF");
      display.display();
      if (btnShort) { modo_prog_activo = !modo_prog_activo; digitalWrite(LED, modo_prog_activo?HIGH:LOW); pantalla = MENU; first = true; drawMenu(menu_index);} 
      if (btnLong) { pantalla = MENU; first = true; drawMenu(menu_index); }
      delay(120);
    } else if (pantalla == CALENDARIO) {
      if (delta != 0) {
        int step = (delta>0)? 1 : -1;
        cal_month += step;
        if (cal_month > 12) { cal_month = 1; cal_year++; }
        else if (cal_month < 1) { cal_month = 12; cal_year--; }
      }
      drawCalendario();
      if (btnShort || btnLong) { pantalla = MENU; drawMenu(menu_index); }
      delay(120);
    }
  }

  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    ProcesarComando(comando);
  }
}