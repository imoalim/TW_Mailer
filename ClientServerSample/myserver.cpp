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
#include <algorithm>

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
struct Message
{
   std::string number;
   std::string text;
};

// Struktur, die die Konfiguration für den LDAP-Server enthält
struct LDAPServer
{
   std::string Host = "ldap.technikum.wien.at";        // Hostname des LDAP-Servers
   int16_t Port = 389;                                 // Portnummer für die Verbindung zum LDAP-Server
   std::string SearchBase = "dc=technikum-wien,dc=at"; // Basis für LDAP-Suchanfragen
   // Anonymer Bind mit Benutzer und Passwort leer
   const char *ldapUri = "ldap://ldap.technikum-wien.at:389"; // URI für die Verbindung zum LDAP-Server
   const int ldapVersion = LDAP_VERSION3;                     // LDAP-Protokollversion
   // Benutzername aus der Umgebungsvariable extrahieren (bash: export ldapuser=<yourUsername>)
   char ldapBindUser[256];     // Bind-Benutzer im LDAP-Format
   char ldapBindPassword[256]; // Bind-Passwort
   char rawLdapUser[128];      // Roher Benutzername
   std::string LDAPUsername;   // Benutzername im C++-String-Format
   std::string LDAPPassword;   // Passwort im C++-String-Format
   bool loggedIn = false;      // Flag, das angibt, ob der Benutzer angemeldet ist
};

// Funktion für die LDAP-Authentifizierung
int ldap(const char *username, const char *password)
{
   // Erstellen einer Instanz der LDAPServer-Struktur
   struct LDAPServer ldapServer;

   // Kopieren des Benutzernamens in die Struktur
   strcpy(ldapServer.rawLdapUser, username);
   // Erstellen des Bind-Benutzernamens im LDAP-Format
   sprintf(ldapServer.ldapBindUser, "uid=%s,ou=people,dc=technikum-wien,dc=at", ldapServer.rawLdapUser);
   printf("Benutzer auf: %s gesetzt\n", ldapServer.ldapBindUser);

   // Kopieren des Passworts in die Struktur
   strcpy(ldapServer.ldapBindPassword, password);

   int rc = 0; // Variable zur Speicherung von LDAP-Rückgabewerten

   // LDAP-Handle erstellen und initialisieren
   LDAP *ldapHandle;
   rc = ldap_initialize(&ldapHandle, ldapServer.ldapUri);
   if (rc != LDAP_SUCCESS)
   {
      fprintf(stderr, "ldap_init fehlgeschlagen\n");
      return EXIT_FAILURE;
   }
   printf("Mit LDAP-Server %s verbunden\n", ldapServer.ldapUri);

   // LDAP-Option für die Protokollversion setzen
   rc = ldap_set_option(
       ldapHandle,
       LDAP_OPT_PROTOCOL_VERSION,
       &ldapServer.ldapVersion);
   if (rc != LDAP_OPT_SUCCESS)
   {
      fprintf(stderr, "ldap_set_option(PROTOCOL_VERSION): %s\n", ldap_err2string(rc));
      ldap_unbind_ext_s(ldapHandle, NULL, NULL);
      return EXIT_FAILURE;
   }

   // TLS initialisieren
   rc = ldap_start_tls_s(
       ldapHandle,
       NULL,
       NULL);
   if (rc != LDAP_SUCCESS)
   {
      fprintf(stderr, "ldap_start_tls_s(): %s\n", ldap_err2string(rc));
      ldap_unbind_ext_s(ldapHandle, NULL, NULL);
      return EXIT_FAILURE;
   }

   // Binden an den LDAP-Server
   BerValue bindCredentials;
   bindCredentials.bv_val = (char *)ldapServer.ldapBindPassword;
   bindCredentials.bv_len = strlen(ldapServer.ldapBindPassword);
   BerValue *servercredp;
   rc = ldap_sasl_bind_s(
       ldapHandle,
       ldapServer.ldapBindUser,
       LDAP_SASL_SIMPLE,
       &bindCredentials,
       NULL,
       NULL,
       &servercredp);
   if (rc == LDAP_SUCCESS)
   {
      // Erfolgreich gebunden
      printf("LDAP-Bind erfolgreich\n");
      ldap_unbind_ext_s(ldapHandle, NULL, NULL);
      return true;
   }
   else
   {
      // Fehler beim Binden
      fprintf(stderr, "LDAP-Bindfehler: %s\n", ldap_err2string(rc));
      ldap_unbind_ext_s(ldapHandle, NULL, NULL);
      return false;
   }
}

