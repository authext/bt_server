// NVS includes
#include "nvs_flash.h"
// My includes
#include "bluetooth_server.hpp"

extern "C" void app_main()
{
    ESP_ERROR_CHECK(nvs_flash_erase());
    ESP_ERROR_CHECK(nvs_flash_init());

    bluetooth_server::instance().start();
}
