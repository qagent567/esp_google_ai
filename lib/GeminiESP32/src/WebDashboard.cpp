#include "WebDashboard.h"

#if GEMINI_ENABLE_WEB_DASHBOARD

#include "GeminiESP32.h"
#include <WiFi.h>

static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="ru">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>GeminiESP32 Dashboard</title>
    <style>
        :root {
            --bg: #0f172a;
            --card: #1e293b;
            --accent: #3b82f6;
            --accent-glow: rgba(59, 130, 246, 0.5);
            --text: #f8fafc;
            --text-muted: #94a3b8;
            --success: #10b981;
            --border: #334155;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; }
        body { background: var(--bg); color: var(--text); min-height: 100vh; display: flex; flex-direction: column; }
        header { background: var(--card); border-bottom: 1px solid var(--border); padding: 1rem 1.5rem; display: flex; justify-content: space-between; align-items: center; }
        .logo { font-size: 1.25rem; font-weight: bold; color: var(--text); display: flex; align-items: center; gap: 0.5rem; }
        .logo span { color: var(--accent); }
        .badge { background: #064e3b; color: var(--success); font-size: 0.75rem; padding: 0.25rem 0.5rem; border-radius: 9999px; border: 1px solid #059669; }
        
        .container { display: grid; grid-template-columns: 1fr 340px; gap: 1.5rem; padding: 1.5rem; flex: 1; max-width: 1400px; margin: 0 auto; width: 100%; }
        @media (max-width: 900px) { .container { grid-template-columns: 1fr; } }

        .card { background: var(--card); border: 1px solid var(--border); border-radius: 1rem; padding: 1.25rem; display: flex; flex-direction: column; }
        .card-title { font-size: 1rem; font-weight: 600; margin-bottom: 1rem; color: var(--text-muted); text-transform: uppercase; letter-spacing: 0.05em; }

        /* Chat Area */
        .chat-container { display: flex; flex-direction: column; height: 600px; }
        .messages { flex: 1; overflow-y: auto; padding: 1rem; display: flex; flex-direction: column; gap: 1rem; }
        .msg { max-width: 80%; padding: 0.75rem 1rem; border-radius: 1rem; line-height: 1.5; font-size: 0.95rem; word-break: break-word; }
        .msg.user { align-self: flex-end; background: var(--accent); color: white; border-bottom-right-radius: 0.25rem; }
        .msg.ai { align-self: flex-start; background: #334155; color: var(--text); border-bottom-left-radius: 0.25rem; }
        
        .input-area { display: flex; gap: 0.5rem; padding: 1rem 0 0 0; border-top: 1px solid var(--border); }
        .input-area input { flex: 1; background: #0f172a; border: 1px solid var(--border); border-radius: 0.75rem; padding: 0.75rem 1rem; color: white; outline: none; font-size: 0.95rem; }
        .input-area input:focus { border-color: var(--accent); box-shadow: 0 0 0 2px var(--accent-glow); }
        .btn { background: var(--accent); color: white; border: none; border-radius: 0.75rem; padding: 0 1.5rem; font-weight: 600; cursor: pointer; transition: all 0.2s; }
        .btn:hover { opacity: 0.9; }

        /* Sidebar Stats */
        .stat-grid { display: grid; grid-template-columns: 1fr 1fr; gap: 0.75rem; margin-bottom: 1.5rem; }
        .stat-box { background: #0f172a; padding: 0.75rem; border-radius: 0.75rem; border: 1px solid var(--border); text-align: center; }
        .stat-val { font-size: 1.25rem; font-weight: bold; color: var(--text); }
        .stat-lbl { font-size: 0.75rem; color: var(--text-muted); margin-top: 0.25rem; }

        /* GPIO Controls */
        .gpio-list { display: flex; flex-direction: column; gap: 0.5rem; }
        .gpio-item { display: flex; justify-content: space-between; align-items: center; background: #0f172a; padding: 0.75rem 1rem; border-radius: 0.75rem; border: 1px solid var(--border); }
        .toggle-btn { background: #334155; color: var(--text-muted); padding: 0.4rem 0.8rem; border-radius: 0.5rem; border: none; cursor: pointer; font-size: 0.85rem; font-weight: 600; }
        .toggle-btn.on { background: var(--success); color: white; }
    </style>
</head>
<body>
    <header>
        <div class="logo">⚡ Gemini<span>ESP32</span> Dashboard</div>
        <div class="badge" id="onlineBadge">● Онлайн</div>
    </header>

    <div class="container">
        <!-- Чат -->
        <div class="card chat-container">
            <div class="card-title">Диалог с Gemini AI</div>
            <div class="messages" id="chatBox">
                <div class="msg ai">Привет! Я встроенный Gemini AI ассистент на ESP32. Чем могу помочь?</div>
            </div>
            <div class="input-area">
                <input type="text" id="promptInput" placeholder="Введите сообщение или команду (напр. 'включи GPIO 2')..." onkeypress="if(event.key==='Enter') sendMsg()">
                <button class="btn" id="sendBtn" onclick="sendMsg()">Отправить</button>
            </div>
        </div>

        <!-- Телеметрия и GPIO -->
        <div style="display: flex; flex-direction: column; gap: 1.5rem;">
            <div class="card">
                <div class="card-title">Телеметрия ESP32</div>
                <div class="stat-grid">
                    <div class="stat-box"><div class="stat-val" id="freeHeap">-</div><div class="stat-lbl">Free RAM</div></div>
                    <div class="stat-box"><div class="stat-val" id="quotaToday">-</div><div class="stat-lbl">Запросов/сутки</div></div>
                    <div class="stat-box"><div class="stat-val" id="modelName">-</div><div class="stat-lbl">Модель</div></div>
                    <div class="stat-box"><div class="stat-val" id="uptimeSec">-</div><div class="stat-lbl">Аптайм</div></div>
                </div>
            </div>

            <div class="card">
                <div class="card-title">Быстрое управление GPIO</div>
                <div class="gpio-list">
                    <div class="gpio-item">
                        <span>GPIO 2 (LED)</span>
                        <button class="toggle-btn" id="pin2Btn" onclick="togglePin(2)">ВЫКЛ</button>
                    </div>
                    <div class="gpio-item">
                        <span>GPIO 4</span>
                        <button class="toggle-btn" id="pin4Btn" onclick="togglePin(4)">ВЫКЛ</button>
                    </div>
                </div>
            </div>
        </div>
    </div>

    <script>
        async function updateStatus() {
            try {
                const res = await fetch('/api/status');
                const data = await res.json();
                document.getElementById('freeHeap').innerText = Math.round(data.freeHeap / 1024) + ' KB';
                document.getElementById('quotaToday').innerText = data.requestsToday + ' / ' + data.dailyLimit;
                document.getElementById('modelName').innerText = data.model || 'Flash';
                document.getElementById('uptimeSec').innerText = Math.round(data.uptime) + ' с';
            } catch (e) {
                console.error(e);
            }
        }
        setInterval(updateStatus, 3000);
        updateStatus();

        let pinStates = { 2: false, 4: false };
        async function togglePin(pin) {
            pinStates[pin] = !pinStates[pin];
            const btn = document.getElementById('pin' + pin + 'Btn');
            btn.className = 'toggle-btn ' + (pinStates[pin] ? 'on' : '');
            btn.innerText = pinStates[pin] ? 'ВКЛ' : 'ВЫКЛ';

            await fetch('/api/gpio', {
                method: 'POST',
                headers: {'Content-Type': 'application/json'},
                body: JSON.stringify({pin: pin, state: pinStates[pin] ? 1 : 0})
            });
        }

        async function sendMsg() {
            const input = document.getElementById('promptInput');
            const txt = input.value.trim();
            if (!txt) return;

            const chatBox = document.getElementById('chatBox');
            chatBox.innerHTML += '<div class="msg user">' + txt + '</div>';
            input.value = '';
            chatBox.scrollTop = chatBox.scrollHeight;

            const sendBtn = document.getElementById('sendBtn');
            sendBtn.disabled = true;
            sendBtn.innerText = 'Думает...';

            try {
                const res = await fetch('/api/chat', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/json'},
                    body: JSON.stringify({prompt: txt})
                });
                const data = await res.json();
                chatBox.innerHTML += '<div class="msg ai">' + data.response.replace(/\\n/g, '<br>') + '</div>';
            } catch (e) {
                chatBox.innerHTML += '<div class="msg ai" style="color: #ef4444;">Ошибка связи с платой.</div>';
            } finally {
                sendBtn.disabled = false;
                sendBtn.innerText = 'Отправить';
                chatBox.scrollTop = chatBox.scrollHeight;
                updateStatus();
            }
        }
    </script>
</body>
</html>
)rawliteral";

static void webServerTask(void* pvParameters) {
    WebDashboard* dash = static_cast<WebDashboard*>(pvParameters);
    while (dash && dash->isRunning()) {
        dash->handle();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelete(NULL);
}

WebDashboard::WebDashboard()
    : _server(nullptr), _ai(nullptr), _port(80), _running(false), _inBackground(false), _taskHandle(nullptr) {
}

WebDashboard::~WebDashboard() {
    stop();
}

bool WebDashboard::begin(GeminiESP32* ai, uint16_t port, bool inBackground) {
    if (!ai) return false;
    _ai = ai;
    _port = port;
    _inBackground = inBackground;

    if (_server) {
        delete _server;
    }
    _server = new WebServer(_port);

    setupRoutes();
    _server->begin();
    _running = true;

    if (_inBackground) {
        xTaskCreatePinnedToCore(
            webServerTask,
            "web_dashboard",
            GEMINI_WEB_STACK_SIZE,
            this,
            1,
            &_taskHandle,
            GEMINI_WEB_TASK_CORE
        );
    }

    return true;
}

void WebDashboard::stop() {
    _running = false;
    if (_taskHandle) {
        vTaskDelete(_taskHandle);
        _taskHandle = nullptr;
    }
    if (_server) {
        _server->stop();
        delete _server;
        _server = nullptr;
    }
}

void WebDashboard::handle() {
    if (_server && _running) {
        _server->handleClient();
    }
}

void WebDashboard::setupRoutes() {
    if (!_server) return;

    _server->on("/", HTTP_GET, [this]() { handleRoot(); });
    _server->on("/api/status", HTTP_GET, [this]() { handleStatus(); });
    _server->on("/api/chat", HTTP_POST, [this]() { handleChat(); });
    _server->on("/api/gpio", HTTP_POST, [this]() { handleGpio(); });
}

void WebDashboard::handleRoot() {
    if (!_server) return;
    _server->send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
}

void WebDashboard::handleStatus() {
    if (!_server || !_ai) return;

    JsonDocument doc;
    doc["freeHeap"] = ESP.getFreeHeap();
    doc["uptime"] = millis() / 1000;
#if GEMINI_ENABLE_USAGE_TRACKER
    doc["requestsToday"] = _ai->getUsage().getStats().requestsToday;
    doc["dailyLimit"] = _ai->getUsage().getStats().dailyRequestLimit;
#else
    doc["requestsToday"] = 0;
    doc["dailyLimit"] = 1500;
#endif
    doc["model"] = _ai->getConfig().getConfig().model;

    String jsonStr;
    serializeJson(doc, jsonStr);
    _server->send(200, "application/json", jsonStr);
}

void WebDashboard::handleChat() {
    if (!_server || !_ai) return;

    if (!_server->hasArg("plain")) {
        _server->send(400, "application/json", "{\"error\":\"Missing body\"}");
        return;
    }

    String body = _server->arg("plain");
    JsonDocument reqDoc;
    DeserializationError err = deserializeJson(reqDoc, body);
    if (err || !reqDoc["prompt"]) {
        _server->send(400, "application/json", "{\"error\":\"Invalid JSON\"}");
        return;
    }

    String prompt = reqDoc["prompt"].as<String>();
    GeminiResponse resp = _ai->query(prompt);

    JsonDocument resDoc;
    resDoc["response"] = resp.text;
    resDoc["success"] = resp.success;
    resDoc["durationMs"] = resp.durationMs;
    resDoc["tokens"] = resp.totalTokens;

    String jsonStr;
    serializeJson(resDoc, jsonStr);
    _server->send(200, "application/json", jsonStr);
}

void WebDashboard::handleGpio() {
    if (!_server) return;

    if (!_server->hasArg("plain")) {
        _server->send(400, "application/json", "{\"error\":\"Missing body\"}");
        return;
    }

    String body = _server->arg("plain");
    JsonDocument reqDoc;
    deserializeJson(reqDoc, body);

    int pin = reqDoc["pin"] | -1;
    int state = reqDoc["state"] | 0;

    if (pin >= 0) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, state ? HIGH : LOW);
        _server->send(200, "application/json", "{\"status\":\"ok\"}");
    } else {
        _server->send(400, "application/json", "{\"error\":\"Invalid pin\"}");
    }
}

#endif // GEMINI_ENABLE_WEB_DASHBOARD
