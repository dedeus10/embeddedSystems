/**
--------------------------------------------------------------------------------
--                                                                            --
--                      UNIVERSIDADE FEDERAL DE SANTA MARIA		              --
--							Centro de Tecnologia			                  --
--						Curso de Engenharia de Computação	                  --
--                                                                            --
--                      Santa Maria - Rio Grande do Sul/BR                    --
--                                                                            --
--------------------------------------------------------------------------------
--                                                                            --
-- Design      : Real Time Home  v2.0                                         --
-- File		   : demostask.c		                                       	  --
-- Author      : Luis Felipe de Deus and Nathanael Jorge Luchetta             --
--                                                                            --
--------------------------------------------------------------------------------
--                                                                            --
-- Created     : 10 Jun 2019                                                  --
-- Update      : 23 Jun 2019                                                  --
--------------------------------------------------------------------------------
--                              Overview                                      --
--                                                                            --
--  Board: SAMD21							                                  --
--  Peripherals: Bluetooth - ATBTLC1000-MR | Temperature: I/O1		          --
--  Features: FreeRTOS v10.0								                  --
--            Tasks: Bluetooth connection, receive and flow control			  --
--            Temperatura SP e estado do lampada salvos em serial flash       --
--------------------------------------------------------------------------------
*/

/// =========================== LIBRARIES ============================================================ 
#include <asf.h>
#include <conf_demo.h>
#include "demotasks.h"
#include "platform.h"
#include "console_serial.h"
#include "at_ble_api.h"
#include "ble_manager.h"
#include "csc_app.h"
#include "cscp.h"
#include "cscs.h"
#include "conf_extint.h"
#include "sio2host.h"
#include "conf_at25dfx.h"

/** =========================== DEFINITIONS ============================================================ 
@def
*/
#define BLE_TASK_PRIORITY      (tskIDLE_PRIORITY + 2)	//3
#define BLE_TASK_DELAY         (200 / portTICK_RATE_MS)

#define ANY_TASK_PRIORITY      (tskIDLE_PRIORITY + 1)	//2
#define TEMP_TASK_DELAY         (100 / portTICK_RATE_MS)

#define GTEMP_TASK_PRIORITY      (tskIDLE_PRIORITY + 3)	//1
#define GTEMP_TASK_DELAY         (100 / portTICK_RATE_MS)


static xSemaphoreHandle t_mutex;

/// =========================== PROTOTYPES OF TASK'S ============================================================ 

static void ble_task(void *p);
static void do_anything_task(void *p);
static void get_temp_task(void *p);

static TaskHandle_t ble_task_handle;
static TaskHandle_t anything_task_handle;
static TaskHandle_t gtemp_task_handle;

uint32_t ble_event_params[BLE_EVENT_PARAM_MAX_SIZE/sizeof(uint32_t)];

/// =========================== PROTOTYPES OF AUX FUNCITONS ============================================================ 
void write_info(uint8_t *aux1, uint8_t *aux2);
void read_info(void);
void configure_ficha(void);

/// =========================== GLOBALS ============================================================ 
char palavra[20];
uint8_t TAM_data = 0;
uint8_t received_data = 0;
char temperature[5] = "";
uint8_t light = 0;
uint8_t setpoint = 0;


/// Received notification data structure 
csc_report_ntf_t recv_ntf_info;

/// Driver_instances
struct spi_module at25dfx_spi;
struct at25dfx_chip_module at25dfx_chip;

/// Data length to be send over the air 
uint16_t send_length = 0;

/// Buffer data to be send over the air 
uint8_t send_data[APP_TX_BUF_SIZE];

/** =========================== CONFIGURE THE LED ============================================================ 
@param void
@return void
*/
static void config_led(void)
{
	struct port_config pin_conf;
	port_get_config_defaults(&pin_conf);

	pin_conf.direction  = PORT_PIN_DIR_OUTPUT;
	port_pin_set_config(LED_0_PIN, &pin_conf);
	port_pin_set_output_level(LED_0_PIN, LED_0_INACTIVE);
}


