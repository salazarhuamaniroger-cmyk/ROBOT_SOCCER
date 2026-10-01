#include <WiFi.h>
#include <ArduinoOTA.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// ================= CONFIGURACIÓN DE RED (MODO ACCESS POINT) =================
const char* ap_ssid = "SoccerBot-AP";
const char* ap_password = "password"; // Minimo 8 caracteres, requerido por el estandar WiFi-SP32-S3
WebServer server(80);

// ================= ASIGNACIÓN DE PINES (TB6612FNG) =================
const int PWMA_PIN = 14;
const int AIN1_PIN = 18;
const int AIN2_PIN = 21; // Motor A (Asumiremos que es el Izquierdo)

const int PWMB_PIN = 7;// modificado del 35 al 7 porque 35 se quemo
const int BIN1_PIN = 16;
const int BIN2_PIN = 17; // Motor B (Asumiremos que es el Derecho)

// ================= WATCHDOG DE SEGURIDAD =================
unsigned long lastCommandTime = 0;
const unsigned long COMMAND_TIMEOUT_MS = 300; // Si no llega orden en 300ms, freno de emergencia
uint32_t lastSeq = 0; // Ultimo numero de secuencia procesado (anti-desorden de paquetes)

// ================= INTERFAZ WEB TACTICAL V4 (PROGMEM) =================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no, orientation=landscape, viewport-fit=cover">
    <title>SOCCERBOT PRO</title>
    <style>
        @import url('https://fonts.googleapis.com/css2?family=Share+Tech+Mono&display=swap');
        :root {
            --bg-base: #050608; --panel-bg: #10121a; --border-dark: #1f2433;
            --text-muted: #64748b; --neon-blue: #00e5ff; --neon-orange: #ff4d00;
            --neon-green: #00ff66; --neon-red: #ff0033;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; user-select: none; -webkit-user-select: none; touch-action: none; }
        html, body { height: 100%; width: 100%; }
        body {
            font-family: 'Segoe UI', sans-serif; background-color: var(--bg-base); color: #fff;
            height: 100vh; width: 100vw;
            height: 100dvh; width: 100dvw;
            overflow: hidden; display: grid;
            grid-template-columns: 1fr 240px 1fr;
            grid-template-rows: minmax(50px, 65px) 1fr;
            gap: 10px;
            padding-top: 10px;
            padding-bottom: 10px;
            padding-left: calc(10px + env(safe-area-inset-left));
            padding-right: calc(10px + env(safe-area-inset-right));
        }
        #orientation-warning {
            display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: var(--neon-red);
            color: white; z-index: 9999; align-items: center; justify-content: center; flex-direction: column; font-size: 24px; font-weight: bold; text-align: center;
        }
        @media screen and (orientation: portrait) { #orientation-warning { display: flex; } body { display: none; } }

        .top-bar {
            grid-column: 1 / 4; background: var(--panel-bg); border: 2px solid var(--border-dark); border-radius: 12px;
            display: flex; align-items: center; justify-content: flex-start; padding: 0 20px; gap: 20px;
            min-height: 0;
        }
        .top-bar.hidden { visibility: hidden; }
        .slider-label { font-family: 'Share Tech Mono', monospace; font-size: 16px; color: var(--neon-orange); font-weight: bold; white-space: nowrap; width: 150px; }
        .slider-wrapper { flex: 1; display: flex; flex-direction: column; position: relative; padding-top: 5px; }
        input[type=range] { width: 100%; -webkit-appearance: none; background: #1e293b; height: 8px; border-radius: 4px; outline: none; z-index: 2; position: relative; }
        input[type=range]::-webkit-slider-thumb { -webkit-appearance: none; width: 28px; height: 28px; border-radius: 50%; background: var(--neon-orange); cursor: pointer; border: 3px solid #fff; box-shadow: 0 0 10px rgba(255, 77, 0, 0.5); }
        .ruler-ticks { display: flex; justify-content: space-between; padding: 0 10px; margin-top: 2px; pointer-events: none; }
        .tick { display: flex; flex-direction: column; align-items: center; font-family: 'Share Tech Mono', monospace; font-size: 9px; color: var(--text-muted); }
        .tick::before { content: ''; width: 2px; height: 6px; background: var(--text-muted); margin-bottom: 2px; }

        .control-zone { background: var(--panel-bg); border: 2px solid var(--border-dark); border-radius: 16px; display: flex; align-items: center; justify-content: center; position: relative; min-height: 0; min-width: 0; overflow: hidden; }
        .center-dash { display: flex; flex-direction: column; gap: 10px; justify-content: flex-start; min-height: 0; overflow: hidden; }
        .mode-selector { display: flex; border: 2px solid var(--border-dark); border-radius: 10px; overflow: hidden; background: var(--panel-bg); }
        .mode-btn { flex: 1; background: transparent; color: var(--text-muted); border: none; padding: 12px 5px; font-size: 12px; font-weight: bold; cursor: pointer; }
        .mode-btn.active { background: #1a1e2b; color: #fff; border-bottom: 2px solid var(--neon-blue); }
        .telemetry-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 8px; }
        .t-card { background: var(--panel-bg); border: 1px solid var(--border-dark); border-radius: 10px; display: flex; flex-direction: column; align-items: center; justify-content: center; padding: 12px 5px; }
        .t-label { font-size: 10px; color: var(--text-muted); }
        .t-value { font-family: 'Share Tech Mono', monospace; font-size: 18px; margin-top: 4px; color: #e2e8f0; }
        .c-green { color: var(--neon-green); text-shadow: 0 0 8px rgba(0,255,102,0.5); }
        .c-red { color: var(--neon-red); text-shadow: 0 0 8px rgba(255,0,51,0.5); }
        .robot-name-display { margin-top: auto; margin-bottom: 10px; text-align: center; font-family: 'Share Tech Mono', monospace; font-size: 26px; font-weight: bold; color: var(--neon-red); letter-spacing: 2px; text-shadow: 0 0 12px rgba(255, 0, 51, 0.4); }

        .view { position: absolute; width: 100%; height: 100%; display: none; align-items: center; justify-content: center; }
        .view.active { display: flex; }
        .btn-group { display: flex; gap: 20px; justify-content: center; align-items: center; width: 100%; height: 100%; }
        .btn-group.vertical { flex-direction: column; }
        .btn-pad { width: clamp(60px, 16vh, 100px); height: clamp(60px, 16vh, 100px); background: linear-gradient(145deg, #1f2433, #151822); border: 2px solid #2a3143; border-radius: 20px; display: flex; align-items: center; justify-content: center; font-size: clamp(24px, 6vh, 35px); color: #64748b; box-shadow: 0 8px 15px rgba(0,0,0,0.4); }
        .btn-pad:active { transform: scale(0.92); }
        .btn-pad.x-axis:active { border-color: var(--neon-blue); color: var(--neon-blue); box-shadow: inset 0 0 15px rgba(0,229,255,0.3); }
        .btn-pad.y-axis:active { border-color: var(--neon-orange); color: var(--neon-orange); box-shadow: inset 0 0 15px rgba(255,77,0,0.3); }

        .track { background: #0d0e14; border: 2px solid #1a1e2b; border-radius: 50px; position: relative; display: flex; align-items: center; justify-content: center; }
        .track.horizontal { width: 90%; height: clamp(60px, 16vh, 100px); }
        .track.vertical { height: 90%; width: clamp(60px, 16vh, 100px); }
        .track-center-line { position: absolute; background: var(--text-muted); opacity: 0.3; }
        .track.horizontal .track-center-line { width: 4px; height: 100%; }
        .track.vertical .track-center-line { height: 4px; width: 100%; }
        .graduations { position: absolute; font-family: 'Share Tech Mono', monospace; font-size: 10px; color: var(--text-muted); pointer-events: none; display: flex; justify-content: space-between; }
        .track.horizontal .graduations { width: 85%; top: 5px; flex-direction: row; }
        .track.horizontal .graduations.bottom { top: auto; bottom: 5px; }
        .track.vertical .graduations { height: 85%; right: 5px; flex-direction: column; align-items: flex-end; }
        .track.vertical .graduations.left { right: auto; left: 5px; align-items: flex-start; }
        .handle { width: clamp(50px, 13vh, 80px); height: clamp(50px, 13vh, 80px); border-radius: 50%; position: absolute; cursor: pointer; box-shadow: 0 4px 10px rgba(0,0,0,0.8); z-index: 10; }
        .handle.x-handle { background: radial-gradient(circle, #00b4d8, #0077b6); border: 3px solid var(--neon-blue); }
        .handle.y-handle { background: radial-gradient(circle, #ff6d00, #dd2c00); border: 3px solid var(--neon-orange); }
    </style>
</head>
<body>
    <div id="orientation-warning">GIRA EL TELÉFONO</div>
    <div class="top-bar" id="speed-limiter">
        <div class="slider-label">LÍMITE PWM: <span id="lbl-limit">255</span></div>
        <div class="slider-wrapper">
            <input type="range" min="50" max="255" value="255" step="5" oninput="updateLimit(this.value)">
            <div class="ruler-ticks"><div class="tick">50</div><div class="tick">100</div><div class="tick">150</div><div class="tick">200</div><div class="tick">255</div></div>
        </div>
    </div>

    <div class="control-zone">
        <div class="view mode-1 active">
            <div class="btn-group">
                <div class="btn-pad x-axis" onpointerdown="startAccX(-1)" onpointerup="stopAccX()" onpointerleave="stopAccX()" onpointercancel="stopAccX()">◀</div>
                <div class="btn-pad x-axis" onpointerdown="startAccX(1)" onpointerup="stopAccX()" onpointerleave="stopAccX()" onpointercancel="stopAccX()">▶</div>
            </div>
        </div>
        <div class="view mode-2">
            <div class="track horizontal" id="track-x">
                <div class="graduations"><span>-255</span><span>-128</span><span></span><span>128</span><span>255</span></div>
                <div class="graduations bottom"><span></span><span></span><span>0</span><span></span><span></span></div>
                <div class="track-center-line"></div>
                <div class="handle x-handle" id="handle-x"></div>
            </div>
        </div>
    </div>

    <div class="center-dash">
        <div class="mode-selector"><button class="mode-btn active" onclick="setMode(1)">BOTONES</button><button class="mode-btn" onclick="setMode(2)">DUAL PRO</button></div>
        <div class="telemetry-grid">
            <div class="t-card"><span class="t-label">NET</span><span class="t-value" id="val-net">--</span></div>
            <div class="t-card"><span class="t-label">BATTERY</span><span class="t-value">12.4V</span></div>
            <div class="t-card"><span class="t-label">GIRO (X)</span><span class="t-value" id="val-x">0</span></div>
            <div class="t-card"><span class="t-label">POWER (Y)</span><span class="t-value" id="val-y">0</span></div>
        </div>
        <div class="robot-name-display" id="ui-robot-name"></div>
    </div>

    <div class="control-zone">
        <div class="view mode-1 active">
            <div class="btn-group vertical">
                <div class="btn-pad y-axis" onpointerdown="startAccY(1)" onpointerup="stopAccY()" onpointerleave="stopAccY()" onpointercancel="stopAccY()">▲</div>
                <div class="btn-pad y-axis" onpointerdown="startAccY(-1)" onpointerup="stopAccY()" onpointerleave="stopAccY()" onpointercancel="stopAccY()">▼</div>
            </div>
        </div>
        <div class="view mode-2">
            <div class="track vertical" id="track-y">
                <div class="graduations"><span>255</span><span>128</span><span></span><span>-128</span><span>-255</span></div>
                <div class="graduations left"><span></span><span></span><span>0</span><span></span><span></span></div>
                <div class="track-center-line"></div>
                <div class="handle y-handle" id="handle-y"></div>
            </div>
        </div>
    </div>

    <script>
        const NOMBRE_ROBOT = "PITBULL V.200";
        document.getElementById('ui-robot-name').innerText = NOMBRE_ROBOT;

        let maxLimit = 255;
        let currentX = 0, currentY = 0;
        let lastSendTime = 0;

        // Numero de secuencia incremental: permite al ESP32 descartar paquetes
        // que lleguen fuera de orden por congestion de red.
        let seqCounter = 0;

        // Indicador simple de estado de red (ultimo fetch exitoso vs fallido)
        function setNetStatus(ok) {
            const el = document.getElementById('val-net');
            el.innerText = ok ? 'ONLINE' : 'ERROR';
            el.className = 't-value ' + (ok ? 'c-green' : 'c-red');
        }

        // Función unificada que envía el vector (X,Y) al ESP32
        function sendVector(force = false) {
            const now = Date.now();
            if (!force && now - lastSendTime < 60) return; // Evita saturar la red (máx 16 envíos/segundo)
            lastSendTime = now;

            document.getElementById('val-x').innerText = Math.round(currentX);
            document.getElementById('val-y').innerText = Math.round(currentY);

            const seq = ++seqCounter;
            fetch(`/move?x=${Math.round(currentX)}&y=${Math.round(currentY)}&seq=${seq}`)
                .then(() => setNetStatus(true))
                .catch(() => setNetStatus(false));
        }

        function setMode(mode) {
            document.querySelectorAll('.mode-btn').forEach((btn, idx) => btn.classList.toggle('active', idx === mode - 1));
            document.querySelectorAll('.mode-1').forEach(el => el.classList.toggle('active', mode === 1));
            document.querySelectorAll('.mode-2').forEach(el => el.classList.toggle('active', mode === 2));
            const topBar = document.getElementById('speed-limiter');
            if(mode === 2) topBar.classList.add('hidden'); else topBar.classList.remove('hidden');
            stopAccX(); stopAccY();
        }

        function updateLimit(val) {
            maxLimit = parseInt(val);
            document.getElementById('lbl-limit').innerText = maxLimit;
        }

        let timerX, timerY; const ACC_STEP = 15; const ACC_DELAY = 30;

        function startAccX(dir) {
            clearInterval(timerX); currentX = 0;
            timerX = setInterval(() => {
                currentX += ACC_STEP * dir;
                if (dir > 0 && currentX > maxLimit) currentX = maxLimit;
                if (dir < 0 && currentX < -maxLimit) currentX = -maxLimit;
                sendVector();
            }, ACC_DELAY);
        }
        function stopAccX() { clearInterval(timerX); currentX = 0; sendVector(true); }

        function startAccY(dir) {
            clearInterval(timerY); currentY = 0;
            timerY = setInterval(() => {
                currentY += ACC_STEP * dir;
                if (dir > 0 && currentY > maxLimit) currentY = maxLimit;
                if (dir < 0 && currentY < -maxLimit) currentY = -maxLimit;
                sendVector();
            }, ACC_DELAY);
        }
        function stopAccY() { clearInterval(timerY); currentY = 0; sendVector(true); }

        // Failsafe extra en el cliente: si la pestaña pasa a segundo plano
        // (se bloquea la pantalla, se cambia de app, etc.) frenamos todo.
        document.addEventListener('visibilitychange', () => {
            if (document.hidden) {
                stopAccX();
                stopAccY();
            }
        });

        function makeLinearJoystick(trackId, handleId, axis) {
            const track = document.getElementById(trackId);
            const handle = document.getElementById(handleId);
            let active = false;
            let heartbeatTimer = null; // Mantiene vivo el watchdog mientras el handle está sostenido quieto

            const startTrack = (e) => {
                active = true;
                handle.style.transition = 'none';
                moveHandle(e);

                // HEARTBEAT: aunque no haya movimiento, reenvía el vector actual
                // cada 100ms mientras el dedo siga sobre el handle. Esto evita que
                // el watchdog del ESP32 (300ms) frene los motores al mantener
                // el joystick fijo en una posición sin desplazarlo.
                clearInterval(heartbeatTimer);
                heartbeatTimer = setInterval(() => {
                    if (active) sendVector(true);
                }, 100);
            };
            const moveHandle = (e) => {
                if (!active) return;
                e.preventDefault();
                const rect = track.getBoundingClientRect();
                const pointer = e.touches ? e.touches[0] : e;

                let rawPos = 0, maxTravel = 0, mathVal = 0;
                if (axis === 'X') {
                    maxTravel = (rect.width / 2) - (handle.offsetWidth / 2) - 5;
                    rawPos = pointer.clientX - (rect.left + rect.width / 2);
                } else {
                    maxTravel = (rect.height / 2) - (handle.offsetHeight / 2) - 5;
                    rawPos = pointer.clientY - (rect.top + rect.height / 2);
                }

                let boundedPos = Math.max(-maxTravel, Math.min(rawPos, maxTravel));
                mathVal = Math.round((boundedPos / maxTravel) * 255);

                if (axis === 'X') {
                    handle.style.transform = `translateX(${boundedPos}px)`;
                    currentX = mathVal;
                } else {
                    handle.style.transform = `translateY(${boundedPos}px)`;
                    currentY = -mathVal;
                }
                sendVector();
            };
            const endTrack = () => {
                if (!active) return;
                active = false;
                clearInterval(heartbeatTimer);
                handle.style.transition = 'transform 0.15s cubic-bezier(0.175, 0.885, 0.32, 1.275)';
                handle.style.transform = 'translate(0px, 0px)';
                if (axis === 'X') currentX = 0; else currentY = 0;
                sendVector(true);
            };

            track.addEventListener('mousedown', startTrack); window.addEventListener('mousemove', moveHandle); window.addEventListener('mouseup', endTrack);
            track.addEventListener('touchstart', startTrack, { passive: false }); window.addEventListener('touchmove', moveHandle, { passive: false }); window.addEventListener('touchend', endTrack);
            track.addEventListener('pointercancel', endTrack);
        }
        makeLinearJoystick('track-x', 'handle-x', 'X');
        makeLinearJoystick('track-y', 'handle-y', 'Y');
    </script>
</body>
</html>
)rawliteral";



// ================= FUNCIÓN MAESTRA DE CONTROL DE MOTORES =================
// Traduce la velocidad requerida (-255 a 255) en los pines lógicos del TB6612FNG
void operarMotor(bool esIzquierdo, int velocidad) {
    int in1 = esIzquierdo ? AIN1_PIN : BIN1_PIN;
    int in2 = esIzquierdo ? AIN2_PIN : BIN2_PIN;
    int pwmPin = esIzquierdo ? PWMA_PIN : PWMB_PIN;

    if (velocidad > 0) {
        // Adelante (CORREGIDO: Se invirtieron los estados lógicos)
        digitalWrite(in1, LOW);
        digitalWrite(in2, HIGH);
        ledcWrite(pwmPin, velocidad);
    } else if (velocidad < 0) {
        // Atrás (CORREGIDO: Se invirtieron los estados lógicos)
        digitalWrite(in1, HIGH);
        digitalWrite(in2, LOW);
        ledcWrite(pwmPin, abs(velocidad));
    } else {
        // Detener (Freno pasivo, se mantiene igual)
        digitalWrite(in1, LOW);
        digitalWrite(in2, LOW);
        ledcWrite(pwmPin, 0);
    }
}

// ================= FRENO DE EMERGENCIA (usado por el watchdog) =================
void detenerTodo() {
    operarMotor(true, 0);
    operarMotor(false, 0);
}

// ================= SETUP GENERAL =================
void setup() {
    Serial.begin(115200);

    // 1. MODO ACCESS POINT: el ESP32 crea su propia red, ya no depende
    //    del hotspot de ningun telefono.
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    Serial.println("\n==============================================");
    Serial.print("Access Point creado: ");
    Serial.println(ap_ssid);
    Serial.print("IP del AP: ");
    Serial.println(WiFi.softAPIP()); // Normalmente 192.168.4.1
    Serial.println("==============================================");

    // 2. INICIALIZACIÓN mDNS (opcional en modo AP, puede fallar en algunos Android)
    if (!MDNS.begin("soccerbot")) {
        Serial.println("Error iniciando mDNS (usa la IP directa si falla)");
    } else {
        Serial.println("mDNS listo. Intenta: http://soccerbot.local");
    }

    // 3. INICIALIZACIÓN OTA
    ArduinoOTA.begin();

    // 4. CONFIGURACIÓN DRIVER TB6612FNG
    pinMode(AIN1_PIN, OUTPUT); pinMode(AIN2_PIN, OUTPUT);
    pinMode(BIN1_PIN, OUTPUT); pinMode(BIN2_PIN, OUTPUT);

    ledcAttach(PWMA_PIN, 1000, 8);
    ledcAttach(PWMB_PIN, 1000, 8);

    detenerTodo();

    // 5. RUTAS HTTP DEL SERVIDOR
    server.on("/", []() {
        server.send(200, "text/html", INDEX_HTML);
    });

    // RUTA ÚNICA DE CINEMÁTICA, ahora con numero de secuencia anti-desorden
    server.on("/move", []() {
        if (server.hasArg("x") && server.hasArg("y")) {
            int x = server.arg("x").toInt(); // Vector de Giro
            int y = server.arg("y").toInt(); // Vector de Potencia

            if (server.hasArg("seq")) {
                uint32_t seq = (uint32_t) server.arg("seq").toInt();
                if (seq < lastSeq) {
                    server.send(200, "text/plain", "STALE");
                    return;
                }
                lastSeq = seq;
            }

            // Algoritmo Arcade Drive (Cinemática de Tracción Diferencial)
            int velIzq = y - x;// de modifico antes y + x
            int velDer = y + x; // antes y-x

            velIzq = constrain(velIzq, -255, 255);
            velDer = constrain(velDer, -255, 255);

            operarMotor(true, velIzq);
            operarMotor(false, velDer);

            lastCommandTime = millis();
        }
        server.send(200, "text/plain", "OK");
    });

    server.begin();
    Serial.println("Servidor de Control Web en ejecución.");

    lastCommandTime = millis();
}

// ================= LOOP PRINCIPAL =================
void loop() {
    ArduinoOTA.handle();
    server.handleClient();

    // ===== WATCHDOG DE SEGURIDAD =====
    if (millis() - lastCommandTime > COMMAND_TIMEOUT_MS) {
        detenerTodo();
    }
}
