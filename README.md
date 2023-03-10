# PCOM-Tema2

## Subscriber:
The TCP client can only perform 2 actions: reading from STDIN and receiving a message from the server. Every time a client connects, his ID will be sent to the server. When data is read from STDIN, the message is processed and then sent to the server. If the message has the value "exit", an exit command is issued and the connection is closed. If a message sent by the server is intercepted, I will check if it is a partial message and continue receiving bytes until a newline is reached (meaning the message was fully received). However, if the message is "exit" or the number of bytes is 0, the connection is closed without printing the received string.

## Server:
Firstly, the sockets for the TCP client, UDP client and STDIN are added to the set of read descriptors. Then, there are 4 actions that the server can perform: reading from STDIN, receiving message from TCP client, establishing a connection between the TCP client and the server and receiving an UDP packet.

## Reading from STDIN:
If the message has the value "exit", then all client that are online will have their connection to the server closed.

## Client Connection:
If a new connection is requested, then I will search the ID in the clients' map. If ID is not found, then a new client is created, populated, and added to the map of clients. On the other hand, if the client is found, we check its state first. If client is online, then I will close the connection on the given socket. Otherwise, I will change the client's information and its state to online. I will also be deleting the previous socket key and value from the map and then insert the new pair with the updated client and his new socket. Then, all the remaining messages from the subscriptions will be sent to him in order of apparition. 

## Receiving message from TCP client:
If the message has the value "exit", then the client will be disconnected. Otherwise, the message will be split into more strings and processed. If it contains the word "subscribe", then the client will be subscribed to the given topic with the given subscription type SF. Otherwise, iff the message has the substring "unsubscribe", then the client will be unsubscribed from the given topic

## Client Disconnect:
When a client is disconnected, its state is set to offline, but it remains in the client map in order to store the subscribed topics' messages that he didn't receive. Once he gets reconnected, all messages will be received by him.

## Subscribe client to topic:
The client and the SF will be put into a subscriber structure and added to the topic's list of subscribers. If the value of SF is 1, then the client will receive all the messages sent to the subscribed topic from when he was away. If SF = 0, then he will not store the remaining messages.

## Unsubscribe client from topic:
The client will be removed from the list of topic's subscribers.

## Receiving message from UDP client:
The message will be analyzed and the UDP package will be created based on the received message. Then, the formatted UDP message will be sent to the topic's subscribers that are online.  For the offline subscribers that have a subscription of type SF = 1, the messages will be kept in their personal message queues that will be emptied when he reconnects.

## Used structures:
- Client: keeps the client ID, IP, port, socket, state, as well as a queue of remaining messages to be sent.
- Subscriber: contains the subscription type and the TCP client himself.
- Topic: retains the topic name and a list of subscribers

All clients are kept in a map, where the key represents the ID and the value is that of the client, and all connected clients will be held in a map with pairs made up of socket and client.  
