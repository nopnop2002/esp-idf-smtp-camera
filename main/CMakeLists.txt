set(srcs "main.c" "smtp_client.c")

if (CONFIG_SHUTTER_ENTER)
	list(APPEND srcs "keyboard.c")
elseif (CONFIG_SHUTTER_GPIO)
	list(APPEND srcs "gpio.c")
elseif (CONFIG_SHUTTER_TCP)
	list(APPEND srcs "tcp_server.c")
elseif (CONFIG_SHUTTER_UDP)
	list(APPEND srcs "udp_server.c")
elseif (CONFIG_SHUTTER_MQTT)
	list(APPEND srcs "mqtt_sub.c")
endif()

idf_component_register(SRCS "${srcs}" INCLUDE_DIRS "." EMBED_TXTFILES gmail_root_cert.pem)

