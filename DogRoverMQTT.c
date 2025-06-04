#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"        // Biblioteca para arquitetura Wi-Fi da Pico com CYW43
#include "pico/unique_id.h"         // Biblioteca com recursos para trabalhar com os pinos GPIO do Raspberry Pi Pico

// Includes do FreeRTOS
#include "FreeRTOS.h"
#include "FreeRTOSConfig.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include <string.h>              // Biblioteca manipular strings
#include <stdlib.h>              // funções para realizar várias operações, incluindo alocação de memória dinâmica (malloc)

#include "hardware/adc.h"        // Biblioteca da Raspberry Pi Pico para manipulação do conversor ADC
#include "pico/cyw43_arch.h"     // Biblioteca para arquitetura Wi-Fi da Pico com CYW43  

#include "lwip/apps/mqtt.h"         // Biblioteca LWIP MQTT -  fornece funções e recursos para conexão MQTT
#include "lwip/apps/mqtt_priv.h"    // Biblioteca que fornece funções e recursos para Geração de Conexões
#include "lwip/dns.h"               // Biblioteca que fornece funções e recursos suporte DNS:
#include "lwip/altcp_tls.h"         // Biblioteca que fornece funções e recursos para conexões seguras usando TLS:

// Imports para configuração dos periféricos da BitDogLab
#include "led_matrix.h"
#include "ssd1306.h"
#include "hardware/pwm.h"
#include "hardware/i2c.h"
#include "font.h"

// Constantes para o Wi-Fi e MQTT
#define WIFI_SSID "Jr telecom _ Taylan"                  // Substitua pelo nome da sua rede Wi-Fi
#define WIFI_PASSWORD "Suta3021"      // Substitua pela senha da sua rede Wi-Fi
#define MQTT_SERVER "192.168.18.4"                // Substitua pelo endereço do host - broket MQTT: Ex: 192.168.1.107
#define MQTT_USERNAME "taylanmayckon"     // Substitua pelo nome da host MQTT - Username
#define MQTT_PASSWORD "embarcatech"     // Substitua pelo Password da host MQTT - credencial de acesso - caso exista

// Definição dos pinos dos LEDs
#define LED_PIN CYW43_WL_GPIO_LED_PIN   // GPIO do CI CYW43
#define LED_BLUE_PIN 12                 // GPIO12 - LED azul
#define LED_GREEN_PIN 11                // GPIO11 - LED verde
#define LED_RED_PIN 13                  // GPIO13 - LED vermelho

// Buzzers
#define BUZZER_A 21 
#define BUZZER_B 10

// Analogicos
#define JOYSTICK_X 27
#define JOYSTICK_Y 26

// Constantes para a matriz de leds
#define IS_RGBW false
#define LED_MATRIX_PIN 7

// Definições da I2C
#define I2C_PORT i2c1
#define I2C_SDA 14
#define I2C_SCL 15
#define endereco 0x3C

// Mutex para leitura do ADC
SemaphoreHandle_t xADCmutex; // Mutex 

// Variáveis da PIO declaradas no escopo global
PIO pio;
uint sm;
// Booleano para o display
bool cor = true;

// Variáveis do PWM (setado para freq. de 312,5 Hz)
uint wrap = 2000;
uint clkdiv = 25;

// Para descrever o rover
typedef struct {
    int position;
    int collects;
    float battery;
    bool alert_obstacle;
    bool alert_collect;
    bool mqtt;
} Rover;

// Iniciando o rover centralizado e com tudo zerado
Rover rover = {12, 0, 100.00, false, false, true};

Led_frame led_matrix; // Matriz para gerar as cores

typedef enum{ // Tipos de célula
    CELL_FREE,
    CELL_OBSTACLE,
    CELL_PLAYER,
    CELL_COLLECT
} Celltype;

Celltype grid[NUM_PIXELS] = { // Grid com o conteúdo de cada célula (LED)
    CELL_FREE, CELL_FREE, CELL_OBSTACLE, CELL_OBSTACLE, CELL_FREE,
    CELL_FREE, CELL_FREE, CELL_FREE,     CELL_FREE,     CELL_FREE,
    CELL_FREE, CELL_FREE,  CELL_PLAYER,  CELL_FREE,     CELL_FREE,
    CELL_FREE, CELL_OBSTACLE, CELL_FREE, CELL_COLLECT,  CELL_FREE,
    CELL_COLLECT, CELL_FREE, CELL_FREE,  CELL_OBSTACLE, CELL_FREE
};

// Definição da escala de temperatura
#ifndef TEMPERATURE_UNITS
#define TEMPERATURE_UNITS 'C' // Set to 'F' for Fahrenheit
#endif

