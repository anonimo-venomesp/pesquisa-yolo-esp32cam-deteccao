#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_heap_caps.h"
#include <WiFiClientSecure.h>
#include <esp_task_wdt.h>

// Pinos da Câmera AI-Thinker ESP32-CAM
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define FLASH_GPIO_NUM     4

// Objetos globais
WebServer server(80);
DNSServer dnsServer;
Preferences preferences;
SemaphoreHandle_t network_mutex = NULL;
SemaphoreHandle_t status_mutex = NULL;

// Memória RTC para detectar se o reset foi físico (RST) ou corte de energia
RTC_DATA_ATTR uint32_t rtc_magic = 0;

// Configurações salvas
String wifi_ssid = "";
String wifi_pass = "";
String api_url = "https://api-esp32cam-deteccao-animais-peconhentos.onrender.com";

// Estado do dispositivo
String device_id = "";
volatile bool wifi_connected = false;
volatile bool api_online = false;
volatile bool bot_registered = false;
String bot_username = "Desconectado";
bool test_photo_sent = false;
volatile bool api_check_active = false;
volatile bool is_transmitting = false;
bool ap_active = false;

// Timings para execução periódica
unsigned long last_capture_ms = 0;
const unsigned long capture_interval_ms = 15000; // 15 segundos

// Estado do alarme visual (Blinking do Flash)
bool alarme_ativo = false;
unsigned long alarme_inicio_ms = 0;
const unsigned long alarme_duracao_ms = 15000; // 15 segundos
unsigned long ultimo_pisca_ms = 0;
int estado_flash = LOW;

