#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <Adafruit_NeoPixel.h>
#include "esp_http_server.h"

// ---------------- LED ----------------
#define LED_PIN    48
#define LED_COUNT  1
#define BRIGHTNESS 50

Adafruit_NeoPixel led(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ---------------- TELEMETRÍA SIMULADA ----------------
float sim_speed     = 0;
float sim_battery   = 100;
float sim_distance  = 0;
float sim_rivalDist = 10;
int   sim_lap       = 1;

// ---------------- HTTP SERVER ----------------
httpd_handle_t httpServer = nullptr;

// ---------------- UTILIDADES ----------------
String getContentType(const String &path) {
    if (path.endsWith(".html")) return "text/html";
    if (path.endsWith(".css"))  return "text/css";
    if (path.endsWith(".js"))   return "application/javascript";
    if (path.endsWith(".png"))  return "image/png";
    if (path.endsWith(".jpg") || path.endsWith(".jpeg")) return "image/jpeg";
    if (path.endsWith(".ico"))  return "image/x-icon";
    if (path.endsWith(".json")) return "application/json";
    return "text/plain";
}

void actualizarSimulacion() {
    sim_speed += 0.5;
    if (sim_speed > 260) sim_speed = 0;

    sim_distance += 0.3;
    if (sim_distance > 9999) sim_distance = 0;

    sim_battery -= 0.01;
    if (sim_battery < 0) sim_battery = 100;

    sim_rivalDist = 5 + 5 * sin(millis() / 1000.0);
}

// ---------------- HANDLERS HTTP ----------------

// /telemetry
esp_err_t telemetry_handler(httpd_req_t *req) {
    actualizarSimulacion();

    String json = "{";
    json += "\"speed\":"     + String(sim_speed, 1)     + ",";
    json += "\"battery\":"   + String(sim_battery, 1)   + ",";
    json += "\"lap\":"       + String(sim_lap)          + ",";
    json += "\"distance\":"  + String(sim_distance, 1)  + ",";
    json += "\"rivalDist\":" + String(sim_rivalDist, 1);
    json += "}";

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json.c_str(), json.length());
    return ESP_OK;
}

// /led
esp_err_t led_handler(httpd_req_t *req) {
    char buf[64];
    int len = httpd_req_get_url_query_len(req) + 1;
    String color;

    if (len > 1 && len < (int)sizeof(buf)) {
        if (httpd_req_get_url_query_str(req, buf, len) == ESP_OK) {
            char param[16];
            if (httpd_query_key_value(buf, "color", param, sizeof(param)) == ESP_OK) {
                color = String(param);
            }
        }
    }

    if (color == "red") {
        led.setPixelColor(0, led.Color(255, 0, 0));
    } else if (color == "green") {
        led.setPixelColor(0, led.Color(0, 255, 0));
    } else {
        led.setPixelColor(0, led.Color(0, 0, 0));
    }
    led.show();

    const char *resp = "OK";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, resp, strlen(resp));
    return ESP_OK;
}

// (opcional) /stream – placeholder sin cámara
esp_err_t stream_handler(httpd_req_t *req) {
    const char *msg = "Stream no implementado aún";
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, msg, strlen(msg));
    return ESP_OK;
}

// estáticos desde LittleFS: /* (default index.html)
esp_err_t static_handler(httpd_req_t *req) {
    String uri = String(req->uri);  // p.ej. "/", "/style.css", "/img/fondo2.jpg"

    if (uri == "/") uri = "/index.html";

    String path = uri; // LittleFS raíz
    if (!LittleFS.exists(path)) {
        const char *msg = "404 Not Found";
        httpd_resp_set_status(req, "404 NOT FOUND");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, msg, strlen(msg));
        return ESP_OK;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        const char *msg = "500 Internal Error";
        httpd_resp_set_status(req, "500 INTERNAL SERVER ERROR");
        httpd_resp_set_type(req, "text/plain");
        httpd_resp_send(req, msg, strlen(msg));
        return ESP_OK;
    }

    String contentType = getContentType(path);
    httpd_resp_set_type(req, contentType.c_str());

    uint8_t buffer[1024];
    while (true) {
        size_t readBytes = file.read(buffer, sizeof(buffer));
        if (readBytes <= 0) break;
        if (httpd_resp_send_chunk(req, (const char *)buffer, readBytes) != ESP_OK) {
            file.close();
            httpd_resp_send_chunk(req, nullptr, 0);
            return ESP_FAIL;
        }
    }
    file.close();
    httpd_resp_send_chunk(req, nullptr, 0);
    return ESP_OK;
}

// ---------------- INICIO HTTP SERVER ----------------
void startHttpServer() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&httpServer, &config) == ESP_OK) {
        // /telemetry
        httpd_uri_t telemetry_uri = {
            .uri      = "/telemetry",
            .method   = HTTP_GET,
            .handler  = telemetry_handler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(httpServer, &telemetry_uri);

        // /led
        httpd_uri_t led_uri = {
            .uri      = "/led",
            .method   = HTTP_GET,
            .handler  = led_handler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(httpServer, &led_uri);

        // /stream (placeholder)
        httpd_uri_t stream_uri = {
            .uri      = "/stream",
            .method   = HTTP_GET,
            .handler  = stream_handler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(httpServer, &stream_uri);

        // estáticos: wildcard
        httpd_uri_t static_uri = {
            .uri      = "/*",
            .method   = HTTP_GET,
            .handler  = static_handler,
            .user_ctx = nullptr
        };
        httpd_register_uri_handler(httpServer, &static_uri);

        Serial.println("HTTP server iniciado");
    } else {
        Serial.println("Error iniciando HTTP server");
    }
}

// ---------------- WIFI AP ----------------
void iniciarWiFiAP() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP("ESP32_TEST", "12345678");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());
}

// ---------------- SETUP / LOOP ----------------
void setup() {
    Serial.begin(115200);
    delay(300);

    led.begin();
    led.setBrightness(BRIGHTNESS);
    led.clear();
    led.show();

    if (!LittleFS.begin(true)) {
        Serial.println("Error montando LittleFS");
    } else {
        Serial.println("LittleFS OK");
        Serial.println("Listado LittleFS:");
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        while (file) {
            Serial.println(file.name());
            file = root.openNextFile();
        }
    }

    iniciarWiFiAP();
    startHttpServer();
}

void loop() {
    // Nada: todo lo maneja el servidor HTTP y las interrupciones
}
