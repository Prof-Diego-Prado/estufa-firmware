/*
 * Dashboard de Controle - Estufa Automatizada (ESP32-CAM AI-Thinker)
 * ---------------------------------------------------------------------
 * - Lê DHT11 (temperatura/umidade do ar)
 * - Lê 2 sensores de umidade do solo via ADS1115 (I2C)
 * - Controla bomba de irrigação (relé) e uma fita de LED endereçável
 *   WS2812B acesa em tom roxo/violeta (aproximação visual de UV — não é
 *   UV-C real, não tem efeito germicida)
 * - Transmite vídeo da ESP32-CAM (porta 81, /stream)
 * - Serve um dashboard web (porta 80) com leituras e controles em tempo real
 *
 * Bibliotecas necessárias (Arduino Library Manager):
 *   - DHT sensor library (Adafruit)
 *   - Adafruit Unified Sensor
 *   - Adafruit ADS1X15
 *   - Adafruit NeoPixel
 *
 * Placa: "AI Thinker ESP32-CAM" no Boards Manager do ESP32 (Espressif Systems)
 * Veja README.md para pinagem, fiação e calibração.
 */

#include "esp_camera.h"
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <DHT.h>
#include <Adafruit_ADS1X15.h>
#include <Adafruit_NeoPixel.h>

#include "camera_pins.h"
#include "dashboard_html.h"
#include "stream_server.h"
#include "secrets.h" // WIFI_SSID, WIFI_PASSWORD, AUTH_USER, AUTH_PASS — veja secrets.h.example

// ---------------------- CONFIGURAÇÃO: EDITE AQUI ----------------------
const char *HOSTNAME = "estufa"; // acesse em http://estufa.local

// WIFI_SSID/WIFI_PASSWORD e AUTH_USER/AUTH_PASS agora vêm de secrets.h
// (arquivo local, fora do Git — veja secrets.h.example para o modelo).
// Necessário porque, com o acesso remoto (túnel), este painel controla
// atuadores (bomba/relé) e fica alcançável pela internet. Troque a senha
// padrão antes de expor a placa fora da rede local.

// Pinos livres na ESP32-CAM (não usados pela câmera). Veja README.md
// para a lista completa de pinos livres/cuidados antes de trocar algo aqui.
#define DHTPIN           4  // pisca rapidinho o LED de flash on-board a cada leitura (ignorar)
#define DHTTYPE         DHT11
#define I2C_SDA         13
#define I2C_SCL         15
#define PUMP_RELAY_PIN   2  // também liga o LED vermelho pequeno on-board (ignorar)
#define LED_STRIP_PIN   12  // DATA da fita WS2812B

// A maioria dos módulos de relé de 1/2 canais é ativa em nível BAIXO.
// Se o seu módulo for ativo em nível ALTO, troque LOW<->HIGH abaixo.
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// Fita de LED endereçável (WS2812B/Neopixel) usada como luz "UV" — na
// prática é uma aproximação visual em roxo/violeta, não UV-C real.
// Ajuste NUM_LEDS para o tamanho real da sua fita.
#define NUM_LEDS 75
Adafruit_NeoPixel ledStrip(NUM_LEDS, LED_STRIP_PIN, NEO_GRB + NEO_KHZ800);
const uint32_t UV_COLOR = Adafruit_NeoPixel::Color(148, 0, 211); // violeta, mais próximo visualmente de UV
const uint8_t UV_BRIGHTNESS = 120; // 0-255

// Calibração dos sensores de umidade do solo (valores brutos do ADS1115).
// Ajuste conforme o README: leia o valor com o sensor seco e com ele em água.
int SOIL1_DRY = 17000, SOIL1_WET = 8000;
int SOIL2_DRY = 17000, SOIL2_WET = 8000;

// Irrigação automática
const unsigned long PUMP_RUN_TIME  = 5000UL;   // duração de cada ciclo (ms)
const unsigned long PUMP_COOLDOWN  = 60000UL;  // espera mínima entre ciclos (ms)
// -----------------------------------------------------------------------

DHT dht(DHTPIN, DHTTYPE);
Adafruit_ADS1115 ads;
WebServer server(80);
bool adsFound = false;

float temperature = NAN, humidity = NAN;
int soil1Pct = 0, soil2Pct = 0;
bool pumpState = false, uvState = false;
bool autoIrrigation = false;
int soilThreshold = 30;

unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL = 2000UL;

bool pumpAutoRunning = false;
unsigned long pumpStartTime = 0;
unsigned long lastPumpCycle = 0;

// ------------------------------- Câmera -------------------------------
bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  if (psramFound()) {
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 15;
    config.fb_count = 1;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Falha ao iniciar a câmera: 0x%x\n", err);
    return false;
  }

  // Corrige a orientação da imagem conforme a câmera fica montada
  // fisicamente na estufa. Se ainda estiver espelhada (invertida na
  // horizontal), descomente a linha do set_hmirror abaixo.
  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_vflip(s, 1);   // corrige imagem de cabeça para baixo
    // s->set_hmirror(s, 1);
  }

  return true;
}

// ------------------------------ Sensores -------------------------------
void readSensors() {
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    Serial.println("DHT11: leitura falhou (checksum/timeout) — confira fiação (VCC=3.3V, GND, DATA=GPIO4)");
  } else {
    Serial.printf("DHT11 ok: %.1f C, %.1f %%\n", t, h);
  }
  if (!isnan(h)) humidity = h;
  if (!isnan(t)) temperature = t;

  // Sem o ADS1115 conectado, não tenta ler (evita travar o barramento I2C
  // e, com isso, atrasar o resto do firmware, inclusive o dashboard).
  if (!adsFound) return;

  int16_t raw1 = ads.readADC_SingleEnded(0);
  int16_t raw2 = ads.readADC_SingleEnded(1);
  soil1Pct = constrain(map(raw1, SOIL1_DRY, SOIL1_WET, 0, 100), 0, 100);
  soil2Pct = constrain(map(raw2, SOIL2_DRY, SOIL2_WET, 0, 100), 0, 100);
}

