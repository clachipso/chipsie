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

#include "Command.hpp"
#include <stdlib.h>
#include <time.h>
#include "sqlite3.h"
#include <vector>
#include <sstream>
#include <chrono>
using namespace std;
using namespace chrono;

static sqlite3 *db;
static steady_clock::time_point motd_tp;
static bool quit_flag = false;

void HandlePrivateMsg(const IrcMessage &msg, MsgQueue *tx_queue);
void HandleUserCmd(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd,  const string &sender, 
    const string &params);
bool IsPriviledged(const string &name, const string &chan);
void ProcessOutputString(string &input, const string &chan, const string &cmd,  
    const string &sender, queue<string> &params);

void HandleCmdAddop(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd,  const string &sender, 
    const string &params);
void HandleCmdRemop(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd,  const string &sender, 
    const string &params);
void HandleCmdAddcmd(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd,  const string &sender, 
    const string &params);
void HandleCmdRemcmd(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd,  const string &sender, 
    const string &params);


bool InitCmdProcessing(const char *db_path)
{
    srand((unsigned int)time(NULL));

    int rc = sqlite3_open(db_path, &db);
    if (rc != SQLITE_OK)
    {
        printf("Failed to open db: %d\n", rc);
        return false;
    }
    printf("Opened database file %s\n", db_path);

    // This may be the first time the application is run with the specified
    // database, we need to ensure that the database has been initialized.
    string sql_str = "SELECT count(*) FROM sqlite_master WHERE type ='table' ";
    sql_str += "AND name ='operators'";
    sqlite3_stmt *table_check = NULL;
    rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
        &table_check, NULL);
    if (rc != SQLITE_OK)
    {
        printf("ERROR: Failed to query operators table existence %d\n", 
            rc);
        return false;
    }
    rc = sqlite3_step(table_check);
    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(table_check);
        printf("ERROR: Failed to query operators table existence\n");
        return false;
    }
    int count = sqlite3_column_int(table_check, 0);
    sqlite3_finalize(table_check);
    if (count == 0)
    {
        // Table doesn't exist
        sqlite3_stmt *table_create;
        sql_str = "CREATE TABLE operators";
        sql_str += "(name text)";
        rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
            &table_create, NULL);
        if (rc == SQLITE_OK)
        {
            rc = sqlite3_step(table_create);
            if (rc == SQLITE_DONE)
            {
                printf("Database operators table created...\n");
            }
            else
            {
                printf("ERROR: Failed to create operators table %d\n", rc);
                sqlite3_finalize(table_create);
                return false;
            }
        }
        else
        {
            printf("ERROR: Failed to create operators table %d\n", rc);
            sqlite3_finalize(table_create);
            return false;
        }
        sqlite3_finalize(table_create);        
    }
    else 
    {
        printf("Verified database has operators table\n");
    }

    // Check for static commands table existence
    table_check = NULL;
    sql_str = "SELECT count(*) FROM sqlite_master WHERE type ='table' AND ";
    sql_str += "name ='static_cmds'";
    rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
        &table_check, NULL);
    if (rc != SQLITE_OK)
    {
        sqlite3_finalize(table_check);
        printf("ERROR: Failed to check if static command table exists %d\n", 
            rc);
        return false;
    }
    rc = sqlite3_step(table_check);
    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(table_check);
        printf("ERROR: Failed to query static_cmd table existence\n");
        return false;
    }
    count = sqlite3_column_int(table_check, 0);
    sqlite3_finalize(table_check);
    if (count == 0)
    {
        // Table doesn't exist
        sqlite3_stmt *table_create = NULL;
        sql_str = "CREATE TABLE static_cmds ";
        sql_str += "(cmd TEXT, response TEXT)";

        sqlite3_finalize(table_create);
        rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
            &table_create, NULL);
        if (rc == SQLITE_OK)
        {
            rc = sqlite3_step(table_create);
            if (rc == SQLITE_DONE)
            {
                printf("Database static_cmds table created...\n");
            }
            else
            {
                sqlite3_finalize(table_create);
                printf("ERROR: Failed to create static_cmds table %d\n", rc);
                return false;
            }
        }
        else
        {
            sqlite3_finalize(table_create);
            printf("ERROR: Failed to create static_cmds table %d\n", rc);
            return false;
        }
        sqlite3_finalize(table_create);
        
    }
    else 
    {
        printf("Verified database has static_cmds table\n");
    }

    // Check for message of the day table existence
    table_check = NULL;
    sql_str = "SELECT count(*) FROM sqlite_master WHERE type ='table' AND ";
    sql_str += "name ='motd'";
    rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
        &table_check, NULL);
    if (rc != SQLITE_OK)
    {
        sqlite3_finalize(table_check);
        printf("ERROR: Failed to check if motd table exists %d\n", 
            rc);
        return false;
    }
    rc = sqlite3_step(table_check);
    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(table_check);
        printf("ERROR: Failed to query motd table existence\n");
        return false;
    }
    count = sqlite3_column_int(table_check, 0);
    sqlite3_finalize(table_check);
    if (count == 0)
    {
        // Table doesn't exist
        sqlite3_stmt *table_create = NULL;
        sql_str = "CREATE TABLE motd ";
        sql_str += "(motd TEXT, rate INTEGER, enabled BOOL)";

        rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
            &table_create, NULL);
        if (rc == SQLITE_OK)
        {
            rc = sqlite3_step(table_create);
            if (rc == SQLITE_DONE)
            {
                printf("Database motd table created...\n");
            }
            else
            {
                sqlite3_finalize(table_create);
                printf("ERROR: Failed to create motd table %d\n", rc);
                return false;
            }
        }
        else
        {
            sqlite3_finalize(table_create);
            printf("ERROR: Failed to create motd table %d\n", rc);
            return false;
        }
        sqlite3_finalize(table_create);

        // Need to insert a basic MOTD
        sqlite3_stmt *motd_create = NULL;
        sql_str = "INSERT INTO motd (rate, enabled) VALUES (15, 0)";
        rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
            &motd_create, NULL);
        if (rc == SQLITE_OK)
        {
            rc = sqlite3_step(motd_create);
            if (rc == SQLITE_DONE)
            {
                printf("Added empty motd\n");
            }
            else
            {
                sqlite3_finalize(motd_create);
                printf("ERROR: Failed to insert empty motd\n");
                return false;
            }
            
        }
        else
        {
            sqlite3_finalize(motd_create);
            printf("ERROR: Failed to create basic motd\n");
            return false;
        }
        sqlite3_finalize(motd_create);
    }
    else 
    {
        printf("Verified database has motd table\n");
    }

    // Set up MOTD clock
    motd_tp = steady_clock::now();

    return true;
}

