#include <iostream>
#include <vector>
#include <map>
#include <bits/stdc++.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "helpers.h"

#define TOPIC_MAX_SIZE 50
#define BUF_LEN 2001

using namespace std;

struct packet_UDP {
	string ip;
	int port;
	string topic_name;
	string data_type;
	string contents;
	string formatted_message;
};

struct client {
	int socket;
    string id, ip;
    int port;
    int state;
    queue<string> remainingMessages;
};

struct subscriber {
	int subscription_type;
	client* subscribed_client;
};

struct topic {
    string name;
    vector<subscriber> subscribers;

	bool operator<(const topic &t) const {
        return name.compare(t.name) < 0;
    }

    bool operator==(const topic &t) const {
        return name.compare(t.name) == 0;
    }

    bool operator>(const topic &t) const {
        return name.compare(t.name) > 0;
    }
};

/**
 * Function printing the online clients; used for debugging
 * */
void print_map_id_clients(unordered_map<int, client*> map_connected_clients) {
	unordered_map<int, client*>::iterator client_iterator_by_sockets;

	printf("Print clients IDs:\n");

	// iterate through map of clients
	for (client_iterator_by_sockets = map_connected_clients.begin();
	 	client_iterator_by_sockets != map_connected_clients.end();
									++client_iterator_by_sockets) {
        client* current_client = client_iterator_by_sockets->second;

		// check if client is connected
        if (current_client->state) {
			cout << "Connected Client id: " << current_client->id
				<< " with port " << current_client->port << "\n";
        }
    }

    printf("\n");
}

/**
 * Function searching topic based on given name and returning index
 * */
int find_topic(string topic_name, vector<topic> topics) {
	// iterate through topics array
	for (int i = 0; i < topics.size(); ++i) {
		// compare topic name with given name
		if (topics[i].name.compare(topic_name) == 0) {
			return i;
		}
	}

	return -1;
}

/**
 * Function calculating the power of 10 value
 * */
int calculate_pow(int pow) {
	int result = 1;

	while (pow > 0) {
		result *= 10;
		pow--;
	}

	return result;
}

/**
 * Function creating the UDP package and formatting the UDP message
 * */
packet_UDP create_udp_package(char *buffer, int port, string ip) {
	packet_UDP package;
	package.port = port;
	package.ip = ip;

	// get data type
	uint8_t data_type = buffer[TOPIC_MAX_SIZE];

	// get topic name
	char buff[TOPIC_MAX_SIZE + 1];
	memset(buff, 0, TOPIC_MAX_SIZE + 1);
	memcpy(buff, buffer, TOPIC_MAX_SIZE);
	std::string topic_name (buff);
	package.topic_name = topic_name;

	if (data_type >= 0 && data_type <= 3) {
		// check data type
		switch (data_type) {
			case 0: {
				package.data_type = "INT";

				// get sign byte
				uint8_t sign = buffer[TOPIC_MAX_SIZE + 1];
				if (sign != 0 && sign != 1) {
					break;
				}

				// get payload of type int
				uint32_t payload_data;
				memcpy(&payload_data, buffer + TOPIC_MAX_SIZE + 2,
														sizeof(uint32_t));
				payload_data = ntohl(payload_data);

				// create content
				string result = std::to_string(payload_data);
				
				if (sign == 1) {
					package.contents = "-" + result;
				} else {
					package.contents = result;
				}

				break;
			}
			case 1: {
				package.data_type = "SHORT_REAL";

				uint16_t payload_data;

				// get payload of type uint19_6
				memcpy(&payload_data, buffer + TOPIC_MAX_SIZE + 1,
														sizeof(uint16_t));
				payload_data = ntohs(payload_data);
				char buff[BUF_LEN];

				// format payload to short real
				snprintf(buff, BUF_LEN, "%.2f", (float) payload_data / 100);
				std::string str(buff);

				// create content
				package.contents = str;
				break;
			}
			case 2: {
				package.data_type = "FLOAT";

				// get sign byte
				uint8_t sign = buffer[TOPIC_MAX_SIZE + 1];
				if (sign != 0 && sign != 1) {
					break;
				}

				uint32_t first;
				uint8_t negative_pow;

				// get first part of Float number, of tye uint32_t
				memcpy(&first, buffer + TOPIC_MAX_SIZE + 2, sizeof(uint32_t));
				first = ntohl(first);

				// get negative power of Float number, of tye uint8_t
				memcpy(&negative_pow,
					buffer + TOPIC_MAX_SIZE + 2 + sizeof(uint32_t),
													sizeof(uint8_t));

				// format payload to type float
				stringstream payload_stream;
				payload_stream << fixed
								<< setprecision((int) negative_pow)
								<< (float) first / calculate_pow((int) negative_pow);

				if (sign == 1) {
					package.contents = "-";
				}

				// add to contents
				package.contents += payload_stream.str();

				break;
			}
			case 3: {
				package.data_type = "STRING";
				
				// get string
				std::string str(buffer + TOPIC_MAX_SIZE + 1);
				// add to contents
				package.contents = str;
				break;
			}
		}

		// format UDP message
		package.formatted_message = package.ip + ":" +
									std::to_string(package.port) + " - " +
									package.topic_name + " - " +
									package.data_type + " - " +
									package.contents;
	}

	return package;
}

