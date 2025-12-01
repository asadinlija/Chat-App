# TCP Chat Aplikacija
Multi-threaded chat aplikacija sa client-server arhitekturom. Implementirana u C++ koristeći TCP socket programiranje.


Projekat za predmet: Mrežno programiranje

Datum: 28.11.2025.

---
## Opis projekta

Chat aplikacija koja omogućava korisnicima da komuniciraju u realnom vremenu preko TCP protokola. Server podržava istovremene konekcije koristeći multi-threading, a klijenti mogu slati broadcast i privatne poruke.

---
## Funkcionalnosti

### Osnovne funkcionalnosti
- **Multi-client podrška** - Server handluje više klijenata istovremeno koristeći threadove
- **User identifikacija** - Login sistem sa jedinstvenim username-ovima
- **Broadcast poruke** - Slanje poruka svim korisnicima
- **Private poruke** - Slanje privatnih poruka korisnicima, 1 na 1 komunikacija
- **User lista** - Pregled online korisnika
- **Historija poruka** - Čuvanje historije poruka u tekstualni fajl
- **User status** - Postavljanje statusa korisnika (active/away/busy)
- **Quit komanda** - Odjavljivanje korisnika iz chata

### Napredne funkcionalnosti
- **Thread-safe operacije** - Mutex za pristup dijeljenim podacima
- **Graceful disconnect** - Pravilno handleovanje izgubljenih konekcija
- **Join/Leave notifikacije** - Real-time notifikacije kada korisnici ulaze/izlaze

---

## Tehnologije
- **Jezik:** C++17
- **Socket API:** Winsock2 (Windows)
- **Threading:** C++ std::thread
- **Sinhronizacija:** std::mutex, std::lock_guard
- **File I/O:** C++ fstream (za chat historiju)
- **Build System:** CMake

## Struktura projekta


```text
mrezno_programiranje/
├── CMakeLists.txt
├── protocol.h
├── server.cpp
├── client.cpp
├── chat_history.txt
└── README.md
```
## Kompajliranje

### Preduslovi:
- C++ compiler (g++, MSVC, MinGW)
- CMake (verzija 4.0+)
- Windows OS (za Winsock2)

### Build instrukcije:

1. Kreirati build folder
```
mkdir build
cd build
```
2. Generisati build fajlove
```
cmake ..
```
3. Kompajlirati
```
cmake --build .
```

Ili ako se koristi MinGW bez CMake-a
```
g++ ../server.cpp -o server -lws2_32
g++ ../client.cpp -o klijent -lws2_32
```


## Pokretanje

Pokreni server:

```
./server
```

Pokreni klijent:

```
./klijent
```

**Input:**

```
Unesi IP adresu servera (ENTER za localhost): 
Unesi username: alice
```
---
 ## Komande
 
Dostupne komande u chatu:

| Komanda          | Opis                                        |
|------------------|---------------------------------------------|
| `tekst_poruke`   | Pošalji poruku svim korisnicima (broadcast) |
| `/msg user text` | Pošalji privatnu poruku korisniku           |
| `/users`         | Prikaži listu online korisnika              |
| `/status status` | Postavi status (active/away/busy)           |
| `/history`       | Prikaži prethodne poruke                    |
| `/quit`          | Izađi iz chata                              |

---

## Testiranje na lokalnoj mreži

### Korak 1: Naći IP adresu server računara

U CMD ukucati:
```
ipconfig
```

Primjer outputa:

IPv4 Address. . . . . . . . . . . : 192.168.1.108

### Korak 2: Omogućiti port u firewall-u (8080)

### Korak 3: Pokrenuti server na jednom računaru

### Korak 4: Klijenti se konektuju sa drugih računara 

```
./klijent

Unesi IP adresu servera (ENTER za localhost): 192.168.1.108    # IP adresa server računara
```

---

## Arhitektura sistema

Sistem je zasnovan na client-server arhitekturi preko TCP protokola.

