#define CMD_TAKE 100
#define CMD_SMTP 200
#define CMD_HALT 400

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