bool ConvertLineToMsg(const string &line, IrcMessage *msg)
{
    size_t cursor = 0;
    while ((line[cursor] == ' ' || line[cursor] == '\t')) 
    {
        cursor++;
        if (cursor >= line.size()) return false;
    }
    
    if (line[cursor] == '@') // Line has tags
    {
        size_t end = line.find(' ', cursor);
        cursor++;
        msg->tags = line.substr(cursor, end - cursor);
        cursor = end;
        while ((line[cursor] == ' ' || line[cursor] == '\t')) 
        {
            cursor++;
            if (cursor >= line.size()) return false;
        }
    }
    else
    {
        msg->tags = "";
    }

    if (line[cursor] == ':')
    {
        size_t end = line.find(' ', cursor);
        cursor++;
        msg->source = line.substr(cursor, end - cursor);
        cursor = end;
        while ((line[cursor] == ' ' || line[cursor] == '\t')) 
        {
            cursor++;
            if (cursor >= line.size()) return false;
        }
    }
    else
    {
        msg->source = "";
    }

    size_t end = line.find(' ', cursor);
    msg->command = line.substr(cursor, end - cursor);
    cursor = end;
    while (line[cursor] == ' ' || line[cursor] == '\t') 
    {
        cursor++;
        if (cursor >= line.size())
        {
            msg->parameters = "";
            return true;
        }
    }

    msg->parameters = line.substr(cursor, line.size() - cursor);
    return true;
}

