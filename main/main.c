#include <stdio.h>
#include <stdlib.h>
#include "esp_wifi.h"
#include "esp_now.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "time.h"
#include "display_spi_config.h"

#define TAG "Sender"
typedef struct __attribute__((packed)){
    uint8_t n_piscadas;
    uint8_t cor_led;
    uint8_t velocidade_piscadas;
} dados_tarefa_t;

typedef struct __attribute__((packed)){
    uint8_t tipo_pacote;
    uint8_t esp_mac_address[6];
    bool concluida;
} dados_resposta_t;

typedef struct {
    uint8_t task_mac_address[6];
    char nome_task[25];
} dados_task_t;

typedef struct{
    dados_tarefa_t dados_tarefa_display;
    dados_task_t dados_task_display;
} dados_display_t;

typedef enum {
    TAREFA_ENVIADA,
    TAREFA_CONCLUIDA,
} state_machine_tarefas;

int cont_tarefas = 0;
QueueHandle_t fila_macs_concluidos;
SemaphoreHandle_t semaforo_display;

uint8_t lista_macs[100][6];
dados_task_t dados_task;


void tela_espera(){
    draw_text(10, 10, "Aguardando conexões...", 0x07E0, 2);
    desenhar_tela();
}

void exibir_ultima_task(dados_display_t dados){
        char texto_mac[20];
        char texto_n_piscadas[10];
        char texto_cor[10];
        char texto_velocidade[10];
        snprintf(texto_mac, sizeof(texto_mac),
         "%02X:%02X:%02X:%02X:%02X:%02X",
         (unsigned)dados.dados_task_display.task_mac_address[0], (unsigned)dados.dados_task_display.task_mac_address[1], (unsigned)dados.dados_task_display.task_mac_address[2],
         (unsigned)dados.dados_task_display.task_mac_address[3], (unsigned)dados.dados_task_display.task_mac_address[4], (unsigned)dados.dados_task_display.task_mac_address[5]);
        snprintf(texto_n_piscadas, sizeof(texto_n_piscadas), "%u", dados.dados_tarefa_display.n_piscadas);
        snprintf(texto_cor, sizeof(texto_cor), "%u", dados.dados_tarefa_display.cor_led);
        snprintf(texto_velocidade, sizeof(texto_velocidade), "%u", dados.dados_tarefa_display.velocidade_piscadas);      
        xSemaphoreTake(semaforo_display, portMAX_DELAY);
        clear_screen(0x0000);
        draw_text(10, 10, "Task: ", 0x07E0, 2);
        draw_text(80, 10, dados.dados_task_display.nome_task, 0xFFFF, 2);
        draw_text(10, 35, "Para: ", 0x07E0, 1);
        draw_text(80, 35, texto_mac, 0xFFFF, 1);
        draw_text(10, 45, "-----------------", 0xFFFF, 2);
        draw_text(10, 65, "Piscadas: ", 0x07E0, 2);
        draw_text(170, 65, texto_n_piscadas, 0xFFFF, 2);
        draw_text(10, 85, "Cor: ", 0x07E0, 2);
        draw_text(170, 85, texto_cor, 0xFFFF, 2);
        draw_text(10, 105, "Velocidade: ", 0x07E0, 2);
        draw_text(170, 105, texto_velocidade, 0xFFFF, 2);

        desenhar_tela();
        xSemaphoreGive(semaforo_display);
        
}




void task_ger_tarefa(void *params) {
    state_machine_tarefas st_tarefa = TAREFA_CONCLUIDA;
    dados_task_t dados_task_interno = *(dados_task_t*)params;
    free(params);
    uint8_t mac_fila[6]; 
    ESP_LOGI(dados_task_interno.nome_task, "Task criada para %d", dados_task_interno.task_mac_address);
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, &dados_task_interno.task_mac_address, 6);
    peer.channel = 0; 
    peer.encrypt = false;
    esp_now_add_peer(&peer);

    srand(rand());
    for(;;) {
        if (st_tarefa == TAREFA_CONCLUIDA) {
            dados_tarefa_t tarefa = {
            .cor_led = rand() % 3,
            .n_piscadas = rand() % 10,
            .velocidade_piscadas = rand() % 800,
            };
            ESP_LOGI(dados_task_interno.nome_task, "Dados enviados: cor led: %d, numero de piscadas: %d, velocidade: %d", tarefa.cor_led,tarefa.n_piscadas,tarefa.velocidade_piscadas);
            esp_now_send(dados_task_interno.task_mac_address, (uint8_t*)&tarefa, sizeof(tarefa));
            st_tarefa = TAREFA_ENVIADA;
            ESP_LOGI("TESTE", "TAREFA ENVIADA");
            dados_display_t dados_display = {
                .dados_tarefa_display = tarefa,
                .dados_task_display = dados_task_interno
            };
            exibir_ultima_task(dados_display);

        } else {
            xQueuePeek(fila_macs_concluidos, &mac_fila, portMAX_DELAY);
            ESP_LOGI("TESTE", "OLHEI A FILA");
            if (memcmp(mac_fila, dados_task_interno.task_mac_address, sizeof(mac_fila)) == 0){
                ESP_LOGI("TESTE", "item removido da queue");
                xQueueReceive(fila_macs_concluidos, &mac_fila, 5000);
                st_tarefa = TAREFA_CONCLUIDA;
            } else {
                vTaskDelay(pdMS_TO_TICKS(500));
            }
            
        }


    }
}
void criar_task(dados_resposta_t resposta){
    cont_tarefas++;   
    dados_task_t *args = malloc(sizeof(*args));
    if (args == NULL)  {
        ESP_LOGE("Task Criação Task", "Falha ao alocar daddos na memória para task");
        return;
    }

    snprintf(args->nome_task, sizeof(args->nome_task),"Task_%d", cont_tarefas);
    memcpy(args->task_mac_address, resposta.esp_mac_address, sizeof(args->task_mac_address));
    BaseType_t resultado = xTaskCreate(task_ger_tarefa, args->nome_task, 4096, (void*)args, 2, NULL);
    if (resultado != pdPASS){
        free(args);
        ESP_LOGE("Task Criação Task", "Falha ao criar task");
    }
    memcpy(resposta.esp_mac_address, lista_macs[cont_tarefas], sizeof(resposta.esp_mac_address));
}

void on_data_sent(const esp_now_send_info_t *tx_info , esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        ESP_LOGI(TAG, "Entregue com sucesso: ", tx_info);
        
    } else {
        ESP_LOGW(TAG, "Falha na entrega");
    }
}

void on_data_recv(const esp_now_recv_info_t *info, const uint8_t *data, int len){
    if (len != sizeof(dados_resposta_t)) return;
    dados_resposta_t resposta;
    memcpy(&resposta, data, sizeof(dados_resposta_t));
    switch(resposta.tipo_pacote){
        case 1:

            if(esp_now_is_peer_exist(resposta.esp_mac_address)) return;
            criar_task(resposta);

            
            break;
        case 2:
            ESP_LOGI("TESTE", "Informações recebidas");
            uint8_t mac[6];
            memcpy(mac, resposta.esp_mac_address, sizeof(resposta.esp_mac_address));
            xQueueSend(fila_macs_concluidos, &mac, 10000);
            break;



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

    fila_macs_concluidos = xQueueCreate(10, sizeof(uint8_t[6]));
    
    semaforo_display = xSemaphoreCreateBinary();

    xSemaphoreGive(semaforo_display);

    config_spi();
    clear_screen(0x0000);
    tela_espera();


}
