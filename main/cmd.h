typedef enum {CMD_TAKE, CMD_SMTP, CMD_HALT} COMMAND;

typedef struct {
	uint16_t command;
	TaskHandle_t taskHandle;
} CMD_t;

typedef struct {
	uint16_t command;
	TaskHandle_t taskHandle;
	char localFileName[64];
	size_t localFileSize;
	char remoteFileName[64];
} SMTP_t;
