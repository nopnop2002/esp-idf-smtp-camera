# Embed the server root certificate into the final binary
#
# (If this was a component, we would set COMPONENT_EMBED_TXTFILES here.)
idf_component_register(SRCS "main.c" "camera.c" "gpio.c" "smtp.c" "tcp_server.c" "udp_server.c" "keyboard.c"
                    INCLUDE_DIRS "."
                    EMBED_TXTFILES server_root_cert.pem)
