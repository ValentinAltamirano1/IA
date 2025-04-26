#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>

const char* ssid = "Valentin";
const char* password = "blaval02";

String luz = "";
String humedad = "";
String temperatura = "";
String condicion = "";
int contador = 0;

ESP8266WebServer server(80);

void setup() {
  Serial.begin(9600);
  Serial.setTimeout(2000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }

  configTime(-3 * 3600, 0, "pool.ntp.org");

  if (!SPIFFS.begin()) {
    Serial.println("Error montando SPIFFS");
    return;
  }

  if (!SPIFFS.exists("/registro.csv")) {
    File f = SPIFFS.open("/registro.csv", "w");
    f.println("luz,humedad,temperatura,condicion,fecha_hora");
    f.close();
  }

  server.on("/", handleRoot);
  server.on("/registro.csv", handleCSV);
  server.begin();

  Serial.println();
  Serial.println("Conectado a WiFi.");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP()); 
  Serial.println("ESP8266 listo y esperando datos...");
}

void loop() {
  if (Serial.available()) {
    String linea = Serial.readStringUntil('\n');
    linea.trim();

    if (linea.startsWith("Valor luz:")) {
      luz = linea.substring(11, linea.indexOf(" -"));
      contador++;
    } else if (linea.startsWith("Valor humedad:")) {
      humedad = linea.substring(15);
      contador++;
    } else if (linea.startsWith("Temperatura:")) {
      temperatura = linea.substring(13, linea.indexOf(" °"));
      contador++;
    } else if (linea.startsWith("Condicion:")) {
      condicion = linea.substring(11);
      contador++;
    }

    if (contador == 4) {
      String fechaHora = getFechaHora();
      File archivo = SPIFFS.open("/registro.csv", "a");
      if (archivo) {
        archivo.print(luz);
        archivo.print(",");
        archivo.print(humedad);
        archivo.print(",");
        archivo.print(temperatura);
        archivo.print(",");
        archivo.print(condicion);
        archivo.print(",");
        archivo.println(fechaHora);
        archivo.close();
        Serial.println("📁 Guardado en CSV:");
        Serial.println(luz + "," + humedad + "," + temperatura + "," + condicion + "," + fechaHora);
      }
      // Reiniciar para próxima lectura
      contador = 0;
      luz = "";
      humedad = "";
      temperatura = "";
      condicion = "";
    }
  }
  server.handleClient();
}

String getFechaHora() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "sin_fecha";
  char buffer[30];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

void handleRoot() {
  server.send(200, "text/html", "<h1>ESP8266 activo</h1><a href='/registro.csv'>Ver CSV</a>");
}

void handleCSV() {
  if (!SPIFFS.exists("/registro.csv")) {
    server.send(404, "text/plain", "Archivo no encontrado");
    return;
  }

  File f = SPIFFS.open("/registro.csv", "r");
  server.streamFile(f, "text/csv");
  f.close();
}
