# Apagador Inteligente

## Descripción
En esta rama del proyecto se encuentra la primera parte para comprenderlo en su totalidad. Cuenta con los pines definidos para la **conexión I2C** necesaria para la comunicación con nuestro **reloj en tiempo real** y **pantalla OLED** para mostrar la fecha y hora en la pantalla, datos directamente extraídos del reloj en tiempo real

## Requisitos
- VSCode con extensión Platformio
- ESP32-C3 Supermini
- Pantalla OLED con comunicación I2C
- Diodo LED
- Reloj de tiempo real DS3231
- Resistencia 220 ohms
- Protoboard
- Cables de Conexión

## Pinouts, circuitos y pines
| ESP32-C3 | Descripción | Componente externo

**GPIO9** | PIN_SCL | SCL (OLED) & SCL (RTC)

**GPIO8** | PIN_SDA | SDA (OLED) & SDA (RTC)

**GPIO20** | LED | Resistencia 220 -> GND

#### Pinout ESP32-C3 Supermini
![Pinout ESP32-C3 Supermini](images/ESP32-C3-Super-Mini-pinout-low.jpg)

#### Pinout RTC DS3231
![Pinout RTC DS3231](images\RTC_DS3231.jpg)

#### Pinout Display OLED
![Pinout Display OLED](images\Display_OLED_I2C.jpg)

## Instalación
Clona el repositorio público en la carpeta en que desees guardarlo para su uso y prueba, una vez terminado el proceso compila el proyecto y cárgalo a tu ESP32 con los pines especificados y los componentes conectados y alimentados (Si tienes otro modelo de ESP32 distinto al del proyecto especifícalo en *"platformio.ini"* y modifica los pines si así lo requieren)

## Uso
Una vez cargado el programa después de unos segundos te debe aparecer la fecha y hora que tenga tu reloj en tiempo real y se actualizará cada segundo pasado, si se des energiza el proyecto y después de un tiempo se vuelve a conectar, se puede apreciar que la hora sigue según el tiempo transcurrido, esto debido a que el reloj en tiempo real conserva la hora gracias a su batería externa.

## Contribución
Para contribuir en el proyecto vean los vídeos del canal para una mayor comprensión y coloque sus sugerencias en los comentarios del [vídeo de YouTube](https://youtu.be/rIrncidcMnk) para tomar en cuenta la mejora. A su vez se es libre de crear un fork del proyecto y hacer sus propias modificaciones. Por favor, mantenga el nombre del autor y el repositorio original en los comentarios de su código.

#### ¡Mucho éxito! :D
___