#include <Arduino.h>
#include "esp_system.h"
#include "esp_camera.h"
#include "esp_heap_caps.h"
#include <WiFi.h>
#include <WebServer.h>

#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15 // 14
#define SIOD_GPIO_NUM  4  // 1
#define SIOC_GPIO_NUM  5  // 2

#define Y2_GPIO_NUM 11
#define Y3_GPIO_NUM 9
#define Y4_GPIO_NUM 8
#define Y5_GPIO_NUM 10
#define Y6_GPIO_NUM 12
#define Y7_GPIO_NUM 18
#define Y8_GPIO_NUM 17
#define Y9_GPIO_NUM 16

#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM  7
#define PCLK_GPIO_NUM  13

const char* ssid = "ESP32_CAM";
const char* password = "12345678";  // mínimo 8 caracteres

WebServer server(80);

camera_config_t config = {
    .pin_pwdn       = PWDN_GPIO_NUM,
    .pin_reset      = RESET_GPIO_NUM,
    .pin_xclk       = XCLK_GPIO_NUM,
    .pin_sscb_sda   = SIOD_GPIO_NUM,
    .pin_sscb_scl   = SIOC_GPIO_NUM,

    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,

    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href  = HREF_GPIO_NUM,
    .pin_pclk  = PCLK_GPIO_NUM,

    .xclk_freq_hz = 20000000,
    .ledc_timer   = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,
    .frame_size   = FRAMESIZE_QVGA,
    .jpeg_quality = 12,
    .fb_count     = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY
};

void iniciarWiFiAP() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid, password);

  IPAddress IP = WiFi.softAPIP();
  Serial.print("WiFi creada. IP: ");
  Serial.println(IP);
}

void streamMJPEG() {
  WiFiClient client = server.client();
  String boundary = "frame";

  client.println("HTTP/1.1 200 OK");
  client.println("Content-Type: multipart/x-mixed-replace; boundary=" + boundary);
  client.println();

  while (client.connected()) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) continue;

    client.printf("--%s\r\n", boundary.c_str());
    client.println("Content-Type: image/jpeg");
    client.printf("Content-Length: %u\r\n\r\n", fb->len);
    client.write(fb->buf, fb->len);
    client.println();

    esp_camera_fb_return(fb);
    delay(50);  // ~20 FPS
  }
}

void setupServidor() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/html",
      "<html><body>"
      "<h1>ESP32-CAM</h1>"
      "<img src='/stream'>"
      "</body></html>"
    );
  });

  server.on("/stream", HTTP_GET, streamMJPEG);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  if (psramFound()) {
    Serial.println("=== PSRAM INFO ===");
    Serial.println("PSRAM detectada");
    Serial.printf("Tamaño PSRAM: %d bytes\n", ESP.getPsramSize());
    Serial.printf("Heap interno libre: %d\n", ESP.getFreeHeap());
    Serial.printf("PSRAM libre: %d\n", ESP.getFreePsram());
  } else {
    Serial.println("=== PSRAM INFO ===");
    Serial.println("PSRAM NO detectada");
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("❌ Error cámara: 0x%x\n", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (!s) {
    Serial.println("❌ Sensor NULL");
    return;
  }

  Serial.printf("✅ PID cámara: 0x%02X\n", s->id.PID);
  
  iniciarWiFiAP();

  setupServidor();

  Serial.println("Cámara IP lista");
  Serial.println("Abre: http://192.168.4.1");
}

void loop() {
  server.handleClient();
}