/**
 * Function searching subscriber based on ID and returning the index
 * */
int find_subscriber(string client_id, topic topic) {
	// iterate hrough subscriber array
	for (int i = 0; i < topic.subscribers.size(); ++i) {
		// coompare client ID with given ID
		if (!topic.subscribers[i].subscribed_client->id.compare(client_id)) {
			return i;
		}
	}

	return -1;
}

/**
 * Function sending given message to client
 * */
void send_udp_message_to_client(const char* message, client tcp_client) {
	int n = send(tcp_client.socket, message, strlen(message), 0);
	DIE(n < 0, "send");
}

/**
 * Function sending the UDP message to subscribers
 * */
void send_udp_message(packet_UDP packet, vector<topic> &topics) {
	string topic_name = packet.topic_name;

	// search topic based on name
	int topic_ind = find_topic(topic_name, topics);
	topic topic;
	string formatted_message = packet.formatted_message;
	formatted_message += "\n";

	if (topic_ind == -1) {
		// topic was not found so create new topic
		topic.name = packet.topic_name;
		topics.push_back(topic);
		return;
	} else {
		// get topic
		topic = topics[topic_ind];
	}

	// get topic's subscribers
	vector<subscriber> subscribers = topic.subscribers;
	const char* message = formatted_message.c_str();

	// iterate through subscribers
	for (int i = 0; i < subscribers.size(); ++i) {
		if (subscribers[i].subscribed_client->state == 1) {
			// send UDP message to subscribers that are online
			send_udp_message_to_client(message,
										*(subscribers[i].subscribed_client));
		} else if (subscribers[i].subscription_type == 1) {
			// add message to offline subscribers' remaining messages queue
			subscribers[i].subscribed_client->remainingMessages.
												push(formatted_message);
		}
	}
}

/**
 * Function subscribing client to given topic
 * */
void subscribe_client(string topic_name, int subscription_type,
							vector<topic> &topics, client *client) {
	int topic_ind = find_topic(topic_name, topics);
	topic topic;

	// topic not found
	if (topic_ind == -1) {
		// create topic and add to the array of topics
		topic.name = topic_name;
		topics.push_back(topic);
		topic_ind = topics.size() - 1;
	}

	// find subscriber based on ID
	int subscriber_index = find_subscriber(client->id, topics[topic_ind]);

	// check if cliet is already subscribed to topic
	if (subscriber_index != -1) {
		// change subscription type
		topics[topic_ind].subscribers[subscriber_index].subscription_type =
															subscription_type;
		return;
	}

	// create subscriber and add data
	subscriber new_subscriber;
	new_subscriber.subscription_type = subscription_type;
	new_subscriber.subscribed_client = client;

	// insert subscriber to the topic's array of subscribers
	topics[topic_ind].subscribers.push_back(new_subscriber);
}

/**
 * Function unsubscribing client from given topic
 * */
void unsubscribe_client(string topic_name,
							vector<topic> &topics, client *client) {
	// search the topic based on name
	int topic_ind = find_topic(topic_name, topics);
	topic topic;

	// topic not found
	if (topic_ind == -1) {
		// create topic and add to the array of topics
		topic.name = topic_name;
		topics.push_back(topic);
		return;
	}

	// find subscriber based on ID
	int subscriber_index = find_subscriber(client->id, topics[topic_ind]);

	// subscriber not found
	if (subscriber_index == -1) {
        return;
    }

	// remove subscriber from the topic's array of subscribers
	topics[topic_ind].subscribers.erase(topics[topic_ind].subscribers.begin()
														+ subscriber_index);
}

/**
 * Function splitting given message
 * */
vector<string> split_message(char *message) {
	char *tmp = strtok(message, " \n");
	vector<string> strings;

	while (tmp != NULL) {
		std::string str(tmp);
		strings.push_back(str);
		tmp = strtok(NULL, " ");
	}

	return strings;
}

