#include "stm32f10x_usart.h"

#include "hal.h"
#include "i2c.h"
#include "parameters.h"
#include "flashsize.h"


char log_buf[4096] = "";
char *log_cur = log_buf;

char line_buf[1024] = "";

FlashParameters parameters;

int CONS_BAUD = 115200;
int BLUETOOTH_BAUD = 38400;
int GPS_BAUD = 19200;

#define BT_AT "AT\r\n"
#define BT_AT_ORGL "AT+ORGL\r\n"
#define BT_AT_INIT "AT+INIT\r\n"
#define BT_AT_RESET "AT+RESET\r\n"
#define BT_AT_VERSION "AT+VERSION?\r\n"
#define BT_AT_NAME "AT+NAME=Ked Tracker 3\r\n"
#define BT_AT_UART "AT+UART=115200,0,0\r\n"

static void setupHardware(void) {
	IO_Configuration(false);
 	UART_Configuration(CONS_BAUD, GPS_BAUD);
}

static void deInit(void) {
  USART_ITConfig(USART1, USART_IT_RXNE, DISABLE);
  USART_ITConfig(USART2, USART_IT_RXNE, DISABLE);
  USART_Cmd(USART1, DISABLE);
  USART_Cmd(USART2, DISABLE);
}

static void sleep(int ms) {
	while (ms--) {
		long wait = 10000;
		while (wait--) {
			__asm__("nop");
		}
	}
}

static void consoleWrite(const char *str) {
	while (*str) {
		CONS_UART_Write(*(str++));
	};
}

static void log(const char *str) {
	while (*str) {
		*log_cur = *str;
		log_cur++;
		str++;
	}
	*log_cur = 0;
}

static int readline(char *dest) {
	uint8_t c = 0;
	int cnt = 0;
	long timeleft = 200;
	while (c != '\n') {
		while (!CONS_UART_Read(c) && --timeleft) {
			sleep(1);
		};
		if (!timeleft) break;
		cnt++;
		*(dest++) = c;
	}
	*dest = 0;
	return cnt;
}

static void setupGreet(void) {
	LED_PCB_On();
	Beep_Note(0x48);
	sleep(50);
	Beep_Note(0x00);
	LED_PCB_Off();
	log("\r\nKed Tracker Setup\r\n");
}

static void setupParameters(void) {
	parameters.setDefault();
	log("* Parameters initialized: ");
	log_cur += parameters.Print(log_cur);
}

static int _btSay(const char *command, int expectedLines) {
	consoleWrite(command);
	log("    >>> "); log(command);
	for (int i=0; i<expectedLines; i++) {
		if (!readline(line_buf)) return 0;
		log("    <<< "); log(line_buf);
	}
	// sleep(3);
	return 1;
}

static int _btChat(void) {
	// format bluetooth name
	char bt_name[32] = "";
	int offset = Format_String(bt_name, "AT+NAME=Ked Tracker ");
	offset += Format_Hex(bt_name + offset, parameters.AcftID, 6);
	bt_name[offset++] = '\r';
	bt_name[offset++] = '\n';
	bt_name[offset++] = 0;


 	if (!_btSay(BT_AT_VERSION, 2)) return 3;
	if (!_btSay(bt_name, 1)) return 2;
	if (!_btSay(BT_AT_UART, 1)) return 1;
	return 0;
}

static int setupBluetooth(void) {
	log("* Setting up bluetooth: \r\n");
	sleep(500);
 	UART_Configuration(BLUETOOTH_BAUD, GPS_BAUD);

 	int err = _btChat();

	if (!err) {
		log("  Done.\r\n");
	}
	else{
		log("  Fail.\r\n");
	}
 	UART_Configuration(CONS_BAUD, GPS_BAUD);
	return err;

}

static void setupGoodbye(void) {
	log("Setup completed!\r\n\r\n");
	LED_PCB_On();
}

static void hang(void) {
	while (true) {
		consoleWrite(log_buf);
		sleep(500);
	}
}

static void error(int err) {
	while(err--) {
		LED_PCB_On();
		Beep_Note(0x48);
		sleep(50);
		Beep_Note(0x00);
		LED_PCB_Off();
		sleep(50);
	}
	hang();
}

typedef void (*pFunction)(void);
static uint32_t JumpAddress;
static pFunction Jump;
static void jumpToBootloader(void) {
	JumpAddress = *(FlashStart + 1);
	Jump = (pFunction)JumpAddress;
	SCB->VTOR = (uint32_t)FlashStart;
	__set_MSP(*FlashStart);
	Jump();
	while (1) continue;
}

int main(void) {
	int err;
	setupHardware();

	setupGreet();
	setupParameters();

	err = setupBluetooth();
	if (err) error(err);

	setupGoodbye();
	consoleWrite(log_buf);
	consoleWrite("Starting bootloader...\r\n");
	sleep(20);
	deInit();
	jumpToBootloader();
	return 0;
}
