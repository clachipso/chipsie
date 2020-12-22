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

#include "ChatProcessing.hpp"

struct IrcMessage
{
    std::string tags;
    std::string source;
    std::string command;
    std::string parameters;    
};

size_t AdvToNonWhitespace(const std::string &line, size_t cursor);
void HandlePrivMessage(const IrcMessage &irc_msg, TwitchConn *tc,
    Database *db);
void HandleUserCmd(const IrcMessage &irc_msg, TwitchConn *tc, Database *db,
    const std::string &chan, const std::string &cmd, 
    const std::string &sender, const std::string &params);
bool IsPrivileged(const std::string &user, const std::string &chan, 
    Database *db);

void ProcessChatLine(const std::string &line, TwitchConn *tc, Database *db) {

    // First break the line down to its IRC message components
    IrcMessage irc_msg;
    size_t cursor = 0;
    cursor = AdvToNonWhitespace(line, cursor);
    if (cursor >= line.size()) return;

    if (line[cursor] == '@') { // Line contains tags
        size_t tag_end = line.find(' ', cursor);
        cursor++;
        irc_msg.tags = line.substr(cursor, tag_end - cursor);
        cursor = tag_end;
        cursor = AdvToNonWhitespace(line, cursor);
        if (cursor >= line.size()) return;
    } else {
        irc_msg.tags = "";
    }

    if (line[cursor] == ':') { // Line contains a source
        size_t src_end = line.find(' ', cursor);
        cursor++;
        irc_msg.source = line.substr(cursor, src_end - cursor);
        cursor = src_end;
        cursor = AdvToNonWhitespace(line, cursor);
        if (cursor >= line.size()) return;
    } else {
       irc_msg.source = "";
    }

    // A command must be present
    size_t cmd_end = line.find(' ', cursor);
    irc_msg.command = line.substr(cursor, cmd_end - cursor);
    cursor = cmd_end;
    cursor = AdvToNonWhitespace(line, cursor);
    if (cursor >= line.size()) return;

    // Params are whatever is left over
    irc_msg.parameters = line.substr(cursor, line.size() - cursor);


    // Next, handle the command
    if (irc_msg.command == "PRIVMSG") { // Private message
        HandlePrivMessage(irc_msg, tc, db);
    
    } else if (irc_msg.command == "WHISPER") { // Direct whisper

    } else if (irc_msg.command == "PING") { // Ping message
        // Always reply with a pong so we don't get booted
        std::string reply = "PONG " + irc_msg.parameters;
        tc->SendMsg(reply);
    } else if (irc_msg.command == "JOIN") {// Join command reply
    
    } else if (irc_msg.command == "USERSTATE") {

    } else if (irc_msg.command == "ROOMSTATE") {

    } else if (irc_msg.command == "CAP") {

    } else if (irc_msg.command == "001") { // Welcome message

    } else if (irc_msg.command == "002") { // Server host ID
    
    } else if (irc_msg.command == "003") { // Server creation time

    } else if (irc_msg.command == "004") { // User info, not used by Twitch

    } else if (irc_msg.command == "353") { // NAME command reply

    } else if (irc_msg.command == "366") { // End of NAME list reply

    } else if (irc_msg.command == "375") { // Message of the day start

    } else if (irc_msg.command == "372") { // Message of the day line
        
    } else if (irc_msg.command == "376") { // Message of the day end

    } else {
        printf("ALERT: Unknown command %s\n", irc_msg.command.c_str());
    }
}

size_t AdvToNonWhitespace(const std::string &line, size_t cursor) {
    while (isspace(line[cursor]) && cursor < line.size()) {
        cursor++;
    }
    return cursor;
}

