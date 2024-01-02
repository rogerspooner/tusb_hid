/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "driver/gpio.h"

#define APP_BUTTON (GPIO_NUM_0) // Use BOOT signal by default
#define INDICATOR_GPIO GPIO_NUM_15 // will output simple indicator voltage
static const char *TAG = "tag1";

/************* TinyUSB descriptors ****************/

#define TUSB_DESC_TOTAL_LEN      (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

/**
 * @brief HID report descriptor
 *
 * In this example we implement Keyboard + Mouse HID device,
 * so we must define both report descriptors
 */
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD) ),
    TUD_HID_REPORT_DESC_MOUSE(HID_REPORT_ID(HID_ITF_PROTOCOL_MOUSE) )
};

/**
 * @brief String descriptor
 */
const char* hid_string_descriptor[6] = {
    // array of pointer to string descriptors
    (char[]){0x09, 0x04},  // 0: is supported language is English (0x0409)
    "RIWS",             // 1: Manufacturer
    "RIWS Keyboard 1",      // 2: Product
    "123456",              // 3: Serials, should use chip ID
    "RIWS HID interface",  // 4: HID
    NULL                   // end of list
};

const tusb_desc_device_t kbd_device_descriptor = {
  .bLength = sizeof(tusb_desc_device_t),
  .bDescriptorType = TUSB_DESC_DEVICE,
  .bcdUSB = 0x0200,
  .bDeviceClass = TUSB_CLASS_MISC,
  .bDeviceSubClass = MISC_SUBCLASS_COMMON,
  .bDeviceProtocol = MISC_PROTOCOL_IAD,
  .bMaxPacketSize0 = CFG_TUD_ENDPOINT0_SIZE,
  .idVendor = USB_ESPRESSIF_VID,
  .idProduct = 0x4101, // RIWS made up
  .bcdDevice = CONFIG_TINYUSB_DESC_BCD_DEVICE,
  .iManufacturer = 0x01,
  .iProduct = 0x02,
  .iSerialNumber = 0x03,
  .bNumConfigurations = 0x01
};


/**
 * @brief Configuration descriptor
 *
 * This is a simple configuration descriptor that defines 1 configuration and 1 HID interface
 */
static const uint8_t hid_configuration_descriptor[] = {
    // Configuration number, interface count, string index, total length, attribute, power in mA
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN, TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),

    // Interface number, string index, boot protocol, report descriptor len, EP In address, size & polling interval
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor), 0x81, 16, 10),
};

#define KEY_ROW_COUNT 2
#define KEY_COL_COUNT 3
static const uint8_t keyboard_rows[KEY_ROW_COUNT] = { 33, 35 }; // outputs
static const uint8_t keyboard_cols[KEY_COL_COUNT] = { 16, 37, 39 }; // inputs
static const uint8_t keyboard_keys[KEY_ROW_COUNT][KEY_COL_COUNT] = {
    { HID_KEY_J, HID_KEY_K, HID_KEY_L } , 
    { HID_KEY_U, HID_KEY_I, HID_KEY_O }
  };


/********* TinyUSB HID callbacks ***************/

// Invoked when received GET HID REPORT DESCRIPTOR request
// Application return pointer to descriptor, whose contents must exist long enough for transfer to complete
uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    // We use only one interface and one HID report descriptor, so we can ignore parameter 'instance'
    return hid_report_descriptor;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t* buffer, uint16_t reqlen)
{
  (void) instance;
  (void) report_id;
  (void) report_type;
  (void) buffer;
  (void) reqlen;

  return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const* buffer, uint16_t bufsize)
{
}

/********* Application ***************/

typedef enum {
    MOUSE_DIR_RIGHT,
    MOUSE_DIR_DOWN,
    MOUSE_DIR_LEFT,
    MOUSE_DIR_UP,
    MOUSE_DIR_MAX,
} mouse_dir_t;

