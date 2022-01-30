#include <time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <errno.h>
#include <shm.h>
#include <unistd.h>

//Generate Ranomd Name for shm 
static void random_name(char *buf) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long rand = ts.tv_nsec;
    for(int i = 0; i < 6; i++) {
        buf[i] = 'A' + (rand & 15) + (rand & 16) * 2;
        rand >>= 5;
    }
}

static int create_shm_file(void)
{
    int retries = 100;
    do {
        char name[] = "/wl_shm-XXXXXX";
        random_name(name + sizeof(name) - 7);
        --retries;
        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0) {
            shm_unlink(name);
            return fd;
        }
    } while (retries > 0 && errno == EEXIST);
    return -1;
}

int allocate_shm(size_t size) {
    int fd = create_shm_file();
    if(fd < 0) {
        return -1;
    }
    
    int ret; 
    do {
        ret = ftruncate(fd, size);
    } while(ret < 0 && errno == EINTR);

    if(ret < 0) {
        close(fd);
        return -1;
    }
    return fd;
}