/** =========================== CONFIGURE SERIAL FLASH ============================================================ 
@param void
@return void
*/
static void at25dfx_init(void)
{
	//! [config_instances]
	struct at25dfx_chip_config at25dfx_chip_config;
	struct spi_config at25dfx_spi_config;
	//! [config_instances]

	//! [spi_setup]
	at25dfx_spi_get_config_defaults(&at25dfx_spi_config);
	at25dfx_spi_config.mode_specific.master.baudrate = AT25DFX_CLOCK_SPEED;
	at25dfx_spi_config.mux_setting = AT25DFX_SPI_PINMUX_SETTING;
	at25dfx_spi_config.pinmux_pad0 = AT25DFX_SPI_PINMUX_PAD0;
	at25dfx_spi_config.pinmux_pad1 = AT25DFX_SPI_PINMUX_PAD1;
	at25dfx_spi_config.pinmux_pad2 = AT25DFX_SPI_PINMUX_PAD2;
	at25dfx_spi_config.pinmux_pad3 = AT25DFX_SPI_PINMUX_PAD3;

	spi_init(&at25dfx_spi, AT25DFX_SPI, &at25dfx_spi_config);
	spi_enable(&at25dfx_spi);
	
	//! [spi_setup]

	//! [chip_setup]
	at25dfx_chip_config.type = AT25DFX_MEM_TYPE;
	at25dfx_chip_config.cs_pin = AT25DFX_CS;

	at25dfx_chip_init(&at25dfx_chip, &at25dfx_spi, &at25dfx_chip_config);

	//! [chip_setup]
}


///@struct Armazena parametros do usuario
typedef struct {
	uint8_t lampada[4];
	uint8_t sp[3];
	
} ficha_user;

ficha_user ficha[1];

/**
@param void
@return void
*/
void configure_ficha(void)
{
	//uint8_t *aux1, *aux2;

	//! [unprotect_sector]
	at25dfx_chip_set_sector_protect(&at25dfx_chip, 0x10000, false);
	//! [unprotect_sector]

	//! [erase_block]
	//at25dfx_chip_erase_block(&at25dfx_chip, 0x10000, AT25DFX_BLOCK_SIZE_4KB);
	//! [erase_block]


	//aux1 = "On";
	//aux2 = "30";

	//! [write_buffer]
	//at25dfx_chip_write_buffer(&at25dfx_chip, 0x10000, aux1, (3));
	//at25dfx_chip_write_buffer(&at25dfx_chip, 0x10020, aux2, (3));

	
	//! [global_protect]
	at25dfx_chip_set_global_sector_protect(&at25dfx_chip, true);
	//! [global_protect]
}

/** =========================== WRITE THE INFO IN SERIAL FLASH ============================================================ 
@param *aux1 ponteiro para informação do estado da lampada
@param *aux2 ponteiro para informação do set point da temperatura
@return void
*/
void write_info(uint8_t *aux1, uint8_t *aux2)
{
	//! [wake_chip]
	at25dfx_chip_wake(&at25dfx_chip);
	//! [wake_chip]
	
	//! [check_presence]
	if (at25dfx_chip_check_presence(&at25dfx_chip) != STATUS_OK) {
		// Handle missing or non-responsive device
	}
	//! [check_presence]
	
	//! [unprotect_sector]
	at25dfx_chip_set_sector_protect(&at25dfx_chip, 0x10000, false);
	//! [unprotect_sector]
	
	//! [erase_block]
	at25dfx_chip_erase_block(&at25dfx_chip, 0x10000, AT25DFX_BLOCK_SIZE_4KB);
	//! [erase_block]
	
	//! [write_buffer]
	at25dfx_chip_write_buffer(&at25dfx_chip, 0x10000, aux1, (3));
	at25dfx_chip_write_buffer(&at25dfx_chip, 0x10020, aux2, (3));
	
	//! [global_protect]
	at25dfx_chip_set_global_sector_protect(&at25dfx_chip, true);
	//! [global_protect]
	
	// sleep
	at25dfx_chip_sleep(&at25dfx_chip);
}

