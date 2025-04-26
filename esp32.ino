#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>

// —————— Tu Wi-Fi aquí ——————
const char* ssid     = "Valentin";
const char* password = "blaval02";

ESP8266WebServer server(80);

// Almacenar lecturas temporales
String lastLuz, lastHum, lastTemp;
int    contador = 0;
bool   readyToLog = false;

// Estado actual (inicia “saludable”)
String currentState = "saludable";

void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) delay(200);

  configTime(-3*3600, 0, "pool.ntp.org");
  SPIFFS.begin();

  // Si no existe, crea CSV con cabecera
  if(!SPIFFS.exists("/registro.csv")) {
    File f = SPIFFS.open("/registro.csv","w");
    f.println("luz,humedad,temperatura,estado,fecha_hora");
    f.close();
  }

  // Rutas web
  server.serveStatic("/","SPIFFS","/state.html");
  server.on("/status",  HTTP_GET,  handleStatus);
  server.on("/setState",HTTP_POST, handleSetState);
  server.serveStatic("/registro.csv","SPIFFS","/registro.csv");

  server.begin();
}

// Recibe por Serial las 3 líneas y marca listo para grabar
void loop() {
  if(Serial.available()) {
    String linea = Serial.readStringUntil('\n'); linea.trim();
    if(linea.startsWith("Valor luz:")) {
      lastLuz = linea.substring(11); contador++;
    }
    else if(linea.startsWith("Valor humedad:")) {
      lastHum = linea.substring(15); contador++;
    }
    else if(linea.startsWith("Temperatura:")) {
      lastTemp = linea.substring(13, linea.indexOf(" °")); contador++;
    }
    if(contador==3) {
      readyToLog = true;
      contador = 0;
    }
  }

  // En cuanto esté lista la muestra, la graba usando currentState
  if(readyToLog) {
    logToCSV();
    readyToLog = false;
  }

  server.handleClient();
}

void logToCSV() {
  String ts = getFechaHora();
  File f = SPIFFS.open("/registro.csv","a");
  f.printf("%s,%s,%s,%s,%s\n",
           lastLuz.c_str(),
           lastHum.c_str(),
           lastTemp.c_str(),
           currentState.c_str(),
           ts.c_str());
  f.close();
  Serial.printf("👉 %s,%s,%s,%s,%s\n",
    lastLuz.c_str(), lastHum.c_str(),
    lastTemp.c_str(), currentState.c_str(),
    ts.c_str());
}

// Devuelve JSON con últimas 3 lecturas + estado actual
void handleStatus() {
  if(!readyToLog) {
    // aunque no haya nueva muestra, devolvemos el último estado
  }
  String json = "{";
  json += "\"lux\":\""+lastLuz+"\","; 
  json += "\"hum\":\""+lastHum+"\","; 
  json += "\"temp\":\""+lastTemp+"\","; 
  json += "\"estado\":\""+currentState+"\"}";
  server.send(200,"application/json",json);
}

// Actualiza currentState desde el formulario
void handleSetState() {
  if(!server.hasArg("estado")) {
    server.send(400,"text/plain","Falta parámetro estado");
    return;
  }
  currentState = server.arg("estado");
  server.send(200,"text/plain","Estado cambiado a "+currentState);
}

// Obtiene fecha/hora formateada
String getFechaHora() {
  struct tm ti;
  if(!getLocalTime(&ti)) return "1970-01-01 00:00:00";
  char buf[20];
  strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",&ti);
  return String(buf);
}