void ProcessMsg(const IrcMessage &msg, MsgQueue *tx_queue)
{
    if (msg.command == "PRIVMSG") // Private message
    {
        HandlePrivateMsg(msg, tx_queue);
    }
    else if (msg.command == "PING") // Ping message
    {
        // Always reply with a pong so we don't get booted
        string line = "PONG " + msg.parameters;
        tx_queue->push(line);
    }
    else if (msg.command == "JOIN") // Join command reply
    {

    }
    else if (msg.command == "001") // Welcome message
    {

    }
    else if (msg.command == "002") // Server host ID
    {

    }
    else if (msg.command == "003") // Server creation time
    {

    }
    else if (msg.command == "004") // User info, not currently used by Twitch
    {

    }
    else if (msg.command == "353") // NAME command reply
    {

    }
    else if (msg.command == "366") // End of NAME list reply
    {

    }
    else if (msg.command == "375") // Message of the day start
    {

    }
    else if (msg.command == "372") // Message of the day line
    {
        
    }
    else if (msg.command == "376") // Message of the day end
    {

    }
    else
    {
        printf("ALERT: Unknown command %s\n", msg.command.c_str());
    }
}

void UpdateMotd(MsgQueue *tx_queue, const string &chan)
{
    string sql_str = "SELECT * FROM motd";
    sqlite3_stmt *motd_stmt = NULL;
    int rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
        &motd_stmt, NULL);
    if (rc != SQLITE_OK)
    {
        printf("ERROR: Failed to create motd pull statement\n");
        return;
    }
    rc = sqlite3_step(motd_stmt);
    if (rc != SQLITE_ROW)
    {
        sqlite3_finalize(motd_stmt);
        printf("ERROR: Failed to retrieve motd row\n");
        return;
    }

    // Is the MOTD disabled?
    if (sqlite3_column_int(motd_stmt, 2) == 0)
    {
        sqlite3_finalize(motd_stmt);
        return;
    }

    string motd = "";
    if (sqlite3_column_text(motd_stmt, 0) == NULL) 
    {
        sqlite3_finalize(motd_stmt);
        return;
    }
    else 
    {
        motd = string((const char *)sqlite3_column_text(motd_stmt, 0));
    }

    int rate_sec = 60 * sqlite3_column_int(motd_stmt, 1);
    sqlite3_finalize(motd_stmt);

    auto curr_time = steady_clock::now();
    auto elapsed = curr_time - motd_tp;
    int elapsed_secs = (int)duration_cast<seconds>(elapsed).count();
    if (elapsed_secs >= rate_sec)
    {
        motd_tp = curr_time;

        string msg = "PRIVMSG #" + chan + " :" + motd;
        tx_queue->push(msg);
    }
}

void HandlePrivateMsg(const IrcMessage &msg, MsgQueue *tx_queue)
{
    size_t cursor = 0;
    while (msg.parameters[cursor] == ' ' || msg.parameters[cursor] == '\t')
    {
        cursor++;
        if (cursor >= msg.parameters.length())
        {
            cursor = msg.parameters.length() - 1;
            break;
        }
    }
    if (msg.parameters[cursor] != '#')
    {
        printf("WARNING: Received malformed PRIVMSG command\n");
        return;
    }
    size_t end = msg.parameters.find(' ', cursor);
    cursor++;
    string channel = msg.parameters.substr(cursor, end - cursor);
    cursor = end + 1;

    cursor = msg.parameters.find(':', cursor);
    if (cursor == string::npos)
    {
        printf("WARNING: Received PRIVMSG with no text\n");
        return;
    }
    cursor++;
    string priv_msg = msg.parameters.substr(cursor);

    // Extract the user sending the command
    cursor = 0;
    end = msg.source.find('!');
    string sender = "";
    if (end != string::npos)
    {
        sender = msg.source.substr(0, end);
    }
    
    // Is the sender trying to issue a command?
    cursor = 0;
    while (priv_msg[cursor] == ' ' || priv_msg[cursor] == '\t')
    {
        cursor++;
        if (cursor >= priv_msg.length())
        {
            cursor = priv_msg.length() - 1;
            break;
        }
    }
    if (priv_msg[cursor] == '!')
    {
        // User is attempting to send command. Extract it
        cursor++;
        end = cursor;
        while (priv_msg[end] != ' ' && priv_msg[end] != '\t')
        {
            end++;
            if (end >= priv_msg.length()) 
            {
                end = priv_msg.length();
                break;
            }
        }
        string user_cmd = priv_msg.substr(cursor, end - cursor);
        string params = "";
        if (end < priv_msg.length())
        {
            params = priv_msg.substr(end + 1);
        }
        HandleUserCmd(msg, tx_queue, channel, user_cmd, sender, params);
    }
}

