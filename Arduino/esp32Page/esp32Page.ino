#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>

// Credentials go here
const char* WIFI_SSID = "";
const char* WIFI_PASS = "";

WebServer server(80);


//  Current state
struct TelemetryState {
  bool armed        = false;
  int  battery      = 100;    // %
  float altitude    = 0.0f;   // meters
  unsigned long lastUpdateMs = 0; // timestamp of last received telemetry
  bool batteryLow   = false;
} state;


//  History ring buffer
static const int HISTORY_N = 60;

struct HistoryPoint {
  unsigned long tMs;
  bool  armed;
  int   battery;
  float altitude;
};

HistoryPoint history[HISTORY_N];
int historyCount = 0;
int historyHead  = 0;

void pushHistory(const TelemetryState& s) {
  history[historyHead] = { millis(), s.armed, s.battery, s.altitude };
  historyHead = (historyHead + 1) % HISTORY_N;
  if (historyCount < HISTORY_N) historyCount++;
}

//  Event log
static const int EVENT_N = 20;

String events[EVENT_N];
int eventCount = 0;
int eventHead  = 0;

void pushEvent(const String& msg) {
  events[eventHead] = msg;
  eventHead = (eventHead + 1) % EVENT_N;
  if (eventCount < EVENT_N) eventCount++;
}

