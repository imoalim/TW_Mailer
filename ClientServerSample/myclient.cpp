#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <sys/stat.h> // Für die Verzeichniserstellung in C++ unter Linux/Unix


///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543

///////////////////////////////////////////////////////////////////////////////
/*void userInput(char input[], int isQuit){  
   
}*/
struct Send
{
  char sender[BUF];
  char receiver[BUF];
  char subject[81];
  int messageNum = 0;
  std::string message;
};

struct List{
   Send send;   
};


int main(int argc, char **argv)
{
   List list;
   Send send_;
   int create_socket;
   char buffer[BUF];
   struct sockaddr_in address;
   int size;
   int isQuit = 1;
   std::string messagesDirectory = "messages";

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   // https://man7.org/linux/man-pages/man2/socket.2.html
   // https://man7.org/linux/man-pages/man7/ip.7.html
   // https://man7.org/linux/man-pages/man7/tcp.7.html
   // IPv4, TCP (connection oriented), IP (same as server)
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS
   // Attention: network byte order => big endian
   memset(&address, 0, sizeof(address)); // init storage with 0
   address.sin_family = AF_INET;         // IPv4
   // https://man7.org/linux/man-pages/man3/htons.3.html
   address.sin_port = htons(PORT);
   // https://man7.org/linux/man-pages/man3/inet_aton.3.html
   if (argc < 2)
   {
      inet_aton("127.0.0.1", &address.sin_addr);
   }
   else
   {
      inet_aton(argv[1], &address.sin_addr);
   }

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A CONNECTION
   // https://man7.org/linux/man-pages/man2/connect.2.html
   if (connect(create_socket,
               (struct sockaddr *)&address,
               sizeof(address)) == -1)
   {
      // https://man7.org/linux/man-pages/man3/perror.3.html
      perror("Connect error - no server available");
      return EXIT_FAILURE;
   }

   // ignore return value of printf
   printf("Connection with server (%s) established\n",
          inet_ntoa(address.sin_addr));

   ////////////////////////////////////////////////////////////////////////////
   // RECEIVE DATA
   // https://man7.org/linux/man-pages/man2/recv.2.html
   size = recv(create_socket, buffer, BUF - 1, 0);
   if (size == -1)
   {
      perror("recv error");
   }
   else if (size == 0)
   {
      printf("Server closed remote socket\n"); // ignore error
   }
   else
   {
      buffer[size] = '\0';
      printf("%s", buffer); // ignore error
   }

   do
   {
         // SEND DATA
         // https://man7.org/linux/man-pages/man2/send.2.html
         // send will fail if connection is closed, but does not set
         // the error of send, but still the count of bytes sent
         if ((send(create_socket, buffer, size, 0)) == -1) 
         {
            // in case the server is gone offline we will still not enter
            // this part of code: see docs: https://linux.die.net/man/3/send
            // >> Successful completion of a call to send() does not guarantee 
            // >> delivery of the message. A return value of -1 indicates only 
            // >> locally-detected errors.
            // ... but
            // to check the connection before send is sense-less because
            // after checking the communication can fail (so we would need
            // to have 1 atomic operation to check...)
            perror("send error");
            break;
         }

         //////////////////////////////////////////////////////////////////////
         // RECEIVE FEEDBACK
         // consider: reconnect handling might be appropriate in somes cases
         //           How can we determine that the command sent was received 
         //           or not? 
         //           - Resend, might change state too often. 
         //           - Else a command might have been lost.
         //
         // solution 1: adding meta-data (unique command id) and check on the
         //             server if already processed.
         // solution 2: add an infrastructure component for messaging (broker)
         //
         size = recv(create_socket, buffer, BUF - 1, 0);
         if (size == -1)
         {
            perror("recv error");
            break;
         }
         else if (size == 0)
         {
            printf("Server closed remote socket\n"); // ignore error
            break;
         }
         else
         {
            buffer[size] = '\0';
            printf("<< %s\n", buffer); // ignore error
            if (strcmp("OK", buffer) != 0)
            {
               fprintf(stderr, "<< Server error occured, abort\n");
               break;
            }
         }
      printf(">> If you want to SEND a message, type 'Send'\n");
      printf(">> If you want to LIST all received messages of a specific user from his inbox, type 'List'\n");
      printf(">> If you want to READ a specific message of a specific user, type 'Read'\n");
      printf(">> If you want to remove a specific message, type 'Delete'\n");
      printf(">> If you want to logout the client, type 'Quit'\n");
if (fgets(buffer, BUF, stdin) != NULL)
      {
         
         int size = strlen(buffer);

// Entfernen Sie Zeilenumbruchzeichen ('\n' oder '\r\n') am Ende der Benutzereingabe
        if (size >= 2 && (buffer[size - 1] == '\n' || buffer[size - 1] == '\r')) {
            buffer[size - 1] =  '\0'; // Setzen Sie das Zeichen am Ende auf Nullterminator
        }
     
     // printf("BUFFER: %s\n", buffer);
      // Erstelle einen Dateinamen für den Benutzer
       
      std::ofstream outputFile;
     // printf("BUFFER: %s\n", buffer);
      /*Hier ist der Ablauf im Client:

Der Client sendet den "Send"-Befehl an den Server.
Der Client wartet auf die Bestätigung vom Server.
Sobald die Bestätigung empfangen wurde, überprüft der Client, ob es sich um "OK" oder "FAIL" handelt.
Abhängig von der Bestätigung kann der Client entweder fortfahren und die Nachricht senden, wenn die Bestätigung "OK" ist, oder eine entsprechende Fehlerbehandlung durchführen, wenn die Bestätigung "FAIL" ist.*/
if (strcmp(buffer, "S") == 0 || strcmp(buffer, "s") == 0) {
    // Sende "OK" an den Client, um die erfolgreiche Nachrichtenübertragung zu bestätigen
    if (send(create_socket, "OK", 3, 0) == -1) {
        send(create_socket, "FAIL", 5, 0);
        perror("Fehler beim Senden der Bestätigung an den Client");
    } else {
        printf("Client hat die Nachricht erfolgreich übertragen und Bestätigung gesendet\n");
// Hier können Sie die Verarbeitung der Nachricht einfügen
        // Überprüfen und Öffnen der Ausgabedatei
        struct stat st;
        if (stat(messagesDirectory.c_str(), &st) != 0) {
            mkdir(messagesDirectory.c_str(), 0777);
        }
        std::ofstream outputFile;
        std::string userFilename = messagesDirectory + "/" + send_.sender + "_messages.txt";
        outputFile.open(userFilename, std::ios::app);

        if (!outputFile) {
            std::cerr << "Fehler beim Öffnen der Ausgabedatei." << std::endl;
        } else {
            // ... Ihre Nachrichtenverarbeitungslogik hier ...
            // Stellen Sie sicher, dass Sie outputFile schließen, wenn Sie fertig sind.
    std::string line;
    
    std::cout << "Bitte geben Sie den Benutzernamen ein: ";
    std::cin.getline(send_.sender, BUF);
    
    // Erstelle einen Dateinamen für den Benutzer
    std::string userFilename = messagesDirectory + "/" + send_.sender + "_messages.txt";
    
    std::ofstream outputFile(userFilename, std::ios::app); // Öffne die Datei für die Ausgabe im Anhänge-Modus
    

        std::cout  << "SEND\n" << ">> Sender: " << send_.sender;
        std::cout << "\n>>Receiver: ";
        std::cin.getline(send_.receiver, BUF);
        std::cout << ">>Subject: ";
        std::cin.getline(send_.subject, 81);
        std::cout << ">>Message (Mehrzeilig eingeben und mit '.' beenden):\n";

        while (std::getline(std::cin, line)) {
            if (line == ".") {
                break;
            }
            send_.message += line + '\n';
        }

        // Erhöhe die Nachrichtennummer und schreibe sie in die Datei
        send_.messageNum++;
        outputFile << "Message Number: " << send_.messageNum << '\n';

        // Schreibe die Benutzereingaben in die Datei
        outputFile << "Sender:" << send_.sender << '\n';
        outputFile << "Receiver: " << send_.receiver << '\n';
        outputFile << "Subject: " << send_.subject << '\n';
        outputFile << "Message:\n" << send_.message;

            outputFile.close();
        }

        // Senden Sie "OK" als Bestätigung an den Server
        send(create_socket, "OK", 3, 0);
    }
      
    }
    if (buffer[0] == 'l' || buffer[0] == 'L') {
    std::cout << "LIST MESSAGES\n";
    std::cout << ">> Please enter the username: ";
    std::cin.getline(send_.sender, BUF);

    std::string listRequest = "LIST " + std::string(send_.sender);
    strncpy(buffer, listRequest.c_str(), BUF);

    if (send(create_socket, buffer, strlen(buffer), 0) == -1) {
        perror("send error");
        break;
    }

    // Receive the list of messages from the server
    size = recv(create_socket, buffer, BUF - 1, 0);
    if (size == -1) {
        perror("recv error");
        break;
    } else if (size == 0) {
        printf("Server closed remote socket\n");
        break;
    } else {
        buffer[size] = '\0';
        printf("Received list of messages:\n%s\n", buffer);
    }
}
    
if (strcmp(buffer, "Q") == 0 || strcmp(buffer, "q") == 0) {
    // Senden Sie den "Quit"-Befehl an den Server
    send(create_socket, buffer, size, 0);
   //printf("testBUFFER: %s\n", buffer);
    // Schließen Sie die Verbindung auf der Client-Seite
    if (shutdown(create_socket, SHUT_RDWR) == -1) {
        perror("shutdown create_socket");
    }
    if (close(create_socket) == -1) {
        perror("close create_socket");
    }
    create_socket = -1;

    // Beenden Sie das Programm
    break;
}
   //////////////////////////////////////////////////////////////////////
   //printf("BUFFER: %s\n", buffer);
   //printf("BUFFER: %s\n", buffer);
      }
   } while (strcmp(buffer, "quit") != 0 || !isQuit);

   ////////////////////////////////////////////////////////////////////////////
   // CLOSES THE DESCRIPTOR
   if (create_socket != -1)
   {
      if (shutdown(create_socket, SHUT_RDWR) == -1)
      {
         // invalid in case the server is gone already
         perror("shutdown create_socket"); 
      }
      if (close(create_socket) == -1)
      {
         perror("close create_socket");
      }
      create_socket = -1;
   }

   return EXIT_SUCCESS;
}