#ifndef MQTT_SERVER
#error Need to define MQTT_SERVER
#endif

// This file includes your client certificate for client server authentication
#ifdef MQTT_CERT_INC
#include MQTT_CERT_INC
#endif

#ifndef MQTT_TOPIC_LEN
#define MQTT_TOPIC_LEN 100
#endif

//Dados do cliente MQTT
typedef struct {
    mqtt_client_t* mqtt_client_inst;
    struct mqtt_connect_client_info_t mqtt_client_info;
    char data[MQTT_OUTPUT_RINGBUF_SIZE];
    char topic[MQTT_TOPIC_LEN];
    uint32_t len;
    ip_addr_t mqtt_server_address;
    bool connect_done;
    int subscribe_count;
    bool stop_client;
} MQTT_CLIENT_DATA_T;


#ifndef DEBUG_printf
#ifndef NDEBUG
#define DEBUG_printf printf
#else
#define DEBUG_printf(...)
#endif
#endif

#ifndef INFO_printf
#define INFO_printf printf
#endif

#ifndef ERROR_printf
#define ERROR_printf printf
#endif

// Temporização da publicação de cada tópico pelo RP2040 (quantos segundos entre cada envio no tópico)
#define TEMPERATURE_WORKER_TIME_S 10 // Temperatura
#define BATTERY_WORKER_TIME_S 10 // Bateria
#define COLLECT_WORKER_TIME_S 10 // Coletas

// Manter o programa ativo - keep alive in seconds
#define MQTT_KEEP_ALIVE_S 60

// QoS - mqtt_subscribe
// At most once (QoS 0)
// At least once (QoS 1)
// Exactly once (QoS 2)
#define MQTT_SUBSCRIBE_QOS 1
#define MQTT_PUBLISH_QOS 1
#define MQTT_PUBLISH_RETAIN 0

// Tópico usado para: last will and testament
#define MQTT_WILL_TOPIC "/online"
#define MQTT_WILL_MSG "0"
#define MQTT_WILL_QOS 1

#ifndef MQTT_DEVICE_NAME
#define MQTT_DEVICE_NAME "pico"
#endif

// Definir como 1 para adicionar o nome do cliente aos tópicos, para suportar vários dispositivos que utilizam o mesmo servidor
#ifndef MQTT_UNIQUE_TOPIC
#define MQTT_UNIQUE_TOPIC 0
#endif

/* References for this implementation:
 * raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
 * pico-examples/adc/adc_console/adc_console.c */

// Requisição para publicar
static void pub_request_cb(__unused void *arg, err_t err);

// Topico MQTT
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name);

// Controle do LED 
static void control_led(MQTT_CLIENT_DATA_T *state, bool on);

// Publicar temperatura
static void publish_temperature(MQTT_CLIENT_DATA_T *state);

// Publicar bateria
static void publish_battery(MQTT_CLIENT_DATA_T *state);

// Publicar materiais coletados
static void publish_collect(MQTT_CLIENT_DATA_T *state);

// Requisição de Assinatura - subscribe
static void sub_request_cb(void *arg, err_t err);

// Requisição para encerrar a assinatura
static void unsub_request_cb(void *arg, err_t err);

// Tópicos de assinatura
static void sub_unsub_topics(MQTT_CLIENT_DATA_T* state, bool sub);

// Dados de entrada MQTT
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags);

// Dados de entrada publicados
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len);

// Publicar temperatura
static void temperature_worker_fn(async_context_t *context, async_at_time_worker_t *worker);
static async_at_time_worker_t temperature_worker = { .do_work = temperature_worker_fn };

// Publicar porcentagem de bateria
static void battery_worker_fn(async_context_t *context, async_at_time_worker_t *worker);
static async_at_time_worker_t battery_worker = { .do_work = battery_worker_fn };

// Publicar quantidade de coletas
static void collect_worker_fn(async_context_t *context, async_at_time_worker_t *worker);
static async_at_time_worker_t collect_worker = { .do_work = collect_worker_fn };

// Conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status);

// Inicializar o cliente MQTT
static void start_client(MQTT_CLIENT_DATA_T *state);

// Call back com o resultado do DNS
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg);

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// -> FUNÇÕES AUXILIARES
// String para armazenar o tempo restante do semáforo
char converted_num; // Armazena um dígito
char converted_string[3]; // Armazena o número convertido (2 dígitos)
// Função que converte int para char
void int_2_char(int num, char *out){
    *out = '0' + num;
}

