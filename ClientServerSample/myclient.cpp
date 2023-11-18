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
#include <vector>
#include <sstream>

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

struct List
{
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
   std::string clientInput;

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
      char clientInput_array[BUF]; // MAX_SIZE entsprechend der maximal erwarteten Größe setzen
      clientInput_array[0] = '\0'; // Initialisiere mit einem leeren String
      std::vector<std::string> inputs;

      // buffer auf null setzten und clientInputarray
      //  Um das Array zu leeren
      memset(clientInput_array, 0, sizeof(clientInput_array[0]) * BUF);
      memset(buffer, 0, sizeof(buffer[0]) * BUF);

      if (fgets(clientInput_array, BUF, stdin) != NULL)
      {
         int size = strlen(clientInput_array);

         // Entfernen Sie Zeilenumbruchzeichen ('\n' oder '\r\n') am Ende der Benutzereingabe
         if (size >= 2 && (clientInput_array[size - 1] == '\n' || clientInput_array[size - 1] == '\r'))
         {
            clientInput_array[size - 1] = '\0'; // Setzen Sie das Zeichen am Ende auf Nullterminator
         }

         if (strcmp(clientInput_array, "Login") == 0 || strcmp(clientInput_array, "login") == 0)
         {
            inputs.push_back(std::string(clientInput_array));
            char LDAPUsername[9];
            char LDAPPassword[BUF];

            std::cout << "LogIn\nEnterUsername (max 8 characters): ";
            std::cin >> LDAPUsername;
            inputs.push_back(LDAPUsername);

            std::cout << "\nEnterPassword: ";
            std::cin >> LDAPPassword;
            inputs.push_back(LDAPPassword);

            std::string combinedString;
            for (const auto &input : inputs)
            {
               combinedString += input + "\n";
            }
            // 'combinedString' in einen const char* umwandeln
            strcpy(buffer, combinedString.c_str());
            if ((send(create_socket, buffer, strlen(buffer), 0)) == -1)
            {
               perror("send error");
               break;
            }

            //(gdb) p buffe
            //$8 = "Login\nif22b252\nYusra2010\n", '\000' <repeats 998 times>
            // memset(buffer, 0, sizeof(buffer[0])*BUF);
         }

         std::ofstream outputFile;

         if (strcmp(clientInput_array, "Send") == 0 || strcmp(clientInput_array, "send") == 0)
         {
            inputs.push_back(std::string(clientInput_array));

            // Überprüfen und Öffnen der Ausgabedatei
            struct stat st;
            if (stat(messagesDirectory.c_str(), &st) != 0)
            {
               mkdir(messagesDirectory.c_str(), 0777);
            }
            std::string userFilename = messagesDirectory + "/" + send_.sender + "_messages.txt";
            outputFile.open(userFilename, std::ios::app);

            if (!outputFile)
            {
               std::cerr << "Fehler beim Öffnen der Ausgabedatei." << std::endl;
            }
            else
            {
               // ... Ihre Nachrichtenverarbeitungslogik hier ...
               // Stellen Sie sicher, dass Sie outputFile schließen, wenn Sie fertig sind.
               std::string line;

               std::cout << "Bitte geben Sie den Benutzernamen ein: ";
               std::cin.getline(send_.sender, BUF);

               // Erstelle einen Dateinamen für den Benutzer
               std::string userFilename = messagesDirectory + "/" + send_.sender + "_messages.txt";

               std::ofstream outputFile(userFilename, std::ios::app); // Öffne die Datei für die Ausgabe im Anhänge-Modus

               std::cout << "SEND\n"
                         << ">> Sender: " << send_.sender;
               std::cout << "\n>>Receiver: ";
               std::cin.getline(send_.receiver, BUF);
               std::cout << ">>Subject: ";
               std::cin.getline(send_.subject, 81);
               std::cout << ">>Message (Mehrzeilig eingeben und mit '.' beenden):\n";

               while (std::getline(std::cin, line))
               {
                  if (line == ".")
                  {
                     break;
                  }
                  send_.message += line + '\n';
               }

               // Erhöhe die Nachrichtennummer und schreibe sie in die Datei
               int lastMessageNum = 0;
               std::ifstream inputFile(userFilename);
               if (inputFile.is_open())
               {
                  std::string line;
                  while (std::getline(inputFile, line))
                  {
                     std::istringstream iss(line);
                     std::string token;
                     if (iss >> token && token == "Message" && iss >> token && token == "Number:")
                     {
                        iss >> lastMessageNum;
                     }
                  }
                  inputFile.close();
               }

               send_.messageNum = lastMessageNum + 1;

               // Schreibe die Benutzereingaben in die Datei
               outputFile << "Message Number: " << send_.messageNum << '\n';
               outputFile << "Sender:" << send_.sender << '\n';
               outputFile << "Receiver: " << send_.receiver << '\n';
               outputFile << "Subject: " << send_.subject << '\n';
               outputFile << "Message:\n"
                          << send_.message;

               outputFile.close();
            }

            // Senden Sie "OK" als Bestätigung an den Server
            inputs.push_back(send_.sender);
            inputs.push_back(send_.receiver);
            inputs.push_back(send_.message);
            inputs.push_back(send_.subject);
            inputs.push_back(std::to_string(send_.messageNum));

            memset(buffer, 0, sizeof(buffer[0]) * BUF);
            // Elemente des Vektors in den Buffer kopieren
            // Einen einzelnen std::string erstellen, in dem die Elemente mit \n getrennt sind
            std::string combinedString;
            for (const auto &input : inputs)
            {
               combinedString += input + "\n";
            }
            // 'combinedString' in einen const char* umwandeln
            strcpy(buffer, combinedString.c_str());
            // memset(buffer, 0, sizeof(buffer[0])*BUF);
            // size_t SendBuffer_size = strlen(buffer);
            // send(create_socket, buffer, SendBuffer_size, 0);
         }
         if (strcmp(clientInput_array, "List") == 0 || strcmp(clientInput_array, "list") == 0)
         // TODO:: falsche eingaben verweigeinere. Sonst kann es zu SigFault kommen
         {
            inputs.push_back(std::string(clientInput_array));
            std::cout << "LIST MESSAGES\nUsername: ";
            std::cin.getline(send_.sender, BUF);
            inputs.push_back(std::string(send_.sender));
            std::string combinedString;
            for (const auto &input : inputs)
            {
               combinedString += input + "\n";
            }
            // 'combinedString' in einen const char* umwandeln
            strcpy(buffer, combinedString.c_str());
            // memset(buffer, 0, sizeof(buffer[0])*BUF);
            // size_t SendBuffer_size = strlen(buffer);

            // std::string listRequest = std::string(send_.sender);
            // strncpy(buffer, listRequest.c_str(), BUF);

            if (send(create_socket, buffer, strlen(buffer), 0) == -1)
            {
               perror("send error");
               break;
            }

            // Receive the list of messages from the server
            size = recv(create_socket, buffer, BUF - 1, 0);
            if (size == -1)
            {
               perror("recv error");
               break;
            }
            else if (size == 0)
            {
               printf("Server closed remote socket\n");
               break;
            }
            else
            {
               buffer[size] = '\0';
               // printf("Received list of messages:\n%s\n", buffer);
               // send(create_socket, "OK", 3, 0);
            }
         }

         if (strcmp(buffer, "Quit") == 0 || strcmp(buffer, "quit") == 0)
         {
            // Senden Sie den "Quit"-Befehl an den Server
            send(create_socket, buffer, size, 0);
            // printf("testBUFFER: %s\n", buffer);
            //  Schließen Sie die Verbindung auf der Client-Seite
            if (shutdown(create_socket, SHUT_RDWR) == -1)
            {
               perror("shutdown create_socket");
            }
            if (close(create_socket) == -1)
            {
               perror("close create_socket");
            }
            create_socket = -1;

            // Beenden Sie das Programm
            break;
         }
         //////////////////////////////////////////////////////////////////////
         // printf("BUFFER: %s\n", buffer);
         //  printf("BUFFER: %s\n", buffer);
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