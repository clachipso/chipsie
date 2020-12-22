
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

#include "jsmn.h"
#include <stdio.h>
#include <stdlib.h>
#include "TwitchConn.hpp"
#include "ChatProcessing.hpp"
#include "Database.hpp"
#include <thread>
#include <chrono>

const char * const DEF_AUTH_CFG_FILE = "auth.json";
const char * const DEF_DB_FILE = "chipsie.db"; 

static AuthData auth;
static TwitchConn tc;
static Database db;

// Loads the server authorization credentials from the auth file.
bool LoadAuthCfg(const char *auth_cfg_file, AuthData *auth_data);

// Main application entry point
int main(const int argc, const char **argv) {
    printf("Chipsie the Twitch Chat Bot Starting Up...\n");

    using namespace std;
    using namespace std::chrono;

#ifdef _WIN32
    // Initialize windows sockets
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
#endif // _WIN32

    if (!LoadAuthCfg(DEF_AUTH_CFG_FILE, &auth)) return -1;
    printf("Loaded credentials...\n");

    if (!db.Init(DEF_DB_FILE)) return -1;
    printf("Database Initialized...\n");

    if (tc.Init(auth) == TWC_ERROR) return -1;
    printf("Twitch connection initialized...\n");

    printf("Chipsie is now running :D\n\n");
    while (true) {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        tc.Update();
        if (tc.GetConnectionStatus() == TWC_ERROR) break;

        while (tc.GetNumRxMsgs() > 0) {
            ProcessChatLine(tc.GetNextRxMsg(), &tc, &db);
        }

        auto elapsed = high_resolution_clock::now() - start_time;
        auto ticks = duration_cast<microseconds>(elapsed).count();
        if (ticks > 33000) ticks = 33000;
        std::this_thread::sleep_for(microseconds(33000 - ticks));
    }

    tc.Shutdown();
    printf("Chipsie the Twitch Chat Bot Shutting Down...Bye Bye!\n");
    return 0;
}

bool LoadAuthCfg(const char *auth_cfg_file, AuthData *auth_data)
{
    FILE *auth_file = NULL;
    errno_t res = fopen_s(&auth_file, auth_cfg_file, "rb");
    if (res) {
        printf("Failed to open auth config file\n");
        return false;
    }

    fseek(auth_file, 0, SEEK_END);
    long file_len = ftell(auth_file);
    if (file_len <= 0) {
        printf("Invalid auth credential file length\n");
        return false;
    }
    rewind(auth_file);

    char *file_str = (char *)malloc(file_len);
    int rc = (int)fread(file_str, 1, file_len, auth_file);
    fclose(auth_file);

    jsmntok tokens[12];
    jsmn_parser parser;
    jsmn_init(&parser);
    rc = jsmn_parse(&parser, file_str, (size_t)file_len, tokens, 12);
    if (rc <= 0) {
        printf("Too few JSON tokens in auth file\n");
        free(file_str);
        return false;
    }

    bool got_oauth = false;
    bool got_client_id = false;
    bool got_nick = false;
    bool got_channel = false;
    int tok_it = 0;
    while (tok_it < rc) {
        std::string key = std::string(&file_str[tokens[tok_it].start], 
            tokens[tok_it].end - tokens[tok_it].start);

        if (key == "token") {
            tok_it++;
            auth_data->oauth = std::string(&file_str[tokens[tok_it].start], 
                tokens[tok_it].end - tokens[tok_it].start);
            if (auth_data->oauth.length() > 0) got_oauth = true;
        } else if (key == "client_id") {
            tok_it++;
            auth_data->client_id = std::string(&file_str[tokens[tok_it].start], 
                tokens[tok_it].end - tokens[tok_it].start);
            if (auth_data->client_id.length() > 0) got_client_id = true;
        } else if (key == "nick") {
            tok_it++;
            auth_data->nick = std::string(&file_str[tokens[tok_it].start], 
                tokens[tok_it].end - tokens[tok_it].start);
            if (auth_data->nick.length() > 0) got_nick = true;
        } else if (key == "channel") {
            tok_it++;
            auth_data->channel = std::string(&file_str[tokens[tok_it].start], 
                tokens[tok_it].end - tokens[tok_it].start);
            if (auth_data->channel.length() > 0) got_channel = true;
        }
        tok_it++;
    }

    free(file_str);
    if (got_oauth && got_client_id && got_nick && got_channel) {
        return true;
    } else {
        printf("ERROR: Invalid credentials in auth file\n");
    }
    return false;
}