// HTML dashboard
const char DASHBOARD_HTML[] PROGMEM = R"=="==(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width,initial-scale=1" />
  <title>Drone Dashboard</title>
  <style>
    body { font-family: system-ui, Arial; margin: 24px; color:#111; }
    .grid { display:grid; grid-template-columns: 1fr; gap: 14px; max-width: 860px; }
    .card { border: 1px solid #ddd; border-radius: 14px; padding: 16px; }
    .row  { display:flex; justify-content:space-between; align-items:center; margin: 10px 0; }
    .badge { padding: 6px 10px; border-radius: 999px; font-weight: 700; font-size: 14px; }
    .ok   { background: #e9f8ee; }
    .no   { background: #fdecec; }
    .warn { background: #fff4d6; }
    progress { width: 100%; height: 18px; }
    small { color: #666; }
    .title { display:flex; justify-content:space-between; align-items:baseline; gap:12px; }
    .muted { color: #666; }
    ul { margin: 0; padding-left: 18px; }
    canvas { width: 100%; height: 220px; }
    .pill { font-size: 12px; padding: 4px 8px; border-radius: 999px; border:1px solid #eee; }
    button {
      border: 1px solid #ddd; background: #fafafa;
      padding: 8px 12px; border-radius: 10px;
      cursor: pointer; font-weight: 600;
    }
    button:hover { background: #f2f2f2; }
  </style>
</head>
<body>
  <div class="grid">

    <!-- Main status card -->
    <div class="card">
      <div class="title">
        <h2 style="margin:0">Drone Dashboard</h2>
        <span id="health" class="pill">Health: --</span>
      </div>

      <div class="row">
        <div>Status</div>
        <div id="status" class="badge">--</div>
      </div>

      <div class="row">
        <div>Battery</div>
        <div><span id="bat">--</span>%</div>
      </div>
      <progress id="batbar" value="0" max="100"></progress>

      <div id="batteryAlert" class="row" style="display:none;">
        <div class="badge warn">⚠ Battery low</div>
        <div class="muted">Charge soon</div>
      </div>

      <div class="row">
        <div>Altitude</div>
        <div><span id="alt">--</span> m</div>
      </div>

      <div class="row">
        <div>Last update</div>
        <div><span id="ago">--</span></div>
      </div>

      <div class="row" style="gap:10px; justify-content:flex-start;">
        <button onclick="window.location='/download.csv'">Download CSV</button>
        <button onclick="window.location='/download.json'">Download JSON</button>
      </div>

      <small>ESP32 serves this page. Unity POSTs to <code>/telemetry</code>. State via <code>/state</code>, history via <code>/history</code>.</small>
    </div>

    <!-- Battery chart card -->
    <div class="card">
      <h3 style="margin:0 0 10px 0">Battery history</h3>
      <canvas id="chart"></canvas>
    </div>

    <!-- Event log card -->
    <div class="card">
      <h3 style="margin:0 0 10px 0">Event log (latest first)</h3>
      <ul id="events"><li class="muted">No events yet.</li></ul>
    </div>

  </div>

  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
  <script>
    let chart;

    // Creates the battery chart on first use
    function ensureChart() {
      const ctx = document.getElementById('chart').getContext('2d');
      chart = new Chart(ctx, {
        type: 'line',
        data: {
          labels: [],
          datasets: [{ label: 'Battery (%)', data: [] }]
        },
        options: {
          responsive: true,
          animation: false,
          scales: { y: { beginAtZero: true, max: 100 } }
        }
      });
    }

    // Updates the small health pill in the top right
    function setHealth(ok, msg) {
      const h = document.getElementById('health');
      h.textContent = "Health: " + msg;
      h.style.borderColor = ok ? "#cfe9d6" : "#f2c7c7";
      h.style.background   = ok ? "#f4fff7" : "#fff6f6";
    }

    // Fetches from the three ESP32 endpoints
    async function loadState()   { return (await fetch('/state',   {cache:'no-store'})).json(); }
    async function loadHistory() { return (await fetch('/history', {cache:'no-store'})).json(); }
    async function loadEvents()  { return (await fetch('/events',  {cache:'no-store'})).json(); }

    // Renders the main status card
    function renderState(s) {
      const st = document.getElementById('status');
      st.textContent = s.armed ? 'LETI (ARMED)' : 'NE LETI (DISARMED)';
      st.className = 'badge ' + (s.armed ? 'ok' : 'no');

      document.getElementById('bat').textContent    = s.battery;
      document.getElementById('batbar').value       = s.battery;
      document.getElementById('alt').textContent    = (s.altitude ?? 0).toFixed(2);
      document.getElementById('batteryAlert').style.display = s.batteryLow ? 'flex' : 'none';

      // "X seconds ago" — only works if Unity sends clientNowMs
      if (s.lastUpdateMs) {
        const sec = Math.round((Date.now() - s.lastUpdateMs) / 1000);
        document.getElementById('ago').textContent = sec + "s ago";
        setHealth(sec <= 3, sec <= 3 ? "OK" : "STALE");
      } else {
        document.getElementById('ago').textContent = "--";
        setHealth(false, "NO DATA");
      }
    }

    // Renders the event log list
    function renderEvents(arr) {
      const ul = document.getElementById('events');
      ul.innerHTML = arr.length
        ? arr.map(e => `<li>${e}</li>`).join('')
        : '<li class="muted">No events yet.</li>';
    }

    // Renders the battery chart
    function renderHistory(hist) {
      if (!chart) ensureChart();
      chart.data.labels           = hist.map(p => Math.round(p.t / 1000) + "s");
      chart.data.datasets[0].data = hist.map(p => p.battery);
      chart.update();
    }

    // Main loop — fetches all three endpoints every second
    async function tick() {
      try {
        renderState(await loadState());
        renderHistory(await loadHistory());
        renderEvents(await loadEvents());
      } catch (e) {
        setHealth(false, "ERROR");
      }
    }

    tick();
    setInterval(tick, 1000);
  </script>
</body>
</html>
)=="==";

//  Route handlers -> Each function handles one endpoint 

// GET / — serves the dashboard webpage
void handleRoot() {
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

// GET /state — returns current drone state as JSON
void handleState() {
  StaticJsonDocument<256> doc;
  doc["armed"]        = state.armed;
  doc["battery"]      = state.battery;
  doc["altitude"]     = state.altitude;
  doc["batteryLow"]   = state.batteryLow;
  doc["lastUpdateMs"] = state.lastUpdateMs;

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// GET /history — returns last 60 telemetry points as JSON array (oldest first)
void handleHistory() {
  StaticJsonDocument<4096> doc;
  JsonArray arr = doc.to<JsonArray>();

  int start = (historyHead - historyCount + HISTORY_N) % HISTORY_N;
  for (int i = 0; i < historyCount; i++) {
    int idx = (start + i) % HISTORY_N;
    JsonObject o = arr.createNestedObject();
    o["t"]        = history[idx].tMs;
    o["armed"]    = history[idx].armed;
    o["battery"]  = history[idx].battery;
    o["altitude"] = history[idx].altitude;
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// GET /events — returns last 20 events as JSON array (newest first)
void handleEvents() {
  StaticJsonDocument<2048> doc;
  JsonArray arr = doc.to<JsonArray>();

  for (int i = 0; i < eventCount; i++) {
    int idx = (eventHead - 1 - i + EVENT_N) % EVENT_N;
    arr.add(events[idx]);
  }

  String out;
  serializeJson(doc, out);
  server.send(200, "application/json", out);
}

// POST /telemetry — receives drone data from Unity
void handleTelemetry() {
  if (!server.hasArg("plain")) {
    server.send(400, "text/plain", "Missing JSON body");
    return;
  }

  StaticJsonDocument<256> doc;
  if (deserializeJson(doc, server.arg("plain"))) {
    server.send(400, "text/plain", "Invalid JSON");
    return;
  }

  bool prevArmed   = state.armed;
  int  prevBattery = state.battery;

  if (doc.containsKey("armed"))    state.armed    = doc["armed"].as<bool>();
  if (doc.containsKey("battery"))  state.battery  = doc["battery"].as<int>();
  if (doc.containsKey("altitude")) state.altitude = doc["altitude"].as<float>();

  state.batteryLow   = (state.battery <= 20);
  state.lastUpdateMs = doc.containsKey("clientNowMs")
                       ? doc["clientNowMs"].as<unsigned long>()
                       : 0;

  pushHistory(state);

  // Generating events for arm/disarm and low battery
  if (state.armed != prevArmed)
    pushEvent(state.armed ? "ARMED" : "DISARMED");
  if (state.batteryLow && prevBattery > 20)
    pushEvent("Battery LOW (<=20%)");

  server.send(200, "text/plain", "OK");
}

// POST working w Unity
void handleEvent() {
  if (!server.hasArg("plain")) { server.send(400, "text/plain", "Missing body"); return; }
  StaticJsonDocument<128> doc;
  deserializeJson(doc, server.arg("plain"));
  if (doc.containsKey("message")) pushEvent(doc["message"].as<String>());
  server.send(200, "text/plain", "OK");
}

// GET /download.csv — downloads as a file
void handleDownloadCSV() {
  String out = "t_ms,armed,battery,altitude\n";
  int start = (historyHead - historyCount + HISTORY_N) % HISTORY_N;

  for (int i = 0; i < historyCount; i++) {
    int idx = (start + i) % HISTORY_N;
    out += String(history[idx].tMs)             + "," +
           String(history[idx].armed ? 1 : 0)   + "," +
           String(history[idx].battery)          + "," +
           String(history[idx].altitude, 2)      + "\n";
  }

  server.sendHeader("Content-Disposition", "attachment; filename=\"telemetry.csv\"");
  server.send(200, "text/csv", out);
}

// GET /download.json — downloads everything (state + history + events) as one JSON file
void handleDownloadJSON() {
  StaticJsonDocument<4096> doc;
  JsonObject root = doc.to<JsonObject>();

  JsonObject cur = root.createNestedObject("current");
  cur["armed"]        = state.armed;
  cur["battery"]      = state.battery;
  cur["altitude"]     = state.altitude;
  cur["batteryLow"]   = state.batteryLow;
  cur["lastUpdateMs"] = state.lastUpdateMs;

  JsonArray hist  = root.createNestedArray("history");
  int start = (historyHead - historyCount + HISTORY_N) % HISTORY_N;
  for (int i = 0; i < historyCount; i++) {
    int idx = (start + i) % HISTORY_N;
    JsonObject p = hist.createNestedObject();
    p["t_ms"]    = history[idx].tMs;
    p["armed"]   = history[idx].armed;
    p["battery"] = history[idx].battery;
    p["altitude"]= history[idx].altitude;
  }

  JsonArray ev = root.createNestedArray("events");
  for (int i = 0; i < eventCount; i++) {
    int idx = (eventHead - 1 - i + EVENT_N) % EVENT_N;
    ev.add(events[idx]);
  }

  String out;
  serializeJson(doc, out);
  server.sendHeader("Content-Disposition", "attachment; filename=\"telemetry.json\"");
  server.send(200, "application/json", out);
}

void setup() {
  Serial.begin(115200);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("ESP32 IP: ");
  Serial.println(WiFi.localIP());

  // Registering endpoints
  server.on("/",              HTTP_GET,  handleRoot);
  server.on("/state",         HTTP_GET,  handleState);
  server.on("/history",       HTTP_GET,  handleHistory);
  server.on("/events",        HTTP_GET,  handleEvents);
  server.on("/telemetry",     HTTP_POST, handleTelemetry);
  server.on("/event",         HTTP_POST, handleEvent);
  server.on("/download.csv",  HTTP_GET,  handleDownloadCSV);
  server.on("/download.json", HTTP_GET,  handleDownloadJSON);

  server.begin();
  Serial.println("HTTP server started");
}

void loop() {
  server.handleClient();
}