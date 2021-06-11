set(srcs "main.c"
	"keyboard.c"
	"gpio.c"
	"smtp_client.c"
	"tcp_server.c"
	"udp_server.c")

idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS "."
    EMBED_TXTFILES gmail_root_cert.pem)

