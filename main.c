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
char sentido;

struct mqtt_connect_client_info_t mqtt_client_info=
{
  "CORERasp/pico_w",  /*client id*/
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

    sentido = motor[0];
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

    if (sentido == 'H'){
      printf("\nSentido do motor: horário\n");
    }
    else if (sentido == 'A') {
      printf("\nSentido do motor: anti-horário\n");
    } else {
      printf("\nSentido do motor: ERRO\n");
    }

    if (duty < 0){
      duty = 0;
    } else if (duty > 100){
      duty = 100;
    }
    printf("\nPorcentagem da potência setada: %d\n", duty);
}
  
static void mqtt_incoming_publish_cb(void *arg, const char *topic, u32_t tot_len) {
  printf("topic %s\n", topic);
}
 
static void mqtt_request_cb(void *arg, err_t err) { 
  printf(("MQTT client request cb: err %d\n", (int)err));
}
 
static void mqtt_connection_cb(mqtt_client_t *client, void *arg, mqtt_connection_status_t status) {
  printf(("MQTT client connection cb: status %d\n", (int)status));

  if (status == MQTT_CONNECT_ACCEPTED) {
    err_t erro = mqtt_sub_unsub(client, "motor/control", 0, &mqtt_request_cb, NULL, 1); //1 se inscreve - 0 de desinscreve
    
    if (erro == ERR_OK) {
      printf("Inscrito com Sucesso!\n");
    } else {
      printf("Falha ao se inscrever!\n");
    }
  } else {
    printf("Conexão rejeitada\n");
  }
}
 
int main(){
  stdio_init_all();           //Configuração inicial da placa
  printf("Iniciando...\n");   //Informativo

  //Iniciando o chip WiFi
  if (cyw43_arch_init()){
    printf("Falha ao Iniciar chip WiFi\n");
    return 1;
  }

  //Configurando a placa como cliente
  cyw43_arch_enable_sta_mode();
  printf("Conectando ao %s\n",WIFI_SSID);

  //Conectando ao roteador
  if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK, 30000)){
    printf("Falha ao conectar\n");
    return 1;
  }
  printf("Conectado ao WiFi %s\n",WIFI_SSID);

  //Conectando ao MQTT
  ip_addr_t addr;
  if(!ip4addr_aton(MQTT_SERVER, &addr)){
    printf("ip error\n");
    return 1;
  }
  printf("Conectando ao MQTT\n");

  mqtt_client_t* cliente_mqtt = mqtt_client_new();    //Criando novo cliente
  mqtt_set_inpub_callback(cliente_mqtt, &mqtt_incoming_publish_cb, &mqtt_incoming_data_cb, NULL);   //Lincando as callbacks de envio e recebimento de dados
  err_t erro = mqtt_client_connect(cliente_mqtt, &addr, 1883, &mqtt_connection_cb, NULL, &mqtt_client_info);
  if(erro != ERR_OK){
    printf("Erro de conexão\n");
    return 1;
  }

  printf("Conectado ao MQTT!\n");

  //PWM
  gpio_set_function(EN_A, GPIO_FUNC_PWM);
  int fatia_pwm = pwm_gpio_to_slice_num(EN_A);
  pwm_config config = pwm_get_default_config();
  pwm_config_set_clkdiv(&config, 4.0f);
  pwm_init(fatia_pwm, &config, true);

  gpio_init(A1);
  gpio_init(A2);

  gpio_set_dir(A1, GPIO_OUT);
  gpio_set_dir(A2, GPIO_OUT);

  while(true){
    printf("Crie uma subscription com o nome INFO/button e aperte o botão bootsel de sua placa para mais informações!\n");
    if (board_button_read()){
      mqtt_publish(cliente_mqtt, "INFO/button", "Envie um Plaintext como motor/control", 37, 0, false, &mqtt_request_cb, NULL);
      sleep_ms(100);
      mqtt_publish(cliente_mqtt, "INFO/button", "H para horário, A para anti-horário + porcentagem desejada", 58, 0, false, &mqtt_request_cb, NULL);
      sleep_ms(100);
      mqtt_publish(cliente_mqtt, "INFO/button", "Ex: H 50, A 100, H 75...", 24, 0, false, &mqtt_request_cb, NULL);
      sleep_ms(100);
    }

    int duty_cycle = ((duty*65535)/(100));

    if (sentido == 'H'){
      gpio_put(A1, true);
      gpio_put(A2, false);
      pwm_set_gpio_level(EN_A, duty_cycle);
    } else if (sentido == 'A'){
      gpio_put(A1, false);
      gpio_put(A2, true);
      pwm_set_gpio_level(EN_A, duty_cycle);
    }

    sleep_ms(1000);
  }
}