/** =========================== READ INFORMATION FROM SERIAL FLASH ============================================================ 
@param void
@return void
*/
void read_info(void)
{
	//! [wake_chip]
	at25dfx_chip_wake(&at25dfx_chip);
	//! [wake_chip]
	
	//! [check_presence]
	if (at25dfx_chip_check_presence(&at25dfx_chip) != STATUS_OK) {
		// Handle missing or non-responsive device
	}
	//! [check_presence]
	
	//! [unprotect_sector]
	at25dfx_chip_set_sector_protect(&at25dfx_chip, 0x10000, false);
	//! [unprotect_sector]
	
	// Le da memoria flash os usuarios
	at25dfx_chip_read_buffer(&at25dfx_chip, 0x10000, ficha[0].lampada, (3));
	at25dfx_chip_read_buffer(&at25dfx_chip, 0x10020, ficha[0].sp, (3));
	
	//! [global_protect]
	at25dfx_chip_set_global_sector_protect(&at25dfx_chip, true);
	//! [global_protect]

	// sleep
	at25dfx_chip_sleep(&at25dfx_chip);
	DBG_LOG(ficha[0].lampada);
	DBG_LOG(ficha[0].sp);
}

/// =========================== FUNCTIONS OF BLUETOOTH FLOW ============================================================ 
static const ble_gap_event_cb_t app_gap_handle = {
	.connected = app_connected_event_handler,
	.disconnected = app_disconnected_event_handler
};


/**
* @brief app_connected_state blemanager notifies the application about state
* @param[in] at_ble_connected_t
*/
static at_ble_status_t app_connected_event_handler(void *params)
{
	ALL_UNUSED(params);
	return AT_BLE_SUCCESS;
}

/**
* @brief app_connected_state ble manager notifies the application about state
* @param[in] connected
*/
static at_ble_status_t app_disconnected_event_handler(void *params)
{
	/* Started advertisement */
	csc_prf_dev_adv();
	ALL_UNUSED(params);
	return AT_BLE_SUCCESS;
}

/** Function used for receive data 
@param ponteiro para o dado recibo
@param tamanho do dado recebido
@return void
*/
static void csc_app_recv_buf(uint8_t *recv_data, uint8_t recv_len)
{
	uint16_t ind = 0;
	if (recv_len){
		for (ind = 0; ind < 20; ind++){
			//sio2host_putchar(recv_data[ind]);
			if(ind < recv_len)
			palavra[ind] = recv_data[ind];
			else
			palavra[ind] = 0;
		}
		DBG_LOG("\r\n");
		received_data = 1;
		TAM_data = recv_data;
		
	}
}

/// Callback called for new data from remote device 
static void csc_prf_report_ntf_cb(csc_report_ntf_t *report_info)
{
	DBG_LOG("\r\n");
	csc_app_recv_buf(report_info->recv_buff, report_info->recv_buff_len);
}


