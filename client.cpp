#include <iostream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <string>
#include <thread>
#include <algorithm>
#include "protocol.h"
#pragma comment(lib, "ws2_32.lib")

using namespace std;

//globalni socket za klijenta
SOCKET client_socket;
//flag sa kojim zaustavljamo glavnu petlju i thread za primanje
bool running = true;

bool send_full(SOCKET s, const char* buffer, int length) {
    int total = 0;
    while (total < length) {
        int bytes = send(s, buffer + total, length - total, 0);
        if (bytes <= 0) {
            return false; // connection closed ili greška
        }
        total += bytes;
    }
    return true;
}

bool recv_full(SOCKET s, char* buffer, int length) {
    int total = 0;
    while (total < length) {
        int bytes = recv(s, buffer + total, length - total, 0);
        if (bytes <= 0) {
            return false; // connection closed ili greška
        }
        total += bytes;
    }
    return true;
}

// helper: pošalji paket
bool send_packet(const Packet& packet) {
    return send_full(client_socket,
                     reinterpret_cast<const char*>(&packet),
                     static_cast<int>(PACKET_SIZE));
}

// helper: primi paket
bool recv_packet(Packet& packet) {
    return recv_full(client_socket,
                     reinterpret_cast<char*>(&packet),
                     static_cast<int>(PACKET_SIZE));
}
/*Thread funkcija koja stalno sluša poruke od servera.
Dok je running == true, pokušava čitati pakete i ispisivati ih na ekran
u zavisnosti od tipa poruke.
*/
void receive_thread() {
    while (running) {
        Packet packet;
        if (!recv_packet(packet)) {
            cout << "\n[VEZA SA SERVEROM PREKINUTA]" << endl;
            running = false;
            break;
        }
        // Obrada poruke prema tipu
        switch (packet.type) {
            case MSG_LOGIN_SUCCESS:
                cout << "[SERVER] " << packet.payload << endl;
                break;
            case MSG_LOGIN_FAIL:
                cout << "[ERROR] " << packet.payload << endl;
                // Ako login ne uspije prekidamo klijenta
                running = false;
                break;
            case MSG_TEXT:
                // Ako je recipient ALL, onda se radi o broadcast poruci
                if (strcmp(packet.recipient, "ALL") == 0) {
                    cout << "[" << packet.sender << "]: " << packet.payload << endl;
                } else {
                    // inače je privatna poruka
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
                // Server javlja da je neki korisnik promijenio status
                cout << "[SERVER] " << packet.sender << " je sada " << packet.payload << endl;
                break;
            default:
                cout << "[Nepoznat tip poruke]" << endl;
        }
    }
}



int main() {
    // INICIJALIZACIJA WINSOCKA
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed\n";
        return 1;
    }
    // Kreiranje TCP socketa
    client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        cout << "Greska pri pravljenju socketa" << endl;
        WSACleanup();
        return -1;
    }

    string server_ip;
    cout << "Unesi IP adresu servera (ENTER za localhost): ";
    getline(cin, server_ip);
    if (server_ip.empty())
        server_ip = "127.0.0.1";
    cout << "Konektujem se na " << server_ip << endl;

    // Podešavanje server adrese
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(8080);
    inet_pton(AF_INET, server_ip.c_str(), &server_addr.sin_addr);
    // Pokušaj konekcije na server

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
    // Pokrećemo thread koji prima poruke od servera
    thread reciever(receive_thread);
    //čekamo malo da stigne LOGIN_SUCCESS ili FAIL
    this_thread::sleep_for(chrono::seconds(1));
    if (!running) {
        cout << "Login nije uspio. Exiting..." << endl;
        reciever.join();
        closesocket(client_socket);
        WSACleanup();
        return -1;
    }
    // Ispis dostupnih komandi
    cout << "\nKomande:" << endl;
    cout << "  <poruka>           - Broadcast poruka, salje se svima" << endl;
    cout << "  /msg <user> <text>  - Privatna poruka" << endl;
    cout << "  /users              - Izlistaj online korisnike" << endl;
    cout << "  /history              -Pokazi historiju chata" << endl;
    cout << "  /status <status>      -Updateuj svoj status: active, away or busy" << endl;
    cout << "  /quit               - Exit" << endl;
    cout << endl;

    //Glavna petlja klijenta: čita unos sa tastature i šalje odgovarajuće pakete
    while (running) {
        string input;
        getline(cin, input);
        if (input.empty()) continue;
        if (input == "/quit") {
            // odjava korisnika
            Packet logout;
            logout.type = MSG_LOGOUT;
            send_packet(logout);
            running = false;
            break;
        }
        else if (input == "/users") {
            // zahtjev za listu online korisnika
            Packet req;
            req.type = MSG_USER_LIST_REQ;
            send_packet(req);
        }else if (input.substr(0,5) == "/msg ") {
            //  privatna poruka: /msg <username> <tekst>
            size_t first_space = input.find(' ', 5);
            if (first_space == string::npos) {
                cout << "Koristi /msg <username> <message> " << endl;
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
            // Zahtjev za historiju chata
            Packet req;
            req.type = MSG_HISTORY_REQ;
            send_packet(req);

        }else if (input.substr(0,8) == "/status ") {
            // Promjena statusa korisnika
            string status = input.substr(8);

            // Pretvaranje u lowercase
            transform(status.begin(), status.end(), status.begin(), ::tolower);

            // Validacija statusa
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
            // Sve ostalo tretiramo kao broadcast poruku
            Packet broadcast;
            broadcast.type = MSG_BROADCAST;
            strncpy(broadcast.sender, username.c_str(), sizeof(broadcast.sender) - 1);
            strncpy(broadcast.payload, input.c_str(), sizeof(broadcast.payload) - 1);
            send_packet(broadcast);
        }
    }
    // Čekamo da se thread za primanje poruka završi
    reciever.join();
    // Zatvaramo socket i čistimo Winsock
    closesocket(client_socket);
    WSACleanup();
    cout << "Diskonektovan sa servera" << endl;



    return 0;
}

