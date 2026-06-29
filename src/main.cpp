/*
 * Ghost Watch Daemon
 *
 * A simple, lightweight screen time tracker that parses JSON events from the
 * Niri IPC socket and stores usage stuff in an SQLite database.
 *
 * Author: Amritanshu Kumar <amritrespawned@gmail.com>
 */
#include "../external/json.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sqlite3.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
using json = nlohmann::json;

// database function
sqlite3 *init_database() {
  sqlite3 *db;
  if (sqlite3_open("screentime.db", &db)) {
    std::cerr << "[ERROR] Cannot open database: " << sqlite3_errmsg(db)
              << std::endl;
    return nullptr;
  }

  const char *sql = "CREATE TABLE IF NOT EXISTS window_usage ("
                    "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                    "app_id TEXT, "
                    "window_title TEXT, "
                    "duration_seconds INTEGER, "
                    "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);";

  char *err_msg = 0;
  if (sqlite3_exec(db, sql, 0, 0, &err_msg) != SQLITE_OK) {
    std::cerr << "[ERROR] SQL error: " << err_msg << std::endl;
    sqlite3_free(err_msg);
  } else {
    std::cout << "[SUCCESS] SQLite Database initialized (screentime.db)!"
              << std::endl;
  }
  return db;
}

int main() {
  // Init database
  sqlite3 *db = init_database();
  if (!db)
    return 1;

  // Connect to niri socket
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

  // State trackers
  int current_focused_id = -1;
  auto focus_start_time = std::chrono::steady_clock::now();
  std::map<int, std::pair<std::string, std::string>> window_directory;

  std::cout << "[LISTENING] Tracking screen time directly to database..."
            << std::endl;
  char buffer[8192];

  while (true) {
    ssize_t bytes_read = read(sock, buffer, sizeof(buffer) - 1);
    if (bytes_read > 0) {
      buffer[bytes_read] = '\0';
      std::string raw_data(buffer);
      std::istringstream stream(raw_data);
      std::string line;

      while (std::getline(stream, line)) {
        if (line.empty())
          continue;

        try {
          json event = json::parse(line);

          if (event.contains("WindowsChanged")) {
            for (auto &window : event["WindowsChanged"]["windows"]) {
              int win_id = window["id"];
              std::string app = window["app_id"].is_string() ? window["app_id"]
                                                             : "Unknown App";
              std::string title =
                  window["title"].is_string() ? window["title"] : "No Title";
              window_directory[win_id] = {app, title};
            }
          }

          // store new window details when they open
          if (event.contains("WindowOpenedOrChanged")) {
            auto window = event["WindowOpenedOrChanged"]["window"];
            int win_id = window["id"];
            std::string app =
                window["app_id"].is_string() ? window["app_id"] : "Unknown App";
            std::string title =
                window["title"].is_string() ? window["title"] : "No Title";

            window_directory[win_id] = {app, title};
          }

          if (event.contains("WindowFocusChanged")) {
            auto focus_data = event["WindowFocusChanged"];
            auto now = std::chrono::steady_clock::now();
            int duration = std::chrono::duration_cast<std::chrono::seconds>(
                               now - focus_start_time)
                               .count();

            int new_id =
                focus_data["id"].is_number() ? (int)focus_data["id"] : -1;

            if (current_focused_id != -1 && current_focused_id != new_id &&
                duration > 0) {
              std::string app = "Unknown", title = "Unknown";

              if (window_directory.count(current_focused_id)) {
                app = window_directory[current_focused_id].first;
                title = window_directory[current_focused_id].second;
              }

              std::cout << "[LOGGED] " << app << " for " << duration << "s"
                        << std::endl;

              // save to sqlite
              std::string sql =
                  "INSERT INTO window_usage (app_id, window_title, "
                  "duration_seconds) VALUES (?, ?, ?);";
              sqlite3_stmt *stmt;
              sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);

              sqlite3_bind_text(stmt, 1, app.c_str(), -1, SQLITE_TRANSIENT);
              sqlite3_bind_text(stmt, 2, title.c_str(), -1, SQLITE_TRANSIENT);
              sqlite3_bind_int(stmt, 3, duration);

              sqlite3_step(stmt);
              sqlite3_finalize(stmt);
            }

            // update state
            current_focused_id = new_id;
            focus_start_time = now;
          }

        } catch (json::parse_error &e) {
          // ignore broken json
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
  sqlite3_close(db);
  return 0;
}
