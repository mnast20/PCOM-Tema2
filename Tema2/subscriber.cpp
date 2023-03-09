#include <iostream>
#include <vector>
#include <bits/stdc++.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"

#define BUF_LEN 2001

using namespace std;

/**
 * Function splitting given message into an array of strings
 * */
int split_message(char *message, vector<string> &tokens) {
	char buffer[BUF_LEN + 1];
	memcpy(buffer, message, BUF_LEN);
	buffer[BUF_LEN] = '\n';
	string str = buffer;
	string token;
	std::stringstream str_stream (str);

	// return tokens;
	int prev_index = 0;

	for (int i = 0; i < BUF_LEN; i++) {
		if (buffer[i] == '\n') {
			std::getline(str_stream, token, '\n');
			tokens.push_back(token);
		}
		
		// no partial message
		if (buffer[i + 1] == '\0') {
			return 1;
		}
	}

	std::getline(str_stream, token, '\n');
	tokens.push_back(token);

	return 0;
}

/**
 * Function sending client's ID to the server
 * **/
void send_id_to_server(int sockfd, char *id) {
	int n = send(sockfd, id, strlen(id), 0);
	DIE(n < 0, "send");
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	DIE(argc < 3, "arguments");

	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUF_LEN];

	fd_set read_fds, tmp_fds;

	// empty set of read descriptors (read_fds) and the temporary set (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	// disable Nagle algorithm
	int nagle = 1;
	ret = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle));
	DIE(ret < 0, "Nagle");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	// create connection to server
	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	send_id_to_server(sockfd, argv[1]);

	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(sockfd, &read_fds);

	while (1) {
		tmp_fds = read_fds;
		ret = select(sockfd + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			// read data from STDIN
			memset(buffer, 0, BUF_LEN);
			fgets(buffer, BUF_LEN - 1, stdin);

			// send the message to server
			n = send(sockfd, buffer, strlen(buffer), 0);
			DIE(n < 0, "send");

			// perform exit/forceful shut command
			if (n == 0 || strncmp(buffer, "exit", 4) == 0) {
				break;
			}
		}

		if (FD_ISSET(sockfd, &tmp_fds)) {
			// data was received from server
			string tmp = "";
			memset(buffer, 0, BUF_LEN);

			n = recv(sockfd, buffer, BUF_LEN, 0);
			DIE(n < 0, "recv");

			if (n == 0) {
				// server forcefully shut
				// client connection will also be closed
				break;
			}

			if (strncmp(buffer, "exit", 4) == 0) {
				// end connection for exit command
				break;
			}

			while (n != 0) {
				char buff[BUF_LEN];

				if (buffer[0] == '\n') {
					// end of buffer reached
					break;
				}

				// split message based on new lines
				vector<string> strings;
				int ret = split_message(buffer, strings);

				if (tmp.empty()) {
					// no partial message

					// write all messages to STDOUT besides the last one
					// which could be partial
					for (int i = 0; i < strings.size() - 1; i++) {
						cout << strings[i] << "\n";
					}
				} else {
					// pending partial message
					tmp += strings[0];
					// write the now completed message
					cout << tmp << '\n';

					// write all other messages to STDOUT besides the last one
					// which could be partial
					for (int i = 1; i < strings.size() - 1; i++) {
						cout << strings[i] << "\n";
					}

					// no partial message
					tmp.clear();

					if (ret == 1 && strings.size() == 1) {
						break;
					}
				}
				
				if (ret == 0) {
					// partial message found
					tmp += strings[strings.size() - 1];
				} else {
					// no partial message, which means end of buffer
					cout << strings[strings.size() - 1] << "\n";
					break;
				}

				// receive data from server
				memset(buffer, 0, BUF_LEN);
				n = recv(sockfd, buffer, BUF_LEN, 0);
				DIE(n < 0, "recv");
			}

		}
	}

	close(sockfd);

	return 0;
}
