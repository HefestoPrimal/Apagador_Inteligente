#ifndef IMAGENES_H
  #define IMAGENES_H
  #include <Arduino.h>

  #define Reloj_Width 22
  #define Reloj_Height 22
  const unsigned char Reloj [] PROGMEM = {
    // 'Reloj, 22x22px
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x03, 0x03, 0x00, 0x04, 0x00, 0x80, 0x08, 
    0x00, 0x40, 0x10, 0x30, 0x20, 0x10, 0x30, 0x20, 0x20, 0x30, 0x10, 0x20, 0x30, 0x10, 0x23, 0xf0, 
    0x10, 0x23, 0xe0, 0x10, 0x20, 0x00, 0x10, 0x20, 0x00, 0x10, 0x10, 0x00, 0x20, 0x10, 0x00, 0x20, 
    0x08, 0x00, 0x40, 0x04, 0x00, 0x80, 0x03, 0x03, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 
    0x00, 0x00
  };

  #define Calendario_Width 22
  #define Calendario_Height 22
  const unsigned char Calendario [] PROGMEM = {
    // 'Calendario, 22x22px
    0x08, 0x00, 0x40, 0x1c, 0x00, 0xe0, 0x7f, 0xff, 0xf8, 0xfc, 0x00, 0xfc, 0xc8, 0x00, 0x4c, 0xc0, 
    0x00, 0x0c, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xfc, 0xc0, 0x00, 0x0c, 0xc0, 0x00, 0x0c, 0xd9, 0x31, 
    0x84, 0xc9, 0x07, 0xf0, 0xc0, 0x0e, 0x38, 0xc9, 0x18, 0x98, 0xd9, 0x19, 0x8c, 0xc0, 0x31, 0xcc, 
    0xc0, 0x31, 0xcc, 0xd9, 0x10, 0x0c, 0xc0, 0x18, 0x0c, 0xc0, 0x1c, 0x18, 0xff, 0xcf, 0xf0, 0x7f, 
    0xe3, 0xe0
  };

#endif

/*
* Instrucciones para añadir una imagen a la pantalla:
1. Seleccionar una imagen (Monocromática)
2. Retirar el fondo y recortarla unicamente al tamaño de la misma imagen
3. Redimensionar la imagen (No más de 128x64)
4. Convertir la imagen a mapa de bits (8 bits por pixel) cambiar bits en byte

Las paginas recomendadas en este orden son:
Retirar fondo -> https://www.remove.bg/es
Redimensionar la imagen -> https://www.iloveimg.com/es/redimensionar-imagen
Convertir a mapa de bits -> https://javl.github.io/image2cpp/

* Configuracion para convertir en mapa de bits:
1.1) Subir la imagen sin fondo y ya redimensionada
2.1) La imagen debe ajustarse para que se vea blanca en un fondo negro (Invertir colores de ser necesario)
2.2) Ajustar el contraste (0-255) a gusto del usuario
2.3) Escalar a tamaño original
2.4) Voltear la imagen hacia una determinada orientación si así se desea
3.1) Visualizar la imagen para ajustar el contraste de ser necesario
4.1) Configurar formato de salida como Codigo de Arduino, solo mapa de bits
4.2) Colocarle un identificador a la imagen (Sin espacios)
4.3) Colocar el modo de dibujo como Horizontal dibujando 1 bit por pixel
4.4) Cambiar bits por bytes
4.5) Copiar el codigo resultante y agregarle constantes de ancho y alto de la imagen. Ejemplo de resultado en el codigo:

#define SpiderManLogo_Width 45
#define SpiderManLogo_Height 60
    const unsigned char SpiderManLogo [] PROGMEM = {
        'Codigos Hexadecimales del Resultado'
    };
*/