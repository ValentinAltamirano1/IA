#include <FS.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <time.h>

time_t currentTime = time(nullptr);

// —————— Credenciales Wi-Fi ——————
const char* ssid     = "Valentin";
const char* password = "blaval02";

ESP8266WebServer server(80);

// —————— Variables globales ——————
String lastLuz  = "";
String lastHum  = "";
String lastTemp = "";
float  prevLuz  = 0, prevHum = 0, prevTemp = 0;
time_t lastWateringTime;
int    contador = 0;
bool   readyToLog = false;
String currentState = "Verde saludable";
String predictedState = "Verde saludable";
bool flagRegado = false;

const char* etiquetasEstado[] = {
  "Marchitez",
  "Clorosis",
  "Verde saludable",
  "Quemaduras"
};

int predecirEstado(
  float luz,
  float humedad,
  float temperatura,
  float luz_diff,
  float humedad_diff,
  float temperatura_diff,
  float hour,
  float weekday,
  float hours_since_watering
) {
  float score_0 = 8.708296 
    + 0.002660*luz 
    - 0.536925*humedad 
    + 0.960294*temperatura 
    - 0.126929*luz_diff 
    + 0.017410*humedad_diff 
    - 0.005553*temperatura_diff 
    - 0.032095*hour 
    + 0.452294*weekday 
    - 0.157082*hours_since_watering;
 
  float score_1 = 8.358092 
    - 0.009844*luz 
    + 1.184317*humedad 
    + 0.149923*temperatura 
    + 0.005814*luz_diff 
    + 0.020343*humedad_diff 
    + 0.022835*temperatura_diff 
    - 0.002927*hour 
    + 0.303541*weekday 
    + 1.149068*hours_since_watering;
 
  float score_2 = -7.999702 
    + 0.962015*luz 
    + 0.410668*humedad 
    + 2.158881*temperatura 
    - 0.172012*luz_diff 
    + 0.005641*humedad_diff 
    - 0.026913*temperatura_diff 
    + 0.202481*hour 
    - 1.811874*weekday 
    - 9.395823*hours_since_watering;
 
  float score_3 = -9.066686 
    - 0.954831*luz 
    - 1.058061*humedad 
    - 3.269098*temperatura 
    + 0.293128*luz_diff 
    - 0.043394*humedad_diff 
    + 0.009630*temperatura_diff 
    - 0.167459*hour 
    + 1.056039*weekday 
    + 8.403837*hours_since_watering;
 
  float best_score = score_0;
  int   best_class = 0;
  if (score_1 > best_score) { best_score = score_1; best_class = 1; }
  if (score_2 > best_score) { best_score = score_2; best_class = 2; }
  if (score_3 > best_score) { best_score = score_3; best_class = 3; }
  return best_class;
}

// ——————  Web embebida ——————
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
        <span id="estadoManual">–</span>
        <label>Estado predicho:</label>
        <span id="estadoPredicho">–</span>
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
    <button onclick="regadoManual()">💧 Marcar riego</button>
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
      document.getElementById('lux').innerText           = d.lux  || '–';
      document.getElementById('hum').innerText           = d.hum  || '–';
      document.getElementById('temp').innerText          = d.temp || '–';
      document.getElementById('estadoManual').innerText  = d.estado_manual || '–';
      document.getElementById('estadoPredicho').innerText= d.estado_predicho || '–';
      document.getElementById('estadoSel').value         = d.estado_manual || 'Verde saludable';
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
    async function regadoManual(){
    await fetch('/regar', { method:'POST' });
    alert('¡Riego registrado!');}
  </script>
</body>
</html>
)rawliteral";