void HandleUserCmd(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd, const string &sender,
    const string &params)
{
    if (cmd == "shutdown")
    {
        if (sender == chan)
        {
            printf("ALERT: Shutdown command given by streamer\n");
            string resp = "PRIVMSG #" + chan + 
                " :OK, I'm shutting down now to recharge. See you later! :)";
            tx_queue->push(resp);
            quit_flag = true;
        }
        else
        {
            printf("ALERT: Unauthorized attempted use of addop cmd by %s\n",
            sender.c_str());
            string resp = "PRIVMSG #" + chan + "Hey @" + sender + 
                ", you aren't allowed to use that command! >(";
            tx_queue->push(resp);
        }        
    }
    else if (cmd == "dice")
    {
        int d6 = rand() % 6 + 1;
        string resp = "PRIVMSG #" + chan + " :You rolled a " + to_string(d6);
        tx_queue->push(resp);
    }
    else if (cmd == "addop")
    {
        HandleCmdAddop(msg, tx_queue, chan, cmd, sender, params);
    }
    else if (cmd == "rmop")
    {
        HandleCmdRemop(msg, tx_queue, chan, cmd, sender, params);
    }
    else if (cmd == "addcmd")
    {
        HandleCmdAddcmd(msg, tx_queue, chan, cmd, sender, params);
    }
    else if (cmd == "rmcmd")
    {
        HandleCmdRemcmd(msg, tx_queue, chan, cmd, sender, params);
    }
    else if (cmd == "motdset")
    {
        if (!IsPriviledged(sender, chan))
        {
            printf("ALERT: Unauthorized attempted use of motdset cmd by %s\n",
                sender.c_str());
            string resp = "PRIVMSG #" + chan + "Hey @" + sender + 
                ", you aren't allowed to use that command! Don't make me angry! >(";
            tx_queue->push(resp);
            return;
        }

        sqlite3_stmt *motd_stmt = NULL;
        string sql_str = "UPDATE motd SET motd = \'" + params + "\' ";
        int rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(),
            &motd_stmt, NULL);
        if (rc != SQLITE_OK)
        {
            printf("WARNING: Failed to create motd update statement\n");
            return;
        }
        rc = sqlite3_step(motd_stmt);
        if (rc == SQLITE_DONE)
        {
            string resp = "PRIVMSG #" + chan + " :OK @" + sender + 
                ", I updated the message of the day :)";
            tx_queue->push(resp);
        }
        else
        {
            printf("WARNING: Failed to update motd\n");
        }
        sqlite3_finalize(motd_stmt);
    }
    else if (cmd == "motdrate")
    {
        if (!IsPriviledged(sender, chan))
        {
            printf("ALERT: Unauthorized attempted use of motdrate cmd by %s\n",
                sender.c_str());
            string resp = "PRIVMSG #" + chan + "Hey @" + sender + 
                ", you aren't allowed to use that command! Don't make me angry! >(";
            tx_queue->push(resp);
            return;
        }

        int cursor = 0;
        while (cursor < params.length() && params[cursor] == ' ') cursor++;
        if (cursor == params.length()) cursor = (int)params.length() - 1; 
        size_t end = params.find(' ', cursor);
        if (end == string::npos) end = params.length();
        string rate_str = params.substr(cursor, end - cursor);
        if (rate_str.empty()) return;

        int rate = (int)strtol(rate_str.c_str(), NULL, 10);
        if (rate < 1)
        {
            string resp = "PRIVMSG #" + chan + " :That's not a valid rate, ";
            resp += "@" + sender + ". Try a number greater than 0 :P";
            tx_queue->push(resp);
        }

        sqlite3_stmt *motd_stmt = NULL;
        string sql_str = "UPDATE motd SET rate = " + rate_str;
        int rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(),
            &motd_stmt, NULL);
        if (rc != SQLITE_OK)
        {
            printf("WARNING: Failed to create motd rate statement\n");
            return;
        }
        rc = sqlite3_step(motd_stmt);
        if (rc == SQLITE_DONE)
        {
            string resp = "PRIVMSG #" + chan + " :OK @" + sender + 
                ", I set the message of the day rate to " + rate_str + " :)";
            tx_queue->push(resp);
        }
        else
        {
            printf("WARNING: Failed to enable motd\n");
        }
        sqlite3_finalize(motd_stmt);
    }
    else if (cmd == "motdon")
    {
        if (!IsPriviledged(sender, chan))
        {
            printf("ALERT: Unauthorized attempted use of motdon cmd by %s\n",
                sender.c_str());
            string resp = "PRIVMSG #" + chan + "Hey @" + sender + 
                ", you aren't allowed to use that command! Don't make me angry! >(";
            tx_queue->push(resp);
            return;
        }

        sqlite3_stmt *motd_stmt = NULL;
        string sql_str = "UPDATE motd SET enabled = 1";
        int rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(),
            &motd_stmt, NULL);
        if (rc != SQLITE_OK)
        {
            printf("WARNING: Failed to create motd enable statement\n");
            return;
        }
        rc = sqlite3_step(motd_stmt);
        if (rc == SQLITE_DONE)
        {
            string resp = "PRIVMSG #" + chan + " :OK @" + sender + 
                ", I enabled the message of the day :)";
            tx_queue->push(resp);
        }
        else
        {
            printf("WARNING: Failed to enable motd\n");
        }
        sqlite3_finalize(motd_stmt);
    }
    else if (cmd == "motdoff")
    {
        if (!IsPriviledged(sender, chan))
        {
            printf("ALERT: Unauthorized attempted use of motdoff cmd by %s\n",
                sender.c_str());
            string resp = "PRIVMSG #" + chan + "Hey @" + sender + 
                ", you aren't allowed to use that command! Don't make me angry! >(";
            tx_queue->push(resp);
            return;
        }

        sqlite3_stmt *motd_stmt = NULL;
        string sql_str = "UPDATE motd SET enabled = 0";
        int rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(),
            &motd_stmt, NULL);
        if (rc != SQLITE_OK)
        {
            printf("WARNING: Failed to create motd disable statement\n");
            return;
        }
        rc = sqlite3_step(motd_stmt);
        if (rc == SQLITE_DONE)
        {
            string resp = "PRIVMSG #" + chan + " :Alright @" + sender + 
                ", I disabled the message of the day :(";
            tx_queue->push(resp);
        }
        else
        {
            printf("WARNING: Failed to disable motd\n");
        }
        sqlite3_finalize(motd_stmt);
    }
    else
    {
        // No dynamic command found. Check database for a static command
        sqlite3_stmt *cmd_stmt = NULL;
        string sql_str = "SELECT * FROM static_cmds WHERE cmd = \'" + cmd;
        sql_str += "\'";
        int rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
            &cmd_stmt, NULL);
        if (rc != SQLITE_OK)
        {
            printf("WARNING: Failed to fetch static cmd %s from db: %d\n", 
                cmd.c_str(), rc);
            return;
        }
        rc = sqlite3_step(cmd_stmt);
        if (rc != SQLITE_ROW)
        {
            sqlite3_finalize(cmd_stmt);
            return;
        }

        queue<string> param_list;
        stringstream sstream(params);
        string temp_str;
        while (getline(sstream, temp_str, ' '))
        {
            param_list.push(temp_str);
        }

        const char *resp_str = (const char *)sqlite3_column_text(cmd_stmt, 1);
        if (resp_str != NULL)
        {
            string raw_string = string(resp_str);
            ProcessOutputString(raw_string, chan, cmd, sender, param_list);
            string resp = "PRIVMSG #" + chan + " :" + raw_string;
            tx_queue->push(resp);
        }
        sqlite3_finalize(cmd_stmt);
    }
}

