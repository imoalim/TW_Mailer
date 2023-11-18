#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <dirent.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_set> // Zum Überprüfen der Eindeutigkeit der Message Numbers
#include <ldap.h> 

///////////////////////////////////////////////////////////////////////////////

#define BUF 1024
#define PORT 6543

///////////////////////////////////////////////////////////////////////////////

int abortRequested = 0;
int create_socket = -1;
int new_socket = -1;

///////////////////////////////////////////////////////////////////////////////

void *clientCommunication(void *data);
void signalHandler(int sig);

///////////////////////////////////////////////////////////////////////////////

// Funktion zum Extrahieren von Nachrichteninformationen
struct MessageInfo
{
   int messageNumber;
   std::string subject;
};

struct LDAPServer{
   std::string Host = "ldap.technikum.wien.at";
   int16_t Port = 389;
   std::string SearchBase = "dc=technikum-wien,dc=at";
}

// Funktion zum Extrahieren von Nachrichteninformationen
std::vector<MessageInfo> extractMessageInfo(const std::string &filepath)
{
   std::ifstream file(filepath);
   std::vector<MessageInfo> messages;

   if (file.is_open())
   {
      std::string line;
      int currentMessageNumber = 0;
      std::unordered_set<int> uniqueMessageNumbers; // Um die Eindeutigkeit der Message Numbers sicherzustellen

      while (std::getline(file, line))
      {
         if (line.find("Message Number:") != std::string::npos)
         {
            // Extrahiere die Message Number aus der Zeile
            int extractedMessageNumber = 0;
            std::istringstream(line.substr(line.find(":") + 1)) >> extractedMessageNumber;

            // Überprüfe, ob die Message Number eindeutig ist
            while (uniqueMessageNumbers.find(extractedMessageNumber) != uniqueMessageNumbers.end())
            {
               // Wenn nicht eindeutig, erhöhe die Message Number
               extractedMessageNumber++;
            }

            // Aktualisiere die aktuelle Message Number
            currentMessageNumber = extractedMessageNumber;

            // Füge die Message Number zur Menge der eindeutigen Message Numbers hinzu
            uniqueMessageNumbers.insert(currentMessageNumber);
         }
         else if (line.find("Subject:") != std::string::npos)
         {
            // Extrahiere das Subject aus der Zeile
            std::string subject = line.substr(line.find(":") + 2);

            // Füge die MessageInfo zum Vektor hinzu
            messages.push_back({currentMessageNumber, subject});
         }
      }

      file.close();
   }
   else
   {
      std::cerr << "Unable to open file: " << filepath << std::endl;
   }

   return messages;
}

// Funktion zum Extrahieren von Nachrichten
std::string extractMessages(const std::string &username)
{
   DIR *dir;
   struct dirent *entry;
   std::stringstream allMessages;
   int totalMessages = 0;
   std::vector<std::string> allSubjects;

   dir = opendir("messages");
   if (dir == NULL)
   {
      perror("Unable to open messages directory");
      return "";
   }

   // Loop durch das Verzeichnis, um Dateien zu finden, die zum Benutzer passen
   while ((entry = readdir(dir)) != NULL)
   {
      if (entry->d_type == DT_REG && strstr(entry->d_name, username.c_str()) != NULL)
      {
         std::string filepath = "messages/" + std::string(entry->d_name);
         std::vector<MessageInfo> messageInfo = extractMessageInfo(filepath);

         for (const auto &info : messageInfo)
         {
            totalMessages++;
            allSubjects.push_back(info.subject);
         }
      }
   }
   closedir(dir);

   // Erstellen der Antwort im gewünschten Format
   allMessages << totalMessages << "\n";
   for (const auto &subject : allSubjects)
   {
      allMessages << subject << "\n";
   }

   return allMessages.str();
}

struct InputSplitter
{
   static std::vector<std::string> split(const char *buffer, int size, char delimiter)
   {
      std::vector<std::string> result;
      std::string current;

      for (int i = 0; i < size; i++)
      {
         if (buffer[i] != delimiter)
         {
            current += buffer[i];
         }
         else
         {
            result.push_back(current);
            current = "";
         }
      }

      // Überprüfen, ob 'current' noch Daten enthält, und sie zum Vektor hinzufügen
      if (!current.empty())
      {
         result.push_back(current);
      }

      return result;
   }
};

