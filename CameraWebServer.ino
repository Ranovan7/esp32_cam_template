#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <base64.h>

#define CAMERA_MODEL_AI_THINKER

#include "camera_pins.h"

const char* ssid = "wifi_name";
const char* password = "password";
const char* serverName = "http://192.168.1.2:80/api/upload";

unsigned long lastTime = 0;
unsigned long timerDelay = 5000;    // 5 seconds

// Define NTP Client to get time
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

// Variables to save date and time
String formattedDate;
String serialNumber = "test-device";

void startCameraServer();

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if(psramFound()){
    Serial.println("PSRAM Found !");
    config.frame_size = FRAMESIZE_SXGA;  // image size
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_VGA;  // image size
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

#if defined(CAMERA_MODEL_ESP_EYE)
  pinMode(13, INPUT_PULLUP);
  pinMode(14, INPUT_PULLUP);
#endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  //initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);//flip it back
    s->set_brightness(s, 1);//up the blightness just a bit
    s->set_saturation(s, -2);//lower the saturation
  }
  //drop down frame size for higher initial frame rate
  // FRAMESIZE_QVGA (320 x 240)
  // FRAMESIZE_CIF (352 x 288)
  // FRAMESIZE_VGA (640 x 480)  * current limit using base64 format
  // FRAMESIZE_SVGA (800 x 600)
  // FRAMESIZE_XGA (1024 x 768)
  // FRAMESIZE_SXGA (1280 x 1024)
  // FRAMESIZE_UXGA (1600 x 1200)
  s->set_framesize(s, FRAMESIZE_VGA);

#if defined(CAMERA_MODEL_M5STACK_WIDE)
  s->set_vflip(s, 1);
  s->set_hmirror(s, 1);
#endif

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  // Initialize a NTPClient to get time
  timeClient.begin();
  // Set offset time in seconds to adjust for your timezone, for example:
  // GMT +1 = 3600
  // GMT +8 = 28800
  // GMT -1 = -3600
  // GMT 0 = 0
  // timeClient.setTimeOffset(3600);

  // take img
  captureImg();
}

void loop() {
  // put your main code here, to run repeatedly:

}

void captureImg(){
    Serial.println("Taking a picture");
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();

    // payload (image) fb->buf, payload length fb->len
    // convert image to base64 string
    if(!fb) {
      Serial.println("Camera capture failed");
      return;
    } else {
      Serial.println("Camera capture succeed");
      String encodedImg = base64::encode(fb->buf, fb->len);
      Serial.println(encodedImg.substring(0, 30));

      while(!timeClient.update()) {
          timeClient.forceUpdate();
      }
      // format -> 2018-05-28T16:00:13Z
      formattedDate = timeClient.getFormattedDate();
      Serial.println(formattedDate);

      // upload image
      HTTPClient http;
      http.begin(serverName);
      http.addHeader("Content-Type", "application/json");

      String httpRequestData = "{\"device\":\"";
      httpRequestData += serialNumber;
      httpRequestData += "\",\"sampling\":\"";
      httpRequestData += formattedDate;
      httpRequestData += "\",\"image\":\"";
      httpRequestData += encodedImg;
      httpRequestData += "\"}";
      Serial.println(httpRequestData.substring(0, 100));

      int httpResponseCode = http.POST(httpRequestData);

      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);

      // Free resources
      http.end();
    }
}