bool IsPriviledged(const string &name, const string &chan)
{
    if (name == chan) return true; // Channel owner is always priviledged

    string sql_str = "SELECT * FROM operators WHERE name = \'" + name + "\'";
    sqlite3_stmt *op_check = NULL;
    int rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
        &op_check, NULL);
    if (rc != SQLITE_OK)
    {
        printf("WARNING: Failed to create op_check statement %d\n", rc);
        return false;
    }
    rc = sqlite3_step(op_check);
    bool priv = false;
    if (rc == SQLITE_ROW) // If a row was returned, user was in table
    {
        priv = true;
    }
    sqlite3_finalize(op_check);
    return priv;
}

void ProcessOutputString(string &input, const string &chan, const string &cmd,  
    const string &sender, queue<string> &params)
{
    size_t cursor = input.find("[");
    while (cursor != string::npos)
    {
        size_t end = input.find("]");
        if (end == string::npos) break;

        string wildcard = input.substr(cursor + 1, end - (cursor + 1));

        if (wildcard == "username")
        {
            input.replace(cursor, wildcard.length() + 2, sender);
        }
        else if (wildcard == "channel")
        {
            input.replace(cursor, wildcard.length() + 2, chan);
        }
        else if (wildcard == "item")
        {
            int item_number = rand() % 5;
            string item_name = "a old boot";
            switch (item_number)
            {
                case 0:
                    item_name = "a magical sword";
                    break;
                case 1:
                    item_name = "a strange smelling potion";
                    break;
                case 2:
                    item_name = "a gold dubloon";
                    break;
                case 3:
                    item_name = "a tattered scroll";
                    break;
                case 4:
                    item_name = "an ancient artifact";
                    break;
            }
            input.replace(cursor, wildcard.length() + 2, item_name);
        }
        else if (wildcard == "param")
        {
            if (params.size() <= 0)
            {
                input = "You didn't format that command right, @" + sender;
                input += " :/";
                return;
            }
            string param_str = params.front();
            params.pop();
            input.replace(cursor, wildcard.length() + 2, param_str);
        }
        else
        {
            input.replace(cursor, wildcard.length() + 2, "ERROR");
        }
        

        cursor = end;
        cursor = input.find("<<");
    }
}

