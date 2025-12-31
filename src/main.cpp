#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <RTClib.h>
#include <PDIO.h>
#include "imagenes.h"

RTC_DS3231 rtc;
DateTime now;

#define PIN_SCL 9 // ESP32 C3 Mini: default I2C SCL = GPIO9
#define PIN_SDA 8 // ESP32 C3 Mini: default I2C SDA = GPIO8

#define LED 20 // ESP32 S3 -> 48

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
  {"AXL", Ajolote_Height, Ajolote_Width, Ajolote},
  {"PNK", SpiderPunk_Height, SpiderPunk_Width, SpiderPunk},
  {"RBL", Rebelion_Height, Rebelion_Width, Rebelion},
  {"TRZ", Trazo_Height, Trazo_Width, Trazo},
  {"RLJ", Reloj_Height, Reloj_Width, Reloj},
  {"LUZ", Foco_Height, Foco_Width, Foco},
  {"CAL", Calendario_Height, Calendario_Width, Calendario},
  {"EVT", Evento_Alcanzado_Height, Evento_Alcanzado_Width, Evento_Alcanzado},
};

const size_t NUM_IMAGENES = sizeof(IMAGENES)/sizeof(IMAGENES[0]);

Adafruit_SSD1306 display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, OLED_RESET);

const Imagen* BuscarImagenID(const char id[4]) {
  if(!id)
    return nullptr;
  for (size_t i = 0; i < NUM_IMAGENES; ++i) {
    if(strncmp(IMAGENES[i].ID, id, 3) == 0) {
      return &IMAGENES[i];
    }
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
  display.display();
  display.drawBitmap((display.width() - img->Ancho) / 2, (display.height() - img->Alto) / 2, img->BitMap, img->Ancho, img->Alto, SSD1306_WHITE);
  display.display();
  delay(tiempoSeg * 1000);
  display.clearDisplay();
  display.display();
}

void setup() {
  Serial.begin(115200);
  delay(2500);

  Wire.begin(PIN_SDA, PIN_SCL);
  delay(100);

  pinMode(LED, OUTPUT);

  if (!display.begin(SSD1306_SWITCHCAPVCC, ADDRESS_SCREEN)) {
    Serial.println("Error inicializando la pantalla OLED");
    digitalWrite(LED, HIGH);
    //while(1);
  }

  // Inicializar RTC DS3231
  if (!rtc.begin()) {
    Serial.println("No se encontro el DS3231 en I2C");
  } else {
    if (rtc.lostPower()) {
      Serial.println("RTC sin hora valida, estableciendo fecha/hora por compilacion");
      rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }
  }

  display.clearDisplay();
  display.display();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println("Sistema Iniciado");
  display.display();
  delay(200);

  Serial.println("Sistema Iniciado");
  delay(1500);
  display.clearDisplay();
  display.display();
}

int scanI2C() {
  byte error, address;
  int nDevices = 0;
  Serial.println("Escaneando puerto I2C...\n");

  for (address = 1; address < 127; address++) {
      Wire.beginTransmission(address);
      error = Wire.endTransmission();

      if (error == 0) {
          String mensaje = "I2C encontrado en la direccion 0x";
          if (address < 16)
              mensaje += "0";
          mensaje += String(address, HEX);
          mensaje += " !";
          Serial.println(mensaje);
          nDevices++;
      } else if (error == 4) {
          String mensaje = "Error desconocido en la direccion 0x";
          if (address < 16)
              mensaje += "0";
          mensaje += String(address, HEX);
          Serial.println(mensaje);
      }
      delay(5);
  }

  if (nDevices == 0) {
      Serial.println("Ningun dispositivo I2C encontrado");
  } else {
      Serial.println("\nEscaneo completado!");
  }
  return nDevices;
}

void ProcesarComando(String cmd) {
  if (cmd != "") {
    for (int i = 0; i < 2; i++) {
      digitalWrite(LED, HIGH);
      delay(250);
      digitalWrite(LED, LOW);
      delay(250);
    }

    if (cmd.startsWith("SCAN")) {
      Serial.println("Se encontraron " + String(scanI2C()) + " dispositivos I2C");

    } else if (cmd.startsWith("IMG")) {
      //* Ejemplo IMG>AXL10
      String IDImg = cmd.substring(4, 7);
      int tiempo = cmd.substring(7, 9).toInt();
      MostrarImagenID(IDImg.c_str(), tiempo);

    } else {
      Serial.println("Comando Desconocido");
    }
  }
}

void loop() {
  // Mostrar fecha y hora del DS3231 en la OLED
  now = rtc.now();

  char fecha[21]; // DD/MM/YYYY HH:MM:SS
  snprintf(fecha, sizeof(fecha), "%02d/%02d/%04d %02d:%02d:%02d",
            now.day(), now.month(), now.year(),
            now.hour(), now.minute(), now.second());

  display.clearDisplay();

  // Iconos 22x22 y texto
  const Imagen* iconoReloj = BuscarImagenID("RLJ");
  const Imagen* iconoCal = BuscarImagenID("CAL");

  const int ICON_W = 22;
  const int ICON_H = 22;

  // Línea superior: icono reloj 22x22 + hora grande
  int xIcono = 0;
  int yIcono = 0; // margen superior
  if (iconoReloj) {
    // Centrar el bitmap dentro de un contenedor 22x22 si la imagen es más pequeña
    int drawW = min(iconoReloj->Ancho, ICON_W);
    int drawH = min(iconoReloj->Alto, ICON_H);
    int offsetX = xIcono + (ICON_W - drawW) / 2;
    int offsetY = yIcono + (ICON_H - drawH) / 2;
    display.drawBitmap(offsetX, offsetY, iconoReloj->BitMap, drawW, drawH, SSD1306_WHITE);
    xIcono += ICON_W + 4; // separación
  }

  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(xIcono, 4);
  char hora[9]; // HH:MM:SS
  snprintf(hora, sizeof(hora), "%02d:%02d:%02d", now.hour(), now.minute(), now.second());
  display.println(hora);

  // Línea inferior: icono calendario 22x22 + fecha algo más grande
  int y2 = 34; // baja un poco para dejar aire con la hora
  int xIcono2 = 0;
  if (iconoCal) {
    int drawW2 = min(iconoCal->Ancho, ICON_W);
    int drawH2 = min(iconoCal->Alto, ICON_H);
    int offsetX2 = 0 + (ICON_W - drawW2) / 2;
    int offsetY2 = y2 + (ICON_H - drawH2) / 2;
    display.drawBitmap(offsetX2, offsetY2, iconoCal->BitMap, drawW2, drawH2, SSD1306_WHITE);
    xIcono2 = ICON_W + 4;
  }

  display.setTextSize(2); // fecha más grande que antes
  display.setCursor(xIcono2, y2 + 4);
  char fechaSolo[9]; // DD/MM/YY
  int yy = now.year() % 100;
  snprintf(fechaSolo, sizeof(fechaSolo), "%02d/%02d/%02d", now.day(), now.month(), yy);
  display.println(fechaSolo);

  display.display();

  // Atender comandos por Serial brevemente
  if (Serial.available()) {
    String comando = Serial.readStringUntil('\n');
    ProcesarComando(comando);
  }

  delay(200);
}
