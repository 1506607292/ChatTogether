#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/stat.h>

short SERVER_PORT = 9999;
int TIMEOUT = 300;
enum PrivilegesCode {
    SUPERUSER, USER, ANONYMOUS
};
const char *fileName = "users";
struct User {
    unsigned int id;
    char nickname[20];
    char password[20];
    char email[20];
};

long GetUserCount(char *fileStr) {
    struct stat fileStat;
    stat(fileStr, &fileStat);
    return fileStat.st_size;
}

struct User GetUserById(unsigned int id) {
    FILE *userFile;
    struct User user;
    if ((userFile = fopen(fileName, "rb")) == 0) {
        memset(&user, 0, sizeof(struct User));
        return user;
    }
    fseek(userFile, sizeof(struct User) * id, SEEK_SET);
    fread(&user, sizeof(struct User), 1, userFile);
    fseek(userFile, 0, SEEK_END);
    fclose(userFile);
    return user;
}

bool SetUserById(struct User user) {
    FILE *userFile;
    if ((userFile = fopen(fileName, "rb+")) == 0) {
        if ((userFile = fopen(fileName, "wb")) == 0) {
            return false;
        }
    }
    fseek(userFile, sizeof(struct User) * user.id, SEEK_SET);
    fwrite(&user, sizeof(struct User), 1, userFile);
    fclose(userFile);
    return true;
}

struct Client {
    socklen_t length;
    struct sockaddr_in address;
    struct User user;
    long long time;
};

enum StatusCode {
    ERROR = -3,
    SHUTDOWN = -2,
    DISCONNECT = -1,
    UNKNOWN = 0,
    CONNECT = 1,
    CHAT = 2,
    RENAME = 3,
};
struct CommonData {
    enum StatusCode code;
    unsigned int group;
    char message[64];
    char data[1024];
};
typedef struct {
    unsigned int size;
    unsigned int capacity;
    struct Client **clients;
} ConnectionTable_, *ConnectionTable;

ConnectionTable TableNew(unsigned int capacity) {
    ConnectionTable table = (ConnectionTable_ *) malloc(sizeof(ConnectionTable_));
    if (table == NULL) {
        return NULL;
    }
    table->clients = (struct Client **) malloc(sizeof(struct Client *) * capacity);
    if (table->clients == NULL) {
        free(table);
        return NULL;
    }
    table->size = 0;
    table->capacity = capacity;
    memset(table->clients, 0, sizeof(struct Client *) * capacity);
    return table;
}

void TableClear(ConnectionTable table) {
    for (unsigned int i = 0; i < table->capacity; i++) {
        if (table->clients[i] != NULL) {
            free(table->clients[i]);
        }
    }
    table->size = 0;
}

void TableDestroy(ConnectionTable table) {
    for (unsigned int i = 0; i < table->capacity; i++) {
        if (table->clients[i] != NULL) {
            free(table->clients[i]);
        }
    }
    free(table->clients);
    free(table);
}

bool TableSet(ConnectionTable table, const struct Client *client) {
    unsigned int flag = client->address.sin_port % table->capacity;
    unsigned int doubleLength = table->capacity - flag < flag ? table->capacity - flag : flag;
    int i = 0;
    while (i < doubleLength) {
        if (table->clients[flag + i] == NULL) {
            table->clients[flag + i] = (struct Client *) malloc(sizeof(struct Client));
            *table->clients[flag + i] = *client;
            table->size++;
            return true;
        } else if (table->clients[flag + i]->address.sin_addr.s_addr == client->address.sin_addr.s_addr
                   && table->clients[flag + i]->address.sin_port == client->address.sin_port) {
            *table->clients[flag + i] = *client;
            return true;
        }
        if (table->clients[flag - i] == NULL) {
            table->clients[flag - i] = (struct Client *) malloc(sizeof(struct Client));
            *table->clients[flag - i] = *client;
            table->size++;
            return true;
        } else if (table->clients[flag - i]->address.sin_addr.s_addr == client->address.sin_addr.s_addr
                   && table->clients[flag - i]->address.sin_port == client->address.sin_port) {
            *table->clients[flag - i] = *client;
            return true;
        }
        i++;
    }
    if (flag - i == 0) {
        i = (int) (flag + i);
        while (i < table->capacity) {
            if (table->clients[i] == NULL) {
                table->clients[i] = (struct Client *) malloc(sizeof(struct Client));
                *table->clients[i] = *client;
                table->size++;
                return true;
            } else if (table->clients[i]->address.sin_addr.s_addr == client->address.sin_addr.s_addr
                       && table->clients[i]->address.sin_port == client->address.sin_port) {
                *table->clients[i] = *client;
                return true;
            }
            i++;
        }
    } else {
        i = (int) (flag - i - 1);
        while (i >= 0) {
            if (table->clients[i] == NULL) {
                table->clients[i] = (struct Client *) malloc(sizeof(struct Client));
                *table->clients[i] = *client;
                table->size++;
                return true;
            } else if (table->clients[i]->address.sin_addr.s_addr == client->address.sin_addr.s_addr
                       && table->clients[i]->address.sin_port == client->address.sin_port) {
                *table->clients[i] = *client;
                return true;
            }
            i--;
        }
    }
    return false;
}