/**
 * Function printing all connected subscribers of given topic
 * */
void print_subscribed_clients(topic topic) {
	vector<subscriber> subscribers = topic.subscribers;

	// iterate through array of topic's subscribers
	for (int i = 0; i < subscribers.size(); i++) {
		client* current_client = subscribers[i].subscribed_client;

		// check if client is online
		if (current_client->state) {
			cout << "Connected Client id: " << current_client->id 
				<< " with port " << current_client->port << "\n";
		}
	}

	printf("\n");
}

/**
 * Function printing all topics; used for debugging
 * */
void show_topics(vector<topic> topics) {
	for (int i = 0; i < topics.size(); i++) {
		cout << "Topic " << topics[i].name <<  "\n";
	}

	printf("\n");
}

/**
 * Function processing message from the TCP client and executing the command
 * */
void execute_tcp_client_command(int socket, char *message,
							vector<topic> &topics,
							unordered_map<int, client*> &map_connected_clients,
							unordered_map<string, client*> &map_id_clients) {
	// find client based on socket
	std::unordered_map<int, client*>::iterator client_iterator =
											map_connected_clients.find(socket);
	
	// client was not found
	if (client_iterator == map_connected_clients.end()) {
		return;
	}

	client* client = client_iterator->second;
	
	// proccess message
	vector<string> strings = split_message(message); 
	char buffer[BUF_LEN];

	if (strings[0].compare("subscribe") == 0) {
		// subscribe command

		// message not valid
		if (strings.size() != 3 || strings[2].length() != 1) {
			return;
		} else {
			char c = strings[2][0];
			if ( c != '0' && c != '1') {
				return;
			}
		}

		// get SF
		int subscription_type = stoi(strings[2]);

		// subscribe client
		subscribe_client(strings[1], subscription_type, topics, client);

		int n = strlen("Subscribed to topic.\n") + 1;
		snprintf(buffer, n, "Subscribed to topic.\n");

		// send subscription message to client
		n = send(socket, buffer, n, 0);
		DIE(n < 0, "send");
	} else if (strings[0].compare("unsubscribe") == 0) {
		// unsubscribe command
		if (strings.size() != 2) {
			return;
		}

		// unsubscribe client
		unsubscribe_client(strings[1], topics, client);
		int n = strlen("Unsubscribed from topic.\n") + 1;
		snprintf(buffer, n, "Unsubscribed from topic.\n");

		// send unsubscribe message to client
		n = send(socket, buffer, n, 0);
		DIE(n < 0, "send");
	} else if (strings[0].compare("show") == 0) {
		// show command used for debugging
		print_map_id_clients(map_connected_clients);
	} else if (strings[0].compare("show_id_clients_subscribed_to_topic") == 0) {
		// show subscribed clients to the topic; used for debugging
		string topic_name = strings[1];

		// search for topic
		int index = find_topic(topic_name, topics);

		// topic not found
		if (index == -1) {
			return;
		}

		print_subscribed_clients(topics[index]);
	} else if (strings[0].compare("show_topics") == 0) {
		// show all topics command; used for debugging
		show_topics(topics);
	}
}

/**
 * Function closing the connection to a single client
 * */
void close_client(int socket, char buffer[BUF_LEN]) {
	// send the exit message to client
	int n = send(socket, buffer, strlen(buffer), 0);
	DIE(n < 0, "send");

	// close connection
	close(socket);
}

/**
 * Function closing clients connections
 * */
void close_clients(unordered_map<int, client*>  map_connected_clients,
					unordered_map<string, client*>  map_id_clients,
													char buffer[BUF_LEN]) {
    std::unordered_map<string, client*>::iterator client_iterator;

	// iterate through map of clients
    for (client_iterator = map_id_clients.begin();
				client_iterator != map_id_clients.end(); ++client_iterator) {
        client* current_client = client_iterator->second;

		// check if client is online
		if (current_client->state == 1) {
			// close connection to client
			close_client(current_client->socket, buffer);
		}

		// free allocated memory for the client
		delete current_client;
	}
}

/**
 * Function finding client in map based on ID
 * */
std::unordered_map<int, client*>::iterator find_client_in_map(string id,
								unordered_map<int, client*> map_connected_clients) {
	std::unordered_map<int, client*>::iterator client_iterator;

	// iterate through map of clients
	for (client_iterator = map_connected_clients.begin();
		client_iterator != map_connected_clients.end(); client_iterator++) {
		client *client = client_iterator->second;

		// check if client's ID matches with given ID
		if (client->id.compare(id) == 0) {
			return client_iterator;
		}
	}

	return map_connected_clients.end();
}

