#include <iostream>
#include <nlohmann/json.hpp>
#include <uwebsockets/App.h>
#include <string>

using json = nlohmann::json;
using namespace std;


// ws - обхект/параметр вебсокета

const string COMMAND = "command";
const string PRIVATE_MSG = "private_msg";
const string SET_NAME = "set_name";
const string USER_ID = "user_id";
const string USER_FROM = "user_from";
const string MESSAGE = "message";
const string NAME = "name";
const string ONLINE = "online";
const string STATUS = "status";
const string BROADCAST = "broadcast";

struct PerSocketData {
    /* Fill with user data */
    int user_id;
    string name;
};

//карта
map<int, PerSocketData*> activeUsers;

typedef uWS::WebSocket < false, true, PerSocketData> UWEBSOCK;

//void sendPrivateResponse (UWEBSOCK* ws, )

string status(PerSocketData* data, bool online)
{
    json request;
    request[COMMAND] = STATUS;
    request[NAME] = data->name;
    request[ONLINE] = online;
    return request.dump();
}



void processMessage(UWEBSOCK* ws, std::string_view message)
{
    //распрсенная инфа
    PerSocketData* data = ws->getUserData();
    auto parsed = json::parse(message);
    string command = parsed[COMMAND];

    if (command == PRIVATE_MSG) 
    {
        int user_id = parsed[USER_ID];
        string user_msg = parsed[MESSAGE];
        int sender = data->user_id;
        json response;
        response[COMMAND] = PRIVATE_MSG;
        response[USER_FROM] = data->user_id;
        response[MESSAGE] = user_msg;
        ws->publish("UserN" + to_string(user_id), response.dump());

    }
    if (command == SET_NAME)
    {
         data->name = parsed[NAME]; 
         ws->publish(BROADCAST, status(data, true));
    }
}

int main() {
    /* ws->getUserData returns one of these */
   

    int latest_id = 10;

    uWS::App().ws<PerSocketData>("/*", {
            /* Settings */
            .idleTimeout = 9999, // через канал 16 сек нет сообшений - закрывать
            .maxBackpressure = 1 * 1024 * 1024,
            .closeOnBackpressureLimit = false,
            .resetIdleTimeoutOnSend = false,
            .sendPingsAutomatically = true,
            /* Handlers */   
            .open = [&latest_id](auto* ws) {
                /* Open event here, you may access ws->getUserData() which points to a PerSocketData struct */
                
                PerSocketData *data = ws->getUserData();
                data->user_id = latest_id++;

                cout << "User " <<data->user_id<<"has connected" << endl;
  
                ws->publish(BROADCAST, status(data, true));

                ws->subscribe("broadcast");//сообщения получают все пользователи
                ws->subscribe("UserN"+to_string(data->user_id));// личный канал

                

                for (auto entry : activeUsers)
                {
                    ws->send(status(entry.second, true), uWS::OpCode::TEXT);
                }

                activeUsers[data->user_id] = data;


            },
            .message = [](auto* ws, std::string_view message, uWS::OpCode opCode) {
               // ws->send(message, opCode, true);
                PerSocketData* data = ws->getUserData();
                cout << "Message from N "<<data->user_id <<": " <<message << endl;
 
                processMessage(ws, message);
            },
            
            .close = [](auto* ws, int /*code*/, std::string_view /*message*/) {
                /* You may access ws->getUserData() here */
                 PerSocketData* data = ws->getUserData();

                  cout << "close" << endl;
                  ws->publish(BROADCAST, status(data, false ));

                  activeUsers.erase(data->user_id); // удалили из карты

            }
            }).listen(9001, [](auto* listen_socket) { //9001 - порт
                if (listen_socket) {
                    std::cout << "Listening on port " << 9001 << std::endl;
                }
            }).run();
}
