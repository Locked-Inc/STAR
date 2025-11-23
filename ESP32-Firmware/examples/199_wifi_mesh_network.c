/**
 * @file 199_wifi_mesh_network.c
 * @brief Self-healing WiFi mesh network with adaptive routing
 *
 * Demonstrates:
 * - ESP-MESH self-organizing network
 * - Shannon-Hartley inspired link quality estimation
 * - Automatic route healing and optimization
 * - Multi-hop data transmission
 *
 * Run same code on multiple ESP32s - they auto-discover and form mesh
 */

#include "star_sensor_dht22.h"

#include <esp_log.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_mesh.h>
#include <esp_mesh_internal.h>
#include <nvs_flash.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <string.h>
#include <math.h>

static const char *s_tag = "WIFI_MESH";

/* Mesh configuration */
#define MESH_ID             {0x77, 0x77, 0x77, 0x77, 0x77, 0x77}
#define MESH_PASSWORD       "mesh_password"
#define MESH_MAX_LAYER (6)
#define MESH_CHANNEL (0) /* Auto select */

/* Shannon-Hartley inspired link quality metrics */
typedef struct {
  int8_t rssi;              /* Signal strength */
  float snr;                /* Signal-to-noise ratio estimate */
  float packet_loss;        /* Packet loss rate 0-1 */
  float link_capacity;      /* Estimated capacity (normalized) */
  uint32_t latency_ms;      /* Round-trip latency */
} link_quality_t;

typedef struct {
  mesh_addr_t addr;
  link_quality_t quality;
  uint32_t last_seen;
} neighbor_info_t;

#define MAX_NEIGHBORS (10)
static neighbor_info_t s_neighbors[MAX_NEIGHBORS];
static int s_neighbor_count = 0;
static bool s_is_root = false;
static int s_mesh_layer = -1;

/**
 * @brief Estimate link capacity using Shannon-Hartley inspired formula
 *
 * C = B * log2(1 + SNR)
 *
 * We estimate SNR from RSSI and noise floor, then normalize to 0-1 range
 */
static float estimate_link_capacity(int8_t rssi, float packet_loss)
{
  /* Estimate SNR from RSSI (noise floor around -95 dBm) */
  float noise_floor = -95.0f;
  float snr_db = rssi - noise_floor;
  if (snr_db < 0) snr_db = 0;

  /* SNR linear ratio */
  float snr_linear = powf(10.0f, snr_db / 10.0f);

  /* Shannon capacity (normalized, assuming 20MHz bandwidth) */
  float capacity = log2f(1.0f + snr_linear) / 10.0f;  /* Normalize to ~0-1 */
  if (capacity > 1.0f) capacity = 1.0f;

  /* Adjust for packet loss */
  capacity *= (1.0f - packet_loss);

  return capacity;
}

/**
 * @brief Update neighbor link quality
 */
static void update_neighbor_quality(const mesh_addr_t* addr, int8_t rssi, bool success)
{
  neighbor_info_t* neighbor = NULL;

  /* Find or create neighbor entry */
  for (int i = 0; i < s_neighbor_count; i++) {
    if (memcmp(&s_neighbors[i].addr, addr, sizeof(mesh_addr_t)) == 0) {
      neighbor = &s_neighbors[i];
      break;
    }
  }

  if (!neighbor && s_neighbor_count < MAX_NEIGHBORS) {
    neighbor = &s_neighbors[s_neighbor_count++];
    memcpy(&neighbor->addr, addr, sizeof(mesh_addr_t));
    neighbor->quality.packet_loss = 0.0f;
  }

  if (neighbor) {
    /* Exponential moving average for RSSI */
    neighbor->quality.rssi = (neighbor->quality.rssi * 7 + rssi) / 8;

    /* Update packet loss estimate */
    float alpha = 0.1f;
    neighbor->quality.packet_loss = neighbor->quality.packet_loss * (1 - alpha) +
                                    (success ? 0.0f : 1.0f) * alpha;

    /* Recalculate link capacity */
    neighbor->quality.link_capacity = estimate_link_capacity(
      neighbor->quality.rssi, neighbor->quality.packet_loss);

    neighbor->last_seen = xTaskGetTickCount();

    ESP_LOGD(s_tag, "Neighbor %02x:%02x:%02x:%02x:%02x:%02x RSSI:%d Loss:%.2f Capacity:%.2f",
             addr->addr[0], addr->addr[1], addr->addr[2],
             addr->addr[3], addr->addr[4], addr->addr[5],
             neighbor->quality.rssi,
             neighbor->quality.packet_loss, neighbor->quality.link_capacity);
  }
}

