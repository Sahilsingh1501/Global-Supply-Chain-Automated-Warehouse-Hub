#ifndef LOCAL_WEB_H
#define LOCAL_WEB_H

#include <winsock2.h>
#include <ws2tcpip.h>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

inline bool sendAll(SOCKET socket, const std::string& data) {
    int sent = 0;
    int total = static_cast<int>(data.size());
    while (sent < total) {
        int chunk = send(socket, data.data() + sent, total - sent, 0);
        if (chunk <= 0) return false;
        sent += chunk;
    }
    return true;
}

inline void respond(SOCKET socket, int status, const std::string& reason,
                    const std::string& type, const std::string& body) {
    std::ostringstream out;
    out << "HTTP/1.1 " << status << " " << reason << "\r\n"
        << "Content-Type: " << type << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Cache-Control: no-store\r\n"
        << "Connection: close\r\n\r\n"
        << body;
    sendAll(socket, out.str());
}

inline std::string readFile(const std::string& path, bool& found) {
    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
        found = false;
        return "";
    }
    found = true;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

inline bool readRequest(SOCKET socket, std::string& method, std::string& path, std::string& body) {
    std::string data;
    char buffer[4096];
    while (data.find("\r\n\r\n") == std::string::npos) {
        int count = recv(socket, buffer, sizeof(buffer), 0);
        if (count <= 0) return false;
        data.append(buffer, buffer + count);
        if (data.size() > 1024 * 1024) return false;
    }
    size_t headerEnd = data.find("\r\n\r\n");
    std::string headers = data.substr(0, headerEnd);
    body = data.substr(headerEnd + 4);
    size_t lineEnd = headers.find("\r\n");
    std::string requestLine = lineEnd == std::string::npos ? headers : headers.substr(0, lineEnd);
    std::istringstream line(requestLine);
    std::string version;
    if (!(line >> method >> path >> version)) return false;
    size_t query = path.find('?');
    if (query != std::string::npos) path = path.substr(0, query);

    size_t contentLength = 0;
    size_t marker = headers.find("Content-Length:");
    if (marker == std::string::npos) marker = headers.find("content-length:");
    if (marker != std::string::npos) {
        contentLength = static_cast<size_t>(std::atoi(headers.c_str() + marker + 15));
    }
    if (contentLength > 1024 * 1024) return false;
    while (body.size() < contentLength) {
        int count = recv(socket, buffer, sizeof(buffer), 0);
        if (count <= 0) return false;
        body.append(buffer, buffer + count);
    }
    if (body.size() > contentLength) body.resize(contentLength);
    return true;
}

template <typename Handler>
int serveLocalWeb(Handler handler) {
    WSADATA data;
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "Could not start the local web server.\n";
        return 1;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        std::cerr << "Could not open a local socket.\n";
        WSACleanup();
        return 1;
    }

    int port = 8080;
    bool bound = false;
    for (int candidate = 8080; candidate <= 8084 && !bound; ++candidate) {
        sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        address.sin_port = htons(static_cast<unsigned short>(candidate));
        if (bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
            port = candidate;
            bound = true;
        }
    }
    if (!bound || listen(listener, 8) != 0) {
        std::cerr << "Ports 8080-8084 are already in use.\n";
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    std::string url = "http://127.0.0.1:" + std::to_string(port) + "/";
    std::cout << "Web interface: " << url << "\n"
              << "Leave this window open. Press Ctrl+C to stop.\n";
    std::string command = "start \"\" \"" + url + "\"";
    std::system(command.c_str());

    for (;;) {
        SOCKET client = accept(listener, 0, 0);
        if (client == INVALID_SOCKET) continue;
        std::string method, path, body;
        if (!readRequest(client, method, path, body)) {
            closesocket(client);
            continue;
        }
        try {
            if (path == "/favicon.ico") {
                respond(client, 204, "No Content", "text/plain", "");
            } else if (path.size() >= 4 && path.compare(0, 4, "/api") == 0) {
                WebResult result = handler(method, path, body);
                std::string reason = result.status == 200 ? "OK" :
                                     result.status == 400 ? "Bad Request" : "Not Found";
                respond(client, result.status, reason, "application/json; charset=utf-8", result.body);
            } else if (path == "/" || path == "/index.html" || path == "/styles.css" || path == "/app.js") {
                std::string file = path == "/" || path == "/index.html" ? "web/index.html" :
                                   path == "/styles.css" ? "web/styles.css" : "web/app.js";
                std::string type = path == "/styles.css" ? "text/css; charset=utf-8" :
                                   path == "/app.js" ? "text/javascript; charset=utf-8" :
                                   "text/html; charset=utf-8";
                bool found = false;
                std::string contents = readFile(file, found);
                if (!found) respond(client, 404, "Not Found", "text/plain; charset=utf-8", "Missing page file.");
                else respond(client, 200, "OK", type, contents);
            } else {
                respond(client, 404, "Not Found", "text/plain; charset=utf-8", "Not found.");
            }
        } catch (const std::exception& error) {
            respond(client, 500, "Error", "application/json; charset=utf-8",
                    std::string("{\"ok\":false,\"message\":\"") + jsonEscape(error.what()) + "\"}");
        }
        closesocket(client);
    }
}

#endif
