/*
 * MIT License
 *
 * Copyright (c) 2020 Aaron C. Smith
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "TwitchConn.hpp"
#include <thread>
#include <chrono>
#include <stdio.h>

static const char * const TWITCH_IRC_ADDR = "irc.chat.twitch.tv";
static const char * const TWITCH_IRC_PORT = "6667";

TwitchConn::TwitchConn() {

}

TwitchConnStatus TwitchConn::Init(const AuthData &auth_data) {
    struct addrinfo hints = { };
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    int rc = getaddrinfo(TWITCH_IRC_ADDR, TWITCH_IRC_PORT, &hints, 
        &hint_results);
    if (rc != 0) {
        printf("ERROR: Failed to get Twitch address info: %d\n",
            WSAGetLastError());
        cstatus = TWC_ERROR;
        return TWC_ERROR;
    }

    sock = INVALID_SOCKET;
    cstatus = TWC_NOT_CONNECTED;
    credentials = auth_data;

    memset(line_buffer, 0, LINE_BUFFER_SIZE);
    line_length = 0;
    return TWC_NOT_CONNECTED;
}

void TwitchConn::Update() {
    using namespace std;
    if (cstatus == TWC_ERROR) return;

    if (cstatus == TWC_NOT_CONNECTED) {
        Connect();
    }

    if (cstatus == TWC_CONNECTED) {
        Receive();
    }

    if (cstatus == TWC_CONNECTED) {
        Send();
    }
}

TwitchConnStatus TwitchConn::GetConnectionStatus() const {
    return cstatus;
}

int TwitchConn::GetNumRxMsgs() const {
    // Not thread safe
    return (int)rx_queue.size();
}

std::string TwitchConn::GetNextRxMsg() {
    // Not thread safe
    std::string msg = "";
    if (rx_queue.size() > 0) {
        msg = rx_queue.front();
        rx_queue.pop();
    }
    return msg;
}

void TwitchConn::SendMsg(const std::string &msg) {
    // Note thread safe
    tx_queue.push(msg);
}

void TwitchConn::Shutdown() {
    if (cstatus == TWC_CONNECTED) {
        closesocket(sock);
        sock = INVALID_SOCKET;
    }
}

void TwitchConn::Connect() {
    using namespace std;

    printf("Attempting twitch connection\n");
    addrinfo *addr_info = hint_results;
    bool connect_ok = false;
    while (addr_info != NULL && !connect_ok) {
        sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            printf("Failed to create socket\n");
            cstatus = TWC_ERROR;
            return;
        }

        int addrlen = (int)addr_info->ai_addrlen;
        int rc = connect(sock, addr_info->ai_addr, addrlen);
        addr_info = addr_info->ai_next;
        if (rc == SOCKET_ERROR) Close();
        else connect_ok = true;
    }
    
    if (!connect_ok) {
        printf("WARNING: Failed to connect to Twitch IRC Server\n");
        printf("\tSocket errno %d\n", WSAGetLastError());
        return;
    }
    printf("Connected to Twitch IRC server\n");

    sprintf_s(tx_buffer, TX_BUFFER_SIZE, "PASS %s\r\n", 
        credentials.oauth.c_str());
    int length = (int)strlen(tx_buffer);
    int rc = send(sock, tx_buffer, length, 0);
    if (rc != length) {
        printf("WARNING: Failed to send OAUTH to Twitch: %d\n", 
            WSAGetLastError());
        Close();
        return;
    }

    this_thread::sleep_for(chrono::milliseconds(100));

    sprintf_s(tx_buffer, TX_BUFFER_SIZE, "NICK %s\r\n", 
        credentials.nick.c_str());
    length = (int)strlen(tx_buffer);
    rc = send(sock, tx_buffer, length, 0);
    if (rc != length) {
        printf("WARNING: Failed to send NICK to Twitch: %d\n", 
            WSAGetLastError());
        Close();
        return;
    }
    printf("Successfully sent credentials to Twitch\n");

    this_thread::sleep_for(chrono::milliseconds(100));

    sprintf_s(tx_buffer, "CAP REQ :twitch.tv/commands\r\n");
    length = (int)strlen(tx_buffer);
    rc = send(sock, tx_buffer, length, 0);
    if (rc != length) {
        printf("WARNING: Failed to send CAP REQ to Twitch: %d\n",
            WSAGetLastError());
        Close();
        return;
    }

    this_thread::sleep_for(chrono::milliseconds(100));

    sprintf_s(tx_buffer, TX_BUFFER_SIZE, "JOIN #%s\r\n", 
        credentials.channel.c_str());
    length = (int)strlen(tx_buffer);
    rc = send(sock, tx_buffer, length, 0);
    if (rc != length) {
        printf("WARNING: Failed to send JOIN to Twitch: %d\n", 
            WSAGetLastError());
        Close();
        return;
    }
    cstatus = TWC_CONNECTED;
    line_length = 0;
    while (rx_queue.size() > 0) rx_queue.pop();
    while (tx_queue.size() > 0) tx_queue.pop();
}

void TwitchConn::Close() {
    closesocket(sock);
    sock = INVALID_SOCKET;
    cstatus = TWC_NOT_CONNECTED;
}

void TwitchConn::Receive() {
    using namespace std;

    FD_SET rx_set;
    FD_SET err_set;
    FD_ZERO(&rx_set);
    FD_ZERO(&err_set);
    FD_SET(sock, &rx_set);
    FD_SET(sock, &err_set);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;
    int rc = select((int)sock + 1, &rx_set, NULL, &err_set, &tv);
    if (rc == SOCKET_ERROR) {
        printf("WARNING: Socket select error %d\n", WSAGetLastError());
        Close();
        return;
    }
    if (rc != 0 && FD_ISSET(sock, &err_set)) {
        printf("WARNING: Connection failure with Twitch IRC server\n");
        Close();
        return;
    }
    if (rc != 0 && FD_ISSET(sock, &rx_set)) {
        rc = recv(sock, rx_buffer, RX_BUFFER_SIZE, 0);
        if (rc == 0) {
            printf("Twitch disconnected socket...\n");
            Close();
            return;
        } else if (rc < 0) {
            printf("WARNING: Socket receive error : %d\n", WSAGetLastError());
            Close();
            return;
        }

        for (int i = 0; i < rc; i++) {
            if (rx_buffer[i] == '\r') {
                if (rx_buffer[i + 1] == '\n') {
                    if (line_length == 0) continue;
                    i++;
                    line_buffer[line_length] = 0;
                    string line = string(line_buffer);
                    rx_queue.push(line); // Not thread safe
                    line_length = 0;
                    printf("> %s\n", line.c_str());
                    continue;
                }
            }
            line_buffer[line_length] = rx_buffer[i];
            line_length++;
            
            if (line_length == LINE_BUFFER_SIZE) {
                printf("WARNING: Twitch violated line buffer length\n");
                printf("Reconnecting...\n");
                Close();
                break;
            }
        }
    }
}

void TwitchConn::Send() {
    using namespace std;
    if (tx_queue.size() < 1) return;

    string line = tx_queue.front();
    tx_queue.pop();

    if (line.size() >= (TX_BUFFER_SIZE - 3)) {
        printf("WARNING: Dropped msg that exceeded max length\n");
    } else {
        printf("< %s\n", line.c_str());
        sprintf_s(tx_buffer, TX_BUFFER_SIZE, "%s\r\n", line.c_str());
        int length = (int)strlen(tx_buffer);
        int bytes_sent = 0;
        while (bytes_sent < length) {
            int rc = send(sock, &tx_buffer[bytes_sent], length - bytes_sent, 
                0);
            if (rc == 0) {
                printf("Twitch disconnected socket...\n");
                Close();
                return;
            }
            if (rc < 0) {
                printf("WARNING: Failed to send msg to Twitch: %d\n", 
                    WSAGetLastError());
                Close();
                return;
            }
            bytes_sent += rc;
        }
    }
}

// Static initializers
const int TwitchConn::RX_BUFFER_SIZE;
const int TwitchConn::TX_BUFFER_SIZE;
const int TwitchConn::LINE_BUFFER_SIZE;