#define DISTANCE_MAX        125
#define DELTA_SCALAR        5

static void mouse_draw_square_next_delta(int8_t *delta_x_ret, int8_t *delta_y_ret)
{
    static mouse_dir_t cur_dir = MOUSE_DIR_RIGHT;
    static uint32_t distance = 0;

    // Calculate next delta
    if (cur_dir == MOUSE_DIR_RIGHT) {
        *delta_x_ret = DELTA_SCALAR;
        *delta_y_ret = 0;
    } else if (cur_dir == MOUSE_DIR_DOWN) {
        *delta_x_ret = 0;
        *delta_y_ret = DELTA_SCALAR;
    } else if (cur_dir == MOUSE_DIR_LEFT) {
        *delta_x_ret = -DELTA_SCALAR;
        *delta_y_ret = 0;
    } else if (cur_dir == MOUSE_DIR_UP) {
        *delta_x_ret = 0;
        *delta_y_ret = -DELTA_SCALAR;
    }

    // Update cumulative distance for current direction
    distance += DELTA_SCALAR;
    // Check if we need to change direction
    if (distance >= DISTANCE_MAX) {
        distance = 0;
        cur_dir++;
        if (cur_dir == MOUSE_DIR_MAX) {
            cur_dir = 0;
        }
    }
}

