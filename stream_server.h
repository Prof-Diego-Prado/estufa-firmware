// Servidor de streaming MJPEG da câmera, rodando em uma porta separada (81).
// Baseado no exemplo oficial "CameraWebServer" da Espressif — é o padrão
// mais testado e estável para expor o vídeo da ESP32-CAM via HTTP.
#pragma once

#include <Arduino.h>
#include "esp_http_server.h"
#include "esp_camera.h"
#include "esp_timer.h"

// Definidas no .ino principal — reaproveitadas aqui para exigir a mesma
// autenticação HTTP Basic do dashboard também no stream de vídeo.
extern const char *AUTH_USER;
extern const char *AUTH_PASS;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char *_STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char *_STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char *_STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static httpd_handle_t stream_httpd = NULL;

inline String base64Encode(const String &in) {
  static const char *tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String out;
  int val = 0, valb = -6;
  for (size_t i = 0; i < in.length(); i++) {
    val = (val << 8) + (uint8_t)in[i];
    valb += 8;
    while (valb >= 0) {
      out += tbl[(val >> valb) & 0x3F];
      valb -= 6;
    }
  }
  if (valb > -6) out += tbl[((val << 8) >> (valb + 8)) & 0x3F];
  while (out.length() % 4) out += '=';
  return out;
}

static bool checkStreamAuth(httpd_req_t *req) {
  static String expected;
  if (expected.length() == 0) {
    expected = "Basic " + base64Encode(String(AUTH_USER) + ":" + String(AUTH_PASS));
  }
  char authHeader[128];
  size_t len = httpd_req_get_hdr_value_len(req, "Authorization") + 1;
  if (len <= 1 || len > sizeof(authHeader)) return false;
  if (httpd_req_get_hdr_value_str(req, "Authorization", authHeader, len) != ESP_OK) return false;
  return expected.equals(authHeader);
}

static esp_err_t stream_handler(httpd_req_t *req) {
  if (!checkStreamAuth(req)) {
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_hdr(req, "WWW-Authenticate", "Basic realm=\"estufa\"");
    httpd_resp_send(req, NULL, 0);
    return ESP_FAIL;
  }

  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t *_jpg_buf = NULL;
  char part_buf[128];

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) return res;

  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Cache-Control", "no-cache");

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      res = ESP_FAIL;
    } else {
      if (fb->format != PIXFORMAT_JPEG) {
        bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
        esp_camera_fb_return(fb);
        fb = NULL;
        if (!jpeg_converted) res = ESP_FAIL;
      } else {
        _jpg_buf_len = fb->len;
        _jpg_buf = fb->buf;
      }
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf(part_buf, sizeof(part_buf), _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) break;
  }
  return res;
}

// O navegador manda um pedido OPTIONS de checagem ("preflight") antes de
// qualquer fetch() de outra origem que inclua o cabeçalho Authorization.
// Sem responder isso, o navegador cancela o pedido real antes de tentar.
static esp_err_t stream_options_handler(httpd_req_t *req) {
  httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, OPTIONS");
  httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Authorization");
  httpd_resp_set_status(req, "204 No Content");
  httpd_resp_send(req, NULL, 0);
  return ESP_OK;
}

// Inicia o servidor de vídeo na porta 81 (o dashboard fica na porta 80).
inline void startCameraServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 81;
  config.ctrl_port = 82;

  httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = NULL
  };
  httpd_uri_t stream_options_uri = {
    .uri = "/stream",
    .method = HTTP_OPTIONS,
    .handler = stream_options_handler,
    .user_ctx = NULL
  };

  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
    httpd_register_uri_handler(stream_httpd, &stream_options_uri);
  }
}