Jedan centralni server održava listu aktivnih korisnika, njihove statuse i historiju poruka, dok se više klijenata istovremeno spaja na taj server i razmjenjuje poruke u realnom vremenu.

Glavne komponente su:
 - Server (server.cpp) - obrađuje sve klijente, poruke, statuse i historiju
 - Klijent (client.cpp) - omogućava korisniku da se spoji na server, šalje komande i prima poruke
 - Protokol (protocol.h) - zajedničke definicije strukture Packet i tipova poruka

### Server
Server je implementiran kao multi-threaded TCP server. Koristi:
 - koristi Winsock2 za mrežnu komunikaciju
 - sluša na portu 8080 na svim interfejsima (INADDR_ANY)
 - za svakog novog klijenta poziva accept() i pokreće poseban thread funkcije handle_client
 - koristi globalne mape za stanje sistema:
    - map<string, SOCKET> online_users (aktivni korisnici i njihovi socketi) 
    - map<string, UserStatus> users_status (status svakog korisnika)
- koristi std::mutex za zaštitu:
    - pristupa listi korisnika
    - pristupa statusima
    - pristupa fajlu sa historijom
Glavne odgovornosti servera:
    - obrada login zahtjeva i provjera zauzetosti username-a
    - slanje broadcast poruka svim korisnicima osim pošiljaoca
    - slanje privatnih poruka ciljnom korisniku
    - vraćanje liste korisnika sa statusima
    - čuvanje i slanje historije chata iz fajla chat_history.txt
    - ažuriranje i broadcast statusa korisnika (active/away/busy)
    - slanje join/leave notifikacija ostalim korisnicima kada se neko uloguje/odloguje

---

### Klijent
Klijent je konzolna aplikacija koja:
 - uspostavlja TCP konekciju ka serveru
 - od korisnika traži username i šalje MSG_LOGIN paket serveru
 - pokreće poseban thread za prijem poruka (receive_thread), koji kontinuirano prima pakete od servera i ispisuje ih na ekran
 - u glavnoj niti čita korisnički unos i prevodi tekst u odgovarajuće pakete
Podržane komande u klijentu:
 - bilo koja poruka bez prefiksa / - broadcast poruka svima (tip MSG_BROADCAST)
 - /msg <user> <tekst> - privatna poruka (tip MSG_PRIVATE)
 - /users - zahtjev za listu korisnika (tip MSG_USER_LIST_REQ)
 - /history - zahtjev za historiju (tip MSG_HISTORY_REQ)
 - /status <active|away|busy> - promjena statusa (tip MSG_STATUS_CHANGE)
 - /quit - odjava (tip MSG_LOGOUT)

---

### Tok komunikacije

1. Pokretanje servera
   - Server inicijalizuje Winsock, kreira socket, binduje se na port 8080 i poziva listen()
2. Priključivanje klijenta
   - Klijent kreira socket, poziva connect() ka serveru i šalje MSG_LOGIN sa zadanim username-om
3. Login
   - Server provjerava da li username već postoji u online_users
   - Ako postoji, šalje MSG_LOGIN_FAIL i prekida konekciju
   - Ako nije zauzet, dodaje korisnika u mapu, postavlja status na STATUS_ACTIVE i šalje MSG_LOGIN_SUCCESS, te broadcastuje ostalim da se novi korisnik pridružio
4. Razmjena poruka
   - Broadcast poruke se šalju svim ostalim korisnicima kao MSG_TEXT sa recipient = ALL
   - Privatne poruke se šalju samo ciljnom korisniku sa recipient = <username>
   - Sve poruke se snimaju u fajl historije preko save_history
5. Status i lista korisnika
   - /status ... šalje MSG_STATUS_CHANGE
6. Historija
   - /history - klijent šalje MSG_HISTORY_REQ, server učitava zadnjih 50 poruka iz chat_history.txt i vraća ih kao jedan MSG_HISTORY paket.