/** =========================== INITIALIZE DE PERIPHERALS AND CREATE THE TASK'S ============================================================ 
@brief Função inicia alguns periféricos e cria tarefas
@param void
@return void
*/
void demotasks_init(void)
{
	
	/// Initialize serial console 
	sio2host_init();
	
	DBG_LOG("Initializing Custom Serial Chat Application");
	
	/// Initialize the buffer address and buffer length based on user input 
	csc_prf_buf_init(&send_data[0], APP_TX_BUF_SIZE);
	
	/// initialize the ble chip  and Set the device mac address 
	ble_device_init(NULL);
	
	/// Initializing the profile 
	csc_prf_init(NULL);
	
	/// Started advertisement 
	csc_prf_dev_adv();
	
	ble_mgr_events_callback_handler(REGISTER_CALL_BACK,
	BLE_GAP_EVENT_TYPE,
	&app_gap_handle);
	
	/// Register the notification handler 
	notify_recv_ntf_handler(csc_prf_report_ntf_cb);
	
	t_mutex  = xSemaphoreCreateMutex();
	
	at30tse_init();
	
	volatile uint16_t thigh = 0;
	thigh = at30tse_read_register(AT30TSE_THIGH_REG,
	AT30TSE_NON_VOLATILE_REG, AT30TSE_THIGH_REG_SIZE);
	
	volatile uint16_t tlow = 0;
	tlow = at30tse_read_register(AT30TSE_TLOW_REG,
	AT30TSE_NON_VOLATILE_REG, AT30TSE_TLOW_REG_SIZE);
	at30tse_write_config_register(
	AT30TSE_CONFIG_RES(AT30TSE_CONFIG_RES_12_bit));
	
	at25dfx_init();
	
	//! [wake_chip]
	at25dfx_chip_wake(&at25dfx_chip);
	//! [wake_chip]
	
	//! [check_presence]
	if (at25dfx_chip_check_presence(&at25dfx_chip) != STATUS_OK) {
		// Handle missing or non-responsive device
	}
	//! [check_presence]
	
	config_led();
	
	configure_ficha();	
	
	// ----- CRIACAO DAS TAREFAS --- //
	xTaskCreate(do_anything_task,
	(const char *) "ANY",
	configMINIMAL_STACK_SIZE,
	NULL,
	ANY_TASK_PRIORITY,
	&anything_task_handle);
	
	DBG_LOG("Task ANY Created");
	
	xTaskCreate(ble_task,
	(const char *) "BLE",
	configMINIMAL_STACK_SIZE,
	NULL,
	BLE_TASK_PRIORITY,
	&ble_task_handle);
	DBG_LOG("Task BLE Created");
	
	
	xTaskCreate(get_temp_task,
	(const char *) "GTEMP",
	configMINIMAL_STACK_SIZE,
	NULL,
	GTEMP_TASK_PRIORITY,
	&gtemp_task_handle);
	DBG_LOG("Task GTEMP Created");

	vTaskSuspend(anything_task_handle);
	
	
}

/** =========================== TASK ============================================================ 
@param void
@return void
*/
static void do_anything_task(void *p){
	
	while(true){
		
	}
	DBG_LOG("Error in ANY Task");
}
///@def
#define EXIT	0
#define TEMP	1
#define ABOUT	2
#define STATE	3
#define LON		4
#define LOFF	5
#define READ	6
#define SP		7
#define SET		8
#define ECHO	9

