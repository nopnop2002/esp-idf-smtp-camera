/**
 * SMTP email client
 *
 * Adapted from the `ssl_mail_client` example in mbedtls.
 *
 * Original Copyright (C) 2006-2016, ARM Limited, All Rights Reserved, Apache 2.0 License.
 * Additions Copyright (C) Copyright 2015-2020 Espressif Systems (Shanghai) PTE LTD, Apache 2.0 License.
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *	   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"

//#include <sys/param.h>

#include "mbedtls/platform.h"
#include "mbedtls/net_sockets.h"
#include "mbedtls/esp_debug.h"
#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/error.h"
//#include "mbedtls/certs.h"
#include "mbedtls/base64.h"
#include "mbedtls/version.h"

#include "cmd.h"

extern QueueHandle_t xQueueSmtp;
extern SemaphoreHandle_t xSemaphoreSmtp;

/* Constants that are configurable in menuconfig */
#define MAIL_SERVER			CONFIG_SMTP_SERVER
#define MAIL_PORT			CONFIG_SMTP_PORT_NUMBER
#define SENDER_MAIL			CONFIG_SMTP_SENDER_MAIL
#define SENDER_PASSWORD		CONFIG_SMTP_SENDER_PASSWORD
#define RECIPIENT_MAIL		CONFIG_SMTP_RECIPIENT_MAIL

#define SERVER_USES_STARTSSL 1

static const char *TAG = "SMTP";

#define BUF_SIZE 512

#define VALIDATE_MBEDTLS_RETURN(ret, min_valid_ret, max_valid_ret, goto_label)	\
	do {																		\
		if (ret < min_valid_ret || ret > max_valid_ret) {						\
			goto goto_label;													\
		}																		\
	} while (0)																	\


/**
 * Root cert for smtp.googlemail.com, taken from gmail_root_cert.pem
 *
 * The PEM file was extracted from the output of this command:
 * openssl s_client -showcerts -connect smtp.googlemail.com:587 -starttls smtp
 *
 * The CA root cert is the last cert given in the chain of certs.
 *
 * To embed it in the app binary, the PEM file is named
 * in the component.mk COMPONENT_EMBED_TXTFILES variable.
 */

extern const uint8_t server_root_cert_pem_start[] asm("_binary_gmail_root_cert_pem_start");
extern const uint8_t server_root_cert_pem_end[]   asm("_binary_gmail_root_cert_pem_end");

static int write_and_get_response(mbedtls_net_context *sock_fd, unsigned char *buf, size_t len)
{
	int ret;
	const size_t DATA_SIZE = 128;
	unsigned char data[DATA_SIZE];
	char code[4];
	size_t i, idx = 0;

	if (len) {
		ESP_LOGD(TAG, "%s", buf);
	}

	if (len && (ret = mbedtls_net_send(sock_fd, buf, len)) <= 0) {
		ESP_LOGE(TAG, "mbedtls_net_send failed with error -0x%x", -ret);
		return ret;
	}

	do {
		len = DATA_SIZE - 1;
		memset(data, 0, DATA_SIZE);
		ret = mbedtls_net_recv(sock_fd, data, len);

		if (ret <= 0) {
			ESP_LOGE(TAG, "mbedtls_net_recv failed with error -0x%x", -ret);
			goto exit;
		}

		data[len] = '\0';
		ESP_LOGD(TAG, "\n%s", data);
		len = ret;
		for (i = 0; i < len; i++) {
			if (data[i] != '\n') {
				if (idx < 4) {
					code[idx++] = data[i];
				}
				continue;
			}

			if (idx == 4 && code[0] >= '0' && code[0] <= '9' && code[3] == ' ') {
				code[3] = '\0';
				ret = atoi(code);
				goto exit;
			}

			idx = 0;
		}
	} while (1);

exit:
	return ret;
}