void setPump(bool on) {
  pumpState = on;
  digitalWrite(PUMP_RELAY_PIN, on ? RELAY_ON : RELAY_OFF);
}

void setUV(bool on) {
  uvState = on;
  ledStrip.fill(on ? UV_COLOR : 0);
  ledStrip.show();
}

// Liga a bomba por PUMP_RUN_TIME quando o solo mais seco cai abaixo do
// limite configurado, respeitando um tempo mínimo de espera entre ciclos.
void handleAutoIrrigation() {
  if (!autoIrrigation) return;
  unsigned long now = millis();

  if (pumpAutoRunning) {
    if (now - pumpStartTime >= PUMP_RUN_TIME) {
      setPump(false);
      pumpAutoRunning = false;
      lastPumpCycle = now;
    }
    return;
  }

  int driest = min(soil1Pct, soil2Pct);
  if (driest < soilThreshold && (now - lastPumpCycle >= PUMP_COOLDOWN)) {
    setPump(true);
    pumpAutoRunning = true;
    pumpStartTime = now;
  }
}

// ------------------------------ Rotas HTTP ------------------------------
bool checkAuth() {
  if (!server.authenticate(AUTH_USER, AUTH_PASS)) {
    server.requestAuthentication();
    return false;
  }
  return true;
}

void handleRoot() {
  if (!checkAuth()) return;
  server.send_P(200, "text/html", DASHBOARD_HTML);
}

// O navegador manda um pedido OPTIONS de checagem ("preflight") antes de
// qualquer fetch() de outra origem que inclua o cabeçalho Authorization
// (ex.: o painel do GitHub Pages buscando dados via túnel).
void handleDataOptions() {
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.sendHeader("Access-Control-Allow-Methods", "GET, OPTIONS");
  server.sendHeader("Access-Control-Allow-Headers", "Authorization");
  server.send(204);
}

void handleData() {
  if (!checkAuth()) return;
  server.sendHeader("Access-Control-Allow-Origin", "*");
  String json = "{";
  json += "\"temperatura\":" + String(isnan(temperature) ? -1 : temperature, 1) + ",";
  json += "\"umidade_ar\":" + String(isnan(humidity) ? -1 : humidity, 1) + ",";
  json += "\"solo1\":" + String(soil1Pct) + ",";
  json += "\"solo2\":" + String(soil2Pct) + ",";
  json += "\"bomba\":" + String(pumpState ? "true" : "false") + ",";
  json += "\"uv\":" + String(uvState ? "true" : "false") + ",";
  json += "\"auto\":" + String(autoIrrigation ? "true" : "false") + ",";
  json += "\"limite\":" + String(soilThreshold);
  json += "}";
  server.send(200, "application/json", json);
}

void handlePump() {
  if (!checkAuth()) return;
  if (server.hasArg("state")) {
    setPump(server.arg("state") == "on");
    autoIrrigation = false; // controle manual assume prioridade
    pumpAutoRunning = false;
  }
  server.send(200, "text/plain", "OK");
}

void handleUV() {
  if (!checkAuth()) return;
  if (server.hasArg("state")) setUV(server.arg("state") == "on");
  server.send(200, "text/plain", "OK");
}

void handleAuto() {
  if (!checkAuth()) return;
  if (server.hasArg("state")) autoIrrigation = (server.arg("state") == "on");
  server.send(200, "text/plain", "OK");
}

void handleThreshold() {
  if (!checkAuth()) return;
  if (server.hasArg("value")) soilThreshold = server.arg("value").toInt();
  server.send(200, "text/plain", "OK");
}

// ---------------------------------- Setup --------------------------------
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(false);

  pinMode(PUMP_RELAY_PIN, OUTPUT);
  digitalWrite(PUMP_RELAY_PIN, RELAY_OFF);

  ledStrip.begin();
  ledStrip.setBrightness(UV_BRIGHTNESS);
  setUV(true); // fita liga sozinha ao iniciar e fica sempre acesa

  dht.begin();
  Wire.begin(I2C_SDA, I2C_SCL);
  adsFound = ads.begin();
  if (!adsFound) {
    Serial.println("ADS1115 não encontrado. Verifique a fiação I2C.");
  }

  if (!initCamera()) {
    Serial.println("Câmera não inicializada — reinicie a placa.");
  }

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false); // desliga o modo de economia de energia do rádio —
                        // sem isso, o servidor web fica lento/instável
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Conectando ao WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(400);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("Conectado! IP: ");
  Serial.println(WiFi.localIP());

  if (MDNS.begin(HOSTNAME)) {
    Serial.printf("Acesse via http://%s.local/\n", HOSTNAME);
  }

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/data", HTTP_OPTIONS, handleDataOptions);
  server.on("/pump", handlePump);
  server.on("/uv", handleUV);
  server.on("/auto", handleAuto);
  server.on("/threshold", handleThreshold);
  server.begin();

  startCameraServer();

  Serial.println("Dashboard pronto (porta 80) | Stream (porta 81)");
}

// ---------------------------------- Loop ---------------------------------
void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastSensorRead >= SENSOR_INTERVAL) {
    lastSensorRead = now;
    readSensors();
  }

  handleAutoIrrigation();
}
