#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int MAX_CLIENTS = 128;
int serverSoc = -1;
int maxSoc = 0;
int g_id = 0;
int id[65536], currMsg[65536];

fd_set activeSoc, readySoc, writeSoc;

char buff[4096 * 42], str[4096 * 42], msg[4096 * 42 + 42];

void fatal(int type) {
	if (type == 1)
		write(2, "Wrong number of arguments\n", strlen("Wrong number of arguments\n"));
	else {
		write(2, "Fatal error\n", strlen("Fatal error\n"));
		close(serverSoc);
	}
	exit(1);
}

void sendAll(int socketId) {
	for (int fd = 0; fd <= maxSoc; fd++) {
		if (fd != socketId && FD_ISSET(fd, &writeSoc)) {
			send(fd, msg, strlen(msg), 0);
		}
	}
}

void handleMessage(int socketId, int buffer) {
	for (int i = 0, j = 0; i < buffer; i++, j++) {
		str[j] = buff[i];
		if (str[j] == '\n') {  // if there is a new line in buffer
			str[j + 1] = '\0';

			if (currMsg[socketId])
				sprintf(msg, "%s", str);
			else
				sprintf(msg, "client %d: %s", id[socketId], str);

			currMsg[socketId] = 0;
			sendAll(socketId);
			j = -1;
		} else if (i == (buffer - 1)) {	 // if end of buffer is reached
			str[j + 1] = '\0';

			if (currMsg[socketId])
				sprintf(msg, "%s", str);
			else
				sprintf(msg, "client %d: %s", id[socketId], str);

			currMsg[socketId] = 1;
			sendAll(socketId);
			break;
		}
	}
}

void handleClientExit(int socketId) {
	sprintf(msg, "server: client %d just left\n", id[socketId]);
	sendAll(socketId);
	FD_CLR(socketId,
		   &activeSoc);	 // Remove the client socket from the set of active sockets
	close(socketId);	 // close the client socket
}

int handleClientJoin(int socketId) {
	struct sockaddr_in clientAddr;

	socklen_t len = sizeof(clientAddr);
	int clientSoc = accept(socketId, (struct sockaddr *)&clientAddr, &len);

	if (clientSoc == -1) return 1;

	FD_SET(clientSoc, &activeSoc);

	id[clientSoc] = g_id++;
	currMsg[clientSoc] = 0;	 // not sure why

	if (clientSoc > maxSoc) maxSoc = clientSoc;

	sprintf(msg, "server: client %d just arrived\n", id[clientSoc]);

	sendAll(socketId);
	return 0;
}

int main(int argc, char **argv) {
	if (argc != 2) fatal(1);

	struct sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = 16777343;
	serverAddr.sin_port = htons(atoi(argv[1]));

	if ((serverSoc = socket(AF_INET, SOCK_STREAM, 0)) == -1) fatal(2);
	if ((bind(serverSoc, (const struct sockaddr *)&serverAddr, sizeof(serverAddr))) != 0)
		fatal(2);
	if (listen(serverSoc, MAX_CLIENTS) != 0) fatal(2);

	bzero(id, sizeof(id));
	FD_ZERO(&activeSoc);
	FD_SET(serverSoc, &activeSoc);
	maxSoc = serverSoc;

	while (1) {
		readySoc = writeSoc = activeSoc;

		if (select(maxSoc + 1, &readySoc, &writeSoc, NULL, NULL) <= 0) continue;

		for (int socketId = 0; socketId <= maxSoc; socketId++) {
			if (FD_ISSET(socketId, &readySoc)) {
				if (socketId == serverSoc) {
					if (handleClientJoin(socketId) == 1) continue;
					break;
				} else {
					int buffer = recv(socketId, buff, 4096 * 42, 0);
					if (buffer <= 0) {
						handleClientExit(socketId);
						break;
					} else {
						handleMessage(socketId, buffer);
					}
				}
			}
		}
	}

	return 0;
}
