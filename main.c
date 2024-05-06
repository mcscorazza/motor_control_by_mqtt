#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "bsp/board.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"

#include "lwip/apps/mqtt.h"
 
#define WIFI_SSID "Corazza"
#define WIFI_PASSWORD "corazza123"
#define MQTT_SERVER "54.146.113.169"
 
#define EN_A 18
#define A1 16
#define A2 17

int duty = 100;
char direction;

struct mqtt_connect_client_info_t mqtt_client_info=
{
  "corazza/pico_w",  /*client id*/
  NULL,               /* user */
  NULL,               /* pass */
  0,                  /* keep alive */
  NULL,               /* will_topic */
  NULL,               /* will_msg */
  0,                  /* will_qos */
  0                   /* will_retain */
#if LWIP_ALTCP && LWIP_ALTCP_TLS
  , NULL
#endif
};

static void mqtt_incoming_data_cb(void *arg, const u8_t *data, u16_t len, u8_t flags) {
    char motor[30];
    memset(motor, "\0", 30);
    strncpy(motor, data, len);

    direction = motor[0];
    int i = 1;
    while (motor[i] != '\0') {
        if (isdigit(motor[i])) {
            char numero[30];
            int j = 0;
            while (isdigit(motor[i])) {
                numero[j++] = motor[i++];
            }
            numero[j] = '\0';
            duty = atoi(numero);
        } else {
            i++;
        }
    }
    printf("------------------------------------------------\n");
    if (direction == 'W'){
      printf("Motor Direction: CW\n");
    }
    else if (direction == 'C') {
      printf("Motor Direction: CCW\n");
    } else {
      printf("Motor Direction: ERROR!\n");
    }

    if (duty < 0){
      duty = 0;
    } else if (duty > 100){
      duty = 100;
    }
    printf("Power percentage: %d\n", duty);
    printf("------------------------------------------------\n\n");
}
  
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
  printf("\nIncoming info form topic %s: \n", topic);
}
 
static void mqtt_request_cb(void *arg, err_t err) { 
  printf(("MQTT client request cb: err %d \n", (int)err));
}
 
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
  printf(("MQTT client connection cb: status %d \n", (int)status));

  if (status == MQTT_CONNECT_ACCEPTED) {
    err_t erro = mqtt_sub_unsub(client, "corazza/control", 0, &mqtt_request_cb, NULL, 1); //1 se inscreve - 0 de desinscreve
    
    if (erro == ERR_OK) {
      printf("Success subscribed!\n");
    } else {
      printf("Subscribe fail!\n");
    }
  } else {
    printf("Subscribe rejected!\n");
  }
}
 
int main(){
  stdio_init_all();           //Configuração inicial da placa
  printf("Starting...\n");   //Informativo

  //Iniciando o chip WiFi
  if (cyw43_arch_init()){
    printf("Fail to init CYW43!\n");
    return 1;
  }

  //Configurando a placa como cliente
  cyw43_arch_enable_sta_mode();
  printf("Connecting to %s...\n",WIFI_SSID);

  //Conectando ao roteador
  if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)){
    printf("Fail to WiFi connection!\n");
    return 1;
  }
  printf("WiFi Connected: %s\n",WIFI_SSID);

  //Conectando ao MQTT
  ip_addr_t addr;
  if(!ip4addr_aton(MQTT_SERVER, &addr)){
    printf("IP error\n");
    return 1;
  }
  printf("Connecting to MQTT...\n");

  mqtt_client_t* cliente_mqtt = mqtt_client_new();    //Criando novo cliente
  mqtt_set_inpub_callback(cliente_mqtt, &mqtt_incoming_publish_cb, &mqtt_incoming_data_cb, NULL);   //Lincando as callbacks de envio e recebimento de dados
  err_t erro = mqtt_client_connect(cliente_mqtt, &addr, 1883, &mqtt_connection_cb, NULL, &mqtt_client_info);
  if(erro != ERR_OK){
    printf("MQTT Connection Error!\n");
    return 1;
  }

  printf("MQTT Connected!\n");

  //PWM
  gpio_set_function(EN_A, GPIO_FUNC_PWM);
  int slice_pwm = pwm_gpio_to_slice_num(EN_A);
  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, 4.0f);
  pwm_init(slice_pwm, &config, true);

  gpio_init(A1);
  gpio_init(A2);

  gpio_set_dir(A1, GPIO_OUT);
  gpio_set_dir(A2, GPIO_OUT);

  while(true){
    printf("Send a PlainText to 'corazza/control' with W [CW] or C [CCW] + Percentage\n");
    int duty_cycle = ((duty*65535)/(100));
    if (direction == 'W'){
      gpio_put(A1, true);
      gpio_put(A2, false);
      pwm_set_gpio_level(EN_A, duty_cycle);
    } else if (direction == 'C'){
      gpio_put(A1, false);
      gpio_put(A2, true);
      pwm_set_gpio_level(EN_A, duty_cycle);
    }
    sleep_ms(1000);
  }
}