/// Funktion zur Extraktion des Nachrichtentextes
std::string extractMessageText(const std::string &fullText)
{
   // Startposition des Nachrichtentextes finden
   size_t startPos = fullText.find("Message: ");
   if (startPos != std::string::npos)
   {
      // Startindex auf die Position nach "Message: " verschieben
      startPos += 9;

      // Endposition des Nachrichtentextes finden (bis zur nächsten "Next Message")
      size_t endPos = fullText.find("\nNext Message");
      if (endPos != std::string::npos)
      {
         // Den Nachrichtentext extrahieren und zurückgeben
         return fullText.substr(startPos, endPos - startPos);
      }
   }

   // Rückgabe eines leeren Texts, wenn das Format nicht erkannt wird
   return "";
}

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
   /**
    * Teilt einen C-String anhand eines Trennzeichens auf.
    *
    * @param buffer Der zu teilende C-String.
    * @param size Die Größe des C-Strings.
    * @param delimiter Das Trennzeichen für die Aufteilung.
    * @return Ein Vektor von Teilen des C-Strings.
    */
   static std::vector<std::string> split(const char *buffer, int size, char delimiter)
   {
      std::vector<std::string> result; // Ergebnisvektor für die Teile des C-Strings
      std::string current;             // Aktueller Teilstring, der aufgebaut wird

      for (int i = 0; i < size; i++)
      {
         if (buffer[i] != delimiter)
         {
            // Zeichen zum aktuellen Teilstring hinzufügen
            current += buffer[i];
         }
         else
         {
            // Aktuellen Teilstring zum Ergebnisvektor hinzufügen und zurücksetzen
            result.push_back(current);
            current = "";
         }
      }

      // Überprüfen, ob 'current' noch Daten enthält, und sie zum Vektor hinzufügen
      if (!current.empty())
      {
         result.push_back(current);
      }

      return result; // Rückgabe des Vektors mit den Teilen des C-Strings
   }
};

enum ValidCommand
{
   LIST,
   SEND,
   LOGIN,
   READ,
   QUIT,
   Delete
};

bool checkCommand(const std::string &command)
{
   // Gültige Befehle als Vektor von Strings
   std::vector<std::string> validCommands = {"List", "list", "Send", "send", "Login", "login", "Read", "read", "Quit", "quit", "Delete", "delete"};

   // Umwandlung des Eingabebefehls zu Kleinbuchstaben
   std::string lowercaseCommand = command;
   std::transform(lowercaseCommand.begin(), lowercaseCommand.end(), lowercaseCommand.begin(), ::tolower);

   // Überprüfung, ob der Befehl in den gültigen Befehlen enthalten ist
   return std::find(validCommands.begin(), validCommands.end(), lowercaseCommand) != validCommands.end();
}

