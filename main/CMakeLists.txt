# Generate secrets.h from secrets.h.example if it doesn't exist
if(NOT EXISTS "${COMPONENT_DIR}/include/secrets.h")
    message(STATUS "secrets.h not found. Creating from secrets.h.example...")
    configure_file(
        "${COMPONENT_DIR}/include/secrets.h.example"
        "${COMPONENT_DIR}/include/secrets.h"
        COPYONLY
    )
    message(STATUS "Created include/secrets.h. Please update with your actual WiFi credentials!")
endif()

idf_component_register(SRCS "main.c" "src/common.c" "src/i2s_if.c" "src/dsp_if.c" "src/task_manager.c" "src/net_manager.c" "src/web_server.c" "src/memory.c"
                    INCLUDE_DIRS "." "include"
                    EMBED_TXTFILES "web/index.html"
                    EMBED_FILES "web/vinyl.png"
                    PRIV_REQUIRES esp_wifi esp_event nvs_flash esp_driver_i2s mdns esp_http_server cjson)

nvs_create_partition_image(nvs "defaults/config.csv" FLASH_IN_PROJECT)