void handleStatus(){
  String j = "{\"lux\":\""+lastLuz
           +"\",\"hum\":\""+lastHum
           +"\",\"temp\":\""+lastTemp
           +"\",\"estado_manual\":\""+currentState
           +"\",\"estado_predicho\":\""+predictedState+"\"}";
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

void logToCSV(bool flagRegado){
  String ts = getFechaHora();
  File f = SPIFFS.open("/registro.csv","a");
  f.printf("%s,%s,%s,%s,%s,%s\n",
           lastLuz.c_str(),
           lastHum.c_str(),
           lastTemp.c_str(),
           currentState.c_str(),
           ts.c_str(),
           flagRegado ? "true" : "false");
  f.close();
}

float escalar(float valor, float media, float std_dev) {
  return (valor - media) / std_dev;
}

void setup(){
  Serial.begin(9600);
  WiFi.begin(ssid,password);
  while(WiFi.status()!=WL_CONNECTED) delay(200);

  configTime(-3*3600,0,"pool.ntp.org");
  SPIFFS.begin();

  // Asumimos un riego inicial 24h antes
  lastWateringTime = time(nullptr) - 24*3600;

  if(!SPIFFS.exists("/registro.csv")){
    File f = SPIFFS.open("/registro.csv","w");
    f.println("luz,humedad,temperatura,estado,fecha_hora,regado");
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
  server.on("/regar", HTTP_POST, [](){
    lastWateringTime = time(nullptr);
    flagRegado = true;
    server.send(200, "text/plain", "Riego registrado");
  });

  server.begin();
}

void loop() {
  if (Serial.available()) {
    String linea = Serial.readStringUntil('\n');
    linea.trim();

    if (linea.startsWith("Valor luz:")) {
      lastLuz = linea.substring(11, linea.indexOf(" -"));
      contador++;
    } else if (linea.startsWith("Valor humedad:")) {
      lastHum = linea.substring(15);
      contador++;
    } else if (linea.startsWith("Temperatura:")) {
      lastTemp = linea.substring(13, linea.indexOf(" °"));
      contador++;
    } else if (linea.startsWith("Condicion:")) {
      // no hacer nada pq sino pisa con lo que viene del arduino
      contador++;
    }
  }

  if (contador == 4) {
    readyToLog = true;
    contador = 0;
  }

  if (readyToLog) {
    float l = lastLuz.toFloat();
    float h = lastHum.toFloat();
    float t = lastTemp.toFloat();
    float l_diff = l - prevLuz; prevLuz = l;
    float h_diff = h - prevHum; prevHum = h;
    float t_diff = t - prevTemp; prevTemp = t;
 
    struct tm ti;
    getLocalTime(&ti);
    float hour    = ti.tm_hour + ti.tm_min/60.0;
    float weekday = ti.tm_wday;
 
    // tiempo desde último riego
    float hrs_w = (time(nullptr) - lastWateringTime) / 3600.0;
 
    // inferir estado
    float l_scaled       = escalar(l,       601.8862, 378.1443);
    float h_scaled       = escalar(h,       466.2933, 280.7134);
    float t_scaled       = escalar(t,       18.2483,   3.1967);
    float l_diff_scaled  = escalar(l_diff,   0.0564,  46.3598);
    float h_diff_scaled  = escalar(h_diff,  -0.0742,  13.0143);
    float t_diff_scaled  = escalar(t_diff,   0.0006,   0.2808);
    float hour_scaled    = escalar(hour,    11.4249,   6.8721);
    float weekday_scaled = escalar(weekday,  3.0120,   1.9306);
    float hrsw_scaled    = escalar(hrs_w,  472.0892, 260.2472);
    
    // Inferir estado con datos escalados
    int code = predecirEstado(
      l_scaled, h_scaled, t_scaled,
      l_diff_scaled, h_diff_scaled, t_diff_scaled,
      hour_scaled, weekday_scaled, hrsw_scaled
    );
    predictedState = etiquetasEstado[code];
    Serial.printf("Predicción: %s | Luz: %s | Hum: %s | Temp: %s | Hora: %.2f | Desde riego: %.2f hrs\n",
      currentState.c_str(), lastLuz.c_str(), lastHum.c_str(), lastTemp.c_str(), hour, hrs_w);
 
    logToCSV(flagRegado);
    flagRegado = false;
    readyToLog = false;
  }

  server.handleClient();
}