/**
 * @brief Select best parent based on link quality
 */
static neighbor_info_t* find_best_parent(void)
{
  neighbor_info_t* best = NULL;
  float best_score = 0;

  for (int i = 0; i < s_neighbor_count; i++) {
    /* Score = capacity * recency_factor */
    uint32_t age = xTaskGetTickCount() - s_neighbors[i].last_seen;
    float recency = 1.0f - (age / (float)(10000));  /* Decay over 10s */
    if (recency < 0) recency = 0;

    float score = s_neighbors[i].quality.link_capacity * recency;

    if (score > best_score) {
      best_score = score;
      best = &s_neighbors[i];
    }
  }

  return best;
}

/**
 * @brief Mesh event handler
 */
static void mesh_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
  switch (event_id) {
    case MESH_EVENT_STARTED:
      ESP_LOGI(s_tag, "Mesh started");
      break;

    case MESH_EVENT_STOPPED:
      ESP_LOGI(s_tag, "Mesh stopped");
      break;

    case MESH_EVENT_PARENT_CONNECTED: {
      mesh_event_connected_t* event = (mesh_event_connected_t*)event_data;
      s_mesh_layer = event->self_layer;
      ESP_LOGI(s_tag, "Connected to parent, layer: %d", s_mesh_layer);

      /* Update parent quality */
      wifi_ap_record_t ap_info;
      esp_wifi_sta_get_ap_info(&ap_info);
      mesh_addr_t parent_addr;
      esp_mesh_get_parent_bssid(&parent_addr);
      update_neighbor_quality(&parent_addr, ap_info.rssi, true);
      break;
    }

    case MESH_EVENT_PARENT_DISCONNECTED: {
      ESP_LOGW(s_tag, "Parent disconnected - initiating self-healing");
      s_mesh_layer = -1;

      /* Mark current parent as having packet loss */
      mesh_addr_t parent_addr;
      if (esp_mesh_get_parent_bssid(&parent_addr) == ESP_OK) {
        update_neighbor_quality(&parent_addr, -90, false);
      }
      break;
    }

    case MESH_EVENT_CHILD_CONNECTED: {
      mesh_event_child_connected_t* event = (mesh_event_child_connected_t*)event_data;
      ESP_LOGI(s_tag, "Child connected: %02x:%02x:%02x:%02x:%02x:%02x",
               event->mac[0], event->mac[1], event->mac[2],
               event->mac[3], event->mac[4], event->mac[5]);
      break;
    }

    case MESH_EVENT_CHILD_DISCONNECTED: {
      mesh_event_child_disconnected_t* event = (mesh_event_child_disconnected_t*)event_data;
      ESP_LOGI(s_tag, "Child disconnected: %02x:%02x:%02x:%02x:%02x:%02x",
               event->mac[0], event->mac[1], event->mac[2],
               event->mac[3], event->mac[4], event->mac[5]);
      break;
    }

    case MESH_EVENT_ROOT_FIXED: {
      s_is_root = true;
      s_mesh_layer = 0;
      ESP_LOGI(s_tag, "Root node - fixed as root");
      break;
    }

    case MESH_EVENT_NETWORK_STATE: {
      mesh_event_network_state_t* event = (mesh_event_network_state_t*)event_data;
      ESP_LOGI(s_tag, "Network state: %s", event->is_rootless ? "rootless" : "has root");
      break;
    }

    default:
      break;
  }
}

