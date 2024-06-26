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
#include <unistd.h> // Für getpass
#include <limits>

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543

///////////////////////////////////////////////////////////////////////////////

struct Send
{
   char sender[9];
   char receiver[BUF];
   char subject[81];
   int messageNum;
   std::string message;
};
struct Info
{
   char loggedUsername[9];
   bool loggedIn = false;
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
   struct Info info;

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
      printf("SEND, LIST, READ, DELETE is only for authenticated users\n>>If you want to SEND a message, type 'Send'\n");
      printf(">> If you want to LOG IN type 'Login'\n");
      printf(">> If you want to LIST all received messages of a specific user from his inbox, type 'List'\n");
      printf(">> If you want to READ a specific message of a specific user, type 'Read'\n");
      printf(">> If you want to remove a specific message, type 'Delete'\n");
      printf(">> If you want to logout the client, type 'Quit'\n");
      char clientInput_array[BUF];
      clientInput_array[0] = '\0';
      std::vector<std::string> inputs;

      // buffer auf null setzten und clientInputarray
      //  Um das Array zu leeren
      memset(clientInput_array, 0, sizeof(clientInput_array[0]) * BUF);
      memset(buffer, 0, sizeof(buffer[0]) * BUF);

      if (fgets(clientInput_array, BUF, stdin) != NULL)
      {
         int size = strlen(clientInput_array);

         // Entfernen von Zeilenumbruchzeichen ('\n' oder '\r\n') am Ende der Benutzereingabe
         if (size >= 2 && (clientInput_array[size - 1] == '\n' || clientInput_array[size - 1] == '\r'))
         {
            clientInput_array[size - 1] = '\0'; //  Zeichen am Ende auf Nullterminator
         }

         if (strcmp(clientInput_array, "Login") == 0 || strcmp(clientInput_array, "login") == 0)
         {
            inputs.push_back(std::string(clientInput_array));
            char LDAPUsername[9];
            char *LDAPPassword;

            std::cout << "LogIn\nEnterUsername (max 8 characters): ";
            std::cin >> LDAPUsername;
            inputs.push_back(LDAPUsername);

            LDAPPassword = getpass("Enter password: ");
            inputs.push_back(LDAPPassword);

            std::string combinedString;
            for (const auto &input : inputs)
            {
               combinedString += input + "\n";
            }
            size = strlen(buffer);
            strcpy(buffer, combinedString.c_str());
            if ((send(create_socket, buffer, strlen(buffer), 0)) == -1)
            {
               perror("send error");
               break;
            }
            char serverResponse[BUF];
            recv(create_socket, serverResponse, sizeof(serverResponse), 0);

            //  '\0' ist Trennzeichen
            char *token = strtok(serverResponse, "\0");

            // Array, um die aufgeteilten Teile zu speichern
            std::vector<std::string> responseParts;

            // Durchlaufe alle Teile
            while (token != nullptr)
            {
               // Füge das aktuelle Teil zum Array hinzu
               responseParts.push_back(token);
               // Hole das nächste Teil
               token = strtok(nullptr, "\0");
            }

            if (responseParts.size() > 0)
            {
               std::cout << "Erstes Teil: " << responseParts[0] << std::endl;
               send(create_socket, "OK", 3, 0);
            }

            // Überprüfe die Bestätigung vom Server
            if (responseParts[0] == "loggedIn")
            {
               info.loggedIn = true;

               // Benutzernamen vom Server empfangen und speichern

               recv(create_socket, info.loggedUsername, sizeof(info.loggedUsername), 0);

               std::cout << "Erfolgreich eingeloggt als: " << info.loggedUsername << std::endl;
            }
            else
            {
               std::cerr << "Fehler beim Einloggen." << std::endl;
               continue;
            }
         }

         std::ofstream outputFile;
         if (info.loggedIn)
         {
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
                  std::string line;
                  strncpy(send_.sender, info.loggedUsername, sizeof(send_.sender) - 1);
                  send_.sender[sizeof(send_.sender) - 1] = '\0'; // Array mit Nullterminator endet

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

                  send_.message.clear(); // Zuerst den Inhalt von send_.message leeren

                  while (std::getline(std::cin, line))
                  {
                     if (line == ".")
                     {
                        break;
                     }
                     send_.message += line;
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
               strcpy(buffer, combinedString.c_str());
            }
            if (strcmp(clientInput_array, "List") == 0 || strcmp(clientInput_array, "list") == 0)
            {
               inputs.push_back(std::string(clientInput_array));
               inputs.push_back(std::string(info.loggedUsername));
               std::string combinedString;
               for (const auto &input : inputs)
               {
                  combinedString += input + "\n";
               }
               strcpy(buffer, combinedString.c_str());

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
               }
            }
            if (strcmp(clientInput_array, "Read") == 0 || strcmp(clientInput_array, "read") == 0)
            {
               char messageNumber[BUF];
               inputs.push_back(std::string(clientInput_array));
               inputs.push_back(std::string(info.loggedUsername));
               std::cout << "READ MESSAGES\nWhich Messsage number: ";
               std::cin.getline(messageNumber, BUF);
               inputs.push_back(std::string(messageNumber));
               std::string combinedString;
               for (const auto &input : inputs)
               {
                  combinedString += input + "\n";
               }

               strcpy(buffer, combinedString.c_str());

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
               }
            }

            if (strcmp(clientInput_array, "Delete") == 0 || strcmp(clientInput_array, "delete") == 0)
            {

               inputs.push_back(std::string(clientInput_array));
               inputs.push_back(std::string(info.loggedUsername));
               std::cout << "Give message number\nNumber: ";
               int messageNum;
               std::cin >> messageNum;
               std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
               inputs.push_back(std::to_string(messageNum));

               std::string combinedString;
               for (const auto &input : inputs)
               {
                  combinedString += input + "\n";
               }
               // 'combinedString' in einen const char* umwandeln
               strcpy(buffer, combinedString.c_str());

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
               }
            }
         }
         else
         {
            printf("\nPlease log in to use all commands\n");
         }

         if (strcmp(clientInput_array, "Quit") == 0 || strcmp(clientInput_array, "quit") == 0)
         {
            // Senden Sie den "Quit" an den Server
            send(create_socket, buffer, size, 0);
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