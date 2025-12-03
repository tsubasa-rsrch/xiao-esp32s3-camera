/*
 * 翼の目（キッチン）v1.3 - 静的IP版
 * Xiao ESP32S3 Sense用
 * IP: 10.0.72.203（固定）
 * エンドポイント:
 *   /        - ステータス画面
 *   /stream  - カメラMJPEGストリーム
 *   /audio   - マイク音量レベル (JSON) ※AC成分計測
 *   /capture - 静止画撮影 (JPEG)
 *   /sd      - SDカード情報
 *   /save    - カメラ画像をSDに保存
 */

#include "esp_camera.h"
#include "ESP_I2S.h"
#include <WiFi.h>
#include <WebServer.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"

// WiFi設定
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";

// 静的IP設定（キッチン: 10.0.72.203）
IPAddress local_IP(192, 168, 1, 100);
IPAddress gateway(10, 0, 0, 1);      // 修正: 実際のゲートウェイ
IPAddress subnet(255, 255, 0, 0);    // 修正: /16 サブネット
IPAddress dns(10, 0, 0, 1);          // 修正: DNSもゲートウェイと同じ

// カメラピン（Xiao ESP32S3 Sense）
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39
#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

// SDカードピン（Xiao ESP32S3 Sense）
#define SD_CS   21

// PDMマイクピン
const int8_t I2S_CLK = 42;
const int8_t I2S_DIN = 41;
const uint32_t SAMPLE_RATE = 16000;

I2SClass I2S;
WebServer server(80);
bool cameraReady = false;
bool micReady = false;
bool sdReady = false;
int photoCount = 0;

// カメラ初期化
bool initCamera() {
  camera_config_t config;
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
  config.frame_size = FRAMESIZE_QVGA;  // 320x240
  config.jpeg_quality = 12;
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_DRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  return (esp_camera_init(&config) == ESP_OK);
}

// マイク初期化
bool initMic() {
  I2S.setPinsPdmRx(I2S_CLK, I2S_DIN);
  return I2S.begin(I2S_MODE_PDM_RX, SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
}

// SDカード初期化
bool initSD() {
  if (!SD.begin(SD_CS)) {
    return false;
  }
  uint8_t cardType = SD.cardType();
  if (cardType == CARD_NONE) {
    return false;
  }
  return true;
}

// MJPEGストリーム
void handleStream() {
  if (!cameraReady) {
    server.send(503, "text/plain", "Camera not ready");
    return;
  }

  WiFiClient client = server.client();
  String response = "HTTP/1.1 200 OK\r\n";
  response += "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
  client.print(response);

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) continue;

    client.printf("--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %d\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.print("\r\n");

    esp_camera_fb_return(fb);
    if (!client.connected()) break;
    delay(30);
  }
}

// 静止画撮影
void handleCapture() {
  if (!cameraReady) {
    server.send(503, "text/plain", "Camera not ready");
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "text/plain", "Capture failed");
    return;
  }

  server.sendHeader("Content-Disposition", "inline; filename=capture.jpg");
  server.send_P(200, "image/jpeg", (const char*)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

// マイク音量取得（AC成分計測版）
void handleAudio() {
  if (!micReady) {
    server.send(503, "application/json", "{\"error\":\"Mic not ready\"}");
    return;
  }

  // 256サンプル読み取り
  const int NUM_SAMPLES = 256;
  int32_t samples[NUM_SAMPLES];
  int32_t sum = 0;
  int count = 0;

  // まずサンプルを収集
  for (int i = 0; i < NUM_SAMPLES; i++) {
    int sample = I2S.read();
    if (sample != 0 && sample != -1) {
      samples[count] = sample;
      sum += sample;
      count++;
    }
  }

  if (count == 0) {
    server.send(200, "application/json", "{\"avg\":0,\"peak\":0,\"level\":0,\"samples\":0}");
    return;
  }

  // DC成分（平均値）を計算
  int32_t dcOffset = sum / count;

  // AC成分（変動）を計算
  int32_t acSum = 0;
  int32_t peak = 0;
  for (int i = 0; i < count; i++) {
    int32_t ac = abs(samples[i] - dcOffset);  // DCオフセットを引いてAC成分を取得
    acSum += ac;
    if (ac > peak) peak = ac;
  }

  int avg = acSum / count;
  int level = map(avg, 0, 2000, 0, 100);  // AC成分用にスケール調整
  level = constrain(level, 0, 100);

  String json = "{\"avg\":" + String(avg) +
                ",\"peak\":" + String(peak) +
                ",\"dc\":" + String(dcOffset) +
                ",\"level\":" + String(level) +
                ",\"samples\":" + String(count) + "}";
  server.send(200, "application/json", json);
}

// 生のPCMデータ取得（スペクトログラム用）
void handleAudioRaw() {
  if (!micReady) {
    server.send(503, "text/plain", "Mic not ready");
    return;
  }

  const int SAMPLES = 16000;  // 1秒分（16kHz）
  static int16_t samples[SAMPLES];  // staticでスタック溢れ防止

  for (int i = 0; i < SAMPLES; i++) {
    samples[i] = (int16_t)I2S.read();
  }

  // バイナリで送信
  server.sendHeader("Content-Disposition", "inline; filename=audio.raw");
  server.send_P(200, "application/octet-stream", (const char*)samples, SAMPLES * 2);
}

// SDカード情報
void handleSD() {
  if (!sdReady) {
    server.send(503, "application/json", "{\"error\":\"SD not ready\"}");
    return;
  }

  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  uint64_t usedBytes = SD.usedBytes() / (1024 * 1024);

  String json = "{\"size_mb\":" + String((uint32_t)cardSize) +
                ",\"used_mb\":" + String((uint32_t)usedBytes) +
                ",\"photos\":" + String(photoCount) + "}";
  server.send(200, "application/json", json);
}

// カメラ画像をSDに保存
void handleSave() {
  if (!cameraReady) {
    server.send(503, "application/json", "{\"error\":\"Camera not ready\"}");
    return;
  }
  if (!sdReady) {
    server.send(503, "application/json", "{\"error\":\"SD not ready\"}");
    return;
  }

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    server.send(500, "application/json", "{\"error\":\"Capture failed\"}");
    return;
  }

  // ファイル名を生成
  String filename = "/photo_" + String(photoCount) + ".jpg";

  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    esp_camera_fb_return(fb);
    server.send(500, "application/json", "{\"error\":\"File create failed\"}");
    return;
  }

  file.write(fb->buf, fb->len);
  file.close();
  esp_camera_fb_return(fb);

  photoCount++;

  String json = "{\"saved\":\"" + filename + "\",\"size\":" + String(fb->len) + "}";
  server.send(200, "application/json", json);
}

