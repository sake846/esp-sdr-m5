/* Official M5Stack drivers handle panel revisions, PMICs and touch controllers. */
#include <M5Unified.h>
#include "m5unified_board.h"
#include "m5unified_input.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <cstring>

#if CONFIG_ESP_SDR_M5STACK_DIN_METER
static portMUX_TYPE encoder_lock = portMUX_INITIALIZER_UNLOCKED;
static volatile int encoder_count;
static unsigned encoder_state;

static void encoder_interrupt(void *) {
    static const int8_t transitions[] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
    unsigned state = gpio_get_level(GPIO_NUM_41) | (gpio_get_level(GPIO_NUM_40) << 1);
    portENTER_CRITICAL_ISR(&encoder_lock);
    int next = encoder_count + transitions[encoder_state * 4 + state];
    if (next >= -128 && next <= 128) encoder_count = next;
    encoder_state = state;
    portEXIT_CRITICAL_ISR(&encoder_lock);
}
#endif

#if M5_EPAPER
static constexpr size_t frame_pixels = SCREEN_WIDTH * SCREEN_HEIGHT;
static uint16_t *paper_frame, *paper_pending, *paper_tx;
static SemaphoreHandle_t paper_lock;
static TaskHandle_t paper_task;

static void paper_worker(void *) {
    uint16_t expanded[SCREEN_WIDTH * M5_DISPLAY_SCALE];
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        xSemaphoreTake(paper_lock, portMAX_DELAY);
        memcpy(paper_tx, paper_pending, frame_pixels * sizeof(uint16_t));
        xSemaphoreGive(paper_lock);
        for (unsigned y = 0; y < SCREEN_HEIGHT; y++) {
            for (unsigned x = 0; x < SCREEN_WIDTH; x++) {
                uint16_t color = paper_tx[y * SCREEN_WIDTH + x];
#if CONFIG_ESP_SDR_M5STACK_PAPERS3
                color = ~color; /* White background on the grayscale panel. */
#endif
                for (unsigned dx = 0; dx < M5_DISPLAY_SCALE; dx++)
                    expanded[x * M5_DISPLAY_SCALE + dx] = color;
            }
            for (unsigned dy = 0; dy < M5_DISPLAY_SCALE; dy++)
                M5.Display.pushImage(0, y * M5_DISPLAY_SCALE + dy,
                                     SCREEN_WIDTH * M5_DISPLAY_SCALE, 1, expanded);
        }
        M5.Display.display();
        M5.Display.waitDisplay();
#if CONFIG_ESP_SDR_M5STACK_PAPER_COLOR
        vTaskDelay(pdMS_TO_TICKS(30000));
#else
        vTaskDelay(pdMS_TO_TICKS(1000));
#endif
    }
}
#endif

