#include "CanBus.h"

#include <driver/twai.h>

#include <cstring>

bool CanBus::begin(int txPin, int rxPin, uint32_t acceptId) {
  twai_general_config_t general = TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)txPin, (gpio_num_t)rxPin, TWAI_MODE_NORMAL);
  general.tx_queue_len = 16;  // A 64-byte packet is 8 frames.
  general.rx_queue_len = 32;
  twai_timing_config_t timing = TWAI_TIMING_CONFIG_1MBITS();
  // The bus also carries focus wheel / accessory traffic, so accept only the gimbal's ID.
  // Single-filter layout for standard frames: ID in bits 31..21; mask bits set to 1 mean "don't care".
  twai_filter_config_t filter = {};
  filter.acceptance_code = acceptId << 21;
  filter.acceptance_mask = ~(0x7FFu << 21);
  filter.single_filter = true;
  return twai_driver_install(&general, &timing, &filter) == ESP_OK && twai_start() == ESP_OK;
}

bool CanBus::send(uint32_t id, const uint8_t* data, size_t len, uint32_t timeoutMs) {
  twai_message_t message = {};
  message.identifier = id;
  message.data_length_code = len;
  memcpy(message.data, data, len);
  return twai_transmit(&message, pdMS_TO_TICKS(timeoutMs)) == ESP_OK;
}

int CanBus::receive(uint8_t* data, uint32_t timeoutMs) {
  twai_message_t message;
  if (twai_receive(&message, pdMS_TO_TICKS(timeoutMs)) != ESP_OK) return -1;
  if (message.extd || message.rtr) return 0;
  memcpy(data, message.data, message.data_length_code);
  return message.data_length_code;
}

void CanBus::service() {
  twai_status_info_t status;
  if (twai_get_status_info(&status) != ESP_OK) return;
  if (status.state == TWAI_STATE_BUS_OFF) {
    twai_initiate_recovery();
  } else if (status.state == TWAI_STATE_STOPPED) {
    twai_start();
  }
}
