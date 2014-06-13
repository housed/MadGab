#ifndef _SERVER_H_
#define _SERVER_H_

#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>

#include <iostream>
#include <string>
#include <process.h>

// Need to link with Ws2_32.lib
#pragma comment (lib, "Ws2_32.lib")
// #pragma comment (lib, "Mswsock.lib")

#define DEFAULT_BUFLEN	512
#define USERNAME_LEN	16
#define MAXCONNECTIONS  16
#define DEFAULT_PORT	"27017"

int g_num_clients_connected = 0;

enum FLAGS {
	USED = 1,
	INMSG = 2,
	OUTMSG = 4
};

struct MessagePacket {
	char username[USERNAME_LEN];
	char message[DEFAULT_BUFLEN - USERNAME_LEN];
};

struct SocketObject { // Just think of it as a socket object
	char outmsg[DEFAULT_BUFLEN];
	char inmsg[DEFAULT_BUFLEN];
	SOCKET *connection;
};

struct ConnPack { // A socket with an id.
	SocketObject *socketObject;
	int id;
};

void SendData(void* info) {
	SocketObject *socketObject = (SocketObject*)info;
	int status = 0;

	printf("Start SendData()\n"); // test

	do {
		//memcpy(socketObject->outmsg, socketObject->inmsg, DEFAULT_BUFLEN);
		memcpy(socketObject->outmsg, socketObject->inmsg, sizeof(socketObject->inmsg));
		
		printf("SendData(): Start of do-while\nSendData(): Starting for loop\n"); // test

		for ( int i = 0; i < MAXCONNECTIONS; i++ ) {
			printf("SendData(): Before checking for invalid sockets\n"); // test
			if ( socketObject->connection[i] != INVALID_SOCKET ) {
				
				printf("SendData(): before send() in server\n"); // test

				//status = send(socketObject->connection[i], socketObject->outmsg, DEFAULT_BUFLEN, 0);
				status = send(socketObject->connection[i], "send() CALLED FROM SERVER", DEFAULT_BUFLEN, 0);

				printf("SendData(): after send() in server\n"); // test

				if ( status == SOCKET_ERROR ) {
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(socketObject->connection[i]);
					socketObject->connection[i] = INVALID_SOCKET;
				}
			}
			else {
				printf("SendData(): All sockets in list are INVALID_SOCKET\n"); //test else statement
			}
		}
		printf("SendData(): out of for loop, but still in do-while\n"); // test
	} while ( status > 0 );
}

void ReceiveData(void* info) {
	ConnPack *pack = (ConnPack*)info;
	SocketObject *socketObject = (SocketObject*)pack->socketObject;
	int i = pack->id;

	MessagePacket message_packet = { NULL };
	char username[USERNAME_LEN] = "username";
	_itoa(i, username + 4, 10);

	int status = 0;

	status = send(socketObject->connection[i], "***send() CALLED IN RECEIVEDATA() FROM SERVER***\n", DEFAULT_BUFLEN, 0); // test

	do {
		status = recv(socketObject->connection[i], (char*)&message_packet, DEFAULT_BUFLEN, 0);
		if ( status > 0 ) { 
			printf("%s : %s\n", message_packet.username, message_packet.message);

			strcpy(username, message_packet.username);

			memcpy(socketObject->inmsg, (char*)&message_packet, DEFAULT_BUFLEN);
			//memcpy(socketObject->inmsg, message_packet.username, strlen(message_packet.username));
		}
		else if ( status <= 0 ) {
			printf("%s has disconnected\n", message_packet.username);

			status = shutdown(*socketObject->connection, SD_SEND);

			g_num_clients_connected--;

			if ( status == SOCKET_ERROR ) {
				printf("shutdown failed with error: %d\n", WSAGetLastError());
			}
			else {
				closesocket(socketObject->connection[i]);
				socketObject->connection[i] = INVALID_SOCKET;
				return;
			}
		}
	} while ( status > 0 );
}

