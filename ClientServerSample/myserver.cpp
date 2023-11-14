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

struct InputSplitter {
    static std::vector<std::string> split(const char* buffer, int size, char delimiter) {
        std::vector<std::string> result;
        std::string current;

        for (int i = 0; i < size; i++) {
            if (buffer[i] != delimiter) {
                current += buffer[i];
            } else {
                result.push_back(current);
                current = "";
            }
        }

        // Überprüfen, ob 'current' noch Daten enthält, und sie zum Vektor hinzufügen
        if (!current.empty()) {
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

      //printf("BUFFERonServer: %s\n", buffer);
      memset(buffer, 0, sizeof(buffer)); // Setzen Sie den gesamten Puffer auf Null
      size = recv(*current_socket, buffer, BUF - 1, 0);

      //printf("SIZE: %d\n", size);
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

       //remove ugly debug message, because of the sent newline of client
      if (buffer[size - 2] == '\r' && buffer[size - 1] == '\n')
      {
         size -= 2;
      }
      else if (buffer[size - 1] == '\n')
      {
         --size;
      }

      buffer[size] = '\0';
      
      //bearbeite den Input
      std::vector<std::string> input = InputSplitter::split(buffer, size, '\n');




 if (input[0]== "s" && input[0]=="S") 
    {
        char username[BUF];
        sscanf(buffer, "L %s", username);  // Extract the username
        DIR *dir;
        struct dirent *entry;
        char message[BUF] = "";
        char filepath[BUF];

        dir = opendir("messages");
        if (dir == NULL) 
        {
            perror("Unable to open messages directory");
            return NULL;
        }

        // Loop through the directory looking for files that match the username
        while ((entry = readdir(dir)) != NULL) 
        {
            if (entry->d_type == DT_REG && strstr(entry->d_name, username) != NULL) 
            { 
                snprintf(filepath, sizeof(filepath), "messages/%s", entry->d_name);
                strcat(message, filepath);
                strcat(message, "\n");
            }
        }
        closedir(dir);

        if (send(*current_socket, message, strlen(message), 0) == -1)
        {
            perror("send message list failed");
            return NULL;
        }
    } 
      else 
      {
          if (send(*current_socket, "OK", 3, 0) == -1)
          {
              perror("send answer failed");
              return NULL;
          }
      }

      /*Ablauf im Server:
Der Server empfängt den Befehl "S" oder "s" vom Client.
Der Server verarbeitet den Befehl und bestätigt die erfolgreiche Verarbeitung, indem er "OK" an den Client sendet, wenn alles in Ordnung ist.
Der Server kann auch "FAIL" an den Client senden, wenn bei der Verarbeitung ein Fehler auftritt.
Nachdem die Bestätigung an den Client gesendet wurde, kann der Server fortfahren, um die Nachricht vom Client zu empfangen, falls erforderlich.*/
if (strcmp(buffer, "Send") == 0) {
    // Sende "OK" an den Client, um die erfolgreiche Nachrichtenübertragung zu bestätigen
    if (send(*current_socket, "OK", 3, 0) == -1) {
        send(*current_socket, "FAIL", 5, 0);
        perror("Fehler beim Senden der Bestätigung an den Client");
    } else {
        printf("Client hat die Nachricht erfolgreich übertragen und Bestätigung gesendet\n");

        // Hier können Sie die Verarbeitung der Nachricht einfügen

        send(*current_socket, "OK", 3, 0);
    }
    // Falls ein Fehler auftritt:
    // send(create_socket, "FAIL", 4, 0);
}else if (strcmp(buffer, "Q") == 0 || strcmp(buffer, "q") == 0) {
    // Senden Sie den "Quit"-Befehl an den Server
     printf("Nachricht Quit erhalten: %s\n", buffer); 
} else {
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