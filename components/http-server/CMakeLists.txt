idf_component_register(SRCS "http-server.c"
                        INCLUDE_DIRS "include"
                        REQUIRES esp_http_server esp_wifi nvs_flash
                        EMBED_FILES "root.html")
