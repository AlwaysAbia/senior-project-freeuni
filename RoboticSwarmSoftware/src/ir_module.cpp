#include "ir_module.hpp"
#include "driver/rmt.h"
#include "globals.hpp"

//-------------------------
// Config / Pins
//-------------------------
static uint8_t s0_pin, s1_pin, s2_pin; // SN74CB3Q3251 addressing pins

// Helper function prototypes (private)
static void setChannel(uint8_t channel);
static void setupRMT(uint8_t tx_pin, uint8_t rx_pin);
static int decodeIRItems(rmt_item32_t* items, size_t num_items);
static int transmitID(uint8_t channel, uint8_t ID);
static int receiveID(uint8_t channel);

// Timing constants
constexpr uint32_t RX_TIMEOUT_MS = 30;

//-------------------------
// Public Setup
//-------------------------
void setupIR(uint8_t s0, uint8_t s1, uint8_t s2, uint8_t tx_pin, uint8_t rx_pin) {
    s0_pin = s0;
    s1_pin = s1;
    s2_pin = s2;

    pinMode(s0_pin, OUTPUT);
    pinMode(s1_pin, OUTPUT);
    pinMode(s2_pin, OUTPUT);

    setupRMT(tx_pin, rx_pin);
}

//-------------------------
// FreeRTOS Task
//-------------------------
void IRsensorTask(void* parameter) {
    while (true) {
        // Placeholder for future periodic IR operations
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

//-------------------------
// Private Functions
//-------------------------
static void setChannel(uint8_t channel) {
    digitalWrite(s0_pin, channel & 0x01);
    digitalWrite(s1_pin, (channel >> 1) & 0x01);
    digitalWrite(s2_pin, (channel >> 2) & 0x01);
}

static void setupRMT(uint8_t tx_pin, uint8_t rx_pin) {
    // TX config
    rmt_config_t txConfig = {};
    txConfig.rmt_mode = RMT_MODE_TX;
    txConfig.channel = RMT_CHANNEL_0;
    txConfig.gpio_num = (gpio_num_t)tx_pin;
    txConfig.clk_div = 80;  // 1us tick
    txConfig.mem_block_num = 1;
    txConfig.tx_config.loop_en = false;
    txConfig.tx_config.carrier_duty_percent = 50;
    txConfig.tx_config.carrier_freq_hz = 38000;
    txConfig.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
    txConfig.tx_config.carrier_en = true;
    txConfig.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
    txConfig.tx_config.idle_output_en = true;

    rmt_config(&txConfig);
    rmt_driver_install(RMT_CHANNEL_0, 0, 0);

    // RX config
    rmt_config_t rxConfig = {};
    rxConfig.rmt_mode = RMT_MODE_RX;
    rxConfig.channel = RMT_CHANNEL_1;
    rxConfig.gpio_num = (gpio_num_t)rx_pin;
    rxConfig.clk_div = 80;
    rxConfig.mem_block_num = 1;
    rxConfig.rx_config.filter_en = true;
    rxConfig.rx_config.filter_ticks_thresh = 100;
    rxConfig.rx_config.idle_threshold = 10000;
    rxConfig.rx_config.rm_carrier = false;

    rmt_config(&rxConfig);
    rmt_driver_install(RMT_CHANNEL_1, 1000, 0); // ring buffer
}

static int decodeIRItems(rmt_item32_t* items, size_t num_items) {
    for (int i = 0; i <= num_items - 9; i++) {
        rmt_item32_t start = items[i];

        bool validStart =
            start.level0 == 0 &&
            start.level1 == 1 &&
            (start.duration0 > 1900 && start.duration0 < 2100) &&
            (start.duration1 > 900  && start.duration1 < 1100);

        if (!validStart) continue;

        uint8_t id = 0;
        for (int j = 0; j < 8; j++) {
            rmt_item32_t bit = items[i + j + 1];
            bool isOne  = (bit.level0 == 1 && bit.level1 == 0);
            bool isZero = (bit.level0 == 0 && bit.level1 == 1);

            if (!(isOne || isZero)) return -3; // Invalid pulse format
            if (bit.duration0 < 500 || bit.duration0 > 620 ||
                bit.duration1 < 500 || bit.duration1 > 620) return -4; // Timing error

            if (isOne) id |= (1 << j); // LSB first
        }

        return id; // Successfully decoded
    }

    return -5; // No valid start bit found
}

static int transmitID(uint8_t channel, uint8_t ID) {
    if (channel > 5) return -1;

    setChannel(channel);

    rmt_item32_t items[9] = {};

    // Start pulse
    items[0].level0 = 1;
    items[0].level1 = 0;
    items[0].duration0 = 2000;
    items[0].duration1 = 1000;

    // Data bits
    for (int i = 0; i < 8; i++) {
        bool bit = (ID >> i) & 0x01;
        items[i + 1].level0 = bit ? 0 : 1;
        items[i + 1].level1 = bit ? 1 : 0;
        items[i + 1].duration0 = 560;
        items[i + 1].duration1 = 560;
    }

    rmt_write_items(RMT_CHANNEL_0, items, 9, true);
    rmt_wait_tx_done(RMT_CHANNEL_0, pdMS_TO_TICKS(100));

    return 0;
}

static int receiveID(uint8_t channel) {
    if (channel > 5) return -1;

    setChannel(channel);

    RingbufHandle_t rb = nullptr;
    rmt_get_ringbuf_handle(RMT_CHANNEL_1, &rb);
    rmt_rx_start(RMT_CHANNEL_1, true);

    size_t length = 0;
    rmt_item32_t* items = (rmt_item32_t*)xRingbufferReceive(rb, &length, pdMS_TO_TICKS(RX_TIMEOUT_MS));
    if (!items || length < sizeof(rmt_item32_t) * 9) {
        rmt_rx_stop(RMT_CHANNEL_1);
        return -2; // Timeout or insufficient data
    }

    int id = decodeIRItems(items, length / sizeof(rmt_item32_t));
    vRingbufferReturnItem(rb, items);

    rmt_rx_stop(RMT_CHANNEL_1);
    return id;
}
