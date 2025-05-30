#include "server.h"

#define PORT 3999
#define KEEPALIVE_IDLE 5
#define KEEPALIVE_INTERVAL 5
#define KEEPALIVE_COUNT 3
#define BUFFER_SIZE 512

static const char *LOGGING_TAG = "tcp_server";

static int client_sock = -1;
static int listen_sock = -1;
static bool server_is_up = false;

// Function to read data from the client socket
static void socket_read_loop(const int sock, const char *client_ip) {

  // call on_peer_connected()
  uint8_t rx_buffer[BUFFER_SIZE];
  client_sock = sock;

  while (1) {
    int len = recv(sock, rx_buffer, sizeof(rx_buffer), 0);
    if (len < 0) {
      ESP_LOGE(LOGGING_TAG, "Receive error from %s: errno %d", client_ip, errno);
      break;
    } else if (len == 0) {
      ESP_LOGW(LOGGING_TAG, "Client %s disconnected gracefully", client_ip);
      break;
    } else {
      ESP_LOGI(LOGGING_TAG, "Received %d bytes from %s", len, client_ip);
      // call on_peer_message(rx_buffer, len);
    }
  }

  client_sock = -1;
  // call on_peer_lost()
}

// Function to handle the server task
static void tcp_server_task(void *pvParameters) {
  char addr_str[128];
  int addr_family = AF_INET;
  int ip_protocol = IPPROTO_IP;
  struct sockaddr_in dest_addr;

  dest_addr.sin_addr.s_addr = htonl(INADDR_ANY);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(PORT);

  // Create the listening socket
  listen_sock = socket(addr_family, SOCK_STREAM, ip_protocol);
  if (listen_sock < 0) {
    ESP_LOGE(LOGGING_TAG, "Unable to create socket: errno %d", errno);
    vTaskDelete(NULL);
    return;
  }

  int opt = 1;
  setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

  ESP_LOGI(LOGGING_TAG, "Socket created");

  int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
  if (err != 0) {
    ESP_LOGE(LOGGING_TAG, "Socket unable to bind: errno %d", errno);
    goto CLEAN_UP;
  }

  ESP_LOGI(LOGGING_TAG, "Socket bound, port %d", PORT);

  err = listen(listen_sock, 1);
  if (err != 0) {
    ESP_LOGE(LOGGING_TAG, "Error occurred during listen: errno %d", errno);
    goto CLEAN_UP;
  }

  while (server_is_up) {  // Check server_is_up flag to determine server state
    ESP_LOGI(LOGGING_TAG, "Waiting for a new connection...");

    struct sockaddr_storage source_addr;
    socklen_t addr_len = sizeof(source_addr);
    int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
    if (sock < 0) {
      ESP_LOGE(LOGGING_TAG, "Unable to accept connection: errno %d", errno);
      continue;
    }

    // Get client IP
    if (source_addr.ss_family == AF_INET) {
      inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
    }

    ESP_LOGI(LOGGING_TAG, "Accepted connection from %s", addr_str);

    // Enable TCP Keep-Alive
    int keepAlive = 1;
    int keepIdle = KEEPALIVE_IDLE;
    int keepInterval = KEEPALIVE_INTERVAL;
    int keepCount = KEEPALIVE_COUNT;
    setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepAlive, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPIDLE, &keepIdle, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPINTVL, &keepInterval, sizeof(int));
    setsockopt(sock, IPPROTO_TCP, TCP_KEEPCNT, &keepCount, sizeof(int));

    // Handle incoming data
    socket_read_loop(sock, addr_str);

    // Cleanup once connection has been closed
    ESP_LOGI(LOGGING_TAG, "Closing connection from %s", addr_str);
    shutdown(sock, 0);
    close(sock);
  }
  
  CLEAN_UP:
  if(listen_sock >= 0){
    shutdown(listen_sock, SHUT_RDWR);
    close(listen_sock);
  }

  ESP_LOGW(LOGGING_TAG, "Server task is shutting down...");
  vTaskDelete(NULL);
}

// Function to open the server and create the task
void server_create() {
  if (!server_is_up) {  // Prevent starting the server if it's already running
    server_is_up = true;
    xTaskCreate(tcp_server_task, "tcp_server", 4096, NULL, 5, NULL);
    ESP_LOGI(LOGGING_TAG, "Server started");
  } else {
    ESP_LOGW(LOGGING_TAG, "Server is already running");
  }
}

void server_close() {
  if (server_is_up) {
    server_is_up = false;

    if(listen_sock >= 0){
      shutdown(listen_sock, SHUT_RDWR);
      close(listen_sock);
    }

  } else {
    ESP_LOGW(LOGGING_TAG, "Server is not running, cannot close it.");
  }
}

bool server_send_message(const uint8_t *msg, uint16_t len) {

  if (client_sock < 0) {
    ESP_LOGW(LOGGING_TAG, "No valid client connected, cannot send message");
    return false;
  }

  if (!msg || len == 0) {
    ESP_LOGE(LOGGING_TAG, "Invalid message: msg is NULL or length is 0");
    return false;
  }

  int sent = send(client_sock, msg, len, 0);
  if (sent == len) {
    ESP_LOGI(LOGGING_TAG, "Sent %d bytes to client", sent);
    return true;
  } else {
    ESP_LOGE(LOGGING_TAG, "Failed to send message: errno %d", errno);
    return false;
  }

}