// Código HTML/CSS/JS do Portal de Configuração e Status
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pt-BR">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Configuração VenomESP</title>
    <style>
        :root {
            --bg-color: #000000;
            --primary: #10b981; /* Verde esmeralda */
            --primary-glow: rgba(16, 185, 129, 0.3);
            --border: #1a1a1a;
            --text-color: #ffffff;
            --text-muted: #777777;
            --input-bg: #070707;
        }
        * {
            box-sizing: border-box;
            margin: 0;
            padding: 0;
        }
        body {
            font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
            background-color: #000000;
            color: var(--text-color);
            min-height: 100vh;
            display: flex;
            justify-content: center;
            align-items: center;
            padding: 20px;
        }
        .container {
            width: 100%;
            max-width: 440px;
            padding: 10px;
        }
        h1 {
            font-size: 32px;
            font-weight: 800;
            text-align: center;
            margin-bottom: 4px;
            color: var(--primary);
            text-shadow: 0 0 10px var(--primary-glow);
        }
        .subtitle {
            text-align: center;
            font-size: 13px;
            color: var(--text-muted);
            margin-bottom: 30px;
            text-transform: uppercase;
            letter-spacing: 2px;
        }
        .section {
            padding: 20px 0;
            border-bottom: 1px solid var(--border);
            margin-bottom: 10px;
        }
        .section:last-of-type {
            border-bottom: none;
        }
        .section-title {
            font-size: 14px;
            font-weight: 600;
            margin-bottom: 12px;
            display: flex;
            align-items: center;
            color: var(--primary);
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .serial-box {
            background: var(--input-bg);
            border: 1px dashed #333333;
            padding: 12px 15px;
            border-radius: 8px;
            font-family: monospace;
            font-size: 14px;
            color: var(--primary);
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 12px;
        }
        .copy-btn {
            background: transparent;
            border: 1px solid var(--primary);
            color: var(--primary);
            padding: 4px 10px;
            border-radius: 6px;
            cursor: pointer;
            font-size: 12px;
            font-weight: 600;
            transition: all 0.2s;
        }
        .copy-btn:hover {
            background: var(--primary);
            color: #000000;
            box-shadow: 0 0 8px var(--primary-glow);
        }
        .instruction-text {
            font-size: 13px;
            color: var(--text-muted);
            line-height: 1.5;
            margin-bottom: 12px;
        }
        .action-buttons {
            display: grid;
            grid-template-columns: 1fr 1fr;
            gap: 10px;
        }
        .action-btn {
            display: block;
            text-align: center;
            text-decoration: none;
            color: #888888;
            padding: 10px;
            border-radius: 8px;
            font-size: 13px;
            font-weight: 600;
            border: 1px solid #222222;
            background: #050505;
            transition: all 0.2s;
        }
        .action-btn:hover {
            color: #ffffff;
            border-color: #444444;
            background: #0c0c0c;
        }
        .telegram-btn {
            border-color: var(--primary);
            color: var(--primary);
        }
        .telegram-btn:hover {
            background: var(--primary);
            color: #000000;
            box-shadow: 0 0 8px var(--primary-glow);
        }
        .form-group {
            margin-bottom: 15px;
        }
        label {
            display: block;
            font-size: 11px;
            color: var(--text-muted);
            margin-bottom: 6px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 0.5px;
        }
        input {
            width: 100%;
            background: var(--input-bg);
            border: 1px solid #222222;
            border-radius: 8px;
            padding: 10px 12px;
            color: #ffffff;
            font-size: 14px;
            outline: none;
            transition: all 0.2s;
        }
        input:focus {
            border-color: var(--primary);
            box-shadow: 0 0 5px rgba(16, 185, 129, 0.2);
        }
        .submit-btn {
            width: 100%;
            background: var(--primary);
            border: none;
            color: #000000;
            padding: 12px;
            border-radius: 8px;
            font-weight: 700;
            font-size: 13px;
            cursor: pointer;
            transition: all 0.2s;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .submit-btn:hover {
            background: #059669;
            box-shadow: 0 0 12px var(--primary-glow);
        }
        .status-row {
            display: flex;
            justify-content: space-between;
            align-items: center;
            margin-bottom: 10px;
            font-size: 13px;
        }
        .status-row:last-child {
            margin-bottom: 0;
        }
        .status-label {
            color: var(--text-muted);
        }
        .status-value {
            font-weight: 600;
            display: flex;
            align-items: center;
            gap: 6px;
        }
        .badge {
            width: 8px;
            height: 8px;
            border-radius: 50%;
            display: inline-block;
        }
        .badge-green { background-color: #10b981; box-shadow: 0 0 8px #10b981; }
        .badge-red { background-color: #ef4444; box-shadow: 0 0 8px #ef4444; }
        .badge-orange { background-color: #f59e0b; box-shadow: 0 0 8px #f59e0b; }
        
        .scan-wrapper {
            display: flex;
            gap: 8px;
        }
        .refresh-btn {
            background: var(--input-bg);
            border: 1px solid #222222;
            color: var(--text-muted);
            padding: 0 12px;
            border-radius: 8px;
            cursor: pointer;
            transition: all 0.2s;
        }
        .refresh-btn:hover {
            color: var(--primary);
            border-color: var(--primary);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>VenomESP</h1>
        <div class="subtitle">Dispositivo de Detecção</div>
        
        <!-- CARD 1: IDENTIFICAÇÃO E TELEGRAM -->
        <div class="section">
            <div class="section-title">👤 Passo 1: Vincular Telegram</div>
            <div class="instruction-text">
                Copie o ID único do seu ESP32-CAM e envie no chat do Bot do Telegram para receber os alertas.
            </div>
            <div class="serial-box">
                <span id="serial-id">%DEVICE_ID%</span>
                <button class="copy-btn" onclick="copySerial()">Copiar</button>
            </div>
            <div class="action-buttons">
                <a href="https://telegram.org" target="_blank" class="action-btn">Instalar Telegram</a>
                <a href="#" id="bot-link" target="_blank" class="action-btn telegram-btn">Abrir Bot</a>
            </div>
        </div>

        <!-- CARD 2: CONFIGURAÇÃO DE REDE -->
        <form action="/save" method="POST" class="section">
            <div class="section-title">📶 Passo 2: Configurar Rede WiFi</div>
            <div class="form-group">
                <label>Nome do WiFi (SSID)</label>
                <div class="scan-wrapper">
                    <input list="wifi-list" name="ssid" id="ssid" required placeholder="Digite ou selecione o WiFi" autocomplete="off" value="%SAVED_SSID%">
                    <datalist id="wifi-list">
                        <!-- Carregado via botão de Scan -->
                    </datalist>
                    <button type="button" onclick="scanNetworks()" class="refresh-btn" id="scan-btn" title="Buscar Redes">🔄</button>
                </div>
            </div>
            <div class="form-group">
                <label>Senha do WiFi</label>
                <input type="password" name="password" id="password" required placeholder="Digite a senha">
            </div>
            <button type="submit" class="submit-btn">Salvar e Conectar</button>
        </form>

        <!-- CARD 3: STATUS DE CONEXÃO -->
        <div class="section">
            <div class="section-title">📊 Status de Conexão</div>
            <div class="status-row">
                <span class="status-label">WiFi Local</span>
                <span class="status-value" id="status-wifi">
                    <span class="badge badge-red"></span> Desconectado
                </span>
            </div>
            <div class="status-row">
                <span class="status-label">Conexão com a API</span>
                <span class="status-value" id="status-api">
                    <span class="badge badge-red"></span> Offline
                </span>
            </div>
            <div class="status-row">
                <span class="status-label">Registro do Bot</span>
                <span class="status-value" id="status-bot">
                    <span class="badge badge-red"></span> Não Registrado
                </span>
            </div>
        </div>
    </div>

    <script>
        let savedSSID = "%SAVED_SSID%";

        function copySerial() {
            var text = document.getElementById("serial-id").innerText;
            navigator.clipboard.writeText(text).then(function() {
                alert("ID copiado!");
            });
        }

        function scanNetworks() {
            let btn = document.getElementById("scan-btn");
            let list = document.getElementById("wifi-list");
            btn.innerText = "⏳";
            btn.disabled = true;
            
            fetch('/scan')
                .then(response => response.json())
                .then(data => {
                    list.innerHTML = "";
                    if (data.length > 0) {
                        data.forEach(net => {
                            let opt = document.createElement("option");
                            opt.value = net;
                            list.appendChild(opt);
                        });
                        alert("Redes WiFi encontradas! Clique no campo de texto para ver a lista.");
                    } else {
                        alert("Nenhuma rede WiFi encontrada no alcance.");
                    }
                    btn.innerText = "🔄";
                    btn.disabled = false;
                })
                .catch(err => {
                    console.error("Erro ao escanear redes:", err);
                    alert("Erro ao buscar redes WiFi próximas.");
                    btn.innerText = "🔄";
                    btn.disabled = false;
                });
        }

        function updateStatus() {
            fetch('/status')
                .then(response => response.json())
                .then(data => {
                    // WiFi Status
                    let wifiEl = document.getElementById("status-wifi");
                    if (data.wifi_connected) {
                        wifiEl.innerHTML = '<span class="badge badge-green"></span> Conectado (' + data.wifi_ip + ')';
                    } else {
                        wifiEl.innerHTML = '<span class="badge badge-red"></span> Desconectado';
                    }

                    // API Status
                    let apiEl = document.getElementById("status-api");
                    if (data.api_online) {
                        apiEl.innerHTML = '<span class="badge badge-green"></span> Conectada';
                    } else {
                        apiEl.innerHTML = '<span class="badge badge-red"></span> Offline';
                    }

                    // Telegram Bot Status
                    let botEl = document.getElementById("status-bot");
                    let botLink = document.getElementById("bot-link");
                    
                    if (data.bot_username && data.bot_username !== "BotDesconhecido" && data.bot_username !== "Desconectado") {
                        botLink.href = "https://t.me/" + data.bot_username;
                    } else {
                        botLink.href = "https://t.me/";
                    }

                    if (data.bot_registered) {
                        botEl.innerHTML = '<span class="badge badge-green"></span> Ativo';
                    } else {
                        botEl.innerHTML = '<span class="badge badge-orange"></span> Pendente';
                    }
                })
                .catch(err => console.error("Erro ao ler status:", err));
        }

        updateStatus();
        setInterval(updateStatus, 10000); // Polling mais leve a cada 10 segundos
    </script>
</body>
</html>
)rawliteral";

// Pisca o Flash sem travar o processamento
void updateAlarme() {
  if (alarme_ativo) {
    if (millis() - alarme_inicio_ms > alarme_duracao_ms) {
      alarme_ativo = false;
      digitalWrite(FLASH_GPIO_NUM, LOW); // Garante que desliga
      Serial.println("Alarme visual encerrado.");
    } else {
      if (millis() - ultimo_pisca_ms > 300) {
        ultimo_pisca_ms = millis();
        estado_flash = (estado_flash == LOW) ? HIGH : LOW;
        digitalWrite(FLASH_GPIO_NUM, estado_flash);
      }
    }
  }
}

// Checa status da API e se o bot está registrado
void checkAPIStatus() {
  if (is_transmitting) return; // Evita colisão de rede/TLS com upload de fotos
  
  if (network_mutex != NULL) {
    if (xSemaphoreTake(network_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
      Serial.println("checkAPIStatus: não foi possível obter o mutex de rede.");
      return;
    }
  }

  api_check_active = true;

  // Aguarda 500ms para estabilização da heap após liberação de conexões anteriores
  delay(500);

  Serial.printf("[Status] Iniciando verificação. Free Heap: %d | Min Heap: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());

  if (api_url.length() == 0) {
    api_online = false;
    api_check_active = false;
    if (network_mutex != NULL) xSemaphoreGive(network_mutex);
    return;
  }

  WiFiClientSecure client;
  client.setInsecure(); // Ignora a validação do certificado SSL para compatibilidade da Render

  HTTPClient http;
  http.setTimeout(25000); // Aumentado para 25 segundos para tolerar lentidão do Render sob carga

  Serial.println("Checando status da API (verificando se o Render acordou)...");

  // 1. Verificar se a API está online e ler dados do Bot
  if (http.begin(client, api_url + "/config")) {
    http.setReuse(false);
    http.addHeader("Connection", "close");
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      int startIdx = payload.indexOf("\"bot_username\":\"");
      if (startIdx != -1) {
        api_online = true;
        startIdx += 16;
        int endIdx = payload.indexOf("\"", startIdx);
        if (endIdx != -1) {
          String temp_bot = payload.substring(startIdx, endIdx);
          if (status_mutex != NULL && xSemaphoreTake(status_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
            bot_username = temp_bot;
            xSemaphoreGive(status_mutex);
          }
        }
        String local_bot = "Desconectado";
        if (status_mutex != NULL && xSemaphoreTake(status_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
          local_bot = bot_username;
          xSemaphoreGive(status_mutex);
        }
        Serial.println("API Online! Bot do Telegram: @" + local_bot);
      } else {
        Serial.println("Erro: Resposta inválida da API. Rede WiFi pode exigir login (Portal Cativo).");
        api_online = false;
      }
    } else {
      Serial.printf("API offline ou em cold start (acordando). Código HTTP: %d | Free Heap: %d bytes\n", httpCode, ESP.getFreeHeap());
      api_online = false;
    }
    http.end();
  } else {
    Serial.println("Falha ao iniciar conexão HTTP para checagem.");
    api_online = false;
  }

  // Encerra a conexão anterior e dá um tempo para a heap respirar antes da próxima conexão TLS
  client.stop();
  delay(200);

  // 2. Verificar se o dispositivo está registrado no bot
  if (api_online && http.begin(client, api_url + "/status-dispositivo/" + device_id)) {
    http.setReuse(false);
    http.addHeader("Connection", "close");
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      if (payload.indexOf("\"registrado\":true") != -1) {
        bot_registered = true;
        Serial.println("Dispositivo já registrado no bot.");
      } else {
        bot_registered = false;
        Serial.println("Dispositivo pendente de registro no bot do Telegram.");
      }
    } else {
      Serial.printf("Erro ao checar registro. Código HTTP: %d | Free Heap: %d bytes\n", httpCode, ESP.getFreeHeap());
      bot_registered = false;
    }
    http.end();
  }
  
  client.stop(); // Garante o encerramento da conexão TLS e liberação dos buffers de heap
  api_check_active = false;

  Serial.printf("[Status] Concluído. Free Heap: %d\n", ESP.getFreeHeap());

  if (network_mutex != NULL) xSemaphoreGive(network_mutex);
}

// Task paralela FreeRTOS para processar conexões de rede em segundo plano (Core 0)
void statusTask(void * pvParameters) {
  while (true) {
    if (wifi_connected) {
      checkAPIStatus();
    } else {
      api_online = false;
      bot_registered = false;
    }
    
    // Se tudo já estiver conectado e validado, verifica apenas a cada 10 minutos (600s) para evitar fragmentar a heap
    if (wifi_connected && api_online && bot_registered) {
      vTaskDelay(pdMS_TO_TICKS(600000));
    } else {
      vTaskDelay(pdMS_TO_TICKS(15000));
    }
  }
}

// Classe auxiliar para transmitir o payload multipart sem alocar um buffer contíguo gigante
class MultipartStream : public Stream {
private:
  String _head;
  uint8_t* _fb_buf;
  size_t _fb_len;
  String _tail;
  
  size_t _head_pos = 0;
  size_t _fb_pos = 0;
  size_t _tail_pos = 0;

  size_t readBytesInternal(uint8_t *buffer, size_t length) {
    size_t bytesRead = 0;
    
    // 1. Ler cabeçalho
    if (_head_pos < _head.length() && bytesRead < length) {
      size_t toRead = _head.length() - _head_pos;
      if (toRead > (length - bytesRead)) toRead = length - bytesRead;
      memcpy(buffer + bytesRead, _head.c_str() + _head_pos, toRead);
      _head_pos += toRead;
      bytesRead += toRead;
    }
    
    // 2. Ler buffer da imagem
    if (_fb_pos < _fb_len && bytesRead < length) {
      size_t toRead = _fb_len - _fb_pos;
      if (toRead > (length - bytesRead)) toRead = length - bytesRead;
      memcpy(buffer + bytesRead, _fb_buf + _fb_pos, toRead);
      _fb_pos += toRead;
      bytesRead += toRead;
    }
    
    // 3. Ler rodapé de fechamento da boundary
    if (_tail_pos < _tail.length() && bytesRead < length) {
      size_t toRead = _tail.length() - _tail_pos;
      if (toRead > (length - bytesRead)) toRead = length - bytesRead;
      memcpy(buffer + bytesRead, _tail.c_str() + _tail_pos, toRead);
      _tail_pos += toRead;
      bytesRead += toRead;
    }
    
    return bytesRead;
  }

public:
  MultipartStream(const String& head, uint8_t* fb_buf, size_t fb_len, const String& tail)
      : _head(head), _fb_buf(fb_buf), _fb_len(fb_len), _tail(tail) {}

  int available() override {
    return (_head.length() - _head_pos) + (_fb_len - _fb_pos) + (_tail.length() - _tail_pos);
  }

  int read() override {
    if (_head_pos < _head.length()) {
      return _head[_head_pos++];
    }
    if (_fb_pos < _fb_len) {
      return _fb_buf[_fb_pos++];
    }
    if (_tail_pos < _tail.length()) {
      return _tail[_tail_pos++];
    }
    return -1;
  }

  int peek() override {
    if (_head_pos < _head.length()) {
      return _head[_head_pos];
    }
    if (_fb_pos < _fb_len) {
      return _fb_buf[_fb_pos];
    }
    if (_tail_pos < _tail.length()) {
      return _tail[_tail_pos];
    }
    return -1;
  }

  size_t readBytes(char *buffer, size_t length) override {
    return readBytesInternal((uint8_t*)buffer, length);
  }
  
  size_t readBytes(uint8_t *buffer, size_t length) override {
    return readBytesInternal(buffer, length);
  }
  
  size_t write(uint8_t) override { return 0; }
  size_t write(const uint8_t *, size_t) override { return 0; }
};

// Captura uma foto da câmera e faz upload via multipart/form-data
void captureAndUpload(bool is_test = false) {
  if (network_mutex != NULL) {
    // Aguarda até 40 segundos para liberar a rede (ex: se o Render estiver no cold start e prendendo o mutex na outra task)
    if (xSemaphoreTake(network_mutex, pdMS_TO_TICKS(40000)) != pdTRUE) {
      Serial.println("Erro: Timeout aguardando liberação do mutex de rede para upload!");
      return;
    }
  }

  is_transmitting = true;

  // Aguarda 500ms para liberação completa da heap pela tarefa em segundo plano
  delay(500);

  Serial.printf("[Upload] Iniciando captura. Free Heap: %d | Min Heap: %d\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());

  Serial.println("Capturando foto...");
  
  // Captura usando luz ambiente para evitar picos de corrente do Flash LED que derrubam o Wi-Fi
  camera_fb_t *fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("Aviso: Falha na captura. Limpando buffer e retentando...");
    delay(300);
    fb = esp_camera_fb_get();
    
    if (!fb) {
      Serial.println("Erro: Segunda tentativa falhou. Aguardando estabilização...");
      delay(500);
      fb = esp_camera_fb_get();
    }
  }

  if (!fb) {
    Serial.println("Erro: Falha crítica ao capturar imagem em todas as tentativas!");
    is_transmitting = false;
    if (network_mutex != NULL) xSemaphoreGive(network_mutex);
    return;
  }

  // Pequena pausa (300ms) para estabilização de tensão e rádio após a captura de imagem
  delay(300);

  Serial.printf("[Upload] Foto capturada (%d bytes). Free Heap: %d\n", fb->len, ESP.getFreeHeap());

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(50000);

  Serial.println("Enviando foto para a API...");

  String boundary = "----ESP32CamBoundary123456";
  String head =
      "--" + boundary +
      "\r\nContent-Disposition: form-data; name=\"dispositivo_id\"\r\n\r\n" +
      device_id + "\r\n--" + boundary +
      "\r\nContent-Disposition: form-data; name=\"is_test\"\r\n\r\n" +
      (is_test ? "true" : "false") + "\r\n--" + boundary +
      "\r\nContent-Disposition: form-data; name=\"file\"; "
      "filename=\"photo.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
  String tail = "\r\n--" + boundary + "--\r\n";

  uint32_t totalLen = head.length() + fb->len + tail.length();

  if (http.begin(client, api_url + "/detectar")) {
    http.setReuse(false);
    http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
    http.addHeader("Connection", "close");

    MultipartStream mpStream(head, fb->buf, fb->len, tail);
    int httpCode = http.sendRequest("POST", &mpStream, totalLen);

    if (httpCode <= 0) {
      Serial.printf("Falha (tentativa 1): %s. Reconectando WiFi e retentando...\n", http.errorToString(httpCode).c_str());
      http.end();
      client.stop();

      // Reconecta o Wi-Fi para resetar o stack TCP e limpar estado corrompido
      WiFi.disconnect(false);
      delay(500);
      WiFi.reconnect();
      unsigned long reconnStart = millis();
      while (WiFi.status() != WL_CONNECTED && millis() - reconnStart < 10000) {
        delay(300);
      }

      if (WiFi.status() == WL_CONNECTED) {
        WiFiClientSecure client2;
        client2.setInsecure();
        if (http.begin(client2, api_url + "/detectar")) {
          http.setReuse(false);
          http.addHeader("Content-Type", "multipart/form-data; boundary=" + boundary);
          http.addHeader("Connection", "close");
          MultipartStream mpStreamRetry(head, fb->buf, fb->len, tail);
          httpCode = http.sendRequest("POST", &mpStreamRetry, totalLen);
          client2.stop();
        }
      } else {
        Serial.println("Falha na reconexão WiFi. Abortando retentativa.");
      }
    }

    if (httpCode > 0) {
      Serial.printf("Resposta HTTP: %d\n", httpCode);
      if (httpCode == HTTP_CODE_OK) {
        String response = http.getString();
        Serial.println("JSON API: " + response);
        if (response.indexOf("\"acionar_alarme\":true") != -1) {
          Serial.println("🚨 ATENÇÃO: Animal Peçonhento Detectado! Acionando flash LED...");
          alarme_ativo = true;
          alarme_inicio_ms = millis();
          ultimo_pisca_ms = 0;
        }
      }
    } else {
      Serial.printf("Falha definitiva: %s\n", http.errorToString(httpCode).c_str());
    }
    http.end();
  } else {
    Serial.println("Não foi possível iniciar conexão HTTP.");
  }

  client.stop();
  esp_camera_fb_return(fb);

  // Pisca o LED uma vez após o upload para sinalizar conclusão da captura
  // (feito aqui, depois de encerrar o socket, para não interferir na transmissão Wi-Fi)
  digitalWrite(FLASH_GPIO_NUM, HIGH);
  delay(80);
  digitalWrite(FLASH_GPIO_NUM, LOW);

  Serial.printf("[Upload] Concluído. Free Heap: %d\n", ESP.getFreeHeap());

  is_transmitting = false;
  if (network_mutex != NULL) xSemaphoreGive(network_mutex);
}

void setup() {
  // Configura o limite do Watchdog para 15 segundos para tolerar o handshake TLS
  #if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR >= 3
    esp_task_wdt_config_t wdt_config = {
      .timeout_ms = 15000,
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
      .trigger_panic = true
    };
    esp_task_wdt_init(&wdt_config);
  #else
    esp_task_wdt_init(15, true);
  #endif

  // Inicializa os mutexes para evitar concorrência TLS e de status
  network_mutex = xSemaphoreCreateMutex();
  status_mutex = xSemaphoreCreateMutex();

  // Desativa detector de brownout (evita reinicializações abruptas sob picos de
  // energia do WiFi/Câmera)
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  // LED Flash
  pinMode(FLASH_GPIO_NUM, OUTPUT);
  digitalWrite(FLASH_GPIO_NUM, LOW);

  Serial.begin(115200);
  Serial.println("\nInicializando Dispositivo...");

  // Inicializa preferências e verifica o motivo do reset via RTC RAM
  preferences.begin("venom-esp", false);
  
  esp_reset_reason_t reason = esp_reset_reason();
  
  // Se a assinatura RTC estiver ativa e NÃO for um reset por software,
  // significa que o botão físico RST foi pressionado enquanto a placa estava ligada.
  if (rtc_magic == 0xDEADC0DE && reason != ESP_RST_SW) {
    preferences.clear();
    Serial.println("⚠️ Botão físico RST pressionado! Credenciais WiFi apagadas!");
  } else {
    Serial.println("Mantendo credenciais WiFi salvas.");
  }

  // Grava a assinatura RTC para o próximo boot
  rtc_magic = 0xDEADC0DE;

  // Carrega configurações salvas (se restarem)
  wifi_ssid = preferences.getString("ssid", "");
  wifi_pass = preferences.getString("password", "");
  api_url = preferences.getString("api_url", "https://api-esp32cam-deteccao-animais-peconhentos.onrender.com");
  if (api_url.length() == 0 || api_url == "http://localhost:8000" || api_url == "http://192.168.1.100:8000" || api_url.indexOf("onrender.com") == -1) {
    api_url = "https://api-esp32cam-deteccao-animais-peconhentos.onrender.com";
  }
  preferences.end();

  // Inicializa o WiFi em modo Station e aguarda a inicialização assíncrona do driver para ler o MAC Address correto
  WiFi.mode(WIFI_STA);
  WiFi.setTxPower(WIFI_POWER_17dBm); // Limita potência de transmissão para economizar consumo de corrente de pico
  WiFi.setSleep(false);              // Desativa o sleep do modem Wi-Fi para evitar falhas de envio de dados
  int retries = 0;
  String macStr = "";
  while (retries < 15) {
    macStr = WiFi.macAddress();
    if (macStr != "00:00:00:00:00:00" && macStr != "") {
      break;
    }
    delay(100);
    retries++;
  }
  macStr.toUpperCase();
  device_id = "ESP32-CAM-" + macStr;
  Serial.println("Device ID único gerado: " + device_id);

  // Inicialização do módulo de câmera
  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;  // VGA (640x480) - tamanho ideal para transmissão TLS estável (~16-22KB)
    config.jpeg_quality = 12;           // Qualidade equilibrada para nitidez e tamanho de payload
    config.fb_count = 2;
    config.grab_mode = CAMERA_GRAB_LATEST;
  } else {
    config.frame_size = FRAMESIZE_VGA;  // VGA se não houver PSRAM
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Câmera falhou ao iniciar: 0x%x\n", err);
  } else {
    Serial.println("Câmera pronta.");
  }

  // Tenta conectar ao WiFi salvo (se houver) de forma síncrona com timeout de 15 segundos
  wifi_connected = false;
  if (wifi_ssid.length() > 0) {
    Serial.printf("Conectando ao WiFi salvo: %s...\n", wifi_ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.setTxPower(WIFI_POWER_17dBm); // Limita potência durante a conexão para evitar queda de tensão
    WiFi.setSleep(false);              // Desativa o sleep do modem Wi-Fi para evitar falhas de envio de dados
    WiFi.begin(wifi_ssid.c_str(), wifi_pass.c_str());
    
    unsigned long start_attempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start_attempt < 15000) {
      delay(500);
      Serial.print(".");
    }
    Serial.println();
    
    if (WiFi.status() == WL_CONNECTED) {
      wifi_connected = true;
      Serial.print("Conectado com sucesso! Endereço IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Falha ao conectar ao WiFi salvo. Liberando rádio para o Portal.");
      WiFi.disconnect(true); // Desativa escaneamento de segundo plano para estabilizar o AP
    }
  }

  // Ativa o Portal de Configuração VenomESP apenas se NÃO estiver conectado ao WiFi
  if (!wifi_connected) {
    WiFi.mode(WIFI_AP_STA); // Mantém STA ativo para permitir varredura de redes próximas
    WiFi.setTxPower(WIFI_POWER_17dBm); // Limita potência de transmissão para economizar consumo de corrente de pico
    WiFi.setSleep(false);              // Desativa o sleep do modem Wi-Fi
    WiFi.softAP("VenomESP", "venomesp");
    ap_active = true;
    Serial.print("Access Point ativo! Conecte-se em 'VenomESP' (senha: venomesp) "
                 "e acesse: http://192.168.4.1 ou http://venom.esp\n");

    // Inicia Servidor DNS para Captive Portal
    dnsServer.start(53, "*", WiFi.softAPIP());
  }

  // Inicia as rotas do WebServer local
  server.on("/", HTTP_GET, []() {
    String html = INDEX_HTML;
    html.replace("%DEVICE_ID%", device_id);
    html.replace("%SAVED_SSID%", wifi_ssid);
    server.send(200, "text/html", html);
  });

  server.on("/scan", HTTP_GET, []() {
    int n = WiFi.scanNetworks();
    String json = "[";
    int count = 0;
    for (int i = 0; i < n; ++i) {
      String ssid = WiFi.SSID(i);
      if (ssid.length() == 0) continue;
      
      bool duplicate = false;
      for (int j = 0; j < i; ++j) {
        if (WiFi.SSID(j) == ssid) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) {
        if (count > 0) json += ",";
        json += "\"" + ssid + "\"";
        count++;
      }
    }
    json += "]";
    server.send(200, "application/json", json);
  });

  server.on("/save", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
      wifi_ssid = server.arg("ssid");
      wifi_pass = server.arg("password");

      // Salva os valores na memória não-volátil (Flash)
      preferences.begin("venom-esp", false);
      preferences.putString("ssid", wifi_ssid);
      preferences.putString("password", wifi_pass);
      preferences.putString("api_url", "https://api-esp32cam-deteccao-animais-peconhentos.onrender.com");
      preferences.end();

      server.send(
          200, "text/html",
          "<html><head><meta charset='UTF-8'></head><body "
          "style='font-family:sans-serif; text-align:center; padding-top: "
          "50px; background:#0b0f19; color:white;'><h3>Configurações salvas "
          "com sucesso!</h3><p>O dispositivo irá reiniciar para tentar se "
          "conectar à rede WiFi informada.</p></body></html>");
      delay(2000);
      ESP.restart();
    } else {
      server.send(400, "text/plain", "Campos ausentes.");
    }
  });

  server.on("/status", HTTP_GET, []() {
    String local_bot = "Desconectado";
    if (status_mutex != NULL && xSemaphoreTake(status_mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
      local_bot = bot_username;
      xSemaphoreGive(status_mutex);
    }
    String json = "{";
    json += "\"device_id\":\"" + device_id + "\",";
    json += "\"saved_ssid\":\"" + wifi_ssid + "\",";
    json += "\"saved_api_url\":\"" + api_url + "\",";
    json +=
        "\"wifi_connected\":" + String(wifi_connected ? "true" : "false") + ",";
    json += "\"wifi_ip\":\"" + WiFi.localIP().toString() + "\",";
    json += "\"api_online\":" + String(api_online ? "true" : "false") + ",";
    json +=
        "\"bot_registered\":" + String(bot_registered ? "true" : "false") + ",";
    json += "\"bot_username\":\"" + local_bot + "\"";
    json += "}";
    server.send(200, "application/json", json);
  });

  // Rota não encontrada: Redirecionamento amigável de Captive Portal
  server.onNotFound([]() {
    server.sendHeader("Location", "http://192.168.4.1/", true);
    server.send(302, "text/plain", "");
  });

  // Cria a tarefa em segundo plano no Core 1 para processar o status da API de forma assíncrona
  xTaskCreatePinnedToCore(
    statusTask,
    "StatusTask",
    8192,
    NULL,
    1, // Prioridade 1
    NULL,
    1  // Pinado ao Core 1 para evitar conflito/starvation no Core 0 do WiFi/sistema
  );

  server.begin();

  // Pisca o flash uma única vez (500ms) para sinalizar que o dispositivo está pronto
  digitalWrite(FLASH_GPIO_NUM, HIGH);
  delay(500);
  digitalWrite(FLASH_GPIO_NUM, LOW);
}

void loop() {
  // Processamento do DNS e do Servidor Web
  if (ap_active) {
    dnsServer.processNextRequest();
  }
  server.handleClient();

  // Atualiza estado de conexão
  wifi_connected = (WiFi.status() == WL_CONNECTED);

  // Se conectou ao WiFi local, desativa o Access Point próprio para ocultá-lo (protegido por mutex)
  if (wifi_connected && ap_active) {
    if (network_mutex != NULL && xSemaphoreTake(network_mutex, pdMS_TO_TICKS(5000)) == pdTRUE) {
      dnsServer.stop();
      WiFi.mode(WIFI_STA); // Transiciona limpamente para apenas estação, desligando o AP
      WiFi.setTxPower(WIFI_POWER_17dBm); // Mantém a potência reduzida
      ap_active = false;
      Serial.println("Conexão WiFi estabelecida! Desativando Access Point 'VenomESP' local e parando DNS.");
      xSemaphoreGive(network_mutex);
    }
  }

  unsigned long now = millis();

  // Controla o pisca do LED Flash se o alarme estiver ligado (Não-bloqueante)
  updateAlarme();

  // Envia a primeira foto de teste assim que o WiFi conectar, a API estiver online e o bot estiver registrado
  if (wifi_connected && api_online && bot_registered && !test_photo_sent) {
    test_photo_sent = true;
    Serial.println("Enviando foto de teste de inicialização...");
    captureAndUpload(true); // is_test = true
    last_capture_ms = millis(); // Registra tempo de término para controle do intervalo
  }

  // Envio periódico de imagens (apenas se registrado)
  if (wifi_connected && api_online && bot_registered && test_photo_sent &&
      (millis() - last_capture_ms >= capture_interval_ms)) {
    captureAndUpload(false); // is_test = false
    last_capture_ms = millis(); // Registra tempo de término após o upload finalizar
  }

  delay(10); // Pequena folga para a CPU do ESP32
}