/**
 * Function connecting a client from the server
 * */
void connect_client(string id, string ip, int port,
					unordered_map<int, client*> &map_connected_clients,
					unordered_map<string, client*> &map_id_clients,
					int socket, fd_set read_fds) {
	// search client based on ID
	std::unordered_map<string, client*>::iterator client_iterator =
												map_id_clients.find(id);
	
	client *current_client;

	if (client_iterator != map_id_clients.end()) {
		// client with given ID does exist inside the map
		current_client = client_iterator->second;
		if (current_client->state == 1) {
			// client is online
			cout << "Client " << id << " already connected.\n";

			if (socket != current_client->socket) {
				// send exit command to the newly opened socket
				char buff[BUF_LEN];
				memset(buff, 0, BUF_LEN);
				memcpy(buff, "exit", strlen("exit"));
				int n = send(socket, buff, strlen(buff), 0);
				DIE(n < 0, "send");
			}

			return;
		}

		int previous_socket = current_client->socket;

		// change client information
		current_client->ip = ip;
    	current_client->port = port;
    	current_client->state = 1;
		current_client->socket = socket;

		// send remaining messages in the messages queue
		while (!current_client->remainingMessages.empty()) {
			string message = current_client->remainingMessages.front();
			const char* message_tmp = message.c_str();
			// send message to client and remove it from queue
			send_udp_message_to_client(message_tmp, *current_client);
			current_client->remainingMessages.pop();
		}

		// delete the previous socket key from the map
		map_connected_clients.erase(previous_socket);
		// add the newly created pair to map
		map_connected_clients.insert(pair<int, client*> (socket, current_client));

		map_id_clients.erase(id);
		map_id_clients.insert(pair<string, client*> (id, current_client));

		cout << "New client " << id << " connected from "
				<< ip << ":" << port << ".\n";
		return;
	}

	// client was not found so a new one is created
	current_client = new client;

	// add data to the client
    current_client->id = id;
    current_client->ip = ip;
    current_client->port = port;
    current_client->state = 1;
	current_client->socket = socket;

	// insert a pair of client and socket to the map
	map_connected_clients.insert(pair<int, client*> (socket, current_client));
	map_id_clients.insert(pair<string, client*> (id, current_client));

	cout << "New client " << id << " connected from " << ip << ":" << port << ".\n";
}

/**
 * Function disconnecting a client from the server
 * */
void disconnect_client(int socket,
							unordered_map<int, client*> &map_connected_clients,
							unordered_map<string, client*> &map_id_clients) {
	// search client based on given socket in map
	std::unordered_map<int, client*>::iterator client_iterator =
											map_connected_clients.find(socket);

	// client not found
	if (client_iterator == map_connected_clients.end()) {
		return;
	}

	// client was found
	client* current_client = client_iterator->second;
	// client's state is changed
	current_client->state = 0;
	map_connected_clients.erase(socket);

	cout << "Client " << current_client->id << " disconnected.\n";
}

/**
 * Function searching for a client based on given socket, and then
 * checking the client's state
 * */
int is_client_connected(int socket,
					unordered_map<int, client*> map_connected_clients) {
    std::unordered_map<int, client*>::iterator client_iterator =
											map_connected_clients.find(socket);

	if (client_iterator == map_connected_clients.end()) {
		return -1;
	}

	client found_client = *(client_iterator->second);

	return found_client.state;
}

char* receive_message(int sockfd) {
	char buffer[BUF_LEN];
	memset(buffer, 0, BUF_LEN);
	int n = recv(sockfd, buffer, sizeof(buffer), 0);
	DIE(n < 0, "recv");

	char *tmp = buffer;

	return tmp;
}