static int write_ssl_and_get_response(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len)
{
	int ret;
	const size_t DATA_SIZE = 128;
	unsigned char data[DATA_SIZE];
	char code[4];
	size_t i, idx = 0;

	if (len) {
		ESP_LOGD(TAG, "%s", buf);
	}

	while (len && (ret = mbedtls_ssl_write(ssl, buf, len)) <= 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			ESP_LOGE(TAG, "mbedtls_ssl_write failed with error -0x%x", -ret);
			goto exit;
		}
	}

	do {
		len = DATA_SIZE - 1;
		memset(data, 0, DATA_SIZE);
		ret = mbedtls_ssl_read(ssl, data, len);

		if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
			continue;
		}

		if (ret <= 0) {
			ESP_LOGE(TAG, "mbedtls_ssl_read failed with error -0x%x", -ret);
			goto exit;
		}

		ESP_LOGD(TAG, "%s", data);

		len = ret;
		for (i = 0; i < len; i++) {
			if (data[i] != '\n') {
				if (idx < 4) {
					code[idx++] = data[i];
				}
				continue;
			}

			if (idx == 4 && code[0] >= '0' && code[0] <= '9' && code[3] == ' ') {
				code[3] = '\0';
				ret = atoi(code);
				goto exit;
			}

			idx = 0;
		}
	} while (1);

exit:
	return ret;
}

static int write_ssl_data(mbedtls_ssl_context *ssl, unsigned char *buf, size_t len)
{
	int ret;

	if (len) {
		ESP_LOGD(TAG, "%s", buf);
	}

	while (len && (ret = mbedtls_ssl_write(ssl, buf, len)) <= 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			ESP_LOGE(TAG, "mbedtls_ssl_write failed with error -0x%x", -ret);
			return ret;
		}
	}

	return 0;
}

static int perform_tls_handshake(mbedtls_ssl_context *ssl)
{
	int ret = -1;
	uint32_t flags;
	char *buf = NULL;
	buf = (char *) calloc(1, BUF_SIZE);
	if (buf == NULL) {
		ESP_LOGE(TAG, "calloc failed for size %d", BUF_SIZE);
		goto exit;
	}

	ESP_LOGI(TAG, "Performing the SSL/TLS handshake...");

	fflush(stdout);
	while ((ret = mbedtls_ssl_handshake(ssl)) != 0) {
		if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
			ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
			goto exit;
		}
	}

	ESP_LOGI(TAG, "Verifying peer X.509 certificate...");

	if ((flags = mbedtls_ssl_get_verify_result(ssl)) != 0) {
		/* In real life, we probably want to close connection if ret != 0 */
		ESP_LOGW(TAG, "Failed to verify peer certificate!");
		mbedtls_x509_crt_verify_info(buf, BUF_SIZE, "  ! ", flags);
		ESP_LOGW(TAG, "verification info: %s", buf);
	} else {
		ESP_LOGI(TAG, "Certificate verified.");
	}

	ESP_LOGI(TAG, "Cipher suite is %s", mbedtls_ssl_get_ciphersuite(ssl));
	ret = 0; /* No error */

exit:
	if (buf) {
		free(buf);
	}
	return ret;
}

typedef struct smtp_client_handle {
	mbedtls_entropy_context entropy;
	mbedtls_ctr_drbg_context ctr_drbg;
	mbedtls_ssl_context ssl;
	mbedtls_x509_crt cacert;
	mbedtls_ssl_config conf;
	mbedtls_net_context server_fd;
} smtp_client_mbedtls_handle_t;

static void smtp_client_delete_mbedtls_conn(smtp_client_mbedtls_handle_t *client)
{
	if (client == NULL) {
		return;
	}
	mbedtls_net_free(&client->server_fd);
	mbedtls_x509_crt_free(&client->cacert);
	mbedtls_entropy_free(&client->entropy);
	mbedtls_ssl_config_free(&client->conf);
	mbedtls_ctr_drbg_free(&client->ctr_drbg);
	mbedtls_ssl_free(&client->ssl);
}

