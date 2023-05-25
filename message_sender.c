#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include "message_slot.h"

int main(int argc, char *argv[]) 
{
    int fd;
    if(argc!=4){
        perror("wrong number of arguments to sender");
        exit(1);
    }
    fd = open(argv[1], O_WRONLY);
    if(fd==-1){
        perror("couldnt open file");
        exit(1);
    }
    if (ioctl(fd, MSG_SLOT_CHANNEL, atoi(argv[2])) == -1)
    {
        perror("channel couldnt be set");
        exit(1);
    }
    if (write(fd, argv[3], strlen(argv[3])) != strlen(argv[3])){//strlen doesnt count the /0 char so it wont be written
        perror("message write failed");
        exit(1);
    }
    close(fd);
    exit(0);
}