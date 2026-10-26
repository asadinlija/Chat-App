# TCP Chat Application
A multi-threaded chat application with a client-server architecture. Implemented in C++ using TCP socket programming.


Course project: Network Programming

Date: 28.11.2025.

---
## Project Description

A chat application that allows users to communicate in real time over the TCP protocol. The server supports simultaneous connections using multi-threading, and clients can send broadcast and private messages.

---
## Features

### Core Features
- **Multi-client support** - The server handles multiple clients simultaneously using threads
- **User identification** - Login system with unique usernames
- **Broadcast messages** - Sending messages to all users
- **Private messages** - Sending private messages to users, 1-on-1 communication
- **User list** - Overview of online users
- **Message history** - Storing message history in a text file
- **User status** - Setting the user's status (active/away/busy)
- **Quit command** - Logging a user out of the chat

### Advanced Features
- **Thread-safe operations** - Mutexes for access to shared data
- **Graceful disconnect** - Proper handling of lost connections
- **Join/Leave notifications** - Real-time notifications when users join/leave

---

## Technologies
- **Language:** C++17
- **Socket API:** Winsock2 (Windows)
- **Threading:** C++ std::thread
- **Synchronization:** std::mutex, std::lock_guard
- **File I/O:** C++ fstream (for chat history)
- **Build System:** CMake

## Project Structure


```text
mrezno_programiranje/
├── CMakeLists.txt
├── protocol.h
├── server.cpp
├── client.cpp
├── chat_history.txt
└── README.md
```
## Compilation

### Prerequisites:
- C++ compiler (g++, MSVC, MinGW)
- CMake (version 4.0+)
- Windows OS (for Winsock2)

### Build instructions:

1. Create a build folder
```
mkdir build
cd build
```
2. Generate build files
```
cmake ..
```
3. Compile
```
cmake --build .
```

Or, if using MinGW without CMake:
```
g++ ../server.cpp -o server -lws2_32
g++ ../client.cpp -o klijent -lws2_32
```


## Running

Start the server:

```
./server
```

Start the client:

```
./klijent
```

**Input:**

```
Unesi IP adresu servera (ENTER za localhost): 
Unesi username: alice
```
(The prompts are printed by the program in Bosnian: "Enter the server IP address (ENTER for localhost)" and "Enter username".)

---
 ## Commands
 
Available commands in the chat:

| Command          | Description                                 |
|------------------|---------------------------------------------|
| `message_text`   | Send a message to all users (broadcast)     |
| `/msg user text` | Send a private message to a user            |
| `/users`         | Show the list of online users               |
| `/status status` | Set status (active/away/busy)               |
| `/history`       | Show previous messages                      |
| `/quit`          | Leave the chat                              |

---

## Testing on a Local Network

### Step 1: Find the IP address of the server computer

In CMD, type:
```
ipconfig
```

Example output:

IPv4 Address. . . . . . . . . . . : 192.168.1.108

### Step 2: Allow the port in the firewall (8080)

### Step 3: Start the server on one computer

### Step 4: Clients connect from other computers

```
./klijent

Unesi IP adresu servera (ENTER za localhost): 192.168.1.108    # IP address of the server computer
```

---

## System Architecture

The system is based on a client-server architecture over the TCP protocol.

A single central server maintains the list of active users, their statuses, and the message history, while multiple clients connect to that server simultaneously and exchange messages in real time.

The main components are:
 - Server (server.cpp) - handles all clients, messages, statuses, and history
 - Client (client.cpp) - lets the user connect to the server, send commands, and receive messages
 - Protocol (protocol.h) - shared definitions of the Packet structure and message types

### Server
The server is implemented as a multi-threaded TCP server. It:
 - uses Winsock2 for network communication
 - listens on port 8080 on all interfaces (INADDR_ANY)
 - calls accept() for each new client and starts a separate thread running the handle_client function
 - uses global maps for system state:
    - map<string, SOCKET> online_users (active users and their sockets)
    - map<string, UserStatus> users_status (status of each user)
- uses std::mutex to protect:
    - access to the user list
    - access to the statuses
    - access to the history file

Main responsibilities of the server:
    - processing login requests and checking whether a username is taken
    - sending broadcast messages to all users except the sender
    - sending private messages to the target user
    - returning the list of users with their statuses
    - storing and sending the chat history from the chat_history.txt file
    - updating and broadcasting user statuses (active/away/busy)
    - sending join/leave notifications to other users when someone logs in/out

---

### Client
The client is a console application that:
 - establishes a TCP connection to the server
 - asks the user for a username and sends a MSG_LOGIN packet to the server
 - starts a separate thread for receiving messages (receive_thread), which continuously receives packets from the server and prints them to the screen
 - reads user input in the main thread and translates the text into the appropriate packets

