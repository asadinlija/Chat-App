#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <algorithm>
#include "protocol.h"
#pragma comment(lib, "ws2_32.lib")

using namespace std;

SOCKET client_socket;
bool running = true;

//helper: posalji paket
void send_packet(const Packet& packet) {
    send(client_socket, (char*)&packet, PACKET_SIZE, 0);
}
// helper funkcija da primi paket
bool recv_packet(Packet& packet) {
    int bytes = recv(client_socket, (char*)&packet, PACKET_SIZE, 0);
    return bytes == PACKET_SIZE;
}
//thread koji slusa poruke od servera
void receive_thread() {
    while (running) {
        Packet packet;
        if (!recv_packet(packet)) {
            cout << "\n[VEZA SA SERVEROM PREKINUTA]" << endl;
            running = false;
            break;
        }
        switch (packet.type) {
            case MSG_LOGIN_SUCCESS:
                cout << "[SERVER] " << packet.payload << endl;
                break;
            case MSG_LOGIN_FAIL:
                cout << "[ERROR] " << packet.payload << endl;
                running = false;
                break;
            case MSG_TEXT:
                if (strcmp(packet.recipient, "ALL") == 0) {
                    cout << "[" << packet.sender << "]: " << packet.payload << endl;
                } else {
                    cout << "[PRIVATNA PORUKA od " << packet.sender << "]: " << packet.payload << endl;
                    cout.flush();
                }
                break;
            case MSG_USER_LIST:
                cout << "[Online korisnici]:" << packet.payload << endl;
                break;
            case MSG_ERROR:
                cout << "[ERROR] " << packet.payload << endl;
                break;
            case MSG_HISTORY:
                cout << "\n----------------CHAT HISTORY----------------\n";
                cout << packet.payload << endl;
                cout << "----------------------------------------------\n";
                break;
            case MSG_STATUS_NOTIFICATION:
                cout << "[SERVER] " << packet.sender << " je sada " << packet.payload << endl;
                break;
            default:
                cout << "[Nepoznat tip poruke]" << endl;
        }
    }
}



int main() {
    // 0. INICIJALIZACIJA WINSOCKA
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed\n";
        return 1;
    }
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        cout << "Greska pri pravljenju socketa" << endl;
        WSACleanup();
        return -1;
    }
    //server adresa
    string server_ip;
    cout << "Unesi IP adresu servera (ENTER za localhost): ";
    getline(cin, server_ip);
    if (server_ip.empty())
        server_ip = "127.0.0.1";
    cout << "Konektujem se na " << server_ip << endl;

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);
//konektujemo se

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cout << "Connection failed. Da li je server pokrenut?" << endl;
        closesocket(client_socket);
        WSACleanup();
        return -1;
    }
    cout << "Povezan na server" << endl;
    //LOGIN
    string username;
    cout << "Unesi username: ";
    getline(cin, username);
    Packet login_paket;
    login_paket.type = MSG_LOGIN;
    strncpy(login_paket.sender, username.c_str(), sizeof(login_paket.sender)-1);
    send_packet(login_paket);
    thread reciever(receive_thread);
    //cekaj malo da stigne login_success ili fail
    this_thread::sleep_for(chrono::seconds(1));
    if (!running) {
        cout << "Login nije uspio. Exiting..." << endl;
        reciever.join();
        closesocket(client_socket);
        WSACleanup();
        return -1;
    }
    cout << "\nKomande:" << endl;
    cout << "  <poruka>           - Broadcast poruka, salje se svima" << endl;
    cout << "  /msg <user> <text>  - Privatna poruka" << endl;
    cout << "  /users              - Izlistaj online korisnike" << endl;
    cout << "  /history              -Pokazi historiju chata" << endl;
    cout << "  /status <status>      -Updateuj svoj status: active, away or busy" << endl;
    cout << "  /quit               - Exit" << endl;
    cout << endl;
    while (running) {
        string input;
        getline(cin, input);
        if (input.empty()) continue;
        if (input == "/quit") {
            Packet logout;
            logout.type = MSG_LOGOUT;
            send_packet(logout);
            running = false;
            break;
        }
        else if (input == "/users") {
            Packet req;
            req.type = MSG_USER_LIST_REQ;
            send_packet(req);
        }else if (input.substr(0,5) == "/msg ") {
            size_t first_space = input.find(' ', 5);
            if (first_space == string::npos) {
                cout << "koristi /msg <username> <message> " << endl;
                continue;
            }
            string recipient = input.substr(5, first_space-5);
            string message = input.substr(first_space+1);
            Packet privatna_poruka;
            privatna_poruka.type = MSG_PRIVATE;
            strncpy(privatna_poruka.sender, username.c_str(), sizeof(privatna_poruka.sender) - 1);
            strncpy(privatna_poruka.recipient, recipient.c_str(), sizeof(privatna_poruka.recipient) - 1);
            strncpy(privatna_poruka.payload, message.c_str(), sizeof(privatna_poruka.payload) - 1);
            send_packet(privatna_poruka);
        }else if (input == "/history") {
            Packet req;
            req.type = MSG_HISTORY_REQ;
            send_packet(req);

        }else if (input.substr(0,8) == "/status ") {
            string status = input.substr(8);

            transform(status.begin(), status.end(), status.begin(), ::tolower);

            // Validacija
            if (status != "active" && status != "away" && status != "busy") {
                cout << "status moze biti <active|away|busy>" << endl;
                continue;
            }

            Packet status_paket;
            status_paket.type = MSG_STATUS_CHANGE;
            strncpy(status_paket.sender, username.c_str(), sizeof(status_paket.sender) - 1);
            strncpy(status_paket.payload, status.c_str(), sizeof(status_paket.payload) - 1);
            send_packet(status_paket);
        }
        else {
            Packet broadcast;
            broadcast.type = MSG_BROADCAST;
            strncpy(broadcast.sender, username.c_str(), sizeof(broadcast.sender) - 1);
            strncpy(broadcast.payload, input.c_str(), sizeof(broadcast.payload) - 1);
            send_packet(broadcast);
        }
    }
    reciever.join();
    closesocket(client_socket);
    WSACleanup();
    cout << "Diskonektovan sa servera" << endl;



    return 0;
}