static int smtp_client_init_mbedtls_conn(smtp_client_mbedtls_handle_t *client)
{
	assert(client != NULL);
	int ret;
	mbedtls_ssl_init(&client->ssl);
	mbedtls_x509_crt_init(&client->cacert);
	mbedtls_ctr_drbg_init(&client->ctr_drbg);
	ESP_LOGD(TAG, "Seeding the random number generator");

	mbedtls_ssl_config_init(&client->conf);

	mbedtls_entropy_init(&client->entropy);
	if ((ret = mbedtls_ctr_drbg_seed(&client->ctr_drbg, mbedtls_entropy_func, &client->entropy,
									 NULL, 0)) != 0) {
		ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned -0x%x", -ret);
		goto exit;
	}

	ESP_LOGD(TAG, "Loading the CA root certificate...");

	ret = mbedtls_x509_crt_parse(&client->cacert, server_root_cert_pem_start,
								 server_root_cert_pem_end - server_root_cert_pem_start);

	if (ret < 0) {
		ESP_LOGE(TAG, "mbedtls_x509_crt_parse returned -0x%x", -ret);
		goto exit;
	}

	ESP_LOGD(TAG, "Setting hostname for TLS session...");

	/* Hostname set here should match CN in server certificate */
	if ((ret = mbedtls_ssl_set_hostname(&client->ssl, MAIL_SERVER)) != 0) {
		ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -ret);
		goto exit;
	}

	ESP_LOGD(TAG, "Setting up the SSL/TLS structure...");

	if ((ret = mbedtls_ssl_config_defaults(&client->conf,
			MBEDTLS_SSL_IS_CLIENT,
			MBEDTLS_SSL_TRANSPORT_STREAM,
			MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
		ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned -0x%x", -ret);
		goto exit;
	}

	mbedtls_ssl_conf_authmode(&client->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
	mbedtls_ssl_conf_ca_chain(&client->conf, &client->cacert, NULL);
	mbedtls_ssl_conf_rng(&client->conf, mbedtls_ctr_drbg_random, &client->ctr_drbg);
#ifdef CONFIG_MBEDTLS_DEBUG
	mbedtls_esp_enable_debug_log(&conf, 4);
#endif

	if ((ret = mbedtls_ssl_setup(&client->ssl, &client->conf)) != 0) {
		ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x", -ret);
		goto exit;
	}

	mbedtls_net_init(&client->server_fd);

	/* params completely initialized till this point */
	//smtp_client_tls_conn_init = true;
	return ret;

exit:
	smtp_client_delete_mbedtls_conn(client);
	return ret;
}

void smtp_client_task(void *pvParameters)
{
	char *buf = NULL;
	unsigned char base64_buffer[128];
	int ret, len;
	size_t base64_len;

	smtp_client_mbedtls_handle_t *client = NULL;
	client = (smtp_client_mbedtls_handle_t*)calloc(1,sizeof(smtp_client_mbedtls_handle_t));
	if (client == NULL) {
		ESP_LOGE(TAG, "Could not allocate memory for smtp_client_mbedtls_handle");
		vTaskDelete(NULL);
	}
	
	ret = smtp_client_init_mbedtls_conn(client);
	if (ret != 0) {
		ESP_LOGE(TAG, "Error in initializing mbedTLS params, returned %02x", ret);
		char error_buf[200];
		mbedtls_strerror(ret, error_buf, 100);
		ESP_LOGE(TAG, "Last error was: -0x%x - %s", -ret, error_buf);
		vTaskDelete(NULL);
	}

	//mbedtls_entropy_context entropy;
	//mbedtls_ctr_drbg_context ctr_drbg;
	//mbedtls_ssl_context ssl;
	//mbedtls_x509_crt cacert;
	//mbedtls_ssl_config conf;
	//mbedtls_net_context server_fd;

	SMTP_t smtpBuf;
	while(1) {
		ESP_LOGI(TAG,"Waitting...");
		xSemaphoreGive(xSemaphoreSmtp);

		xQueueReceive(xQueueSmtp, &smtpBuf, portMAX_DELAY);
		ESP_LOGI(TAG,"smtpBuf.command=%d", smtpBuf.command);
		if (smtpBuf.command == CMD_HALT) break;

		xSemaphoreTake(xSemaphoreSmtp, portMAX_DELAY);
		ESP_LOGI(TAG,"smtpBuf.localFileName[%s]", smtpBuf.localFileName);
		ESP_LOGI(TAG,"smtpBuf.remoteFileName[%s]", smtpBuf.remoteFileName);

		ESP_LOGI(TAG, "Connecting to %s:%s...", MAIL_SERVER, MAIL_PORT);

		if ((ret = mbedtls_net_connect(&client->server_fd, MAIL_SERVER, MAIL_PORT, MBEDTLS_NET_PROTO_TCP)) != 0) {
			ESP_LOGE(TAG, "mbedtls_net_connect returned -0x%x", -ret);
			goto exit;
		}

		ESP_LOGI(TAG, "Connected.");

		mbedtls_ssl_set_bio(&client->ssl, &client->server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

		buf = (char *) calloc(1, BUF_SIZE);
		if (buf == NULL) {
			ESP_LOGE(TAG, "calloc failed for size %d", BUF_SIZE);
			goto exit;
		}
#if SERVER_USES_STARTSSL
		/* Get response */
		ret = write_and_get_response(&client->server_fd, (unsigned char *) buf, 0);
		VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);

		ESP_LOGI(TAG, "Writing EHLO to server...");
		len = snprintf((char *) buf, BUF_SIZE, "EHLO %s\r\n", "ESP32");
		ret = write_and_get_response(&client->server_fd, (unsigned char *) buf, len);
		VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);

		ESP_LOGI(TAG, "Writing STARTTLS to server...");
		len = snprintf((char *) buf, BUF_SIZE, "STARTTLS\r\n");
		ret = write_and_get_response(&client->server_fd, (unsigned char *) buf, len);
		VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);

		ret = perform_tls_handshake(&client->ssl);
		if (ret != 0) {
			goto exit;
		}

#else /* SERVER_USES_STARTSSL */
		ret = perform_tls_handshake(&client->ssl);
		if (ret != 0) {
			goto exit;
		}

		/* Get response */
		ret = write_ssl_and_get_response(&client->ssl, (unsigned char *) buf, 0);
		VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);
		ESP_LOGI(TAG, "Writing EHLO to server...");

		len = snprintf((char *) buf, BUF_SIZE, "EHLO %s\r\n", "ESP32");
		ret = write_ssl_and_get_response(&client->ssl, (unsigned char *) buf, len);
		VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);