int main(int argc, char *argv[])
{
	setvbuf(stdout, NULL, _IONBF, BUFSIZ);
	DIE(argc < 2, "arguments");

	int socket_TCP, socket_UDP, newsockfd, portno, dest;
	char buffer[BUF_LEN];
	char client_id_buff[BUF_LEN];
	struct sockaddr_in serv_addr, cli_addr, udp_addr;
	int n, i, ret;
	socklen_t clilen, udplen;

	unordered_map<string, client*>  map_id_clients;
	unordered_map<int, client*>  map_connected_clients;
	vector<topic> topics;

	fd_set read_fds;	// set for reading used in select()
	fd_set tmp_fds;		// set used temporarily
	int fdmax;			// maximum value of fd from read_fds set

	// empty set of read descriptors (read_fds) and the temporary set (tmp_fds)
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// create TCP socket
	socket_TCP = socket(AF_INET, SOCK_STREAM, 0);
	DIE(socket_TCP < 0, "TCP socket");

	// disable Nagle algorithm
	int nagle = 1;
	ret = setsockopt(socket_TCP, IPPROTO_TCP, TCP_NODELAY, &nagle, sizeof(nagle));
	DIE(ret < 0, "Nagle");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	// bind TCP client socket to port
	ret = bind(socket_TCP, (struct sockaddr *) &serv_addr,
										sizeof(struct sockaddr));
	DIE(ret < 0, "TCP bind");

	// enable reuse port action
	int reuse_port = 1;
	ret = setsockopt(socket_TCP, SOL_SOCKET, SO_REUSEADDR, &reuse_port, sizeof(reuse_port));
	DIE(ret < 0, "Reuse port");

	// listen on the TCP client socket
	ret = listen(socket_TCP, MAX_CLIENTS);
	DIE(ret < 0, "TCP listen");

	// create UDP socket
	socket_UDP = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(socket_UDP < 0, "UDP socket");

	// bind UDP client socket to port
	ret = bind(socket_UDP, (struct sockaddr *) &serv_addr,
										sizeof(struct sockaddr));
	DIE(ret < 0, "UDP bind");

	// the sockets for TCP clients, UDP clients and STDIN are added to the read_fds set
	FD_SET(STDIN_FILENO, &read_fds);
	FD_SET(socket_TCP, &read_fds);
	FD_SET(socket_UDP, &read_fds);
	fdmax = max(socket_TCP, socket_UDP);

	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			// read data from STDIN
			memset(buffer, 0, BUF_LEN);
			fgets(buffer, BUF_LEN - 1, stdin);
			buffer[strlen(buffer) - 1] = '\0';

			// check if server received exit command
			if (buffer == NULL || strlen(buffer) == 0 || strncmp(buffer, "exit", 4) == 0) {
				// close all clients
				close_clients(map_connected_clients, map_id_clients, buffer);
				break;
			}

			continue;
		}

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i != socket_UDP) {
					// client TCP actions
					if (i == socket_TCP) {
					// accept a connection request from the inactive socket
					clilen = sizeof(cli_addr);
					newsockfd = accept(socket_TCP, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					// socket is added to the set of read descriptors
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) { 
						fdmax = newsockfd;
					}

					// receive ID from client
					memset(buffer, 0, BUF_LEN);
					int n = recv(newsockfd, buffer, sizeof(buffer), 0);
					DIE(n < 0, "recv");

					// create connection
					connect_client(buffer, inet_ntoa(cli_addr.sin_addr),
											ntohs(cli_addr.sin_port),
											map_connected_clients, map_id_clients,
											newsockfd, read_fds);

					fflush(stdout);	
					} else {
						// receive message from subscriber
						memset(buffer, 0, BUF_LEN);
						int n = recv(i, buffer, sizeof(buffer), 0);
						DIE(n < 0, "recv");
						buffer[strlen(buffer) - 1] = '\0';

						if (n == 0 || strncmp(buffer, "exit", 4) == 0) {
							// client received exit/forceful shut command
							// client is disconnected from server
							disconnect_client(i, map_connected_clients, map_id_clients);
							close(i);
							FD_CLR(i, &read_fds);
						} else {
							// process message and execute command
							execute_tcp_client_command(i, buffer, topics,
														map_connected_clients, map_id_clients);
						}

					}
				} else {
					// UDP client actions
					memset(buffer, 0, BUF_LEN);
					udplen = sizeof(udp_addr);
					// receive message from UDP client
					ret = recvfrom(socket_UDP, buffer, BUF_LEN, 0,
								(struct sockaddr *) &udp_addr, &udplen);
					DIE(ret < 0, "UDP receive");
					// UDP client was closed so the program moves on
					if (ret == 0) {
						continue;
					}

					std::string ip(inet_ntoa(udp_addr.sin_addr));
					int port = ntohs(udp_addr.sin_port);

					// create an UDP packet
					packet_UDP packet = create_udp_package(buffer, port, ip);
					
					// the packet's formatted message is not valid
					if (packet.formatted_message.empty()) {
						continue;
					}

					// send the UDP message to respective clients
					send_udp_message(packet, topics);
				}
			}
		}
	}

	close(socket_TCP);

	return 0;
}
