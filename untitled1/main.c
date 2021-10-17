#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/shm.h>
#include <string.h>

#ifndef _SHMDATA_H_HEADER
#define _SHMDATA_H_HEADER
#define TEXT_SZ 2048
struct shared_use_st {
    int written;//作为一个标志，非0：表示可读，0表示可写
    char text[TEXT_SZ];//记录写入和读取的文本
};
#endif

int main() {
    for (int ii = 0; ii < 1; ii++) {
        printf("%d\n", ii);
        int shmId = shmget(1222, 1024 * sizeof(int), IPC_CREAT | 0666);
        if (shmId == -1) {
            fprintf(stderr, "Get memory failed\n");
            exit(EXIT_FAILURE);
        }
        int *shmPointer = shmat(shmId, (void *) 0, 0);
        for(int i=0;i<1024;i++){
            shmPointer[i] = i*3;
        }
        for(int i=0;i<1024;i++){
            printf("%d\n",shmPointer[i]);
        }
        if (shmPointer == (void *) -1) {
            perror("");
            fprintf(stderr, "link memory failed\n");
            exit(EXIT_FAILURE);
        }
        if (shmdt(shmPointer) == -1) {
            fprintf(stderr, "depart memory failed\n");
        }
        if (shmctl(shmId, IPC_RMID, 0) == -1) {
            fprintf(stderr, "delete memory failed\n");
        }
    }
    sleep(100);
}