#endif /* SERVER_USES_STARTSSL */

		/* Authentication */
		ESP_LOGI(TAG, "Authentication...");

		ESP_LOGI(TAG, "Write AUTH LOGIN");
		len = snprintf( (char *) buf, BUF_SIZE, "AUTH LOGIN\r\n" );
		ret = write_ssl_and_get_response(&client->ssl, (unsigned char *) buf, len);
		VALIDATE_MBEDTLS_RETURN(ret, 200, 399, exit);

		ESP_LOGI(TAG, "Write USER NAME");
		ret = mbedtls_base64_encode((unsigned char *) base64_buffer, sizeof(base64_buffer),
			&base64_len, (unsigned char *) SENDER_MAIL, strlen(SENDER_MAIL));
		if (ret != 0) {
			ESP_LOGE(TAG, "Error in mbedtls encode! ret = -0x%x", -ret);
			goto exit;
		}
		len = snprintf((char *) buf, BUF_SIZE, "%s\r\n", base64_buffer);
		ret = write_ssl_and_get_response(&client->ssl, (unsigned char *) buf, len);
		VALIDATE_MBEDTLS_RETURN(ret, 300, 399, exit);

		ESP_LOGI(TAG, "Write PASSWORD");
		ret = mbedtls_base64_encode((unsigned char *) base64_buffer, sizeof(base64_buffer),
			&base64_len, (unsigned char *) SENDER_PASSWORD, strlen(SENDER_PASSWORD));
		if (ret != 0) {
			ESP_LOGE(TAG, "Error in mbedtls encode! ret = -0x%x", -ret);
			goto exit;
		}
		len = snprintf((char *) buf, BUF_SIZE, "%s\r\n", base64_buffer);
		ret = write_ssl_and_get_response(&client->ssl, (unsigned char *) buf, len);
		VALIDATE_MBEDTLS_RETURN(ret, 200, 399, exit);

		/* Compose email */
		ESP_LOGI(TAG, "Write MAIL FROM");
		len = snprintf((char *) buf, BUF_SIZE, "MAIL FROM:<%s>\r\n", SENDER_MAIL);
		ret = write_ssl_and_get_response(&client->ssl, (unsigned char *) buf, len);
		VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);

		ESP_LOGI(TAG, "Write RCPT");
		len = snprintf((char *) buf, BUF_SIZE, "RCPT TO:<%s>\r\n", RECIPIENT_MAIL);
		ret = write_ssl_and_get_response(&client->ssl, (unsigned char *) buf, len);
		VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);

		ESP_LOGI(TAG, "Write DATA");
		len = snprintf((char *) buf, BUF_SIZE, "DATA\r\n");
		ret = write_ssl_and_get_response(&client->ssl, (unsigned char *) buf, len);
		VALIDATE_MBEDTLS_RETURN(ret, 300, 399, exit);

		ESP_LOGI(TAG, "Write Content");
		/* We do not take action if message sending is partly failed. */
		len = snprintf((char *) buf, BUF_SIZE,
			"From: %s\r\nSubject: mbed TLS mail\r\n"
			"To: %s\r\n"
			"MIME-Version: 1.0 (mime-construct 1.9)\n",
			"ESP32 SMTP Client", RECIPIENT_MAIL);

		/**
		 * Note: We are not validating return for some ssl_writes.
		 * If by chance, it's failed; at worst email will be incomplete!
		 */
		ret = write_ssl_data(&client->ssl, (unsigned char *) buf, len);

		/* Multipart boundary */
		len = snprintf((char *) buf, BUF_SIZE,
			"Content-Type: multipart/mixed;boundary=XYZabcd1234\n"
			"--XYZabcd1234\n");
		ret = write_ssl_data(&client->ssl, (unsigned char *) buf, len);

		/* Text */
		len = snprintf((char *) buf, BUF_SIZE,
			"Content-Type: text/plain; charset=UTF-8; format=flowed\n"
			"Content-Transfer-Encoding: 7bit\n"
			"Your ESP-IDF is %s.\r\n"
			"Your MBEDTLS is %s.\r\n"
			"\n\n--XYZabcd1234\n",
			esp_get_idf_version(), MBEDTLS_VERSION_STRING_FULL);
		ret = write_ssl_data(&client->ssl, (unsigned char *) buf, len);

		/* Attachment */