void int_2_string(int num){
    if(num<9){ // Gera string para as menores que 10
        int_2_char(num, &converted_num); // Converte o dígito à direita do número para char
        converted_string[0] = '0'; // Char para melhorar o visual
        converted_string[1] = converted_num; // Int convertido para char
        converted_string[2] = '\0'; // Terminador nulo da String 
    }
    else{ // Gera a string para as maiores/iguais que 10
        int divider = num/10; // Obtém as dezenas
        int_2_char(divider, &converted_num);
        converted_string[0] = converted_num;

        int_2_char(num%10, &converted_num); // Obtém a parte das unidades
        converted_string[1] = converted_num; // Int convertido para char
        converted_string[2] = '\0'; // Terminador nulo da String
    }
}

// Função para configurar o PWM e iniciar com 0% de DC
void set_pwm(uint gpio, uint wrap){
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    pwm_set_clkdiv(slice_num, clkdiv);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_enabled(slice_num, true); 
    pwm_set_gpio_level(gpio, 0);
}

// Inicializar os Pinos GPIO para acionamento dos LEDs da BitDogLab
void gpio_led_bitdog(void){
    // Configuração dos LEDs como saída
    gpio_init(LED_BLUE_PIN);
    gpio_set_dir(LED_BLUE_PIN, GPIO_OUT);
    gpio_put(LED_BLUE_PIN, false);
    
    gpio_init(LED_GREEN_PIN);
    gpio_set_dir(LED_GREEN_PIN, GPIO_OUT);
    gpio_put(LED_GREEN_PIN, false);
    
    gpio_init(LED_RED_PIN);
    gpio_set_dir(LED_RED_PIN, GPIO_OUT);
    gpio_put(LED_RED_PIN, false);
}