int main(void)
{
   socklen_t addrlen;
   struct sockaddr_in address, cliaddress;
   int reuseValue = 1;
   struct MessageInfo messageInfo;

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
   struct LDAPServer ldapServer;
   struct MessageInfo messageInfo;

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

      // bearbeite den Input
      std::vector<std::string> input = InputSplitter::split(buffer, size, '\n');

      std::string command = input[0];

      if (checkCommand(command))
      {

         if (command == "Login" || command == "login")
         {
            ldapServer.LDAPUsername = input[1].substr(0, 9);   // Begrenzt auf die ersten 9 Zeichen
            ldapServer.LDAPPassword = input[2].substr(0, BUF); // Begrenzt auf die ersten BUF Zeichen
            if (ldap(ldapServer.LDAPUsername.c_str(), ldapServer.LDAPPassword.c_str()))
            {
               send(*current_socket, "loggedIn", 9, 0);

               // Warte auf die Bestätigung, dass der Benutzer eingeloggt ist
               char serverResponse[BUF];
               recv(*current_socket, serverResponse, sizeof(serverResponse), 0);

               if (strcmp(serverResponse, "OK") == 0)
               {
                  send(*current_socket, ldapServer.LDAPUsername.c_str(), strlen(ldapServer.LDAPUsername.c_str()), 0);

                  ldapServer.loggedIn = true;
                  printf("Benutzer %s ist eingeloggt.\n", ldapServer.LDAPUsername.c_str());
               }
               else
               {
                  send(*current_socket, "FAIL", 5, 0);
                  perror("\nLDAP-Fehler");
               }
            }
         }
         if (ldapServer.loggedIn)
         {
            if (command == "Send" || command == "send")
            {
               printf("logedIN");
               // TODO:: es geht beim send befehl zweimal hinein. FIX BUG
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
               }
            }
            if (command == "Read" || command == "read")
            {
               char username[BUF];
               char messageNumber[BUF];
               strcpy(username, input[1].c_str());
               strcpy(messageNumber, input[2].c_str());
               std::vector<Message> messages; // Vektor für gefundene Nachrichten

               DIR *dir;
               struct dirent *entry;

               dir = opendir("messages");
               if (dir == NULL)
               {
                  perror("Unable to open messages directory");
                  return NULL;
               }

               // Durchsuchen Sie das Verzeichnis nach Dateien, die zum Benutzer passen
               while ((entry = readdir(dir)) != NULL)
               {
                  if (entry->d_type == DT_REG && strstr(entry->d_name, username) != NULL)
                  {
                     std::string filepath = "messages/" + std::string(entry->d_name);

                     std::ifstream file(filepath);
                     if (!file.is_open())
                     {
                        perror("Unable to open message file");
                        closedir(dir);
                        return NULL;
                     }

                     Message currentMessage; // Struktur für die aktuelle Nachricht
                     currentMessage.number = messageNumber;

                     // Durchsuchen Sie die Datei nach der gewünschten Nachrichtennummer
                     std::string line;
                     bool foundMessage = false;

                     // Öffne eine temporäre Datei für den Schreibzugriff
                     std::ofstream tempFile("tempFile.txt");

                     while (std::getline(file, line))
                     {
                        std::string messageNumberString(messageNumber);
                        if (line.find("Message Number: " + messageNumberString) != std::string::npos)
                        {
                           foundMessage = true;

                           // Schreibe die Zeile mit der Nachrichtennummer in die temporäre Datei
                           tempFile << line << std::endl;

                           // Drucken Sie alle folgenden Zeilen der Nachricht, bis eine neue Nachricht beginnt
                           while (std::getline(file, line) && !line.empty())
                           {
                              if (line.find("Next Message") != std::string::npos)
                              {
                                 // Nachricht bis zur nächsten "Next Message" gefunden, beende die Schleife
                                 break;
                              }
                              tempFile << line << std::endl;
                           }

                           // Fügen Sie die aktuelle Nachricht zum Vektor hinzu
                           messages.push_back(currentMessage);
                           break; // Nachricht gefunden, Schleife verlassen
                        }
                     }

                     if (!foundMessage)
                     {
                        std::cout << "Nachricht mit Nummer " << messageNumber << " nicht gefunden." << std::endl;
                        send(*current_socket, "FAIL", 5, 0);
                     }

                     file.close();
                     tempFile.close();

                     // Überschreibe die ursprüngliche Datei mit der temporären Datei
                     std::remove(filepath.c_str());
                     std::rename("tempFile.txt", filepath.c_str());
                  }
               }

               closedir(dir);

               if (!messages.empty())
               {
                  // Drucken Sie die gesammelten Nachrichten
                  for (const auto &msg : messages)
                  {
                     // Extrahiere den Nachrichtentext
                     std::string messageText = extractMessageText(msg.text);

                     // Drucke den Nachrichtentext
                     std::cout << messageText << std::endl;
                  }
                  send(*current_socket, "OK", 3, 0);
               }
            }
            if (command == "List" || command == "list")
            {
               char username[BUF];
               strcpy(username, input[1].c_str());

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
                        // speichere die zurückgegebenen Informationen
                        std::vector<MessageInfo> messageInfo = extractMessageInfo(filepath);

                        int messageCount = messageInfo.size();
                        std::vector<std::string> subjects;

                        // füge Betreffzeilen zur allSubjects hinzu
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
                        send(*current_socket, "FAIL: File does not exist", 26, 0);
                        return NULL;
                     }
                  }
               }
               closedir(dir);

               printf("Anzahl der Nachrichten für Benutzer %s: %d\n", username, totalMessages);
               printf("Betreff der Nachrichten:\n");

               for (const auto &subject : allSubjects)
               {
                  printf("%s\n", subject.c_str());
               }

               if (send(*current_socket, "OK", 3, 0) == -1)
               {
                  perror("send OK failed");
                  return NULL;
               }
            }

            if (command == "Delete" || command == "delete")
            {
               std::string username = input[1];
               int messageNumber = std::stoi(input[2]);

               DIR *dir;
               struct dirent *entry;
               bool messageFound = false;

               dir = opendir("messages");
               if (dir == NULL)
               {
                  perror("Unable to open messages directory");
                  return NULL;
               }

               while ((entry = readdir(dir)) != NULL)
               {
                  if (entry->d_type == DT_REG && strstr(entry->d_name, username.c_str()) != NULL)
                  {
                     std::string filepath = "messages/" + std::string(entry->d_name);

                     std::ifstream inputFile(filepath);
                     std::ofstream outputFile("tempDatei.txt");
                     std::string line;

                     if (inputFile.is_open())
                     {
                        while (std::getline(inputFile, line))
                        {
                           // Überprüfe, ob die Zeile die zu löschende Nachricht enthält
                           if (line.find("Message Number: " + std::to_string(messageNumber)) == std::string::npos)
                           {
                              // Füge die Zeile zur temporären Datei hinzu, wenn die Nachricht nicht gefunden wurde
                              outputFile << line << std::endl;
                           }
                           else
                           {
                              messageFound = true;
                           }
                        }
                        inputFile.close();
                        outputFile.close();
                     }
                     else
                     {
                        std::cerr << "File does not exist: " << filepath << std::endl;
                        send(*current_socket, "FAIL: File does not exist", 26, 0);
                     }

                     // Lösche die ursprüngliche Datei und benenne die temporäre Datei um
                     if (remove(filepath.c_str()) != 0)
                     {
                        std::cerr << "Fehler beim Löschen der ursprünglichen Datei." << std::endl;
                        send(*current_socket, "FAIL: Error deleting message", 29, 0);
                        return NULL;
                     }
                     if (rename("tempDatei.txt", filepath.c_str()) != 0)
                     {
                        std::cerr << "Fehler beim Umbenennen der temporären Datei." << std::endl;
                        send(*current_socket, "FAIL: Error renaming file", 24, 0);
                        return NULL;
                     }

                     break; // Datei gefunden, Schleife verlassen
                  }
               }
               closedir(dir);

               if (messageFound)
               {
                  send(*current_socket, "OK", 3, 0);
               }
               else
               {
                  std::cout << "Nachricht mit Nummer " << messageNumber << " nicht gefunden." << std::endl;
                  send(*current_socket, "FAIL: Message not found", 24, 0);
               }
            }
         }
         if (command == "Quit" || command == "quit")
         {
            // Senden Sie den "Quit"-Befehl an den Server
            printf("Nachricht Quit erhalten: %s\n", input[BUF].c_str());
         }
      }
      else
      {
         printf("Nachricht erhalten: %s\n", buffer); // Fehler ignorieren
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