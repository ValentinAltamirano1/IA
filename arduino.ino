#include <math.h>

const int sensor_luz     = A0;    // Fotocelda
const int sensor_humedad = A1;    // Sensor de humedad
const int sensor_temp    = A2;    // Termistor (NTC)
const int led_rojo       = 8;

int valor_luz;
int brillo_led;
unsigned long tiempoPrevio   = 0;
const unsigned long intervalo = 600000; // 10 min

void setup() {
  pinMode(sensor_luz, INPUT);
  pinMode(sensor_humedad, INPUT);
  pinMode(sensor_temp, INPUT);
  pinMode(led_rojo, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  if (millis() - tiempoPrevio >= intervalo) {
    tiempoPrevio = millis();

    // ===== LUZ (brillo del LED) =====
    valor_luz  = 1023 - analogRead(sensor_luz);
    brillo_led = map(valor_luz, 0, 1023, 0, 255);
    analogWrite(led_rojo, brillo_led);
    Serial.println(brillo_led);      // <<-- sólo brillo

    // ===== HUMEDAD =====
    int humedad = analogRead(sensor_humedad);
    Serial.println(humedad);

    // ===== TEMPERATURA (NTC) =====
    int adcValue   = analogRead(sensor_temp);
    double voltage = (adcValue / 1023.0) * 5.0;
    double resistance = (5.0 - voltage) / voltage * 4700.0;
    double tempC = 1.0 / (log(resistance / 10000.0) / 3950.0
                   + 1.0 / (25.0 + 273.15)) - 273.15;
    Serial.println(tempC);           // <<-- sólo valor numérico

    // Línea en blanco para separar bloques
    Serial.println();
  }
}
