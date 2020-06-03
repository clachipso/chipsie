
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

#include "Networking.hpp"
#include "Command.hpp"
#include "jsmn.h"
#include <stdio.h>
#include <string>
#include <map>
#include <chrono>
#include <thread>
using namespace std;

const char * const DEF_AUTH_CFG_FILE = "auth/auth.json";
const char * const DEF_DB_FILE_NAME = "chipsie.db"; 

static AuthData auth_data;
static MsgQueue rx_queue;
static MsgQueue tx_queue;
static IrcMessage rx_msg;

// Loads the server authorization credentials from the auth file.
bool LoadAuthCfg(const char *auth_cfg_file);

int main(const int argc, const char **argv)
{
    printf("Chipsie the Twitch Chat Bot Starting Up...\n");

    // First, load the authorization credentials from the auth file. Note that
    // this file must exist prior to application launch.
    const char *auth_file_path = DEF_AUTH_CFG_FILE;
    if (argc > 1)
    {
        auth_file_path = argv[1];
    }
    if (!LoadAuthCfg(auth_file_path))
    {
        printf("Failed to load authorization credentials from file %s\n",
            auth_file_path);
        return -1;
    }

    // Initialize the necessary networking tasks
    if (InitNetworking(auth_data) != NET_OK)
    {
        printf("Failed to intiailze networking\n");
        return -1;
    }

    // Initialize the command processing
    const char *db_file_path = DEF_DB_FILE_NAME;
    if (argc > 2)
    {
        db_file_path = argv[2];
    }
    if (!InitCmdProcessing(db_file_path))
    {
        printf("Failed to initialize command processing\n");
        return -1;
    }

    // If we got here, we should be good to go. Enter our forever loop.
    while (true)
    {
        NetStatus ns = UpdateNetworking(&rx_queue, &tx_queue);
        if (ns == NET_ERROR)
        {
            break;
        }
        else if (ns == NET_CONNECT_FAILED)
        {
            // TODO: Add a random backoff to avoid hammering on the Twitch IRC
            // server
            this_thread::sleep_for(chrono::seconds(2));
        }

        // Handle any messages received from the server
        while (rx_queue.size() > 0)
        {
            string line = rx_queue.front();
            rx_queue.pop();
            if (!ConvertLineToMsg(line, &rx_msg))
            {
                printf("WARNING: Failed to convert rx line to IRC message\n");
                continue;
            }
            ProcessMsg(rx_msg, &tx_queue);
        }
        
        // This wait ensures that the networking won't spam the Twitch server.
        // We only send 1 message per iteration, so this works with the Twitch
        // guidelines of 100msgs/30seconds for users
        this_thread::sleep_for(chrono::milliseconds(5));
    }

    StopNetworking();
    printf("Chipsie the Twitch Chat Bot Shutting Down...Bye Bye!\n");
    return 0;
}

bool LoadAuthCfg(const char *auth_cfg_file)
{
    FILE *auth_file = NULL;
    errno_t res = fopen_s(&auth_file, auth_cfg_file, "r");
    if (res) 
    {
        printf("Failed to open auth config file\n");
        return false;
    }

    fseek(auth_file, 0, SEEK_END);
    long file_len = ftell(auth_file);
    if (file_len <= 0) 
    {
        printf("Invalid auth credential file length\n");
        return false;
    }
    fseek(auth_file, 0, SEEK_SET);

    char *file_str = (char *)malloc(file_len);
    fread(file_str, 1, file_len, auth_file);
    fclose(auth_file);
    printf("Read in auth credential file of %d bytes\n", file_len);

    jsmntok tokens[12];
    jsmn_parser parser;
    jsmn_init(&parser);
    int rc = jsmn_parse(&parser, file_str, (size_t)file_len, tokens, 12);
    if (rc <= 0) 
    {
        printf("Wanted 8 json tokens, got %d tokens\n", rc);
        free(file_str);
        return false;
    }

    bool got_oauth = false;
    bool got_client_id = false;
    bool got_nick = false;
    bool got_channel = false;
    int tok_it = 0;
    while (tok_it < rc)
    {
        string key = string(&file_str[tokens[tok_it].start], 
            tokens[tok_it].end - tokens[tok_it].start);

        if (key == "token")
        {
            tok_it++;
            auth_data.oauth = string(&file_str[tokens[tok_it].start], 
                tokens[tok_it].end - tokens[tok_it].start);
            got_oauth = true;
        }
        else if (key == "client_id")
        {
            tok_it++;
            auth_data.client_id = string(&file_str[tokens[tok_it].start], 
                tokens[tok_it].end - tokens[tok_it].start);
            got_client_id = true;
        }
        else if (key == "nick")
        {
            tok_it++;
            auth_data.nick = string(&file_str[tokens[tok_it].start], 
                tokens[tok_it].end - tokens[tok_it].start);
            got_nick = true;
        }
        else if (key == "channel")
        {
            tok_it++;
            auth_data.channel = string(&file_str[tokens[tok_it].start], 
                tokens[tok_it].end - tokens[tok_it].start);
            got_channel = true;
        }
        tok_it++;
    }

    free(file_str);
    if (got_oauth && got_client_id && got_nick && got_channel)
    {
        printf("Loaded auth credentials successfully! 8D\n");
        return true;
    }
    else
    {
        printf("Got oauth %d client_id %d nick %d channel %d\n", got_oauth, 
            got_client_id, got_nick, got_channel);
    }
    return false;
}