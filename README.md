# TW_Mailer_basic
Write a socket-based client-server application in C/C++ to send and receive messages like an internal mail-server.  The client connects to the server and communicates through a stream socket connection in a proprietary plain-text format delimited by new-line or “dot + new-line” (see detail-description).

**Usage Client:** ./twmailer-client <ip> <port>   IP and port define the address of the server application.  
***Usage Server:*** ./twmailer-server <port> <mail-spool-directoryname>  

The port describes the listening port of the server; the mail-spool-directory the folder to persist the incoming and outgoing messages and meta-data. The server is here designed as iterative server (no simultaneous request handling).
Hereby the server responds to the following commands: 
- **SEND:** client sends a message to the server. 
- **LIST:** lists all received messages of a specific user from his inbox. 
- **READ:** display a specific message of a specific user. 
- **DEL:** removes a specific message.
- **QUIT:** logout the client.
