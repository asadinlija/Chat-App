#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <thread>
#include <vector>  //pravimo server sa threadovima
#include <mutex>
#include <map>
#include <string>
#include <fstream>
#include <ctime>
#include <algorithm>
// Linkujem Winsock biblioteku
#pragma comment(lib, "ws2_32.lib")
#include "protocol.h"

using namespace std;

// Mapa: username -> socket (ko je trenutno online)
map <string, SOCKET> online_users;
// Mapa: username -> status (active/away/busy)
map <string, UserStatus> users_status;
// Mutexi za zaštitu zajedničkih struktura pri više threadova
mutex status_mutex;
mutex users_mutex;
mutex history_mutex;
// Fajl u koji se snima historija chata
const string HISTORY_FILE = "chat_history.txt";

// Funkcija koja snima poruku u historiju chata (u fajl).
// Format u fajlu: timestap|sender|recipient|message

void save_history(const string& sender, const string& recipient, const string& message) {
    lock_guard<mutex> lock(history_mutex);
    ofstream file(HISTORY_FILE, ios::app);
    if (file.is_open()) {
        time_t now = time(nullptr);
        file << now << "|" << sender << "|" << recipient << "|" << message << endl;
        file.close();
    }else {
        cout << "[ERROR]: Ne mogu otvoriti history fajl!" << endl;
    }
}
// Funkcija koja učitava zadnjih max_lines poruka iz historije

string load_history(int max_lines=50) {
    lock_guard<mutex> lock(history_mutex);
    ifstream file(HISTORY_FILE);
    if (!file.is_open()) {
        return "Historija chata nije dostupna";
    }
    vector<string> lines;
    string line;
    // Učitavamo sve linije iz fajla u vektor
    while (getline(file, line)) {
        lines.push_back(line);
    }
    file.close();
    // Određujemo od koje linije da počnemo
    int start = max(0, (int)lines.size()-max_lines);
    string result;

    for (int i=start; i<lines.size(); i++) {
        size_t pos1 = lines[i].find('|');
        size_t pos2 = lines[i].find('|', pos1+1);
        size_t pos3 = lines[i].find('|', pos2+1);

        if (pos1 == string::npos || pos2 == string::npos || pos3 == string::npos) {
            continue;  // Loš format linije
        }
        string timestamp_str = lines[i].substr(0, pos1);
        string sender = lines[i].substr(pos1+1, pos2-pos1-1);
        string recipient = lines[i].substr(pos2+1, pos3-pos2-1);
        string message = lines[i].substr(pos3+1);

        // Pretvaranje timestampa u HH:MM:SS format
        time_t timestamp = stoll(timestamp_str);
        char time_buf[20];
        strftime(time_buf, sizeof(time_buf), "%H:%M:%S", localtime(&timestamp));
        if (recipient == "ALL")
            result += "[" + string(time_buf) + "]" + sender + ":" + message + "\n";
        else
            result += "[" + string(time_buf) + "]" + sender + " ->" + recipient + ":" + message + "\n";
    }
    return result.empty() ? "Nema jos nikakvih poruka" : result;
}


// Helper funkcija za slanje Packet-a na dati socket

void send_packet(SOCKET sock, const Packet& paket) {
    send(sock, (char*)&paket, PACKET_SIZE, 0);
}
// Helper funkcija za primanje Packet-a sa datog socketa

bool recv_packet(SOCKET sock, Packet& paket) {
    int bytes = recv(sock, (char*)&paket, PACKET_SIZE, 0);
    return bytes == PACKET_SIZE;
}

