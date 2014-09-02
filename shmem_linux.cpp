
#define SEMAPHORE

#define BUF_SIZE 1024*10*4
	long long int numops = 10000000;

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <semaphore.h>
#include <errno.h>
#include <stddef.h>
#define UNIX_SOCK

char szName[]="MyFileMappingObject";
char szMsg[]="Message from first process.";
char szEv1[] = "event1";
char szEv2[] = "event2";
//char udspath[] = "/tmp/test1";

char szPipe1[] = "/tmp/parent";
char szPipe2[] = "/tmp/child";
char udspath[] = "/tmp/uds-test";

void child_shmem(sem_t *sem0, sem_t * semn, int n, char * pBuf, long long int numops)
{
	sem_wait(semn);
	char buf[BUF_SIZE/4];
	memset(buf, 4, sizeof(buf));
	buf[sizeof(buf)-1] = 0;

	clock_t c = clock();
	for(long long int i=0;i<numops;++i) {
		//Write it to the memory.
		memcpy(pBuf, buf, sizeof(buf));
		sem_post(sem0);
		sem_wait(semn);
	}
	clock_t e = clock();
	printf("Shared Memory Child %d: Time taken(Write): %ld for %lld operations for %ld bytes\n", n, e-c, numops, sizeof(buf));

	c = clock();
	for(long long int i=0;i<numops;++i) {
		sem_wait(semn);
		memcpy(buf, pBuf+n*BUF_SIZE/4, sizeof(buf));
		sem_post(sem0);
	}
	e = clock();
	printf("Shared Memory Child %d: Time taken(Read): %ld for %lld operations for %ld bytes\n", n, e-c, numops, sizeof(buf));
}

void child_write(sem_t * sem0, sem_t * semn, char * pBuf, int n, long long int numops)
{

	//Do it with first process.
	sem_post(semn);				
	char buf[BUF_SIZE/4];

	clock_t c = clock();
	for(long long int i=0;i<numops;++i) {
		sem_wait(sem0);
		//Write it to the memory.
		memcpy(buf, pBuf, sizeof(buf));
		sem_post(semn);
	}
	clock_t e = clock();
	printf("Shared Memory Parent Child %d-Root: Time taken(Write): %ld for %lld operations for %ld bytes\n", n, e-c, numops, sizeof(buf));

	memset(buf, 10-n, sizeof(buf));
	buf[sizeof(buf)-1] = 0;

	c = clock();
	for(long long int i=0;i<numops;++i) {
		memcpy(buf, pBuf+n*BUF_SIZE/4, sizeof(buf));
		sem_post(semn);
		sem_wait(sem0);
	}
	e = clock();
	printf("Shared Memory Parent Child %d-Root: Time taken(Read): %ld for %lld operations for %ld bytes\n", n, e-c, numops, sizeof(buf));
}


int main(int argc, const char * argv[])
{

		int	hMapFile;
		key_t key;
		char* pBuf;
		int	mode;
	
		//freopen("test-output.txt","w",stdout);

	key = ftok("shmem_linux.cpp", 'R');
	hMapFile = shmget(key, BUF_SIZE, 0777|IPC_CREAT);
	if(hMapFile==-1) {
		printf("shmget failed with errno: %d\n", errno);
		return 1;
	}

	pBuf = (char*)shmat(hMapFile, NULL, 0);

	if (pBuf == (char*)-1) {
	   printf("Could not attach shared memory (%d).\n",
			  errno);
	   return 1;
	}


	//Open semaphores.
	sem_t* sem0 = sem_open("sem_main", O_CREAT, 0777);
	sem_t* sem1 = sem_open("sem_p1", O_CREAT, 0777);
	sem_t* sem2 = sem_open("sem_p2", O_CREAT, 0777);
	sem_t* sem3 = sem_open("sem_p3", O_CREAT, 0777);

	//Shared memory is ready.
	freopen("test-output.txt","w",stdout);
	pid_t cp = fork();
	if(cp==0) {
	c1sm:
		//Child c1
  		freopen("test-output1.txt","a",stdout);
		child_shmem(sem0, sem1, 1, pBuf, numops);
		goto retlabel;

	} else {
		pid_t cp = fork();
		if(cp==0) {
		c2sm:
			//Child c2
	  		freopen("test-output2.txt","a",stdout);
			child_shmem(sem0, sem2, 2, pBuf, numops);
			//freopen("/dev/tty","w",stdout);
			goto retlabel;

		} else {
			pid_t cp = fork();
			if(cp==0) {
			c3sm:
			  freopen("test-output3.txt","a",stdout);
			  child_shmem(sem0, sem3, 3, pBuf, numops);
			  goto retlabel;

			} else {
				child_write(sem0, sem1, pBuf, 1, numops);
				child_write(sem0, sem2, pBuf, 2, numops);
				child_write(sem0, sem3, pBuf, 3, numops);
			}
		}
	}

retlabel:

	sem_close(sem0);
	sem_close(sem1);
	sem_close(sem2);
	sem_close(sem3);

	shmdt(pBuf);
	return 0;
}

