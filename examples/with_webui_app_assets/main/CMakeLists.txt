# The application owns the Web UI bytes. EMBED_FILES links them into the
# firmware as _binary_<name>_start/_end symbols (dots and slashes in the file
# name become underscores); main.c hands them to the library through
# wifi_cfg_webui_set_asset_provider().
#
# Under PlatformIO the equivalent is `board_build.embed_files` in
# platformio.ini, which produces the same symbol names.
idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                    EMBED_FILES
                        "../www/index.html"
                        "../www/assets/app.js"
                        "../www/assets/index.css")
