/*
 * SPDX-FileCopyrightText: 2010-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <stdio.h>
#include <time.h>
#include <inttypes.h>
#include <string.h>
#include <wchar.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "axp202.h"
#include "i2c_helper.h"
#include "pcf8563.h"
#include "hagl_hal.h"
#include "hagl.h"
#include "font6x9.h"

// Desenha o texto ampliado
void draw_text(void *surface, int x, int y, int w, int h, const wchar_t *text)
{
  wchar_t code;
  hagl_bitmap_t bitmap;
  bitmap.buffer = (uint8_t *)malloc(6 * 9 * sizeof(hagl_color_t));
  int count = 0;
  while((code = *text++))
  {
    hagl_get_glyph(surface, code, 0xffff, &bitmap, font6x9);
    hagl_blit_xywh(surface, x + count * w, y, w, h, &bitmap);
    ++count;
  }
  free(bitmap.buffer);
}

void draw_time(void *surface, pcf8563_t *pcf)
{
  // Obtém hora atual
  struct tm rtc;
  pcf8563_read(pcf, &rtc);

  // Formata hora e data para exibição
  wchar_t timeText[16];
  wchar_t dateText[16];
  swprintf(timeText, sizeof(timeText), u"%02d:%02d", rtc.tm_hour, rtc.tm_min);
  swprintf(dateText, sizeof(dateText), u"%02d/%02d/%d", rtc.tm_mday, rtc.tm_mon + 1, rtc.tm_year + 1900);

  // Calcula posições desejadas
  int w = 30;
  int h = 45;
  int charCount = wcslen(timeText);
  int x = DISPLAY_WIDTH / 2 - charCount * w / 2;
  int y = DISPLAY_HEIGHT / 2 - h * 3 / 2;

  // Exibe as informações na tela
  draw_text(surface, x, y, w, h, timeText);

  // Repete
  w = 12;
  h = 18;
  charCount = wcslen(dateText);
  x = DISPLAY_WIDTH / 2 - charCount * w / 2;
  y = DISPLAY_HEIGHT / 2 - h / 2;
  draw_text(surface, x, y, w, h, dateText);
  //hagl_put_text(surface, dateText, x, y, 0xffff, font6x9);
}

void draw_battery(void *surface, axp202_t *axp)
{
  float fuel;
  axp202_read(axp, AXP202_FUEL_GAUGE, &fuel);

  wchar_t fuelText[16];
  swprintf(fuelText, sizeof(fuelText), u"%.0f%%", fuel);
  hagl_put_text(surface, fuelText, 10, 10, 0xffff, font6x9);
}

esp_err_t config_axp202(axp202_t *axp, i2c_port_t port)
{
  esp_err_t result;
  i2c_master_bus_handle_t bus_handle;
  result = i2c_master_get_bus_handle(I2C_NUM_0, &bus_handle);
  if(result != ESP_OK) return result;

  i2c_device_config_t dev_config = {
    .dev_addr_length = I2C_ADDR_BIT_7,
    .device_address = AXP202_ADDRESS,
    .scl_speed_hz = 1000,
  };
  i2c_master_dev_handle_t dev_handle;
  result = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
  if(result != ESP_OK) return result;

  axp->handle = dev_handle;
  axp->write = &i2c_write;
  axp->read = &i2c_read;
  return axp202_init(axp);
}

esp_err_t config_rtc(pcf8563_t *pcf, i2c_port_t port)
{
  esp_err_t result;
  i2c_master_bus_handle_t bus_handle;
  result = i2c_master_get_bus_handle(I2C_NUM_0, &bus_handle);
  if(result != ESP_OK) return result;

  i2c_device_config_t dev_config = {
    .dev_addr_length = I2C_ADDR_BIT_7,
    .device_address = PCF8563_ADDRESS,
    .scl_speed_hz = 1000,
  };
  i2c_master_dev_handle_t dev_handle;
  result = i2c_master_bus_add_device(bus_handle, &dev_config, &dev_handle);
  if(result != ESP_OK) return result;

  pcf->handle = dev_handle;
  pcf->write = i2c_write;
  pcf->read = i2c_read;
  return pcf8563_init(pcf);
}

void app_main(void)
{
  printf("Hello world!\n");
  if(i2c_init(I2C_NUM_0) == ESP_OK)
  {
    printf("Inicializando RTC...\n");
    pcf8563_t pcf;
    if(config_rtc(&pcf, I2C_NUM_0) == ESP_OK)
    {
      printf("Recuperando dados do RTC...\n");
      struct tm rtc;
      char buffer[128];
      pcf8563_read(&pcf, &rtc);
      strftime(buffer, 128, "%c (day%j)", &rtc);
      printf("RTC: %s\n", buffer);
    }

    printf("Inicializando AXP202...\n");
    axp202_t axp;
    if(config_axp202(&axp, I2C_NUM_0) == ESP_OK)
    {
      printf("AXP202 deu certo!\n");
    }

    printf("Inicializando tela...\n");
    hagl_backend_t *display = hagl_init();

    if(display)
    {
      printf("Tela inicializada.\n");
      hagl_clear(display);
      while(1)
      {
        hagl_clear(display);
        draw_time(display, &pcf);
        draw_battery(display, &axp);
        hagl_flush(display);
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }
    }
  }
  else
  {
    printf("Algo deu errado!\n");
    /*rtc.tm_year = 2024 - 1900;
    rtc.tm_mon = 12 - 1;
    rtc.tm_mday = 16;
    rtc.tm_hour = 19;
    rtc.tm_min = 33;
    rtc.tm_sec = 30;
    pcf8563_write(&pcf, &rtc);*/
  }
}
