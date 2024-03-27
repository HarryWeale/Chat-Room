# Chat Room
A basic chat room on Linux, essentially a group chat.<br>
Users can chat anonymously, and there are join and exit messages.

## Usage
Run this script on a Linux instance.
```
gcc -c chat-room-server.c
gcc -o chat-room-server chat-room-server.c
./chat-room-server
```
Users join through Telnet, a protocol allowing connection to a remote host over a TCP/IP network.<br>
They must specify the IP of the host and the port that the host is listening over.
```
telnet [host IP] [port]
```
They are then connected and can message each other anonymously.
