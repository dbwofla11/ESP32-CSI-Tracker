#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "rom/ets_sys.h"

// [수정] 채널 1번 고정 및 실제 MAC 설정
#define TARGET_CHANNEL 1
static const uint8_t TX_MAC[] = {0x30, 0x30, 0xF9, 0x69, 0x02, 0x08};
static const char *TAG = "CSI_TX_S3";

static void wifi_init_s3() {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));


    
    ESP_ERROR_CHECK(esp_wifi_set_mac(WIFI_IF_STA, TX_MAC)); // MAC 고정
    ESP_ERROR_CHECK(esp_wifi_start());

    // [추가] 주변 AP와 연결 시도 중단 (채널 고정의 핵심)
    ESP_ERROR_CHECK(esp_wifi_disconnect());
    // [추가] MAC 고정 전 혹은 후에 Promiscuous 모드 활성화
    ESP_ERROR_CHECK(esp_wifi_set_promiscuous(true));

    esp_wifi_set_max_tx_power(80);
    // [중요] 채널 1번으로 강제 고정 (HT20 모드로 간섭 최소화)
    ESP_ERROR_CHECK(esp_wifi_set_channel(TARGET_CHANNEL, WIFI_SECOND_CHAN_NONE));
    ESP_LOGI(TAG, "Wi-Fi Fixed to Channel %d", TARGET_CHANNEL);
}

void app_main() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_flash_init();
    }

    wifi_init_s3();

    // ESP-NOW 초기화 및 Peer 추가
    ESP_ERROR_CHECK(esp_now_init());
    esp_now_peer_info_t peer = {
        .channel = TARGET_CHANNEL,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
        .peer_addr = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff} // 브로드캐스트
    };
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    // 패킷 정의 사기 패킷 만들기 
    uint8_t send_data[150]; 
    memset(send_data, 0, sizeof(send_data));

    // [802.11 Mac Header 흉내내기]
    send_data[0] = 0x08; // Frame Control: Data 패킷으로 위장 (또는 0x80 Beacon)
    send_data[1] = 0x00;
    send_data[2] = 0x00; // Duration

    // Destination MAC (Broadcast: FF:FF:FF:FF:FF:FF)
    memset(&send_data[4], 0xFF, 6); 

    // Source MAC (우리 TX 주소: 30:30:F9:69:02:08)
    memcpy(&send_data[10], TX_MAC, 6);

    // BSSID (보통 Source MAC과 동일하게 설정)
    memcpy(&send_data[16], TX_MAC, 6);

    // Sequence Number (22~23번 바이트는 비워둠)

    // 나머지 부분을 의미 있는 데이터로 채움 (128바이트 이상 권장)
    for(int i=24; i<150; i++) {
        send_data[i] = (uint8_t)i; 
    }

    // 패킷 전송 시작 
    ESP_LOGI(TAG, "Starting Fake-Packet CSI Transmission...");

    while (1) {
        // [강력 고정] 매번 채널을 다시 설정하는 것은 좋으나, 
        // esp_now_send 직전에 설정하는 것이 확실합니다.
        esp_wifi_set_channel(TARGET_CHANNEL, WIFI_SECOND_CHAN_NONE);
        
        esp_err_t res = esp_now_send(peer.peer_addr, send_data, sizeof(send_data));
        
        if (count++ % 50 == 0) { // 5초에 한 번만 출력 (100ms * 50)
            ESP_LOGI(TAG, "Still Sending... (Total: %d)", count);
        }   

        if (res != ESP_OK) {
            ESP_LOGW(TAG, "Send error: %s", esp_err_to_name(res));
        }

        // 너무 빠르면 RX(수신측)의 시리얼 출력이 밀려서 안 보일 수 있습니다.
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }
}