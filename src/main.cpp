#include "../external/json.hpp"
#include <cstdlib>
#include <iostream>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
using json = nlohmann::json;

int main() {
  const char *socket_path = std::getenv("NIRI_SOCKET");
  if (!socket_path) {
    std::cerr << "[ERROR] NIRI_SOCKET not found." << std::endl;
    return 1;
  }

  // Socket init
  int sock = socket(AF_UNIX, SOCK_STREAM, 0);
  if (sock == -1) {
    perror("socket");
    return 1;
  }

  // Address setup
  struct sockaddr_un addr;
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

  // Socket connect
  if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    perror("connect");
    return 1;
  }
  std::cout << "[SUCCESS] Connected to Niri IPC!" << std::endl;

  // Niri Handshake
  std::string subscribe_msg = "\"EventStream\"\n";

  if (write(sock, subscribe_msg.c_str(), subscribe_msg.length()) == -1) {
    perror("write");
    return 1;
  }
  std::cout << "[SUCCESS] EventStream requested!" << std::endl;

  // Event loop
  std::cout << "[LISTENING] Waiting for Niri events... (Try switching windows!)"
            << std::endl;

  char buffer[8192];

  while (true) {
    ssize_t bytes_read = read(sock, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';
      // Convert the buffer to a string and split it into lines
      std::string raw_data(buffer);
      std::istringstream stream(raw_data);
      std::string line;

      while (std::getline(stream, line)) {
        if (line.empty())
          continue;

        try {
          // Parse the string into a JSON object (I am an idiot when it comes to
          // parsing stuff)
          json event = json::parse(line);
          if (event.contains("WindowOpenedOrChanged")) {
            auto window = event["WindowOpenedOrChanged"]["window"];
            std::string app =
                window["app_id"].is_string() ? window["app_id"] : "Unknown App";
            std::string title =
                window["title"].is_string() ? window["title"] : "No Title";

            std::cout << "[WINDOW UPDATE] " << app << " | " << title
                      << std::endl;
          }
          if (event.contains("WindowFocusChanged")) {
            int id = event["WindowFocusChanged"]["id"];
            std::cout << ">>> FOCUS SHIFTED TO WINDOW ID: " << id << " <<<"
                      << std::endl;
          }

        } catch (json::parse_error &e) {
        }
      }
    } else if (bytes_read == 0) {
      std::cout << "[DISCONNECTED] Compositor closed the connection."
                << std::endl;
      break;
    } else {
      perror("read");
      break;
    }
  }

  close(sock);
  return 0;
}
