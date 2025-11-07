#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint>
#include <cstring>

// Message types - šta klijent/server šalju
enum UserStatus: uint8_t {
    STATUS_ACTIVE = 0,
    STATUS_AWAY = 1,
    STATUS_BUSY = 2
};
enum MessageType : uint8_t {
    MSG_LOGIN = 1,           // Klijent -> Server: Login request
    MSG_LOGIN_SUCCESS = 2,   // Server -> Klijent: Login OK
    MSG_LOGIN_FAIL = 3,      // Server -> Klijent: Username zauzet

    MSG_BROADCAST = 4,       // Klijent -> Server: Poruka za sve
    MSG_PRIVATE = 5,         // Klijent -> Server: Privatna poruka

    MSG_USER_LIST_REQ = 6,   // Klijent -> Server: Traži listu korisnika
    MSG_USER_LIST = 7,       // Server -> Klijent: Lista korisnika

    MSG_HISTORY_REQ = 8,     // Klijent -> Server: Traži history
    MSG_HISTORY = 9,         // Server -> Klijent: Šalje history

    MSG_LOGOUT = 10,         // Klijent -> Server: Izlazim

    MSG_TEXT = 11,           // Server -> Klijent: Neko ti je poslao poruku
    MSG_ERROR = 12,           // Server -> Klijent: Greška

    MSG_STATUS_CHANGE = 13,
    MSG_STATUS_NOTIFICATION = 14
};

// Glavni paket - sve poruke koriste ovu strukturu
struct Packet {
    uint8_t type;           // Tip poruke (iz enum-a gore)
    char sender[32];        // Ko šalje (username)
    char recipient[32];     // Kome (username ili "ALL" za broadcast)
    uint16_t payload_length; // Dužina payload-a
    char payload[512];      // Sadržaj poruke

    // Konstruktor - inicijalizuj na 0
    Packet() {
        memset(this, 0, sizeof(Packet));
        type = 0;
        payload_length = 0;
    }

    // Helper: postavi payload
    void set_payload(const char* data) {
        strncpy(payload, data, sizeof(payload) - 1);
        payload[sizeof(payload) - 1] = '\0';
        payload_length = strlen(payload);
    }
};

// Veličina paketa u bajtovima
const size_t PACKET_SIZE = sizeof(Packet);

#endif // PROTOCOL_H
