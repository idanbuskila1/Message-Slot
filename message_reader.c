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
    int fd, len_read;
    char *msg[BUF_LEN];

    if (argc != 3)
    {
        perror("wrong number of arguments to reader");
        exit(1);
    }
    fd = open(argv[1], O_RDONLY);
    if (fd == -1)
    {
        perror("couldnt open file");
        exit(1);
    }
    if (ioctl(fd, MSG_SLOT_CHANNEL, atoi(argv[2])) == -1)
    {
        perror("channel couldnt be set");
        exit(1);
    }
    if((len_read = read(fd,msg,BUF_LEN)) == -1){
        perror("couldnt read message");
        exit(1);
    }
    close(fd);
    if(write(1, msg, len_read)!= len_read){
        perror("Failed to print message");
        exit(1);
    }
    exit(0);
}