#if 0
		len = snprintf((char *) buf, BUF_SIZE,
			"Content-Type: image/png;name=esp_logo.png\n"
			"Content-Transfer-Encoding: base64\n"
			"Content-Disposition:attachment;filename=\"esp_logo.png\"\n\n");
#else
		len = snprintf((char *) buf, BUF_SIZE,
			"Content-Type: image/jpeg;name=%s\n"
			"Content-Transfer-Encoding: base64\n"
			"Content-Disposition:attachment;filename=\"%s\"\n\n", 
			smtpBuf.remoteFileName,	smtpBuf.remoteFileName);
#endif
		ret = write_ssl_data(&client->ssl, (unsigned char *) buf, len);


		/* Image contents... */
		ESP_LOGI(TAG, "Reading file");
		int read_bytes = ((sizeof (base64_buffer) - 1) / 4) * 3;
		unsigned char read_buffer[128];
		//FILE* fp = fopen("/spiffs/esp_logo.png", "rb");
		FILE* fp = fopen(smtpBuf.localFileName, "rb");
		if (fp == NULL) {
			ESP_LOGE(TAG, "Failed to open file for reading");
		} else {
			while (!feof(fp)) {
				//int read_length = fread(read_buffer, read_bytes, 1, fp);
				int read_length = fread(read_buffer, 1, read_bytes, fp);
				ESP_LOGD(TAG, "read_length=%d", read_length);
				ret = mbedtls_base64_encode((unsigned char *) base64_buffer, sizeof(base64_buffer),
					&base64_len, read_buffer, read_length);
				if (ret != 0) {
					ESP_LOGE(TAG, "Error in mbedtls encode! ret = -0x%x", -ret);
					goto exit;
				}
				len = snprintf((char *) buf, BUF_SIZE, "%s\r\n", base64_buffer);
				ret = write_ssl_data(&client->ssl, (unsigned char *) buf, len);
			}
			fclose(fp);
		}

		len = snprintf((char *) buf, BUF_SIZE, "\n--XYZabcd1234\n");
		ret = write_ssl_data(&client->ssl, (unsigned char *) buf, len);

		len = snprintf((char *) buf, BUF_SIZE, "\r\n.\r\n");
		ret = write_ssl_and_get_response(&client->ssl, (unsigned char *) buf, len);
		VALIDATE_MBEDTLS_RETURN(ret, 200, 299, exit);
		ESP_LOGI(TAG, "Email sent!");

		/* Close connection */
		mbedtls_ssl_close_notify(&client->ssl);
		ret = 0; /* No errors */

exit:
		mbedtls_ssl_session_reset(&client->ssl);
		mbedtls_net_free(&client->server_fd);

		if (ret != 0) {
			mbedtls_strerror(ret, buf, 100);
			ESP_LOGE(TAG, "Last error was: -0x%x - %s", -ret, buf);
		}

		putchar('\n'); /* Just a new line */
		if (buf) {
			free(buf);
		}
	} // end while
	vTaskDelete(NULL);
}