int main(void)
{
   socklen_t addrlen;
   struct sockaddr_in address, cliaddress;
   int reuseValue = 1;

   ////////////////////////////////////////////////////////////////////////////
   // SIGNAL HANDLER
   // SIGINT (Interrup: ctrl+c)
   // https://man7.org/linux/man-pages/man2/signal.2.html
   if (signal(SIGINT, signalHandler) == SIG_ERR)
   {
      perror("signal can not be registered");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // CREATE A SOCKET
   // https://man7.org/linux/man-pages/man2/socket.2.html
   // https://man7.org/linux/man-pages/man7/ip.7.html
   // https://man7.org/linux/man-pages/man7/tcp.7.html
   // IPv4, TCP (connection oriented), IP (same as client)
   if ((create_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
   {
      perror("Socket error"); // errno set by socket()
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // SET SOCKET OPTIONS
   // https://man7.org/linux/man-pages/man2/setsockopt.2.html
   // https://man7.org/linux/man-pages/man7/socket.7.html
   // socket, level, optname, optvalue, optlen
   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEADDR,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reuseAddr");
      return EXIT_FAILURE;
   }

   if (setsockopt(create_socket,
                  SOL_SOCKET,
                  SO_REUSEPORT,
                  &reuseValue,
                  sizeof(reuseValue)) == -1)
   {
      perror("set socket options - reusePort");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // INIT ADDRESS
   // Attention: network byte order => big endian
   memset(&address, 0, sizeof(address));
   address.sin_family = AF_INET;
   address.sin_addr.s_addr = INADDR_ANY;
   address.sin_port = htons(PORT);

   ////////////////////////////////////////////////////////////////////////////
   // ASSIGN AN ADDRESS WITH PORT TO SOCKET
   if (bind(create_socket, (struct sockaddr *)&address, sizeof(address)) == -1)
   {
      perror("bind error");
      return EXIT_FAILURE;
   }

   ////////////////////////////////////////////////////////////////////////////
   // ALLOW CONNECTION ESTABLISHING
   // Socket, Backlog (= count of waiting connections allowed)
   if (listen(create_socket, 5) == -1)
   {
      perror("listen error");
      return EXIT_FAILURE;
   }

   while (!abortRequested)
   {
      /////////////////////////////////////////////////////////////////////////
      // ignore errors here... because only information message
      // https://linux.die.net/man/3/printf
      printf("Waiting for connections...\n");

      /////////////////////////////////////////////////////////////////////////
      // ACCEPTS CONNECTION SETUP
      // blocking, might have an accept-error on ctrl+c
      addrlen = sizeof(struct sockaddr_in);
      if ((new_socket = accept(create_socket,
                               (struct sockaddr *)&cliaddress,
                               &addrlen)) == -1)
      {
         if (abortRequested)
         {
            perror("accept error after aborted");
         }
         else
         {
            perror("accept error");
         }
         break;
      }

      /////////////////////////////////////////////////////////////////////////
      // START CLIENT
      // ignore printf error handling
      printf("Client connected from %s:%d...\n",
             inet_ntoa(cliaddress.sin_addr),
             ntohs(cliaddress.sin_port));
      clientCommunication(&new_socket); // returnValue can be ignored
      new_socket = -1;
   }

   // frees the descriptor
   if (create_socket != -1)
   {
      if (shutdown(create_socket, SHUT_RDWR) == -1)
      {
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

void *clientCommunication(void *data)
{
   char buffer[BUF];
   int size;
   int *current_socket = (int *)data;

   ////////////////////////////////////////////////////////////////////////////
   // SEND welcome message
   strcpy(buffer, "Welcome to myserver!\r\nPlease enter your commands...\r\n");
   if (send(*current_socket, buffer, strlen(buffer), 0) == -1)
   {
      perror("send failed");
      return NULL;
   }

   do
   {
      /////////////////////////////////////////////////////////////////////////
      // RECEIVE

      // printf("BUFFERonServer: %s\n", buffer);
      memset(buffer, 0, sizeof(buffer)); // Setzen Sie den gesamten Puffer auf Null
      size = recv(*current_socket, buffer, BUF - 1, 0);

      // printf("SIZE: %d\n", size);
      if (size == -1)
      {
         if (abortRequested)
         {
            perror("recv error after aborted");
         }
         else
         {
            perror("recv error");
         }
         break;
      }
      if (size == 0)
      {
         printf("Client closed remote socket\n"); // ignore error
         break;
      }

      // remove ugly debug message, because of the sent newline of client
      if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
      {
         size -= 2;
      }
      else if (buffer[size - 1] == '\n')
      {
         --size;
      }

      buffer[size] = '\0';
      // TODO:: falls OK nicht InputSplittre aufrufen

      // bearbeite den Input
      std::vector<std::string> input = InputSplitter::split(buffer, size, '\n');

      std::string command = input[0];
      bool loggedIn = false;
      if(command == "Login"){
         std::string LDAPUsername = input[1];
         std::string LDAPPassword = input[2];
         loggedIn = true;
      }

      switch (command[0])
      {
      case 's':
      case 'S':
      if(loggedIn) printf("logedIN");
      //TODO:: es geht beim send befehl zweimal hinein. FIX BUG
      {
         if (send(*current_socket, "OK", 3, 0) == -1)
         {
            send(*current_socket, "FAIL", 5, 0);
            perror("Fehler beim Senden der Bestätigung an den Client");
         }
         else
         {
            printf("Client hat die Nachricht erfolgreich übertragen und Bestätigung gesendet\n");

            send(*current_socket, "OK", 3, 0);
         }
         // Falls ein Fehler auftritt:
         // send(create_socket, "FAIL", 4, 0);
         break;
      }
      case 'l':
      case 'L':
      {
         char username[BUF];
         strcpy(username, input[1].c_str());

         // Nachrichteninformationen extrahieren
         int totalMessages = 0;
         std::vector<std::string> allSubjects;

         DIR *dir;
         struct dirent *entry;

         dir = opendir("messages");
         if (dir == NULL)
         {
            perror("Unable to open messages directory");
            return NULL;
         }

         // Loop durch das Verzeichnis, um Dateien zu finden, die zum Benutzer passen
         while ((entry = readdir(dir)) != NULL)
         {
            if (entry->d_type == DT_REG && strstr(entry->d_name, username) != NULL)
            {
               std::string filepath = "messages/" + std::string(entry->d_name);

               // Überprüfe, ob die Datei existiert, bevor sie geöffnet wird
               if (access(filepath.c_str(), F_OK) != -1)
               {
                  // Hier speicherst du die zurückgegebenen Informationen
                  std::vector<MessageInfo> messageInfo = extractMessageInfo(filepath);

                  int messageCount = messageInfo.size();
                  std::vector<std::string> subjects;

                  // Hier fügst du die Betreffzeilen zur allSubjects hinzu
                  for (const auto &info : messageInfo)
                  {
                     subjects.push_back(info.subject);
                  }

                  totalMessages += messageCount;
                  allSubjects.insert(allSubjects.end(), subjects.begin(), subjects.end());
               }
               else
               {
                  std::cerr << "File does not exist: " << filepath << std::endl;
                  // Sende eine Fehlermeldung an den Client
                  send(*current_socket, "FAIL: File does not exist", 26, 0);
                  return NULL;
               }
            }
         }
         closedir(dir);

         // Zeige die Nachrichteninformationen auf der Serverkonsole an
         printf("Anzahl der Nachrichten für Benutzer %s: %d\n", username, totalMessages);
         printf("Betreff der Nachrichten:\n");

         for (const auto &subject : allSubjects)
         {
            printf("%s\n", subject.c_str());
         }

         // Sende "OK" an den Client, wenn die Nachrichteninformationen erfolgreich angezeigt wurden
         if (send(*current_socket, "OK", 3, 0) == -1)
         {
            perror("send OK failed");
            return NULL;
         }

         break;
      }

      case 'q':
      case 'Q':
         // Senden Sie den "Quit"-Befehl an den Server
         printf("Nachricht Quit erhalten: %s\n", input[BUF].c_str());
         break;

      default:
         printf("Nachricht erhalten: %s\n", buffer); // Fehler ignorieren
         break;
      }
      if (send(*current_socket, "OK", 3, 0) == -1)
      {
         if (send(*current_socket, "OK", 3, 0) == -1)
         {
            perror("send answer failed");
            return NULL;
         }
      }
   } while (strcmp(buffer, "q") != 0 || !abortRequested);

   // closes/frees the descriptor if not already
   if (*current_socket != -1)
   {
      if (shutdown(*current_socket, SHUT_RDWR) == -1)
      {
         perror("\nshutdown new_socket");
      }
      if (close(*current_socket) == -1)
      {
         perror("\nclose new_socket");
      }
      *current_socket = -1;
   }

   return NULL;
}

void signalHandler(int sig)
{
   if (sig == SIGINT)
   {
      printf("abort Requested... "); // ignore error
      abortRequested = 1;
      /////////////////////////////////////////////////////////////////////////
      // With shutdown() one can initiate normal TCP close sequence ignoring
      // the reference count.
      // https://beej.us/guide/bgnet/html/#close-and-shutdownget-outta-my-face
      // https://linux.die.net/man/3/shutdown
      if (new_socket != -1)
      {
         if (shutdown(new_socket, SHUT_RDWR) == -1)
         {
            perror("\nshutdown new_socket");
         }
         if (close(new_socket) == -1)
         {
            perror("\nclose new_socket");
         }
         new_socket = -1;
      }

      if (create_socket != -1)
      {
         if (shutdown(create_socket, SHUT_RDWR) == -1)
         {
            perror("\nshutdown create_socket");
         }
         if (close(create_socket) == -1)
         {
            perror("\nclose create_socket");
         }
         create_socket = -1;
      }
   }
   else
   {
      exit(sig);
   }
}