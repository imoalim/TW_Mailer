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
struct Sender
{
  char name[BUF];
  char receiver[BUF];
  char subject[81];
  int messageNum = 0;
  std::string message;
};

struct List{
   Sender sender;   
};


int main(int argc, char **argv)
{
   List list;
   Sender sender;
   int create_socket;
   char buffer[BUF];
   struct sockaddr_in address;
   int size;
    int isQuit = 0;
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


char expression[BUF] = {0};
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
      
      /*
if (fgets(buffer, BUF, stdin) != NULL)
      {
         int size = strlen(buffer);
         // remove new-line signs from string at the end
         if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
         {
            size -= 2;
            buffer[size] = 0;
         }
         else if (buffer[size - 1] == '\n')
         {
            --size;
            buffer[size] = 0;
         }
         isQuit = strcmp(buffer, "quit") == 0;
      }     
        break;
        */

      printf(">> If you want to SEND a message, type 'S'\n");
      printf(">> If you want to LIST all received messages of a specific user from his inbox, type 'L'\n");
      printf(">> If you want to READ a specific message of a specific user, type 'R'\n");
      printf(">> If you want to remove a specific message, type 'D'\n");
      printf(">> If you want to logout the client, type 'Q'\n");

      std::cin.getline(expression, BUF); // Liest eine Zeile ein, inklusive Leerzeichen
      // Erstelle einen Dateinamen für den Benutzer
   
      std::ofstream outputFile;


    std::string line;
   

   if (expression[0] == 's' || expression[0] == 'S') {
    struct stat st;
    if (stat(messagesDirectory.c_str(), &st) != 0) {
        mkdir(messagesDirectory.c_str(), 0777);
    }

    std::cout << "Bitte geben Sie den Benutzernamen ein: ";
    std::cin.getline(sender.name, BUF);

    std::string userFilename = messagesDirectory + "/" + sender.name + "_messages.txt";
    outputFile.open(userFilename, std::ios::app);

    std::cout << "SEND\n";
    std::cout << ">>Receiver: ";
    std::cin.getline(sender.receiver, BUF);
    std::cout << ">>Subject: ";
    std::cin.getline(sender.subject, 81);
    std::cout << ">>Message (Mehrzeilig eingeben und mit '.' beenden):\n";

    while (std::getline(std::cin, line)) {
       if (line == ".") {
          break;
       }
       sender.message += line + '\n';
    }

    sender.messageNum++;
    outputFile << "Message Number: " << sender.messageNum << '\n';
    outputFile << "Receiver: " << sender.receiver << '\n';
    outputFile << "Subject: " << sender.subject << '\n';
    outputFile << "Message:\n" << sender.message;

    outputFile.close();
}
else if (expression[0] == 'l' || expression[0] == 'L') {
    // TODO: Add code to list all messages received by a specific user
    std::cout << "Please enter the username: ";
    std::cin.getline(sender.name, BUF);
    std::string userFilename = messagesDirectory + "/" + sender.name + "_messages.txt";

    std::ifstream inputFile(userFilename);
    if (!inputFile) {
       std::cout << "No messages for user: " << sender.name << std::endl;
    }
    else {
       std::cout << "Messages for user: " << sender.name << std::endl;
       std::cout << "-----------------------------------\n";
          
       while (std::getline(inputFile, line)) {
          std::cout << line << std::endl;
       }

       inputFile.close();
    }
}
else if (expression[0] == 'q' || expression[0] == 'Q') {
    isQuit = 0;
}
         //////////////////////////////////////////////////////////////////////
   
   } while (strcmp(expression, "quit") != 0 &&!isQuit);






         ////////////////////////////////////////////////////////////////////

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