7. Odjava
   - /quit šalje MSG_LOGOUT. Server uklanja korisnika iz online_users i users_status, te šalje ostalima notifikaciju da je korisnik napustio chat.
   - Ako konekcija pukne, server detektuje recv fail i radi isto čišćenje u cleanup dijelu


### Dizajn protokola

#### Osnovna struktura paketa
Sav saobraćaj između klijenta i servera ide kroz fiksnu strukturu Packet definisanu u protocol.h
```
struct Packet {
uint8_t type;           // Tip poruke 
char sender[32];        // Ko šalje (username)
char recipient[32];     // Kome (username ili "ALL" za broadcast)
uint16_t payload_length; // Dužina payload-a
char payload[512];      // Sadržaj poruke
};
```

#### Ključne ideje dizajna
 - Fiksna veličina (PACKET_SIZE = sizeof(Packet)) - pojednostavljuje send/recv, tj. uvijek šaljemo/primamo istu količinu bajtova
 - Polja sender i recipient uvijek putuju unutar paketa, pa server ne mora dodatno mapirati (socket, username) pri broadcastu.
 - payload služi kao univerzalni tekstualni sadržaj: poruka, lista korisnika, historija, opis greške, novi status itd.

### Tipovi poruka:

- MSG_LOGIN - Login request (klijent -> server)
- MSG_LOGIN_SUCCESS - Potvrda da je login uspio (server -> klijent)
- MSG_LOGIN_FAIL - vraća poruku greške kada je username već u upotrebi (server -> klijent)
- MSG_BROADCAST - Broadcast poruka (klijent -> server)
- MSG_PRIVATE - Privatna poruka (klijent -> server)
- MSG_USER_LIST_REQ - Zahtjev za listu korisnika (klijent -> server)
- MSG_USER_LIST - payload sadrži listu korisnika i njihovih statusa (server -> klijent)
- MSG_HISTORY - payload sadrži formatiranu historiju (server -> klijent)
- MSG_HISTORY_REQ - Zahtjev za historiju chata (klijent -> server)
- MSG_STATUS_CHANGE - Promjena statusa (klijent -> server)
- MSG_LOGOUT - signal da se korisnik svjesno odjavljuje (klijent -> server)
- MSG_TEXT - opšta tekstualna poruka prema klijentima (server -> klijent)
- MSG_ERROR - sadrži opis greške (server -> klijent)
- MSG_STATUS_NOTIFICATION - server obavješava sve ostale klijente da je određeni korisnik promijenio status. (server -> klijent)

---

## Problemi koji se mogu desiti i rješenja

### Problem: "Connection failed. Da li je server pokrenut?"
**Uzrok:** Server nije pokrenut ili radi na drugom portu.

**Rješenje:** 
- Pokrenuti server.exe prije klijenta.
- Provjeriti da port 8080 nije zauzet od drugog procesa
- Provjeriti firewall (privremeno ga isključiti ili dozvoliti aplikaciju).


### Problem: Server javlja: "Error binding socket: 10048"
**Uzrok:** Port 8080 je već zauzet.

**Rješenje:** U CMD upisati: 
```
netstat -ano | findstr :8080
```
Pronaći PID procesa i ugasiti ga

```
taskkill /PID <pid> /F
```
---

### Principi Mrežnog programiranja koji su demonstrirani

Projekat demonstrira sljedeće principe:
1. **Socket programiranje** - kreiranje i upravljanje mrežnim socketima (socket, bind, listen, accept, connect)
2. **TCP protokol** - pouzdani protokol komunikacije, orijentisan na konekciju koji obezbjeđuje redoslijed, potvrde i isporuku podataka bez gubitaka
3. **Client-Server arhitektura** - centralizovani server model
4. **Konkurentnost** - server može istovremeno obrađivati više klijenata korištenjem multi-threadinga gdje svaki klijent dobija sopstvenu nit izvršavanja
5. **Sinhronizacija** - std::mutex i std::lock_guard osiguravaju thread-safe pristup zajedničkim strukturama (lista korisnika, statusi, historija)

### Autor

Amina Šadinlija

email: asadinlija@gmail.com