// Define a movimentação com base no servidor MQTT
void mqttMoveInput(MQTT_CLIENT_DATA_T *state){
    int initial_pos = rover.position; // Armazena a posição inicial do rover
    grid[rover.position] = CELL_FREE; // Prepara o grid para o movimento

    DEBUG_printf("[mqttMoveInput] %s\n", state->data);

    // Botão para cima
    if(lwip_stricmp((const char *)state->data, "up") == 0){
        if(rover.position>=5 && rover.mqtt){ // Restringe a posição para a anterior, caso o movimento vá além do permitido
            rover.position-=5;
        }
        else{
            rover.alert_obstacle=true;
        }
    }
    // Botão para baixo
    else if(lwip_stricmp((const char *)state->data, "down") == 0){
        if(rover.position<=19 && rover.mqtt){ // Restringe a posição para a anterior, caso o movimento vá além do permitido
            rover.position+=5;
        }
        else{
            rover.alert_obstacle=true;
        }
    }
    // Botão para esquerda
    else if(lwip_stricmp((const char *)state->data, "left") == 0){
        // Quando permite o movimento
        if(rover.position % 5 != 0 && rover.mqtt){
            rover.position--;
        }
        // Movimento invalido
        else{
            rover.alert_obstacle = true;
        }
    }
    // Botão para direita
    else if(lwip_stricmp((const char *)state->data, "right") == 0){
        // Quando permite o movimento
        if(rover.position % 5 != 4 && rover.mqtt){
            rover.position++;
        }
        // Movimento invalido
        else{
            rover.alert_obstacle = true;
        }
    }

    if(grid[rover.position] == CELL_COLLECT){ // Incrementa caso tenha sido feita uma coleta
        rover.collects++;
        rover.alert_collect=true;
    }

    if(grid[rover.position] == CELL_OBSTACLE){ // Gera alerta e trava o rover caso tenha obstaculo no destino
        rover.position = initial_pos;
        rover.alert_obstacle=true;
    }

    grid[rover.position] = CELL_PLAYER; // Atualiza o GRID com a nova posição do player
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// -> Tasks do FreeRTOS
void vMQTT_HandlerTask(void *params){
    INFO_printf("[MQTT Client] Inicializando\n");

    // Cria registro com os dados do cliente
    static MQTT_CLIENT_DATA_T state;

    // Inicializa a arquitetura do cyw43
    if (cyw43_arch_init()) {
        panic("Failed to inizialize CYW43");
    }

    // Usa identificador único da placa
    char unique_id_buf[5];
    pico_get_unique_board_id_string(unique_id_buf, sizeof(unique_id_buf));
    for(int i=0; i < sizeof(unique_id_buf) - 1; i++) {
        unique_id_buf[i] = tolower(unique_id_buf[i]);
    }

    // Gera nome único, Ex: pico1234
    char client_id_buf[sizeof(MQTT_DEVICE_NAME) + sizeof(unique_id_buf) - 1];
    memcpy(&client_id_buf[0], MQTT_DEVICE_NAME, sizeof(MQTT_DEVICE_NAME) - 1);
    memcpy(&client_id_buf[sizeof(MQTT_DEVICE_NAME) - 1], unique_id_buf, sizeof(unique_id_buf) - 1);
    client_id_buf[sizeof(client_id_buf) - 1] = 0;
    INFO_printf("Device name %s\n", client_id_buf);

    state.mqtt_client_info.client_id = client_id_buf;
    state.mqtt_client_info.keep_alive = MQTT_KEEP_ALIVE_S; // Keep alive in sec
#if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
    state.mqtt_client_info.client_user = MQTT_USERNAME;
    state.mqtt_client_info.client_pass = MQTT_PASSWORD;
#else
    state.mqtt_client_info.client_user = NULL;
    state.mqtt_client_info.client_pass = NULL;
#endif
    static char will_topic[MQTT_TOPIC_LEN];
    strncpy(will_topic, full_topic(&state, MQTT_WILL_TOPIC), sizeof(will_topic));
    state.mqtt_client_info.will_topic = will_topic;
    state.mqtt_client_info.will_msg = MQTT_WILL_MSG;
    state.mqtt_client_info.will_qos = MQTT_WILL_QOS;
    state.mqtt_client_info.will_retain = true;
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // TLS enabled
#ifdef MQTT_CERT_INC
    static const uint8_t ca_cert[] = TLS_ROOT_CERT;
    static const uint8_t client_key[] = TLS_CLIENT_KEY;
    static const uint8_t client_cert[] = TLS_CLIENT_CERT;
    // This confirms the indentity of the server and the client
    state.mqtt_client_info.tls_config = altcp_tls_create_config_client_2wayauth(ca_cert, sizeof(ca_cert),
            client_key, sizeof(client_key), NULL, 0, client_cert, sizeof(client_cert));
#if ALTCP_MBEDTLS_AUTHMODE != MBEDTLS_SSL_VERIFY_REQUIRED
    WARN_printf("Warning: tls without verification is insecure\n");
#endif
#else
    state->client_info.tls_config = altcp_tls_create_config_client(NULL, 0);
    WARN_printf("Warning: tls without a certificate is insecure\n");
#endif
#endif

    // Conectar à rede WiFI - fazer um loop até que esteja conectado
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        panic("Failed to connect");
    }
    INFO_printf("\nConnected to Wifi\n");

    //Faz um pedido de DNS para o endereço IP do servidor MQTT
    cyw43_arch_lwip_begin();
    int err = dns_gethostbyname(MQTT_SERVER, &state.mqtt_server_address, dns_found, &state);
    cyw43_arch_lwip_end();

    // Se tiver o endereço, inicia o cliente
    if (err == ERR_OK) {
        start_client(&state);
    } else if (err != ERR_INPROGRESS) { // ERR_INPROGRESS means expect a callback
        panic("dns request failed");
    }

    // Loop condicionado a conexão mqtt
    while (!state.connect_done || mqtt_client_is_connected(state.mqtt_client_inst)) {
        cyw43_arch_poll();
        cyw43_arch_wait_for_work_until(make_timeout_time_ms(10000));
    }

    INFO_printf("mqtt client exiting\n");
    return;
}

// Task para controlar a matriz de LEDs endereçáveis
// Função para atualizar a matriz de LEDs com as cores do grid
void update_led_matrix_from_grid(){
    Led_color green = {0, 255, 0};
    Led_color blue = {0, 0, 255};
    Led_color orange = {255, 165, 0};
    Led_color off = {0, 0, 0};

    for (int i = 0; i < NUM_PIXELS; i++) {
        switch (grid[i]) {
            case CELL_FREE:
                led_matrix.led[i] = off;
                break;
            case CELL_OBSTACLE:
                led_matrix.led[i] = orange;
                break;
            case CELL_PLAYER:
                led_matrix.led[i] = green;
                break;
            case CELL_COLLECT:
                led_matrix.led[i] = blue;
                break;
        }
    }
}

