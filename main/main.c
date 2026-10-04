#include <stdio.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time.h"

#define TAG "Sender"
typedef struct {
    int n_piscadas;
    int cor_led;
    int velocidade_piscadas;
} dados_tarefa_t;

typedef struct {
    int esp;
    bool concluida;
} dados_resposta_t;

typedef enum {
    TAREFA_ENVIADA,
    TAREFA_CONCLUIDA,
} state_machine_tarefas;

state_machine_tarefas st_tarefas1 = TAREFA_CONCLUIDA;
state_machine_tarefas st_tarefas2 = TAREFA_CONCLUIDA;

uint8_t receptor_mac1[] = {0xD4, 0xE9, 0xF4, 0x66, 0x48, 0x54};
uint8_t receptor_mac2[] = {0xD4, 0xE9, 0xF4, 0xA3, 0x53, 0x88};

void on_data_sent(const esp_now_send_info_t *tx_info , esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Entregue com sucesso");
        
    } else {
        ESP_LOGW(TAG, "Falha na entrega");
    }
}

void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len){
    if (len != sizeof(dados_resposta_t)) return;

    dados_resposta_t resposta;
    memcpy(&resposta, data, sizeof(dados_resposta_t));

    if (resposta.concluida){
        if (resposta.esp == 1) st_tarefas1 = TAREFA_CONCLUIDA;
        if (resposta.esp == 2) st_tarefas2 = TAREFA_CONCLUIDA;
    }

}

void task_mandar_tarefa_1(void *params) {

    srand(rand());
    for(;;) {
        
        if (st_tarefas1 == TAREFA_CONCLUIDA) {
            dados_tarefa_t tarefa = {
            .cor_led = rand() % 3,
            .n_piscadas = rand() % 10,
            .velocidade_piscadas = rand() % 1500,
            };
            ESP_LOGI("Task_1", "Dados enviados: cor led: %d, numero de piscadas: %d, velocidade: %d", tarefa.cor_led,tarefa.n_piscadas,tarefa.velocidade_piscadas);
            esp_now_send(receptor_mac1, (uint8_t*)&tarefa, sizeof(tarefa));
            st_tarefas1 = TAREFA_ENVIADA;

        } else {
            vTaskDelay(pdMS_TO_TICKS(500));
        }


    }
}
void task_mandar_tarefa_2(void *params) {
    int timeout = 0;
    srand(rand());
    for(;;) {
        
        if (st_tarefas2 == TAREFA_CONCLUIDA) {
            dados_tarefa_t tarefa = {
            .cor_led = rand() % 3,
            .n_piscadas = rand() % 10,
            .velocidade_piscadas = rand() % 1500,
            };
            ESP_LOGI("Task_2", "Dados enviados: cor led: %d, numero de piscadas: %d, velocidade: %d", tarefa.cor_led,tarefa.n_piscadas,tarefa.velocidade_piscadas);
            esp_now_send(receptor_mac2, (uint8_t*)&tarefa, sizeof(tarefa));
            st_tarefas2 = TAREFA_ENVIADA;
            timeout = 0;
        } else {
            vTaskDelay(pdMS_TO_TICKS(500));
        }


    }
}

void app_main(void)
{
    nvs_flash_init();
    esp_netif_init();
    esp_event_loop_create_default();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();

    esp_now_init();
    esp_now_register_send_cb(on_data_sent);
    esp_now_register_recv_cb(on_data_recv);

    //configuração peers
    esp_now_peer_info_t peer1 = {};
    memcpy(peer1.peer_addr, receptor_mac1, 6);
    peer1.channel = 0; 
    peer1.encrypt = false;
    esp_now_add_peer(&peer1);

    esp_now_peer_info_t peer2 = {};
    memcpy(peer2.peer_addr, receptor_mac2, 6);
    peer2.channel = 0; 
    peer2.encrypt = false;
    esp_now_add_peer(&peer2);

    

    xTaskCreate(task_mandar_tarefa_1, "Mandar_tarefa 1", 2048, NULL, 5, NULL);
    xTaskCreate(task_mandar_tarefa_2, "Mandar_tarefa 2", 2048, NULL, 5, NULL);
}