void HandleCmdAddop(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd,  const string &sender, 
    const string &params)
{
    if (sender != chan)
    {
        printf("ALERT: Unauthorized attempted use of addop cmd by %s\n",
            sender.c_str());
        string resp = "PRIVMSG #" + chan + "Hey @" + sender + 
            ", you aren't allowed to use that command! Don't make me angry! >(";
        tx_queue->push(resp);
        return;
    }

    int cursor = 0;
    while (cursor < params.length() && params[cursor] == ' ') cursor++;
    if (cursor == params.length()) cursor = (int)params.length() - 1; 
    size_t end = params.find(' ', cursor);
    if (end == string::npos) end = params.length();
    string op_name = params.substr(cursor, end - cursor);
    if (op_name.empty()) return;

    sqlite3_stmt *op_check = NULL;
    string sql_str = "SELECT * FROM operators WHERE name = \'" + op_name;
    sql_str += "\'";
    int rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
        &op_check, NULL);
    if (rc != SQLITE_OK)
    {
        printf("WARNING: Failed to create operator check statement %d\n", 
            rc);
        return;
    }
    rc = sqlite3_step(op_check);
    if (rc == SQLITE_ROW)
    {
        printf("WARNING: Attempted to readd %s to operators\n", 
            op_name.c_str());
        string resp = "PRIVMSG #" + chan + " :Uhh, @" + chan +", " + op_name + 
            " is already an operator ;P";
        tx_queue->push(resp);
    }
    else
    {
        sqlite3_stmt *op_add = NULL;
        sql_str = "INSERT INTO operators (name) VALUES (\'";
        sql_str += "" + op_name + "\')";
        rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
            &op_add, NULL);
        if (rc != SQLITE_OK)
        {
            printf("WARNING: Failed to create op add statement %d\n", rc);
        }
        else
        {
            rc = sqlite3_step(op_add);
            if (rc == SQLITE_DONE)
            {
                printf("Added %s to operators\n", op_name.c_str());
                string resp = "PRIVMSG #" + chan + " :" +  op_name + 
                    " is now an operator. Be nice to me! ;)";
                tx_queue->push(resp);
            }
            else
            {
                printf("WARNING: Failed to add %s to operators %d %s\n",
                    op_name.c_str(), rc, sqlite3_errmsg(db));
            }
            sqlite3_finalize(op_add);
        }
    }
    sqlite3_finalize(op_check);
}