/**
 * @brief IP event handler for root node
 */
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
  if (event_id == IP_EVENT_STA_GOT_IP) {
    ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
    ESP_LOGI(s_tag, "Root got IP: " IPSTR, IP2STR(&event->ip_info.ip));
  }
}

/**
 * @brief Sensor data packet for mesh transmission
 */
typedef struct {
  uint8_t node_mac[6];
  int8_t layer;
  float temperature;
  float humidity;
  float link_quality;
  uint32_t timestamp;
} sensor_mesh_packet_t;

/**
 * @brief Data receive task
 */
static void mesh_rx_task(void *arg)
{
  mesh_addr_t from;
  mesh_data_t data;
  uint8_t buf[256];
  int flag;

  data.data = buf;
  data.size = sizeof(buf);

  while (1) {
    esp_err_t err = esp_mesh_recv(&from, &data, portMAX_DELAY, &flag, NULL, 0);
    if (err == ESP_OK && data.size == sizeof(sensor_mesh_packet_t)) {
      sensor_mesh_packet_t* pkt = (sensor_mesh_packet_t*)data.data;

      ESP_LOGI(s_tag, "RX from %02x:%02x:%02x:%02x:%02x:%02x (L%d): Temp=%.1f, Hum=%.1f, LQ=%.2f",
               pkt->node_mac[0], pkt->node_mac[1], pkt->node_mac[2],
               pkt->node_mac[3], pkt->node_mac[4], pkt->node_mac[5],
               pkt->layer, pkt->temperature, pkt->humidity, pkt->link_quality);

      /* Update neighbor info */
      update_neighbor_quality(&from, -60, true);  /* Assume good if received */

      /* If root, could forward to cloud here */
      if (s_is_root) {
        ESP_LOGI(s_tag, "Root received sensor data - could forward to cloud");
      }
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

/**
 * @brief Sensor transmit task
 */
static void mesh_tx_task(void *arg)
{
  dht22_handle_t* dht = (dht22_handle_t*)arg;
  uint8_t mac[6];
  esp_wifi_get_mac(WIFI_IF_STA, mac);

  while (1) {
    if (esp_mesh_is_root() || s_mesh_layer > 0) {
      dht22_data_t sensor_data;
      star_sensor_dht22_read(dht, &sensor_data);

      /* Find best neighbor for quality reporting */
      neighbor_info_t* best = find_best_parent();

      sensor_mesh_packet_t pkt = {
        .layer = s_mesh_layer,
        .temperature = sensor_data.temperature_c,
        .humidity = sensor_data.humidity_rh,
        .link_quality = best ? best->quality.link_capacity : 0,
        .timestamp = xTaskGetTickCount()
      };
      memcpy(pkt.node_mac, mac, 6);

      mesh_data_t data = {
        .data = (uint8_t*)&pkt,
        .size = sizeof(pkt),
        .proto = MESH_PROTO_BIN,
        .tos = MESH_TOS_P2P
      };

      /* Send to root */
      mesh_addr_t root_addr;
      esp_mesh_get_routing_table(&root_addr, 1, NULL);

      esp_err_t err = esp_mesh_send(&root_addr, &data, MESH_DATA_P2P, NULL, 0);
      if (err == ESP_OK) {
        ESP_LOGI(s_tag, "TX: Temp=%.1f, Hum=%.1f, Layer=%d",
                 sensor_data.temperature_c, sensor_data.humidity_rh, s_mesh_layer);
      } else {
        ESP_LOGW(s_tag, "TX failed: %s", esp_err_to_name(err));
        /* Update quality on failure */
        if (best) {
          update_neighbor_quality(&best->addr, best->quality.rssi, false);
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(5000));
  }
}

/**
 * @brief Network health monitor task
 */
static void health_monitor_task(void *arg)
{
  while (1) {
    int total_nodes = esp_mesh_get_total_node_num();

    float avg_quality = 0;
    for (int i = 0; i < s_neighbor_count; i++) {
      avg_quality += s_neighbors[i].quality.link_capacity;
    }
    if (s_neighbor_count > 0) avg_quality /= s_neighbor_count;

    ESP_LOGI(s_tag, "=== Mesh Health ===");
    ESP_LOGI(s_tag, "Layer: %d, Is Root: %s", s_mesh_layer, s_is_root ? "YES" : "NO");
    ESP_LOGI(s_tag, "Total Nodes: %d, Neighbors: %d", total_nodes, s_neighbor_count);
    ESP_LOGI(s_tag, "Avg Link Quality: %.2f", avg_quality);

    /* List neighbors */
    for (int i = 0; i < s_neighbor_count; i++) {
      ESP_LOGI(s_tag, "  %02x:%02x:%02x:%02x:%02x:%02x RSSI:%d Capacity:%.2f",
               s_neighbors[i].addr.addr[0], s_neighbors[i].addr.addr[1],
               s_neighbors[i].addr.addr[2], s_neighbors[i].addr.addr[3],
               s_neighbors[i].addr.addr[4], s_neighbors[i].addr.addr[5],
               s_neighbors[i].quality.rssi,
               s_neighbors[i].quality.link_capacity);
    }

    vTaskDelay(pdMS_TO_TICKS(10000));
  }
}

void wifi_mesh_example(void)
{
  /* Initialize NVS */
  nvs_flash_init();

  /* Initialize networking */
  esp_netif_init();
  esp_event_loop_create_default();
  esp_netif_create_default_wifi_mesh_netifs(NULL, NULL);

  /* Initialize WiFi */
  wifi_init_config_t wifi_cfg = WIFI_INIT_CONFIG_DEFAULT();
  esp_wifi_init(&wifi_cfg);
  esp_wifi_set_storage(WIFI_STORAGE_FLASH);
  esp_wifi_start();

  /* Initialize ESP-MESH */
  esp_mesh_init();

  /* Register event handlers */
  esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL);
  esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL);

  /* Configure mesh */
  mesh_cfg_t mesh_cfg = MESH_INIT_CONFIG_DEFAULT();

  /* Mesh ID - same on all nodes */
  uint8_t mesh_id[6] = MESH_ID;
  memcpy(&mesh_cfg.mesh_id, mesh_id, 6);

  /* Mesh password */
  mesh_cfg.channel = MESH_CHANNEL;
  mesh_cfg.router.ssid_len = strlen(MESH_PASSWORD);
  memcpy(&mesh_cfg.router.password, MESH_PASSWORD, strlen(MESH_PASSWORD));

  /* Mesh AP settings */
  mesh_cfg.mesh_ap.max_connection = 6;
  memcpy(&mesh_cfg.mesh_ap.password, MESH_PASSWORD, strlen(MESH_PASSWORD));

  esp_mesh_set_config(&mesh_cfg);

  /* Set max layer */
  esp_mesh_set_max_layer(MESH_MAX_LAYER);

  /* Enable self-organized networking */
  esp_mesh_set_self_organized(true, true);

  /* Start mesh */
  esp_mesh_start();

  ESP_LOGI(s_tag, "WiFi Mesh started - waiting for network formation...");

  /* Initialize sensor */
  static dht22_handle_t s_dht;
  dht22_config_t dht_cfg = {
    .data_pin = GPIO_NUM_4,
    .enable_pullup = false
  };
  star_sensor_dht22_init(&s_dht, &dht_cfg);

  /* Start mesh tasks */
  xTaskCreate(mesh_rx_task, "mesh_rx", 4096, NULL, 5, NULL);
  xTaskCreate(mesh_tx_task, "mesh_tx", 4096, &s_dht, 4, NULL);
  xTaskCreate(health_monitor_task, "health", 4096, NULL, 3, NULL);

  ESP_LOGI(s_tag, "Mesh sensor node running");
}

void app_main(void) { wifi_mesh_example(); }