void handle_client(SOCKET client_socket) {

    cout << "\nNova konekcija, cekam LOGIN..." << endl;
    string username;
    bool logged_in = false;
    while (true) {
        Packet paket;
        if (!recv_packet(client_socket, paket)) {
            cout << "Klijent diskonektovan" << endl;
            break;
        }
        switch (paket.type) {
            case MSG_LOGIN: {
                username = paket.sender;
                //provjerim da li je username vec zauzet
                users_mutex.lock();
                if (online_users.find(username) != online_users.end()) {
                    users_mutex.unlock();
                    Packet response;
                    response.type = MSG_LOGIN_FAIL;
                    strcpy(response.payload, "Username je vec zauzet");
                    send_packet(client_socket, response);
                    closesocket(client_socket);
                    return;
                }
                // username je slobodan pa dodamo korisnika
                online_users[username] = client_socket;
                users_mutex.unlock();
                logged_in = true;

                cout << username << " se ulogovao" << endl;

                Packet response;
                response.type = MSG_LOGIN_SUCCESS;
                strcpy(response.payload, "Dobrodosli u chat!");
                send_packet(client_socket, response);

                Packet notifikacija;
                notifikacija.type = MSG_TEXT;
                strcpy(notifikacija.sender, "SERVER");
                strcpy(notifikacija.recipient, "ALL");
                string msg = username + " se pridruzio razgovoru";
                users_mutex.lock();
                for (auto& [user, sock]: online_users) {
                    if (user != username)
                        send_packet(sock, notifikacija);

                }
                users_status[username] = STATUS_ACTIVE;
                users_mutex.unlock();
                break;
            }
            case MSG_BROADCAST: {
            if (!logged_in) break;

                cout << username << " (broadcast): " << paket.payload << endl;
                save_history(username, "ALL", paket.payload);

                Packet msg;
                msg.type = MSG_TEXT;
                strcpy(msg.sender, username.c_str());
                strcpy(msg.recipient, "ALL");
                strcpy(msg.payload, paket.payload);

                users_mutex.lock();
                for (auto& [user, sock]: online_users) {
                    if (user != username) {
                        send_packet(sock, msg);
                    }
                }


                users_mutex.unlock();
                break;

            }
            case MSG_USER_LIST_REQ: {
                if (!logged_in) break;

                // Pravimo listu korisnika SA statusom
                string user_list;
                users_mutex.lock();
                status_mutex.lock();

                for (auto& [user, sock] : online_users) {
                    user_list += user;

                    // Dodaj status
                    UserStatus status = users_status[user];
                    if (status == STATUS_AWAY) {
                        user_list += " (away)";
                    } else if (status == STATUS_BUSY) {
                        user_list += " (busy)";
                    } else {
                        user_list += " (active)";
                    }

                    user_list += ", ";
                }

                status_mutex.unlock();
                users_mutex.unlock();

                if (!user_list.empty()) {
                    user_list.pop_back(); // Ukloni zadnji razmak
                    user_list.pop_back(); // Ukloni zadnji zarez
                }

                Packet response;
                response.type = MSG_USER_LIST;
                strcpy(response.payload, user_list.c_str());
                send_packet(client_socket, response);

                break;
            }
            case MSG_LOGOUT: {
                cout << username << " se odlogovao" << endl;
                goto cleanup;
            }
            case MSG_PRIVATE: {
                if (!logged_in) break;

                string recipient = paket.recipient;
                cout << username << " -> " << recipient << ": " << paket.payload << endl;

                save_history(username, recipient, paket.payload);

                // Nadji primaoca
                users_mutex.lock();
                auto it = online_users.find(recipient);
                if (it != online_users.end()) {
                    Packet msg;
                    msg.type = MSG_TEXT;
                    strcpy(msg.sender, username.c_str());
                    strcpy(msg.recipient, recipient.c_str());
                    strcpy(msg.payload, paket.payload);
                    send_packet(it->second, msg);
                } else {
                    // Korisnik nije online
                    Packet error;
                    error.type = MSG_ERROR;
                    strcpy(error.payload, "User not found or offline");
                    send_packet(client_socket, error);
                }
                users_mutex.unlock();

                break;
            }
            case MSG_HISTORY_REQ: {
                if (!logged_in) break;
                 cout << username << " user je zatrazio historiju chata"  << endl;
                string history = load_history(50);
                Packet response;
                response.type = MSG_HISTORY;
                strncpy(response.payload, history.c_str(), sizeof(response.payload)-1);
                send_packet(client_socket, response);
                break;
            }
            case MSG_STATUS_CHANGE: {
                if (!logged_in) break;
                string status_str = paket.payload;
                UserStatus new_status;

                if (status_str == "active") {
                    new_status = STATUS_ACTIVE;
                }else if (status_str == "away") {
                    new_status = STATUS_AWAY;
                }else if (status_str == "busy") {
                    new_status = STATUS_BUSY;
                }else {
                    Packet error;
                    error.type = MSG_ERROR;
                    strcpy(error.payload, "Invalidan status. Koristi: active, away, busy");
                    send_packet(client_socket, error);
                    break;
                }
                status_mutex.lock();
                users_status[username] = new_status;
                status_mutex.unlock();

                cout << username << " userov status je promijenjen na " << status_str << endl;

                //obavijestim korisnika
                Packet response;
                response.type = MSG_TEXT;
                strcpy(response.sender, "SERVER");
                string msg = "Vas status sada je: " + status_str;
                strcpy(response.payload, msg.c_str());
                send_packet(client_socket, response);

                //obavijestimo ostale korisnike
                Packet notifikacija;
                notifikacija.type = MSG_STATUS_NOTIFICATION;
                strcpy(notifikacija.sender, username.c_str());
                strcpy(notifikacija.payload, status_str.c_str());

                users_mutex.lock();
                for (auto& [user, sock]: online_users) {
                    if (user != username) {
                        send_packet(sock, notifikacija);
                    }
                }
                users_mutex.unlock();
                break;
            }
            default:
                cout << "Nepoznat tip poruke: " << (int)paket.type << endl;
                break;
            }
    }
    cleanup:
    if (logged_in) {
        users_mutex.lock();
        online_users.erase(username);
        users_mutex.unlock();
        Packet notifikacija;
        notifikacija.type = MSG_TEXT;
        strcpy(notifikacija.sender, "SERVER");
        string msg = username + " user je napustio razgovor";
        strcpy(notifikacija.payload, msg.c_str());
        users_mutex.lock();
        for (auto& [user, sock]: online_users) {
            send_packet(sock, notifikacija);
        }
        users_mutex.unlock();

        status_mutex.lock();
        users_status.erase(username);
        status_mutex.unlock();
    }
    closesocket(client_socket);
    cout << "Socket zatvoren za " << username << endl;




}