void HandleCmdAddcmd(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd,  const string &sender, 
    const string &params)
{
    if (!IsPriviledged(sender, chan))
    {
        printf("ALERT: Unauthorized attempted use of addcmd by %s\n",
            sender.c_str());
        string resp = "PRIVMSG #" + chan + " :Hey @" + sender + 
            ", you aren't allowed to use that command! Don't make me angry! >(";
        tx_queue->push(resp);
        return;
    }

    size_t cursor = 0;
    while (cursor < params.length() && params[cursor] == ' ') cursor++;
    if (cursor == params.length()) return;
    size_t end = params.find(' ', cursor);
    if (end == string::npos) return;
    string cmd_name = params.substr(cursor, end - cursor);

    cursor = (int)end + 1;
    while (cursor < params.length() && params[cursor] == ' ') cursor++;
    if (cursor == params.length()) return;
    string cmd_resp = params.substr(cursor);

    // Do we update or insert?
    sqlite3_stmt *cmd_check = NULL;
    string sql_str = "SELECT * FROM static_cmds WHERE cmd = \'" + cmd_name;
    sql_str += "\'";
    int rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
        &cmd_check, NULL);
    if (rc != SQLITE_OK)
    {
        printf("WARNING: Failed to check static cmd %s from db: %d\n", 
            cmd.c_str(), rc);
        return;
    }
    rc = sqlite3_step(cmd_check);
    sqlite3_finalize(cmd_check);

    // Need to double up backticks in response to make SQL happy
    cursor = cmd_resp.find('\'');
    while (cursor != string::npos)
    {
        cmd_resp.insert(cursor, 1, '\'');
        cursor = cmd_resp.find('\'', cursor + 2);
    }
    
    sqlite3_stmt *cmd_set = NULL;
    if (rc == SQLITE_ROW)
    {
        // Update
        sql_str = "UPDATE static_cmds SET response = \'" + cmd_resp + "\' ";
        sql_str += "WHERE cmd = \'" + cmd_name + "\'";
    }
    else
    {
        // Insert
        sql_str = "INSERT INTO static_cmds (cmd, response) VALUES ";
        sql_str += "(\'" + cmd_name + "\', \'" + cmd_resp + "\')";
    }
    rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
        &cmd_set, NULL);
    if (rc != SQLITE_OK)
    {
        printf("WARNING: Failed to create static_cmd set statement %d. %s\n",
            rc, sql_str.c_str());
        return;
    }
    rc = sqlite3_step(cmd_set);
    sqlite3_finalize(cmd_set);
    if (rc == SQLITE_DONE)
    {
        printf("Set command %s to %s\n", cmd_name.c_str(), 
            cmd_resp.c_str());
        string resp = "PRIVMSG #" + chan + " :OK " + sender + 
            ", I added the " + cmd_name + " command! :D";
        tx_queue->push(resp);
    }
}