Commands supported by the client:
 - any message without the / prefix - broadcast message to everyone (type MSG_BROADCAST)
 - /msg <user> <text> - private message (type MSG_PRIVATE)
 - /users - request for the user list (type MSG_USER_LIST_REQ)
 - /history - request for the history (type MSG_HISTORY_REQ)
 - /status <active|away|busy> - status change (type MSG_STATUS_CHANGE)
 - /quit - logout (type MSG_LOGOUT)

---

### Communication Flow

1. Starting the server
   - The server initializes Winsock, creates a socket, binds to port 8080, and calls listen()
2. Client connection
   - The client creates a socket, calls connect() to the server, and sends MSG_LOGIN with the chosen username
3. Login
   - The server checks whether the username already exists in online_users
   - If it exists, it sends MSG_LOGIN_FAIL and closes the connection
   - If it is not taken, it adds the user to the map, sets the status to STATUS_ACTIVE, sends MSG_LOGIN_SUCCESS, and broadcasts to the others that a new user has joined
4. Message exchange
   - Broadcast messages are sent to all other users as MSG_TEXT with recipient = ALL
   - Private messages are sent only to the target user with recipient = <username>
   - All messages are saved to the history file via save_history
5. Status and user list
   - /status ... sends MSG_STATUS_CHANGE
6. History
   - /history - the client sends MSG_HISTORY_REQ, and the server loads the last 50 messages from chat_history.txt and returns them as a single MSG_HISTORY packet.
7. Logout
   - /quit sends MSG_LOGOUT. The server removes the user from online_users and users_status, and notifies the others that the user has left the chat.
   - If the connection drops, the server detects a recv failure and performs the same cleanup in the cleanup section


### Protocol Design

#### Basic Packet Structure
All traffic between the client and the server goes through a fixed Packet structure defined in protocol.h
```
struct Packet {
uint8_t type;           // Message type
char sender[32];        // Who is sending (username)
char recipient[32];     // To whom (username or "ALL" for broadcast)
uint16_t payload_length; // Payload length
char payload[512];      // Message content
};
```

#### Key Design Ideas
 - Fixed size (PACKET_SIZE = sizeof(Packet)) - simplifies send/recv, i.e. we always send/receive the same number of bytes
 - The sender and recipient fields always travel inside the packet, so the server does not need to additionally map (socket, username) when broadcasting.
 - payload serves as universal text content: a message, user list, history, error description, new status, etc.

### Message Types:

- MSG_LOGIN - Login request (client -> server)
- MSG_LOGIN_SUCCESS - Confirmation that the login succeeded (server -> client)
- MSG_LOGIN_FAIL - returns an error message when the username is already in use (server -> client)
- MSG_BROADCAST - Broadcast message (client -> server)
- MSG_PRIVATE - Private message (client -> server)
- MSG_USER_LIST_REQ - Request for the user list (client -> server)
- MSG_USER_LIST - payload contains the list of users and their statuses (server -> client)
- MSG_HISTORY - payload contains the formatted history (server -> client)
- MSG_HISTORY_REQ - Request for the chat history (client -> server)
- MSG_STATUS_CHANGE - Status change (client -> server)
- MSG_LOGOUT - signal that the user is deliberately logging out (client -> server)
- MSG_TEXT - general text message to clients (server -> client)
- MSG_ERROR - contains an error description (server -> client)
- MSG_STATUS_NOTIFICATION - the server notifies all other clients that a given user has changed status. (server -> client)

---

## Possible Problems and Solutions

### Problem: "Connection failed. Da li je server pokrenut?" ("...Is the server running?")
**Cause:** The server is not running or is running on a different port.

**Solution:**
- Start server.exe before the client.
- Check that port 8080 is not in use by another process
- Check the firewall (temporarily disable it or allow the application).


### Problem: Server reports: "Error binding socket: 10048"
**Cause:** Port 8080 is already in use.

**Solution:** In CMD, type:
```
netstat -ano | findstr :8080
```
Find the process PID and terminate it

```
taskkill /PID <pid> /F
```
---

### Network Programming Principles Demonstrated

The project demonstrates the following principles:
1. **Socket programming** - creating and managing network sockets (socket, bind, listen, accept, connect)
2. **TCP protocol** - a reliable, connection-oriented communication protocol that ensures ordered, acknowledged, and lossless data delivery
3. **Client-server architecture** - a centralized server model
4. **Concurrency** - the server can handle multiple clients simultaneously using multi-threading, where each client gets its own thread of execution
5. **Synchronization** - std::mutex and std::lock_guard ensure thread-safe access to shared structures (user list, statuses, history)

### Author

Amina Šadinlija

email: asadinlija@gmail.com