struct Client *TableGet(ConnectionTable table, const struct Client *client) {
    unsigned int flag = client->address.sin_port % table->capacity;
    unsigned int doubleLength = table->capacity - flag < flag ? table->capacity - flag : flag;
    int i = 0;
    while (i < doubleLength) {
        if (table->clients[flag + i] != NULL
            && table->clients[flag + i]->address.sin_addr.s_addr == client->address.sin_addr.s_addr
            && table->clients[flag + i]->address.sin_port == client->address.sin_port) {
            return table->clients[flag + i];
        }
        if (table->clients[flag - i] != NULL
            && table->clients[flag - i]->address.sin_addr.s_addr == client->address.sin_addr.s_addr
            && table->clients[flag - i]->address.sin_port == client->address.sin_port) {
            return table->clients[flag - i];
        }
        i++;
    }
    if (flag - i == 0) {
        i = (int) (flag + i);
        while (i < table->capacity) {
            if (table->clients[i] != NULL
                && table->clients[i]->address.sin_addr.s_addr == client->address.sin_addr.s_addr
                && table->clients[i]->address.sin_port == client->address.sin_port) {
                return table->clients[i];
            }
            i++;
        }
    } else {
        i = (int) (flag - i - 1);
        while (i >= 0) {
            if (table->clients[i] != NULL
                && table->clients[i]->address.sin_addr.s_addr == client->address.sin_addr.s_addr
                && table->clients[i]->address.sin_port == client->address.sin_port) {
                return table->clients[i];
            }
            i--;
        }
    }
    return NULL;
}

bool TableErase(ConnectionTable table, const struct Client *client) {
    unsigned int flag = client->address.sin_port % table->capacity;
    unsigned int doubleLength = table->capacity - flag < flag ? table->capacity - flag : flag;
    int i = 0;
    while (i < doubleLength) {
        if (table->clients[flag + i] != NULL
            && table->clients[flag + i]->address.sin_addr.s_addr == client->address.sin_addr.s_addr
            && table->clients[flag + i]->address.sin_port == client->address.sin_port) {
            free(table->clients[flag + i]);
            table->clients[flag + i] = NULL;
            table->size--;
            return true;
        }
        if (table->clients[flag - i] != NULL
            && table->clients[flag - i]->address.sin_addr.s_addr == client->address.sin_addr.s_addr
            && table->clients[flag - i]->address.sin_port == client->address.sin_port) {
            free(table->clients[flag - i]);
            table->clients[flag - i] = NULL;
            table->size--;
            return true;
        }
        i++;
    }
    if (flag - i == 0) {
        i = (int) (flag + i);
        while (i < table->capacity) {
            if (table->clients[i] != NULL
                && table->clients[i]->address.sin_addr.s_addr == client->address.sin_addr.s_addr
                && table->clients[i]->address.sin_port == client->address.sin_port) {
                free(table->clients[i]);
                table->clients[i] = NULL;
                table->size--;
                return true;
            }
            i++;
        }
    } else {
        i = (int) (flag - i - 1);
        while (i >= 0) {
            if (table->clients[i] != NULL
                && table->clients[i]->address.sin_addr.s_addr == client->address.sin_addr.s_addr
                && table->clients[i]->address.sin_port == client->address.sin_port) {
                free(table->clients[i]);
                table->clients[i] = NULL;
                table->size--;
                return true;
            }
            i--;
        }
    }
    return false;
}


