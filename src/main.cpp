#include <iostream>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <string>

int main() {
    const char* socket_path = std::getenv("NIRI_SOCKET");
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
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
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
    std::cout << "[LISTENING] Waiting for Niri events... (Try switching windows!)" << std::endl;

    char buffer[4096];
    while (true) {
        ssize_t bytes_read = read(sock, buffer, sizeof(buffer) - 1);
        if (bytes_read > 0) {
            buffer[bytes_read] = '\0';
            std::cout << "\n--- EVENT RECEIVED ---\n" << buffer << std::endl;
        } else if (bytes_read == 0) {
            std::cout << "[DISCONNECTED] Compositor closed the connection." << std::endl;
            break;
        } else {
            perror("read");
            break;
        }
    }

    close(sock);
    return 0;
}