void HandlePrivMessage(const IrcMessage &irc_msg, TwitchConn *tc, 
    Database *db) {
    using namespace std;

    size_t cursor = AdvToNonWhitespace(irc_msg.parameters, 0);
    if (irc_msg.parameters[cursor] != '#') {
        printf("WARNING: Received malformed PRIVMSG command\n");
        return;
    }

    size_t end = irc_msg.parameters.find(' ', cursor);
    cursor++;
    string channel = irc_msg.parameters.substr(cursor, end - cursor);
    cursor = end + 1;

    cursor = irc_msg.parameters.find(':', cursor);
    if (cursor == string::npos) {
        printf("WARNING: Received PRIVMSG with no text\n");
        return;
    }
    cursor++;
    string priv_msg = irc_msg.parameters.substr(cursor);

    // Extract the user sending the command
    cursor = 0;
    end = irc_msg.source.find('!');
    string sender = "";
    if (end != string::npos) {
        sender = irc_msg.source.substr(0, end);
    } else {
        printf("WARNING: Could not determine sender of PRIVMSG\n");
    }
    
    // Is the sender trying to issue a command?
    cursor = AdvToNonWhitespace(priv_msg, 0);
    if (priv_msg[cursor] == '!') { // Extract command and handle it
        cursor++;
        end = cursor;
        while (end < priv_msg.length() && !isspace(priv_msg[end])) end++;
        
        string user_cmd = priv_msg.substr(cursor, end - cursor);
        string params = "";
        if (end < priv_msg.length()) {
            params = priv_msg.substr(end + 1);
        }
        HandleUserCmd(irc_msg, tc, db, channel, user_cmd, sender, params);
    } else {
        // TODO: mod stuff
    }
}

void HandleUserCmd(const IrcMessage &irc_msg, TwitchConn *tc, Database *db,
    const std::string &chan, const std::string &cmd, 
    const std::string &sender, const std::string &params) {
    
    using namespace std;

    printf("Got cmd %s from %s\n", cmd.c_str(), sender.c_str());
    if (cmd == "addadmin") {
        if (sender != chan) {
            printf("ALERT: Unauthorized attempted use of addop cmd by %s\n",
                sender.c_str());
            string resp = "PRIVMSG #" + chan + "Hey @" + sender + 
                ", you aren't allowed to use that command! >(";
            tc->SendMsg(resp);
            return;
        }

        int cursor = 0;
        while (cursor < params.length() && params[cursor] == ' ') cursor++;
        if (cursor == params.length()) cursor = (int)params.length() - 1; 
        size_t end = params.find(' ', cursor);
        if (end == string::npos) end = params.length();
        string admin_name = params.substr(cursor, end - cursor);
        if (admin_name.empty()) return;

        if (!db->IsAdmin(admin_name)) {
            db->AddAdmin(admin_name);
            printf("Added %s to admins\n", admin_name.c_str());
            string resp = "PRIVMSG #" + chan + " :" +  admin_name + 
                " is now a Chipsie admin. Be nice to me! ;)";
            tc->SendMsg(resp);
        }
    } else if (cmd == "remadmin") {
        if (sender != chan) {
            printf("ALERT: Unauthorized attempted use of addop cmd by %s\n",
                sender.c_str());
            string resp = "PRIVMSG #" + chan + "Hey @" + sender + 
                ", you aren't allowed to use that command! >(";
            tc->SendMsg(resp);
            return;
        }

        int cursor = 0;
        while (cursor < params.length() && params[cursor] == ' ') cursor++;
        if (cursor == params.length()) cursor = (int)params.length() - 1; 
        size_t end = params.find(' ', cursor);
        if (end == string::npos) end = params.length();
        string admin_name = params.substr(cursor, end - cursor);
        if (admin_name.empty()) return;
        if (db->IsAdmin(admin_name)) {
            db->RemAdmin(admin_name);
            printf("Removed admin %s\n", admin_name.c_str());
            string resp = "PRIVMSG #" + chan + " :OK " + sender + 
                ", I removed " + admin_name + " as a Chipsie admin! :D";
            tc->SendMsg(resp);
        }
    }
 }

bool IsPrivileged(const std::string &user, const std::string &chan, 
    Database *db) {
    if (user == chan) return true;
    if (db->IsAdmin(user)) return true;
    return false;
}