void m5unified_board_init(void) {
    auto cfg = M5.config();
    cfg.internal_imu = cfg.internal_rtc = cfg.internal_mic = cfg.internal_spk = false;
    cfg.external_display_value = 0;
    cfg.output_power = false;
    cfg.pmic_button = false;
    cfg.clear_display = false;
    cfg.fallback_board = static_cast<m5::board_t>(CONFIG_ESP_SDR_M5GFX_BOARD);
    M5.begin(cfg);
#if !CONFIG_ESP_SDR_M5STACK_CORES3
    /* Also clear the temporary GPIO46 power hold used by M5Unified detection. */
    static const gpio_num_t quiet_pins[] = {
#if CONFIG_ESP_SDR_M5STACK_STICKS3
        GPIO_NUM_46, /* IR transmitter. */
#elif CONFIG_ESP_SDR_M5STACK_PAPER_COLOR
        GPIO_NUM_45, GPIO_NUM_46, GPIO_NUM_48, /* Codec, amplifier, IR. */
#elif CONFIG_ESP_SDR_M5STACK_DIN_METER
        GPIO_NUM_3, /* Buzzer; keep GPIO46 power hold high. */
#elif CONFIG_ESP_SDR_M5STACK_PAPERS3
        GPIO_NUM_21, /* Buzzer. */
#elif CONFIG_ESP_SDR_M5STACK_STICKC_PLUS
        GPIO_NUM_2, GPIO_NUM_9, /* Buzzer, IR. */
#else
        GPIO_NUM_9, /* IR. */
#endif
    };
    for (auto pin : quiet_pins) {
        ESP_ERROR_CHECK(gpio_set_level(pin, 0));
        ESP_ERROR_CHECK(gpio_set_direction(pin, GPIO_MODE_OUTPUT));
    }
#endif
    if (M5.Display.width() != SCREEN_WIDTH * M5_DISPLAY_SCALE)
        M5.Display.setRotation(1);
    ESP_ERROR_CHECK(M5.Display.width() == SCREEN_WIDTH * M5_DISPLAY_SCALE &&
                    M5.Display.height() == SCREEN_HEIGHT * M5_DISPLAY_SCALE ? ESP_OK : ESP_FAIL);
    M5.Display.setBrightness(128);
    M5.Display.setSwapBytes(false); /* Renderer rows are already in wire byte order. */
    M5.Display.setAutoDisplay(false);
#if M5_EPAPER
    paper_frame = static_cast<uint16_t *>(heap_caps_calloc(3 * frame_pixels, sizeof(uint16_t),
                                                         MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    ESP_ERROR_CHECK(paper_frame ? ESP_OK : ESP_ERR_NO_MEM);
    paper_pending = paper_frame + frame_pixels;
    paper_tx = paper_pending + frame_pixels;
    paper_lock = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(paper_lock ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_ERROR_CHECK(xTaskCreate(paper_worker, "m5_epaper", 6144, nullptr, 1, &paper_task) == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
#endif
#if CONFIG_ESP_SDR_M5STACK_DIN_METER
    gpio_config_t pins = {};
    pins.pin_bit_mask = (UINT64_C(1) << 40) | (UINT64_C(1) << 41);
    pins.mode = GPIO_MODE_INPUT;
    pins.pull_up_en = GPIO_PULLUP_ENABLE;
    pins.intr_type = GPIO_INTR_ANYEDGE;
    ESP_ERROR_CHECK(gpio_config(&pins));
    encoder_state = gpio_get_level(GPIO_NUM_41) | (gpio_get_level(GPIO_NUM_40) << 1);
    esp_err_t result = gpio_install_isr_service(0);
    ESP_ERROR_CHECK(result == ESP_ERR_INVALID_STATE ? ESP_OK : result);
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_40, encoder_interrupt, nullptr));
    ESP_ERROR_CHECK(gpio_isr_handler_add(GPIO_NUM_41, encoder_interrupt, nullptr));
#endif
}

unsigned m5unified_board_buttons(void) {
    M5.update();
#if CONFIG_ESP_SDR_M5STACK_DIN_METER
    return M5.BtnA.isPressed() ? 2 : 0;
#elif CONFIG_ESP_SDR_M5STACK_STICKC || CONFIG_ESP_SDR_M5STACK_STICKC_PLUS || CONFIG_ESP_SDR_M5STACK_STICKS3
    static bool chord;
    return m5unified_two_buttons(unsigned(M5.BtnA.isPressed()) | (unsigned(M5.BtnB.isPressed()) << 1), &chord);
#elif CONFIG_ESP_SDR_M5STACK_PAPER_COLOR
    return unsigned(M5.BtnA.isPressed()) | (unsigned(M5.BtnB.isPressed()) << 1) | (unsigned(M5.BtnC.isPressed()) << 2);
#else
    if (!M5.Touch.getCount()) return 0;
    const auto &touch = M5.Touch.getDetail();
    return touch.isPressed() ? m5unified_touch_button(touch.x, touch.y, M5.Display.width(), M5.Display.height()) : 0;
#endif
}

int m5unified_board_encoder(void) {
#if CONFIG_ESP_SDR_M5STACK_DIN_METER
    portENTER_CRITICAL(&encoder_lock);
    int steps = encoder_count / 4;
    encoder_count %= 4;
    portEXIT_CRITICAL(&encoder_lock);
    return steps;
#else
    return 0;
#endif
}

void m5unified_board_row(unsigned y, const uint16_t *pixels) {
#if M5_EPAPER
    memcpy(paper_frame + y * SCREEN_WIDTH, pixels, SCREEN_WIDTH * sizeof(uint16_t));
#else
    M5.Display.pushImage(0, y, SCREEN_WIDTH, 1, pixels);
#endif
}

void m5unified_board_flush(void) {
#if M5_EPAPER
    if (xSemaphoreTake(paper_lock, 0) == pdTRUE) {
        memcpy(paper_pending, paper_frame, frame_pixels * sizeof(uint16_t));
        xSemaphoreGive(paper_lock);
        xTaskNotifyGive(paper_task);
    }
#endif
}
