#!/usr/bin/env python
import socket
import select
import struct
import fcntl

# Broadcast chat messages to all connected clients
def broadcast_data(sock, message):
    for socket in CONNECTION_LIST:
        try:
            if socket != server_socket and socket != sock:
                socket.send(message.encode())  
        except:
            continue

if __name__ == "__main__":
     
    # List to keep track of socket descriptors
    CONNECTION_LIST = []
    RECV_BUFFER = 4096 
    PORT = 42000
    HOST = '128.59.64.121'

    # Create server socket
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server_socket.bind((HOST, PORT))
    server_socket.listen(10)

    CONNECTION_LIST.append(server_socket)

    print("## Listening on " + HOST + ":" + str(PORT))
    
    while True:
        read_sockets, _, _ = select.select(CONNECTION_LIST, [], [])
        
        for sock in read_sockets:
            # New connection
            if sock == server_socket:
                sockfd, addr = server_socket.accept()
                (clientip, clientport) = addr
                CONNECTION_LIST.append(sockfd)
                print("## New Connection %s:%s" % (clientip, clientport))

                # sockfd.send("Welcome to the CSEE 4840 Lab2 chat server".encode())
                # check if joined

                broadcast_data(sockfd, "## %s has joined\n" % clientip)

            # Packet from a client
            else:
                try:
                    data = sock.recv(RECV_BUFFER)
                    (clientip, clientport) = sock.getpeername()

                    # If there is data, someone sent something in the room 
                    if data:
                        message = "<%s> " % clientip + data.decode()
                        broadcast_data(sock, message)
                        print(message.strip())

                    # No data: the client closed the connection
                    else:
                        broadcast_data(sock, "## %s has left\n" % clientip)
                        print("## Closed connection %s:%s" % addr)
                        sock.close()
                        CONNECTION_LIST.remove(sock)  

                except:
                    continue

    server_socket.close()