static void app_send_hid_demo(void)
{
    // Keyboard output: Send key 'a/A' pressed and released
    static unsigned int hid_count = 0;
    ESP_LOGI(TAG, "Sending Keyboard report %d", hid_count);
    uint8_t keycode[6] = {HID_KEY_A + (hid_count%16)};
    hid_count++;
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, keycode);
    vTaskDelay(pdMS_TO_TICKS(50));
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);

    // Mouse output: Move mouse cursor in square trajectory
    ESP_LOGI(TAG, "Sending Mouse report");
    int8_t delta_x;
    int8_t delta_y;
    for (int i = 0; i < (DISTANCE_MAX / DELTA_SCALAR) * 4; i++) {
        // Get the next x and y delta in the draw square pattern
        mouse_draw_square_next_delta(&delta_x, &delta_y);
        tud_hid_mouse_report(HID_ITF_PROTOCOL_MOUSE, 0x00, delta_x, delta_y, 0, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static uint8_t keys_prev_pressed[KEY_ROW_COUNT][KEY_COL_COUNT];
static uint64_t key_col_mask = 0;
static uint64_t key_row_mask = 0;

static void init_keyboard_scan(void)
{
    gpio_config_t row_config = {
        .pin_bit_mask = 0, // to be updated
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = false,
        .pull_down_en = false,
    };
    gpio_config_t col_config = {
        .pin_bit_mask = 0, // to be updated
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = false,
        .pull_down_en = true,
    };
    int i;
    for (i = 0; i < KEY_ROW_COUNT; i++)
      row_config.pin_bit_mask |= BIT64(keyboard_rows[i]);
    ESP_ERROR_CHECK(gpio_config(&row_config));
    for (i = 0; i < KEY_COL_COUNT; i++)
      col_config.pin_bit_mask |= BIT64(keyboard_cols[i]);
    ESP_ERROR_CHECK(gpio_config(&col_config));

    key_row_mask = 0;
    for (int row = 0; row < KEY_ROW_COUNT ; row ++ )
    { key_row_mask |= BIT64(keyboard_rows[row]) ;
      gpio_set_level(keyboard_rows[row], 0);
    }
    key_col_mask = 0;
    for (int col = 0; col < KEY_COL_COUNT; col++)
      key_col_mask |= BIT64(keyboard_cols[col]);
    for (int row = 0; row < KEY_ROW_COUNT; row++)
      for (int col = 0; col < KEY_COL_COUNT; col++)
        keys_prev_pressed[row][col] = 0;
}


static void send_usb_key_stroke(void)
{
    uint8_t keycodes[6];
    uint8_t modifier = 0;
    for (int i=0; i<6; i++)
      keycodes[i] = 0;
    int k = HID_KEY_NONE;
    for (int row = 0; row < KEY_ROW_COUNT; row++ )
      for (int col = 0; col < KEY_COL_COUNT; col++ )
      { if (keys_prev_pressed[row][col] != 0)
          keycodes[k++] = keyboard_keys[row][col];
        k = k % 6;
      }
    ESP_LOGI("kbdscan","Sending key strokes for %d keys",k);
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, modifier, keycodes);
}

static void scan_keyboard(void)
{
    // iterate through keyboard_rows[], setting one high at a time.
    // read values from keyboard_cols[]
    // if values change between scans, then notify host PC of key up or key down.
    // uint64_t io_data = 0;
    static uint8_t keys_now_pressed[KEY_ROW_COUNT][KEY_COL_COUNT];
    ESP_LOGI("kbdscan", "Scanning keyboard. Row mask = x%llx. Col mask = x%llx. ", key_row_mask, key_col_mask);
    for (int row = 0; row < KEY_ROW_COUNT ; row ++ )
    {
        // Set current row high
        gpio_set_level(keyboard_rows[row], 1);
        // Delay for test probe. Not really needed
        // Read columns
        // io_data = REG_READ(GPIO_IN_REG) & key_col_mask;
        for (int col = 0; col < KEY_COL_COUNT; col++ )
            if (gpio_get_level (keyboard_cols[col]))
            {   ESP_LOGI("kbdscan","Key x%x down: row %d: col %d (GPIO row %d, col %d)",
                    keyboard_keys[row][col], row, col, keyboard_rows[row], keyboard_cols[col]);
                keys_now_pressed[row][col] = 1;
            }
            else
                keys_now_pressed[row][col] = 0;
        // clear current row
        gpio_set_level(keyboard_rows[row], 0);
    }
    int changed_pressed = 0;
    for (int row = 0; row < KEY_ROW_COUNT; row++)
      for (int col = 0; col < KEY_COL_COUNT; col++)
      { if (keys_now_pressed[row][col] != keys_prev_pressed[row][col])
          changed_pressed = 1;
        keys_prev_pressed[row][col] = keys_now_pressed[row][col];
      }
    if (changed_pressed)
      send_usb_key_stroke();

}

void app_main(void)
{
    // Initialize button that will trigger HID reports
    const gpio_config_t boot_button_config = {
        .pin_bit_mask = BIT64(APP_BUTTON),
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = true,
        .pull_down_en = false,
    };
    const gpio_config_t indicator_config = {
        .pin_bit_mask = BIT64(INDICATOR_GPIO),
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = false,
        .pull_down_en = false,
    };
    int i;
    unsigned int iIndication = 0;
    ESP_ERROR_CHECK(gpio_config(&boot_button_config));
    ESP_ERROR_CHECK(gpio_config(&indicator_config));
    for (i=0; i<10; i++)
    { iIndication ^= 1;
      gpio_set_level(INDICATOR_GPIO, iIndication);
      vTaskDelay(pdMS_TO_TICKS(100));
    }
    init_keyboard_scan();
    ESP_LOGI(TAG, "USB initialization (Roger woz ere)");
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = &kbd_device_descriptor,
        .string_descriptor = hid_string_descriptor,
        .string_descriptor_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
        .external_phy = false,
        .configuration_descriptor = hid_configuration_descriptor,
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB initialization DONE");

    while (1) {
        if (tud_mounted()) {
            static bool send_hid_data = true;
            if (send_hid_data) {
                app_send_hid_demo();
            } else
            {
              // toggle gpio2
              iIndication ^= 1;
              gpio_set_level(INDICATOR_GPIO, iIndication);
              scan_keyboard();
              vTaskDelay(pdMS_TO_TICKS(500));
            }
            send_hid_data = !gpio_get_level(APP_BUTTON);
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