int main(int argc, char *argv[]) {
    struct User user;
    user.id = 0;
    unsigned int groupNum = 1024;
    unsigned int groupSize = 1024;
    int serverFileDescriptor;
    struct sockaddr_in serverAddress;
    serverFileDescriptor = socket(AF_INET, SOCK_DGRAM, 0); //AF_INET:IPV4;SOCK_DGRAM:UDP
    if (serverFileDescriptor < 0) {
        puts("Create socket fail!");
        return -1;
    }
    puts("Create socket successfully");
    memset(&serverAddress, 0, sizeof(serverFileDescriptor));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(SERVER_PORT);
    if (bind(serverFileDescriptor, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) < 0) {
        puts("Socket bind fail");
        return -2;
    }
    puts("Bind successfully");
    ConnectionTable tables[groupNum];
    for (unsigned int i = 0; i < 1024; i++) {
        tables[i] = TableNew(groupSize);
        if (tables[i] == NULL) {
            puts("Create table failed");
            return -3;
        }
    }
    printf("Create Group successfully and Group Size is : %d\n", groupSize);
    puts("Turn on successfully");
    struct Client clientBuf;
    struct CommonData buf;
    clientBuf.length = sizeof(clientBuf.address);
    int count=0;
    while (true) {
        printf("%d\n",count++);
        long int count = recvfrom(serverFileDescriptor, &buf, sizeof(struct CommonData), 0,
                                  (struct sockaddr *) &clientBuf.address, &clientBuf.length);
        if (count == -1) {
            puts("Receive data fail");
            return -4;
        }
        printf("%hhu.", *(char *) (&clientBuf.address.sin_addr.s_addr));
        printf("%hhu.", *((char *) (&clientBuf.address.sin_addr.s_addr) + 1));
        printf("%hhu.", *((char *) (&clientBuf.address.sin_addr.s_addr) + 2));
        printf("%hhu:", *((char *) (&clientBuf.address.sin_addr.s_addr) + 3));
        printf("%d", clientBuf.address.sin_port);
        printf("\t Code : %d\tGroup : %d\t", buf.code, buf.group);
        if (buf.group >= groupSize) {
            puts("");
            continue;
        }
        ConnectionTable table = tables[buf.group];
        printf("size: %d\n", table->size);
        struct Client *client = TableGet(table, &clientBuf);
        if (client == NULL) {
            if (buf.code == CONNECT) {
                strcpy(clientBuf.user.nickname, "Unnamed");
                clientBuf.time = time(NULL);
                if (!TableSet(table, &clientBuf)) {
                    buf.code = ERROR;
                    strcpy(buf.message, "Server : Connect Unsuccessfully");
                } else {
                    strcpy(buf.message, "Server : Connected");
                }
            } else {
                strcpy(buf.message, "Server : Disconnected");
            }

        } else {
            if (time(NULL) - client->time > TIMEOUT) {
                strcpy(buf.message, "Server : Time out");
                TableErase(table, client);
            } else {
                client->time = time(NULL);
                if (buf.code == SHUTDOWN) {
                    break;
                } else if (buf.code == DISCONNECT) {
                    TableErase(table, &clientBuf);
                    strcpy(buf.message, "Server : Disconnect successfully");
                } else if (buf.code == RENAME) {
                    strcpy(TableGet(table, &clientBuf)->user.nickname, buf.data);
                    sprintf(buf.message, "Server : Set username (Name:%s) successfully", buf.data);
                } else if (buf.code == CHAT) {
                    sprintf(buf.message, "From %hhu.%hhu.%hhu.%hhu:%d NickName:%s",
                            *((char *) (&clientBuf.address.sin_addr.s_addr) + 0),
                            *((char *) (&clientBuf.address.sin_addr.s_addr) + 1),
                            *((char *) (&clientBuf.address.sin_addr.s_addr) + 2),
                            *((char *) (&clientBuf.address.sin_addr.s_addr) + 3),
                            clientBuf.address.sin_port,
                            client->user.nickname);
                    for (int i = 0; i < table->capacity; i++) {
                        if (table->clients[i] != NULL) {
                            if (time(NULL) - table->clients[i]->time > TIMEOUT) {
                                free(table->clients[i]);
                                table->clients[i] = NULL;
                                table->size--;
                                continue;
                            }
                            sendto(serverFileDescriptor, &buf, sizeof(struct CommonData), 0,
                                   (struct sockaddr *) &table->clients[i]->address, table->clients[i]->length);
                        }
                    }
                    continue;
                } else {
                    buf.code = UNKNOWN;
                    strcpy(buf.message, "Unknown");
                }
            }
        }
        strcpy(buf.data, "");
        sendto(serverFileDescriptor, &buf, sizeof(struct CommonData), 0,
               (struct sockaddr *) &clientBuf.address, clientBuf.length);
    }
    puts("Shut down successfully");
    for (unsigned int i = 0; i < 1024; i++) {
        TableDestroy(tables[i]);
    }
    close(serverFileDescriptor);
    return 0;
}
