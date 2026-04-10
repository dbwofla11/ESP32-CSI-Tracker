#include <stdio.h>
#include <string.h>
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// static const char *TAG = "CSI_RX_FINAL";

// TX 보드의 Source MAC 주소 (TX 코드와 반드시 일치해야 함)
static const uint8_t TX_MAC[] = {0x30, 0x30, 0xF9, 0x69, 0x02, 0x08};

static void wifi_csi_cb(void *ctx, wifi_csi_info_t *info)
{

    if (!info) return;

    // 일단 아무 패킷이나 잡히면 그 패킷의 MAC 주소를 무조건 출력
    // (RSSI가 어느 정도 강한 것만 필터링해서 노이즈 제거)
    if (info->rx_ctrl.rssi > -80) {
        printf("Detected MAC: %02X:%02X:%02X:%02X:%02X:%02X",
               info->mac[0], info->mac[1], info->mac[2],
               info->mac[3], info->mac[4], info->mac[5]);
        printf("| RSSI: %d | Data Len: %d\n", 
               info->rx_ctrl.rssi, info->len);
    }

    if (memcmp(info->mac, TX_MAC, 6) == 0) {
        printf("🎯 [MATCH] RSSI: %3d | Len: %d | Type: %s\n", 
               info->rx_ctrl.rssi, 
               info->len,
               info->rx_ctrl.sig_mode == 0 ? "Non-HT" : "HT");
    }

}

void csi_init()
{
    // 1. 기존 설정 초기화
    esp_wifi_set_csi(false);
    esp_wifi_set_promiscuous(false);
    // 절전모드 해제 
    esp_wifi_set_ps(WIFI_PS_NONE);

    // 5. 필터 설정 (Data 패킷 수신)
    wifi_promiscuous_filter_t filter = {
        .filter_mask = WIFI_PROMIS_FILTER_MASK_ALL
    };
    esp_wifi_set_promiscuous_filter(&filter);

    // 6. 채널 고정 및 무차별 모드 활성화
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);


    // 2. CSI 기본 설정
    wifi_csi_config_t csi_config = {
        .lltf_en = true,
        .htltf_en = true,
        .stbc_htltf2_en = true,
        .ltf_merge_en = true,
        .channel_filter_en = false,
        .manu_scale = false,
        .shift = false,
    };
    ESP_ERROR_CHECK(esp_wifi_set_csi_config(&csi_config));
    

    // 3. 콜백 등록
    ESP_ERROR_CHECK(esp_wifi_set_csi_rx_cb(wifi_csi_cb, NULL));    


    //CSI 활성화 
    ESP_ERROR_CHECK(esp_wifi_set_csi(true));


    printf("🚀 [CSI] Monitoring Started on Channel 1 (MAC Filter: %02x:%02x...)\n", TX_MAC[0], TX_MAC[1]);
}

void app_main(void)
{
    // NVS 초기화
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wi-Fi가 완전히 시작될 시간을 줍니다.
    vTaskDelay(pdMS_TO_TICKS(3500));
    
    csi_init();
}