void HandleCmdRemcmd(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd,  const string &sender, 
    const string &params)
{
    if (!IsPriviledged(sender, chan))
    {
        printf("ALERT: Unauthorized attempted use of rmcmd by %s\n",
            sender.c_str());
        string resp = "PRIVMSG #" + chan + " :Hey @" + sender + 
            ", you aren't allowed to use that command! Don't make me angry! >(";
        tx_queue->push(resp);
        return;
    }

    int cursor = 0;
    while (cursor < params.length() && params[cursor] == ' ') cursor++;
    if (cursor == params.length()) return;
    size_t end = params.find(' ', cursor);
    if (end == string::npos) end = params.length();
    string cmd_name = params.substr(cursor, end - cursor);

    // Check to see if command is in db
    sqlite3_stmt *cmd_check = NULL;
    string sql_str = "SELECT * FROM static_cmds WHERE cmd = \'" + cmd_name;
    sql_str += "\'";
    int rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
        &cmd_check, NULL);
    if (rc != SQLITE_OK)
    {
        printf("WARNING: Failed to check static cmd %s from db: %d\n", 
            cmd.c_str(), rc);
        return;
    }
    rc = sqlite3_step(cmd_check);
    sqlite3_finalize(cmd_check);

    if (rc == SQLITE_ROW)
    {
        sql_str = "DELETE FROM static_cmds WHERE cmd = \'" + cmd_name + "\'";
        sqlite3_stmt *cmd_rm = NULL;
        rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
            &cmd_rm, NULL);
        if (rc != SQLITE_OK)
        {
            printf("WARNING: Failed to create static_cmd delete statement %d\n",
                rc);
            return;
        }
        rc = sqlite3_step(cmd_rm);
        sqlite3_finalize(cmd_rm);
        if (rc == SQLITE_DONE)
        {
            printf("Removed command %s\n", cmd_name.c_str());
            string resp = "PRIVMSG #" + chan + " :OK " + sender + 
                ", I removed the " + cmd_name + " command! :D";
            tx_queue->push(resp);
        }
        else
        {
            printf("WARNING: Failed to delete command from db: %d\n", rc);
        }
        
    }
    else
    {
        string resp = "PRIVMSG #" + chan + " :Sorry @" + sender + 
            ", but I couldn't find a " + cmd_name + " command :(";
        tx_queue->push(resp);
    }
}

void HandleCmdRemop(const IrcMessage &msg, MsgQueue *tx_queue, 
    const string &chan, const string &cmd,  const string &sender, 
    const string &params)
{
    if (sender != chan)
    {
        printf("ALERT: Unauthorized attempted use of rmop cmd by %s\n",
            sender.c_str());
        string resp = "PRIVMSG #" + chan + "Hey @" + sender + 
            ", you aren't allowed to use that command! Don't make me angry! >(";
        tx_queue->push(resp);
        return;
    }

    int cursor = 0;
    while (cursor < params.length() && params[cursor] == ' ') cursor++;
    if (cursor == params.length()) cursor = (int)params.length() - 1; 
    size_t end = params.find(' ', cursor);
    if (end == string::npos) end = params.length();
    string op_name = params.substr(cursor, end - cursor);
    if (op_name.empty()) return;

    sqlite3_stmt *op_check = NULL;
    string sql_str = "SELECT * FROM operators WHERE name = \'" + op_name;
    sql_str += "\'";
    int rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
        &op_check, NULL);
    if (rc != SQLITE_OK)
    {
        printf("WARNING: Failed to remove operator check statement %d\n", 
            rc);
        return;
    }
    rc = sqlite3_step(op_check);
    if (rc == SQLITE_ROW)
    {
        sql_str = "DELETE FROM operators WHERE name = \'" + op_name + "\'";
        sqlite3_stmt *cmd_rm = NULL;
        rc = sqlite3_prepare_v2(db, sql_str.c_str(), (int)sql_str.length(), 
            &cmd_rm, NULL);
        if (rc != SQLITE_OK)
        {
            printf("WARNING: Failed to create operators delete statement %d\n",
                rc);
            return;
        }
        rc = sqlite3_step(cmd_rm);
        sqlite3_finalize(cmd_rm);
        if (rc == SQLITE_DONE)
        {
            printf("Removed operator %s\n", op_name.c_str());
            string resp = "PRIVMSG #" + chan + " :OK " + sender + 
                ", I removed " + op_name + " as an operator! :D";
            tx_queue->push(resp);
        }
        else
        {
            printf("WARNING: Failed to delete command from db: %d\n", rc);
        }
    }
    else
    {
        string resp = "PRIVMSG #" + chan + " :Hmm @" + sender + 
            ", there's no operator named " + op_name + " O_o";
        tx_queue->push(resp);
    }
    sqlite3_finalize(op_check);
}

bool GetQuitFlag()
{
    return quit_flag;
}