int main(void) {
	SetConsoleTitle("Server");

	// Initialize Winsock
	WSADATA wsaData;
	int wsaStatus = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if ( wsaStatus != 0 ) {
		printf("WSAStartup failed with error: %d\n", wsaStatus);
		return 1;
	}

	////////////////////////
	////////////////////////
	////////////////////////

	struct addrinfo *result = NULL,
		             hints = { NULL };
	int status = 0;

	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	// Resolve the server address and port http://msdn.microsoft.com/en-us/library/windows/desktop/ms738520(v=vs.85).aspx
	status = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
	if ( status != 0 ) {
		printf("getaddrinfo failed with error: %d\n", status);
		WSACleanup();
		return 1;
	}

	////////////////////////
	////////////////////////
	////////////////////////

	SOCKET listen_socket = INVALID_SOCKET; // This is the server's socket
	SOCKET client_sockets[MAXCONNECTIONS] = { NULL }; // An array (or list) of potential client sockets that connect to this server
	for ( int i = 0; i < MAXCONNECTIONS; i++ ) {
		client_sockets[i] = INVALID_SOCKET; // Initialize all possible sockets with INVALID_SOCKETS
	}

	// The socket function creates a socket that is bound to a specific transport service provider. http://msdn.microsoft.com/en-us/library/windows/desktop/ms740506(v=vs.85).aspx
	listen_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if ( listen_socket == INVALID_SOCKET ) {
		printf("socket failed with error: %ld\n", WSAGetLastError());
		freeaddrinfo(result);
		WSACleanup();
		return 1;
	}

	// The bind function associates a local address with a socket. http://msdn.microsoft.com/en-us/library/windows/desktop/ms737550(v=vs.85).aspx
	status = bind(listen_socket, result->ai_addr, (int)result->ai_addrlen);
	if ( status == SOCKET_ERROR ) {
		printf("bind failed with error: %d\n", WSAGetLastError());
		freeaddrinfo(result);
		closesocket(listen_socket);
		WSACleanup();
		return 1;
	}

	freeaddrinfo(result);

	SocketObject socket_object = { NULL };
	socket_object.connection = client_sockets;

	// Here the address of the empty list of clients is sent to a SendData() thread. 
	// Client connections are made in the following while loop
	_beginthread(SendData, 0, &socket_object);

	////////////////////////
	////////////////////////
	////////////////////////

	// Create & destroy client sockets as they connect & disconnect from the server.
	// Also receives information from, and echos back to, all connected & valid clients.
	while ( 1 ) {
		for ( int i = 0; i < g_num_clients_connected; i++ ) {                               // For each client that's connected to this server...
			if ( client_sockets[i] == INVALID_SOCKET ) {                                    // ...if this client is now invalid...
				std::swap(client_sockets[i], client_sockets[g_num_clients_connected - 1]);  // ...swap out the invalid client from the list of valid ones...
				g_num_clients_connected--;                                                  // ...and decrement the total connected client count to reflect the swap out.
			}
		}

		if ( g_num_clients_connected >= MAXCONNECTIONS ) {
			continue;
		}

		// The listen function places a socket in a state in which it is listening for an incoming connection. http://msdn.microsoft.com/en-us/library/windows/desktop/ms739168(v=vs.85).aspx
		status = listen(listen_socket, SOMAXCONN);
		if ( status == SOCKET_ERROR ) {
			printf("listen failed with error: %d\n", WSAGetLastError());
			closesocket(listen_socket);
			WSACleanup();
			return 1;
		}

		// The accept function permits an incoming connection attempt on a socket. http://msdn.microsoft.com/en-us/library/windows/desktop/ms737526(v=vs.85).aspx
		client_sockets[g_num_clients_connected] = accept(listen_socket, NULL, NULL);
		if ( client_sockets[g_num_clients_connected] == INVALID_SOCKET ) {
			printf("accept failed with error: %d\n", WSAGetLastError());
			closesocket(listen_socket);
			WSACleanup();
			return 1;
		}

		ConnPack client_socket;
		client_socket.socketObject = &socket_object;
		client_socket.id = g_num_clients_connected;

		// Create a new "Connection" thread per each connected client
		_beginthread(ReceiveData, 0, &client_socket);

		std::cout << ++g_num_clients_connected << " successful connections." << std::endl;
	}

	////////////////////////
	////////////////////////
	////////////////////////

	closesocket(listen_socket);
	WSACleanup();

	return 0;
}

#endif