// ステータス画面
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>翼の目（キッチン）</title>";
  html += "<meta charset='utf-8'>";
  html += "<meta http-equiv='refresh' content='5'>";
  html += "<style>body{background:#111;color:#fff;font-family:sans-serif;text-align:center;padding:20px;}";
  html += ".ok{color:#0f0;}.ng{color:#f00;}img{max-width:100%;border:2px solid #333;}</style></head>";
  html += "<body><h1>翼の目（キッチン）v1.3 Static IP</h1>";
  html += "<p>IP: " + WiFi.localIP().toString() + "</p>";
  html += "<p>カメラ: <span class='" + String(cameraReady ? "ok'>OK" : "ng'>NG") + "</span></p>";
  html += "<p>マイク: <span class='" + String(micReady ? "ok'>OK" : "ng'>NG") + "</span></p>";
  html += "<p>SDカード: <span class='" + String(sdReady ? "ok'>OK" : "ng'>NG") + "</span></p>";
  if (sdReady) {
    html += "<p>SD容量: " + String((uint32_t)(SD.cardSize() / (1024 * 1024))) + " MB</p>";
  }
  html += "<h2>カメラ映像</h2>";
  html += "<img src='/stream'>";
  html += "<h2>エンドポイント</h2>";
  html += "<p><a href='/stream'>/stream</a> - MJPEGストリーム</p>";
  html += "<p><a href='/capture'>/capture</a> - 静止画撮影</p>";
  html += "<p><a href='/audio'>/audio</a> - マイク音量 (JSON)</p>";
  html += "<p><a href='/sd'>/sd</a> - SDカード情報</p>";
  html += "<p><a href='/save'>/save</a> - 画像をSDに保存</p>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== 翼の目（キッチン）v1.3 ===");
  Serial.println("Mode: Static IP (10.0.72.203)");

  // SDカード初期化（カメラより先に！）
  sdReady = initSD();
  Serial.printf("SDカード: %s\n", sdReady ? "OK" : "NG");
  if (sdReady) {
    Serial.printf("SD容量: %llu MB\n", SD.cardSize() / (1024 * 1024));
  }

  // カメラ初期化
  cameraReady = initCamera();
  Serial.printf("カメラ: %s\n", cameraReady ? "OK" : "NG");

  // マイク初期化
  micReady = initMic();
  Serial.printf("マイク: %s\n", micReady ? "OK" : "NG");

  // 静的IP設定
  if (!WiFi.config(local_IP, gateway, subnet, dns)) {
    Serial.println("静的IP設定に失敗");
  }

  // WiFi接続（DHCPで自動取得）
  Serial.printf("WiFi接続中: %s\n", ssid);
  WiFi.begin(ssid, password);

  int retry = 0;
  while (WiFi.status() != WL_CONNECTED && retry < 30) {
    delay(500);
    Serial.print(".");
    retry++;
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi接続成功！");
    Serial.print("ブラウザでアクセス: http://");
    Serial.println(WiFi.localIP());

    // ルート設定
    server.on("/", handleRoot);
    server.on("/stream", handleStream);
    server.on("/capture", handleCapture);
    server.on("/audio", handleAudio);
    server.on("/audio_raw", handleAudioRaw);
    server.on("/sd", handleSD);
    server.on("/save", handleSave);
    server.begin();
    Serial.println("サーバー開始");
  } else {
    Serial.println("\nWiFi接続失敗");
  }
}

void loop() {
  server.handleClient();
  delay(1);
}