void vLedMatrixTask(){
    // Inicializando a PIO
    pio = pio0;
    sm = 0;
    uint offset = pio_add_program(pio, &ws2812_program);
    ws2812_program_init(pio, sm, offset, LED_MATRIX_PIN, 800000, IS_RGBW);

    float matrix_intensity;

    Led_color green = {0, 255, 0};
    Led_color blue = {0, 0, 255};
    Led_color orange = {255, 165, 0};
    Led_color off = {0, 0, 0};

    while(true){
        // Atualiza as cores na matriz RGB
        update_led_matrix_from_grid();

        // Envia para a matriz física da BitDogLab
        matrix_update_leds(&led_matrix, 0.01);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


// Task para controlar o display OLED
void vDisplayOLEDTask(){
    // Configurando a I2C
    i2c_init(I2C_PORT, 400 * 1000);

    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);                    // Set the GPIO pin function to I2C
    gpio_pull_up(I2C_SDA);                                        // Pull up the data line
    gpio_pull_up(I2C_SCL);                                        // Pull up the clock line
    ssd1306_t ssd;                                                // Inicializa a estrutura do display
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT); // Inicializa o display
    ssd1306_config(&ssd);                                         // Configura o display
    ssd1306_send_data(&ssd);                                      // Envia os dados para o display
    // Limpa o display. O display inicia com todos os pixels apagados.
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    while(true){
        ssd1306_fill(&ssd, false); // Limpa o display

        // Frame que será reutilizado para todos
        ssd1306_rect(&ssd, 0, 0, 128, 64, cor, !cor);
        // Nome superior
        ssd1306_rect(&ssd, 0, 0, 128, 12, cor, cor); // Fundo preenchido
        ssd1306_draw_string(&ssd, "DogRover", 4, 3, true); // String: Semaforo
        ssd1306_draw_string(&ssd, "TM", 107, 3, true);
        // Bateria
        int_2_string((int)rover.battery);
        ssd1306_draw_string(&ssd, "BATERIA: ", 4, 16, false);
        ssd1306_draw_string(&ssd, converted_string, 76, 16, false);
        // Modo
        ssd1306_draw_string(&ssd, "MODO: ", 4, 28, false);
        if(rover.mqtt){
            ssd1306_draw_string(&ssd, "MQTT", 52, 28, false);
        }
        else{
            ssd1306_draw_string(&ssd, "BITDOGLAB", 52, 28, false);
        }
        // Coletas
        int_2_string(rover.collects);
        ssd1306_draw_string(&ssd, "COLETAS: ", 4, 40, false);
        ssd1306_draw_string(&ssd, converted_string, 76, 40, false);

        ssd1306_send_data(&ssd); // Envia os dados para o display, atualizando o mesmo
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// Task para os alertas sonoros
void vBuzzerTask(){
    set_pwm(BUZZER_A, wrap);
    set_pwm(BUZZER_B, wrap);

    int buzzer_obstacle_count = 0;
    int buzzer_collect_count = 0;

    while(true){
        // Alerta de obstáculo
        if(rover.alert_obstacle){
            if(buzzer_obstacle_count%2==0){
                pwm_set_gpio_level(BUZZER_A, wrap*0.05);
                pwm_set_gpio_level(BUZZER_B, wrap*0.05);
            }
            else{
                pwm_set_gpio_level(BUZZER_A, 0);
                pwm_set_gpio_level(BUZZER_B, 0);
            }
            buzzer_obstacle_count++;

            if(buzzer_obstacle_count==4){ // Condição que para o alerta sonoro
                buzzer_obstacle_count=0;
                rover.alert_obstacle=false;
            }
        }

        // Alerta de coleta
        else if(rover.alert_collect){
            if(buzzer_collect_count<4 || buzzer_collect_count%2==0){
                pwm_set_gpio_level(BUZZER_A, wrap*0.05);
                pwm_set_gpio_level(BUZZER_B, wrap*0.05);
            }
            else{
                pwm_set_gpio_level(BUZZER_A, 0);
                pwm_set_gpio_level(BUZZER_B, 0);
            }
            buzzer_collect_count++;
            
            if(buzzer_collect_count==8){
                buzzer_collect_count=0;
                rover.alert_collect=false;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50)); // Pequeno delay para reduzir o consumo de CPU
          
    }
}

// Task para decair a bateria
void vBatteryDropTask(){
    while(true){
        rover.battery-=0.1;
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Task para controle manual
void vBitDogLabModeTask(){
    // Configurando o ADC
    adc_init();
    adc_gpio_init(JOYSTICK_X); // Canal 1
    adc_gpio_init(JOYSTICK_Y); // Canal 0

    int initial_pos;

    uint16_t vrx_value;
    uint16_t vry_value;

    while(true){
        // Caso não esteja no modo do mqtt server
        if(!rover.mqtt){
            if(xSemaphoreTake(xADCmutex, portMAX_DELAY) == pdTRUE){ // Verifica se consegue usar o ADC
                // Leitura do Eixo X (Canal 1)
                adc_select_input(1);
                vrx_value = adc_read();
                // Leitura do Eixo Y (Canal 0)
                adc_select_input(0);
                vry_value = adc_read();

                xSemaphoreGive(xADCmutex); // Libera o Mutex do ADC
            }

            initial_pos = rover.position; // Armazena a posição inicial

            grid[rover.position] = CELL_FREE; // Prepara o grid para o movimento

            // Direita
            if(vrx_value>3500){
                if(rover.position % 5 != 4){ // Quando permite o movimento
                    rover.position++;
                }
                else{ // Movimento invalido
                    rover.alert_obstacle = true;
                }
            }
            // Esquerda
            else if(vrx_value<500){
                if(rover.position % 5 != 0){ // Quando permite o movimento
                    rover.position--;
                }
                else{ // Movimento invalido
                    rover.alert_obstacle = true;
                }
            }

            // Cima
            if(vry_value>3500){
                if(rover.position>=5){ // Quando permite o movimento
                    rover.position-=5;
                }
                else{ // Movimento invalido
                    rover.alert_obstacle = true;
                }
            }
            // Baixo
            else if(vry_value<500){
                if(rover.position<=19){ // Quando permite o movimento
                    rover.position+=5;
                }
                else{ // Movimento invalido
                    rover.alert_obstacle=true;
                }
            }

            if(grid[rover.position] == CELL_COLLECT){ // Incrementa caso tenha sido feita uma coleta
                rover.collects++;
                rover.alert_collect=true;
            }

            if(grid[rover.position] == CELL_OBSTACLE){ // Gera alerta e trava o rover caso tenha obstaculo no destino
                rover.position = initial_pos;
                rover.alert_obstacle=true;
            }

            grid[rover.position] = CELL_PLAYER; // Atualiza o GRID com a nova posição do player
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// Task para leitura da temperatura
float robot_temperature; // Temperatura definida globalmente

void vReadTemperatureTask(){
    adc_set_temp_sensor_enabled(true);

    while(true){
        // Proteção de Mutex para o ADC
        if(xSemaphoreTake(xADCmutex, portMAX_DELAY) == pdTRUE){
            adc_select_input(4);
            const float conversionFactor = 3.3f / (1 << 12);

            float adc = (float)adc_read() * conversionFactor;
            robot_temperature = 27.0f - (adc - 0.706f) / 0.001721f;
            xSemaphoreGive(xADCmutex); // Liberando o mutex
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vLedRGBTask(){
    // Inicializando os pinos do Led RGB como PWM
    set_pwm(LED_BLUE_PIN, wrap);
    set_pwm(LED_RED_PIN, wrap);
    set_pwm(LED_GREEN_PIN, wrap);

    int led_obstacle_count = 0;

    while(true){
        // Aumenta a intensidade do LED azul conforme a bateria descarrega
        pwm_set_gpio_level(LED_BLUE_PIN, wrap*(1-(rover.battery/100))/4);

        // Pisca o led nos alertas
        if(rover.alert_obstacle){
            pwm_set_gpio_level(LED_RED_PIN, wrap*0.05);
            pwm_set_gpio_level(LED_GREEN_PIN, wrap*0.05);
            vTaskDelay(pdMS_TO_TICKS(50));
            pwm_set_gpio_level(LED_RED_PIN, 0);
            pwm_set_gpio_level(LED_GREEN_PIN, 0);
            vTaskDelay(pdMS_TO_TICKS(50));
            pwm_set_gpio_level(LED_RED_PIN, wrap*0.05);
            pwm_set_gpio_level(LED_GREEN_PIN, wrap*0.05);
            vTaskDelay(pdMS_TO_TICKS(50));
            pwm_set_gpio_level(LED_RED_PIN, 0);
            pwm_set_gpio_level(LED_GREEN_PIN, 0);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
// -> Função main
int main(void) {
    // Inicializa todos os tipos de bibliotecas stdio padrão presentes que estão ligados ao binário.
    stdio_init_all();

    // Inicializa o conversor ADC
    adc_init();

    // Inicializa o Mutex para leitura do ADC
    xADCmutex = xSemaphoreCreateMutex();

    xTaskCreate(vMQTT_HandlerTask, "MQTT Handler Task", configMINIMAL_STACK_SIZE, NULL, 10, NULL); // Prioridade mais alta
    xTaskCreate(vLedMatrixTask, "Led Matrix Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vDisplayOLEDTask, "Display OLED Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL); 
    xTaskCreate(vBuzzerTask, "Buzzer Alert Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vBatteryDropTask, "Battery Drop Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vBitDogLabModeTask, "BitDogLab Mode Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vReadTemperatureTask, "Read Temperature Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);
    xTaskCreate(vLedRGBTask, "Led RGB Task", configMINIMAL_STACK_SIZE, NULL, tskIDLE_PRIORITY, NULL);

    vTaskStartScheduler();
    panic_unsupported();
}



/* References for this implementation:
 * raspberry-pi-pico-c-sdk.pdf, Section '4.1.1. hardware_adc'
 * pico-examples/adc/adc_console/adc_console.c */
// Requisição para publicar
static void pub_request_cb(__unused void *arg, err_t err) {
    if (err != 0) {
        ERROR_printf("pub_request_cb failed %d", err);
    }
}

//Topico MQTT
static const char *full_topic(MQTT_CLIENT_DATA_T *state, const char *name) {
#if MQTT_UNIQUE_TOPIC
    static char full_topic[MQTT_TOPIC_LEN];
    snprintf(full_topic, sizeof(full_topic), "/%s%s", state->mqtt_client_info.client_id, name);
    return full_topic;
#else
    return name;
#endif
}

// Controle do LED 
static void control_led(MQTT_CLIENT_DATA_T *state, bool on) {
    // Publish state on /state topic and on/off led board
    const char* message = on ? "On" : "Off";
    if (on)
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
    else
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);

    mqtt_publish(state->mqtt_client_inst, full_topic(state, "/led/state"), message, strlen(message), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
}

// Publicar temperatura
static void publish_temperature(MQTT_CLIENT_DATA_T *state) {
    static float old_temperature;
    const char *temperature_key = full_topic(state, "/temperature");
    float temperature = robot_temperature;
    if (temperature != old_temperature) {
        old_temperature = temperature;
        // Publica no tópico /temperature
        char temp_str[16];
        snprintf(temp_str, sizeof(temp_str), "%.2f", temperature);
        INFO_printf("Publishing %s to %s\n", temp_str, temperature_key);
        mqtt_publish(state->mqtt_client_inst, temperature_key, temp_str, strlen(temp_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }
}

// Publicar bateria
static void publish_battery(MQTT_CLIENT_DATA_T *state){
    static float old_battery;
    const char *battery_key = full_topic(state, "/battery");
    float battery = rover.battery;
    if(battery != old_battery){
        old_battery = battery;
        // Publica no tópico /battery
        char bat_str[16];
        snprintf(bat_str, sizeof(bat_str), "%.2f", battery);
        INFO_printf("Publishing %s to %s\n", bat_str, battery_key);
        mqtt_publish(state->mqtt_client_inst, battery_key, bat_str, strlen(bat_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }
}

// Publicar coletas
static void publish_collect(MQTT_CLIENT_DATA_T *state){
    static int old_collect;
    const char *collect_key = full_topic(state, "/collect");
    int collect = rover.collects;
    if(collect != old_collect){
        old_collect = collect;
        // Publica no tópico /battery
        char col_str[16];
        snprintf(col_str, sizeof(col_str), "%d", collect);
        INFO_printf("Publishing %s to %s\n", col_str, collect_key);
        mqtt_publish(state->mqtt_client_inst, collect_key, col_str, strlen(col_str), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);
    }
}

// Requisição de Assinatura - subscribe
static void sub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (err != 0) {
        panic("subscribe request failed %d", err);
    }
    state->subscribe_count++;
}

// Requisição para encerrar a assinatura
static void unsub_request_cb(void *arg, err_t err) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (err != 0) {
        panic("unsubscribe request failed %d", err);
    }
    state->subscribe_count--;
    assert(state->subscribe_count >= 0);

    // Stop if requested
    if (state->subscribe_count <= 0 && state->stop_client) {
        mqtt_disconnect(state->mqtt_client_inst);
    }
}

// Tópicos de assinatura
static void sub_unsub_topics(MQTT_CLIENT_DATA_T* state, bool sub) {
    mqtt_request_cb_t cb = sub ? sub_request_cb : unsub_request_cb;
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/mqtt_mode"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/movement"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/ping"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
    mqtt_sub_unsub(state->mqtt_client_inst, full_topic(state, "/exit"), MQTT_SUBSCRIBE_QOS, cb, state, sub);
}

// Dados de entrada MQTT
static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
#if MQTT_UNIQUE_TOPIC
    const char *basic_topic = state->topic + strlen(state->mqtt_client_info.client_id) + 1;
#else
    const char *basic_topic = state->topic;
#endif
    strncpy(state->data, (const char *)data, len);
    state->len = len;
    state->data[len] = '\0';

    DEBUG_printf("Topic: %s, Message: %s\n", state->topic, state->data);

    // Ações com base nas leituras dos tópicos MQTT
    if (strcmp(basic_topic, "/mqtt_mode") == 0){
        if (lwip_stricmp((const char *)state->data, "On") == 0 || strcmp((const char *)state->data, "1") == 0)
            rover.mqtt = true;
        else if (lwip_stricmp((const char *)state->data, "Off") == 0 || strcmp((const char *)state->data, "0") == 0)
            rover.mqtt = false;

    } else if (strcmp(basic_topic, "/movement") == 0) {
        mqttMoveInput(state);

    } else if (strcmp(basic_topic, "/ping") == 0) {
        char buf[11];
        snprintf(buf, sizeof(buf), "%u", to_ms_since_boot(get_absolute_time()) / 1000);
        mqtt_publish(state->mqtt_client_inst, full_topic(state, "/uptime"), buf, strlen(buf), MQTT_PUBLISH_QOS, MQTT_PUBLISH_RETAIN, pub_request_cb, state);

    } else if (strcmp(basic_topic, "/exit") == 0) {
        state->stop_client = true; // stop the client when ALL subscriptions are stopped
        sub_unsub_topics(state, false); // unsubscribe
    }
}

// Dados de entrada publicados
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    strncpy(state->topic, topic, sizeof(state->topic));
}

// Publicar temperatura
static void temperature_worker_fn(async_context_t *context, async_at_time_worker_t *worker) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)worker->user_data;
    publish_temperature(state);
    async_context_add_at_time_worker_in_ms(context, worker, TEMPERATURE_WORKER_TIME_S * 1000);
}

// Publicar bateria
static void battery_worker_fn(async_context_t *context, async_at_time_worker_t *worker){
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)worker->user_data;
    publish_battery(state);
    async_context_add_at_time_worker_in_ms(context, worker, BATTERY_WORKER_TIME_S * 1000);
}

// Publicar coletas
static void collect_worker_fn(async_context_t *context, async_at_time_worker_t *worker){
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)worker->user_data;
    publish_collect(state);
    async_context_add_at_time_worker_in_ms(context, worker, COLLECT_WORKER_TIME_S * 1000);
}

// Conexão MQTT
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
    MQTT_CLIENT_DATA_T* state = (MQTT_CLIENT_DATA_T*)arg;
    if (status == MQTT_CONNECT_ACCEPTED) {
        state->connect_done = true;
        sub_unsub_topics(state, true); // subscribe;

        // indicate online
        if (state->mqtt_client_info.will_topic) {
            mqtt_publish(state->mqtt_client_inst, state->mqtt_client_info.will_topic, "1", 1, MQTT_WILL_QOS, true, pub_request_cb, state);
        }

        // Publica a temperatura
        temperature_worker.user_data = state;
        async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &temperature_worker, 0);

        // Publica a bateria
        battery_worker.user_data = state;
        async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &battery_worker, 0);

        // Publica as coletas
        collect_worker.user_data = state;
        async_context_add_at_time_worker_in_ms(cyw43_arch_async_context(), &collect_worker, 0);

    } else if (status == MQTT_CONNECT_DISCONNECTED) {
        if (!state->connect_done) {
            panic("Failed to connect to mqtt server");
        }
    }
    else {
        panic("Unexpected status");
    }
}

// Inicializar o cliente MQTT
static void start_client(MQTT_CLIENT_DATA_T *state) {
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    const int port = MQTT_TLS_PORT;
    INFO_printf("Using TLS\n");
#else
    const int port = MQTT_PORT;
    INFO_printf("Warning: Not using TLS\n");
#endif

    state->mqtt_client_inst = mqtt_client_new();
    if (!state->mqtt_client_inst) {
        panic("MQTT client instance creation error");
    }
    INFO_printf("IP address of this device %s\n", ipaddr_ntoa(&(netif_list->ip_addr)));
    INFO_printf("Connecting to mqtt server at %s\n", ipaddr_ntoa(&state->mqtt_server_address));

    cyw43_arch_lwip_begin();
    if (mqtt_client_connect(state->mqtt_client_inst, &state->mqtt_server_address, port, mqtt_connection_cb, state, &state->mqtt_client_info) != ERR_OK) {
        panic("MQTT broker connection error");
    }
#if LWIP_ALTCP && LWIP_ALTCP_TLS
    // This is important for MBEDTLS_SSL_SERVER_NAME_INDICATION
    mbedtls_ssl_set_hostname(altcp_tls_context(state->mqtt_client_inst->conn), MQTT_SERVER);
#endif
    mqtt_set_inpub_callback(state->mqtt_client_inst, mqtt_incoming_publish_cb, mqtt_incoming_data_cb, state);
    cyw43_arch_lwip_end();
}

// Call back com o resultado do DNS
static void dns_found(const char *hostname, const ip_addr_t *ipaddr, void *arg) {
    MQTT_CLIENT_DATA_T *state = (MQTT_CLIENT_DATA_T*)arg;
    if (ipaddr) {
        state->mqtt_server_address = *ipaddr;
        start_client(state);
    } else {
        panic("dns request failed");
    }
}