int main() {
    // Inicijalizacija WINSOCKa
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed\n";
        return 1;
    }
    cout << "Winsock inicijalizovan\n";

    //Prvo, napravimo socket
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        cout << "Greska u pravljenju socketa: " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }
    cout << "Socket kreiran" << endl;


    // Definišimo adresu i port
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // Bind socket na port
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cout << "Greska u bindingu socketa: " << WSAGetLastError() << "\n";
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }


    // Počnemo sa slušanjem
    if (listen(server_socket, 5) == SOCKET_ERROR) {
        cout << "Greska pri slusanju: " << WSAGetLastError() << "\n";
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }
    cout << "Server slusa na portu 8080...\n";

    // Prihvatimo konekciju
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);

    while (true) {
        struct sockaddr_in client_addr;
        int client_len = sizeof(client_addr);
        SOCKET client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
        if (client_socket == INVALID_SOCKET) {
            cout << "Prihvatanje neuspjelo: " << WSAGetLastError() << endl;
            continue;
        }
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr.s_addr, client_ip, INET_ADDRSTRLEN);
        cout << "Client se konektovao sa: " << client_ip << endl;
        thread client_thread(handle_client, client_socket);
        client_thread.detach();
        cout << "Thread napravljen za klijenta" << endl;
    }

    closesocket(server_socket);
    WSACleanup();  // Cleanup Winsock

    return 0;
}