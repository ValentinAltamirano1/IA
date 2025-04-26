#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>

// —————— Credenciales Wi-Fi ——————
const char* ssid     = "Valentin";
const char* password = "blaval02";

ESP8266WebServer server(80);

// —————— Variables globales ——————
String lastLuz  = "";
String lastHum  = "";
String lastTemp = "";
int    contador = 0;
bool   readyToLog = false;
String currentState = "Verde saludable";  // estado inicial

// —————— Página web embebida ——————
const char PAGE_STATE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="utf-8">
  <title>Maceta Inteligente</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    *{margin:0;padding:0;box-sizing:border-box;}
    body{font-family:'Segoe UI',sans-serif;background:linear-gradient(120deg,#e0ffe0,#f0fff0);padding:1rem;color:#333;}
    h1{text-align:center;margin-bottom:1rem;font-size:2.2rem;color:#2a6f2a;text-shadow:1px 1px 2px rgba(0,0,0,0.1);}
    .card{max-width:480px;margin:0 auto;background:#fff;border-radius:12px;box-shadow:0 4px 12px rgba(0,0,0,0.1);padding:1.5rem;}
    .status{display:flex;justify-content:space-between;align-items:center;margin-bottom:1rem;}
    .status span{font-weight:bold;font-size:1.1rem;color:#2a6f2a;}
    fieldset{border:1px solid #ddd;border-radius:8px;padding:1rem;margin-bottom:1rem;background:#f9fff9;}
    legend{font-weight:bold;color:#4f8f4f;}
    label{display:block;margin:0.5rem 0;font-size:0.95rem;}
    select,button{width:100%;padding:0.6rem;font-size:1rem;border-radius:6px;border:1px solid #ccc;margin-top:0.5rem;}
    select:focus,button:focus{outline:none;border-color:#2a6f2a;box-shadow:0 0 0 2px rgba(42,111,42,0.2);}
    button{background:#2a6f2a;color:#fff;cursor:pointer;transition:background 0.2s;margin-bottom:1rem;}
    button:hover{background:#3b8f3b;}
    .description{font-size:0.9rem;color:#555;margin-top:0.25rem;padding-left:0.5rem;border-left:3px solid #2a6f2a;background:#f0fff0;border-radius:4px;}
    a.csv-link{display:inline-block;text-decoration:none;color:#2a6f2a;font-size:0.9rem;margin-top:0.5rem;}
    a.csv-link:hover{text-decoration:underline;}
  </style>
</head>
<body>
  <h1>Maceta Inteligente</h1>
  <div class="card">
    <div class="status">
      <div>
        <label>Estado actual:</label>
        <span id="estadoTxt">–</span>
      </div>
      <button onclick="toggleForm()">Cambiar Estado</button>
    </div>
    <fieldset>
      <legend>Última lectura</legend>
      <label>Luz: <span id="lux">–</span></label>
      <label>Humedad: <span id="hum">–</span></label>
      <label>Temp.: <span id="temp">–</span></label>
    </fieldset>
    <form id="stateForm" style="display:none;" onsubmit="cambiarEstado(event)">
      <label for="estadoSel">Selecciona nueva condición:</label>
      <select id="estadoSel" required>
        <option value="Verde saludable">Verde saludable</option>
        <option value="Marchitez">Marchitez</option>
        <option value="Pudrición">Pudrición</option>
        <option value="Clorosis">Clorosis</option>
        <option value="Quemaduras">Quemaduras</option>
      </select>
      <div class="description" id="desc">Selecciona una opción para ver su descripción.</div>
      <button type="submit">Guardar</button>
    </form>
    <a class="csv-link" href="/registro.csv" target="_blank">📁 Ver histórico completo</a>
  </div>
  <script>
    const descriptions = {
      "Verde saludable":"Hojas firmes, sin manchas.",
      "Marchitez":"Hojas flácidas, colgantes.",
      "Pudrición":"Tallos o base blanda, hojas amarillas.",
      "Clorosis":"Hojas amarillas (sin flacidez).",
      "Quemaduras":"Manchas marrones/crujientes en bordes."
    };
    async function cargar(){
      let r = await fetch('/status');
      let d = await r.json();
      document.getElementById('lux').innerText       = d.lux  || '–';
      document.getElementById('hum').innerText       = d.hum  || '–';
      document.getElementById('temp').innerText      = d.temp || '–';
      document.getElementById('estadoTxt').innerText = d.estado|| '–';
      document.getElementById('estadoSel').value     = d.estado|| 'Verde saludable';
      updateDesc();
    }
    function toggleForm(){
      const f = document.getElementById('stateForm');
      f.style.display = f.style.display==='none'?'block':'none';
    }
    function updateDesc(){
      document.getElementById('desc').innerText =
        descriptions[document.getElementById('estadoSel').value];
    }
    document.getElementById('estadoSel')
            .addEventListener('change', updateDesc);
    async function cambiarEstado(e){
      e.preventDefault();
      const nuevo = document.getElementById('estadoSel').value;
      await fetch('/setState',{
        method:'POST',
        headers:{'Content-Type':'application/x-www-form-urlencoded'},
        body:'estado='+encodeURIComponent(nuevo)
      });
      toggleForm();
      cargar();
    }
    setInterval(cargar,8000);
    cargar();
  </script>
</body>
</html>
)rawliteral";

// —————— Handlers y utilidades ——————
void handleStatus(){
  String j = "{\"lux\":\""+lastLuz
           +"\",\"hum\":\""+lastHum
           +"\",\"temp\":\""+lastTemp
           +"\",\"estado\":\""+currentState+"\"}";
  server.send(200,"application/json",j);
}

void handleSetState(){
  if(!server.hasArg("estado")){
    server.send(400,"text/plain","Falta parametro estado");
    return;
  }
  currentState = server.arg("estado");
  server.send(200,"text/plain","Estado cambiado a "+currentState);
}

String getFechaHora(){
  struct tm ti;
  if(!getLocalTime(&ti)) return "1970-01-01 00:00:00";
  char buf[20];
  strftime(buf,sizeof(buf),"%Y-%m-%d %H:%M:%S",&ti);
  return String(buf);
}

void logToCSV(){
  String ts = getFechaHora();
  File f = SPIFFS.open("/registro.csv","a");
  f.printf("%s,%s,%s,%s,%s\n",
           lastLuz.c_str(),
           lastHum.c_str(),
           lastTemp.c_str(),
           currentState.c_str(),
           ts.c_str());
  f.close();
}

void setup(){
  Serial.begin(9600);
  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED) delay(200);

  configTime(-3*3600,0,"pool.ntp.org");
  SPIFFS.begin();

  if(!SPIFFS.exists("/registro.csv")){
    File f = SPIFFS.open("/registro.csv","w");
    f.println("luz,humedad,temperatura,estado,fecha_hora");
    f.close();
  }

  server.on("/",         HTTP_GET,  [](){ server.send_P(200,"text/html",PAGE_STATE); });
  server.on("/status",   HTTP_GET,  handleStatus);
  server.on("/setState", HTTP_POST, handleSetState);
  server.on("/registro.csv", HTTP_GET, [](){
    File f = SPIFFS.open("/registro.csv","r");
    server.streamFile(f,"text/csv");
    f.close();
  });

  server.begin();
}

void loop(){
  if(Serial.available()){
    String l = Serial.readStringUntil('\n');
    l.trim();
    if(contador==0){
      lastLuz = l;  // primer println() → brillo
      contador++;
    } else if(contador==1){
      lastHum = l;  // segundo → humedad
      contador++;
    } else if(contador==2){
      lastTemp = l; // tercero → temperatura
      contador++;
    }
    if(contador==3){
      readyToLog = true;
      contador = 0;
    }
  }

  if(readyToLog){
    logToCSV();
    readyToLog = false;
  }

  server.handleClient();
}