/**=========================== BLUETOOTH TASK ============================================================ 
@brief Tarefa do bluetooth, recebe eventos e aplica ações devido alguma ação do usuario
@param ponteiro dos parametros
@return void
*/
static void ble_task(void *p)
{
	char *str_print1;
	uint8_t set_temp = 0;
	char txt[20] = "Temp: ";
	char txt0[60] = "***P. S. Embarcados\n\r**UFSM/CT\n\r*L.Felipe and Nathanael";
	char buf[30];
	uint8_t menu = -1;
	uint8_t init = 1;
	
	read_info();
	if(strcmp(ficha[0].lampada,"On") == 0){
		light = 1;
		port_pin_set_output_level(LED_0_PIN, LED_0_ACTIVE);
		} else if(strcmp(ficha[0].lampada,"Of") == 0){
		light = 0;
		port_pin_set_output_level(LED_0_PIN, LED_0_INACTIVE);
	}
	
	while(true)
	{
		ble_event_task();
		if(received_data == 1){
			if(init == 1){
				init = 0;
				str_print1 = "*Real Time Home*\n\r--- Bem Vindo ----";
				csc_prf_send_data(&str_print1[0], strlen(str_print1));
				str_print1 = "";
			}
			if(set_temp == 1){
				menu  = SET;
				}else if (strcmp(palavra, "exit") == 0){
				menu = EXIT;
				} else if (strcmp(palavra, "temp") == 0){
				menu = TEMP;
				} else if (strcmp(palavra, "about") == 0){
				menu = ABOUT;
				} else if (strcmp(palavra, "state") == 0){
				menu = STATE;
				} else if (strcmp(palavra, "lon") == 0){
				menu = LON;
				} else if (strcmp(palavra, "loff") == 0){
				menu = LOFF;
				} else if (strcmp(palavra, "read") == 0){
				menu = READ;
				} else if (strcmp(palavra, "sp") == 0) {
				menu = SP;
				} else{
				menu = ECHO;
			}
			
			switch (menu)
			{
				case TEMP:
					set_temp = 0;
				
					strcat(txt, temperature);
					strcpy(str_print1, txt);
					csc_prf_send_data(&str_print1[0], strlen(str_print1));
					DBG_LOG(str_print1);
					strcpy(txt,"Temp: ");
					strcpy(temperature,"");
				
				break;
				case ABOUT:
					set_temp = 0;
					strcpy(str_print1, txt0);
					csc_prf_send_data(&str_print1[0], strlen(str_print1));
					DBG_LOG(str_print1);
				break;
				
				case STATE:
					set_temp = 0;
					strcat(txt, temperature);
					strcpy(str_print1, txt);
					csc_prf_send_data(&str_print1[0], strlen(str_print1));
					DBG_LOG(str_print1);
					strcpy(txt,"Temp: ");
					strcpy(temperature,"");
					if(light == 0){
						strcpy(str_print1, "Lampada Desligada");
						csc_prf_send_data(&str_print1[0], strlen(str_print1));
						DBG_LOG(str_print1);
						} else{
						strcpy(str_print1, "Lampada Ligada");
						csc_prf_send_data(&str_print1[0], strlen(str_print1));
						DBG_LOG(str_print1);
					}
				break;
				
				case LON:
					set_temp = 0;
					light = 1;
					
					port_pin_set_output_level(LED_0_PIN, LED_0_ACTIVE);
					read_info();
					write_info("On", ficha[0].sp);
					strcpy(str_print1, "Lampada Ligada");
					csc_prf_send_data(&str_print1[0], strlen(str_print1));
					DBG_LOG(str_print1);
				break;
				
				case LOFF:
					set_temp = 0;
					light = 0;
					port_pin_set_output_level(LED_0_PIN, LED_0_INACTIVE);
					read_info();
					write_info("Of", ficha[0].sp);
					strcpy(str_print1, "Lampada Desligada");
					csc_prf_send_data(&str_print1[0], strlen(str_print1));
					DBG_LOG(str_print1);
				break;
				
				case ECHO:
					set_temp = 0;
					str_print1 = palavra;
					csc_prf_send_data(&str_print1[0], strlen(str_print1));
				break;
				
				case READ:
					set_temp = 0;
					read_info();
					strcpy(buf,"SP Temp: ");
					str_print1 = ficha[0].sp;
					strcat(buf, str_print1);
					strcpy(str_print1, buf);
					csc_prf_send_data(&str_print1[0], strlen(str_print1));
					DBG_LOG(str_print1);
				
				
					strcpy(buf,"Lampada: ");
					strcpy(str_print1,"");
					read_info();
					str_print1 = ficha[0].lampada;
					strcat(buf, str_print1);
					strcpy(str_print1, buf);
					csc_prf_send_data(&str_print1[0], strlen(str_print1));
					DBG_LOG(str_print1);
				break;
				
				case SP:
					str_print1 = "Informe a temperatura: ";
					csc_prf_send_data(&str_print1[0], strlen(str_print1));
					set_temp = 1;
				break;
				
				case SET:
					read_info();
					write_info(ficha[0].lampada, palavra);
					str_print1 = "SetPoint Alterado com sucesso !";
					csc_prf_send_data(&str_print1[0], strlen(str_print1));
					set_temp = 0;
				break;
				
				case EXIT:
					at_ble_disconnect(ble_disconnected_state_handler(ble_event_params), AT_BLE_TERMINATED_BY_USER);
				break;
				
				default:
					DBG_LOG("ERRO 404 not found");
				break;
			}
			
			received_data = 0;
		}
	}
}

/** =========================== GET TEMPERATURE TASK ============================================================ 
@brief Tarefa da leitura de temperatura, efetua comunicação i2c e recupera a temperatura do sensor
@param ponteiro dos parametros
@return void
*/
static void get_temp_task(void *p){
	double temp_res = 0;
	char buf [10];
	char *str_print;
	while(true){
	
		temp_res = at30tse_read_temperature();
		int ires = temp_res;
		itoa(ires,buf,10 );
		char t [10] = "Temp: ";
		strcpy(temperature, buf);
		strcat(t,buf);
		str_print = t;
		DBG_LOG(str_print);
		
		vTaskResume(ble_task_handle);
		vTaskDelay(GTEMP_TASK_DELAY);
	}
	DBG_LOG("Error in GTEMP Task");
}
