
#define SEMAPHORE

#define BUF_SIZE 1024*10*4
	long long int numops = 10000000;
#ifdef _WIN32
#include <windows.h>
#include <time.h>
#else

#endif

#include <stdio.h>
#include <string.h>

#ifdef _WIN32

char szName[]="MyFileMappingObject";
char szMsg[]="Message from first process.";
char szEv1[] = "event0";
const char * namedpipe = "\\\\.\\pipe\test-pipe";
const char * szEv2[] = {"event1", "event2", "event3"};
enum WProcess {
	P0,
	P1,
	P2,
	P3
};

const int numchild = 3;
const char * outfiles[] = {"outfile.txt", "outfile1.txt", "outfile2.txt", "outfile3.txt"};

void SignalEvent(HANDLE pevent)
{
#ifdef SEMAPHORE
	ReleaseSemaphore(pevent, 1, NULL);
#else
	SetEvent(pevent);
#endif
}

int RunChildProcess(char * pBuf, HANDLE myevent, HANDLE pevent, int num, long long int numops)
{
	freopen(outfiles[num], "a", stdout);
	char buf[BUF_SIZE/4];
	memset(buf, 4, sizeof(buf));
	buf[sizeof(buf)-1] = 0;

	WaitForSingleObject(myevent, INFINITE);	
	clock_t c = clock();
	for(long long int i=0;i<numops;++i) {
		//Write it to the memory.
		memcpy(pBuf, buf, sizeof(buf));
		SignalEvent(pevent);
		WaitForSingleObject(myevent, INFINITE);
	}
	clock_t e = clock();
	printf("Shared Memory Child %d: Time taken(Write): %ld for %lld operations for %ld bytes\n", num, e-c, numops, sizeof(buf));

	c = clock();
	for(long long int i=0;i<numops;++i) {
		WaitForSingleObject(myevent, INFINITE);
		memcpy(buf, pBuf+num*BUF_SIZE/4, sizeof(buf));
		SignalEvent(pevent);
	}
	e = clock();
	printf("Shared Memory Child %d: Time taken(Read): %ld for %lld operations for %ld bytes\n", num, e-c, numops, sizeof(buf));

	//Named pipe implementation will comehere.
	return 0;
}

void ChildWriteSM(HANDLE pevent, HANDLE cevent, char * pBuf, int n, long long int numops)
{
	//Do it with first process.
	SignalEvent(cevent);
	char buf[BUF_SIZE/4];

	clock_t c = clock();
	for(long long int i=0;i<numops;++i) {
		WaitForSingleObject(pevent, INFINITE);
		//Write it to the memory.
		memcpy(buf, pBuf, sizeof(buf));
		SignalEvent(cevent);
	}
	clock_t e = clock();
	printf("Shared Memory Parent Child %d-Root: Time taken(Write): %ld for %lld operations for %ld bytes\n", n, e-c, numops, sizeof(buf));

	memset(buf, 10-n, sizeof(buf));
	buf[sizeof(buf)-1] = 0;

	c = clock();
	for(long long int i=0;i<numops;++i) {
		memcpy(buf, pBuf+n*BUF_SIZE/4, sizeof(buf));
		SignalEvent(cevent);
		WaitForSingleObject(pevent, INFINITE);
	}
	e = clock();
	printf("Shared Memory Parent Child %d-Root: Time taken(Read): %ld for %lld operations for %ld bytes\n", n, e-c, numops, sizeof(buf));
}

int RunParentProcess(const char * procname, char * pBuf, HANDLE pevent, HANDLE cevent[], int numevents)
{
	HANDLE namedpipes[numchild];
	for(int i=0;i<numchild;++i) {
	
		namedpipes[i] = CreateNamedPipe(namedpipe, PIPE_ACCESS_DUPLEX, 
										   PIPE_TYPE_BYTE, 
										   numchild, BUF_SIZE/4, BUF_SIZE/4, 5000, NULL);
	}

	freopen(outfiles[0], "a", stdout);

	//Launch that many child processes.
	char * argv[] = {"P0", "P1", "P2", "P3"};
	STARTUPINFO startupinfo;
	memset(&startupinfo, 0, sizeof(startupinfo));
	startupinfo.cb = sizeof(startupinfo);
    startupinfo.dwX          = 100;
    startupinfo.dwY          = 100;
    startupinfo.dwXSize      = 300;
    startupinfo.dwYSize      = 400;
	startupinfo.dwFlags      = STARTF_USEPOSITION | STARTF_USESIZE;
	startupinfo.lpTitle      = "Tally Window";
	startupinfo.wShowWindow  = SW_SHOWDEFAULT;
	PROCESS_INFORMATION pi;
	memset(&pi, 0, sizeof(pi));
	char procs[2000];

	for(int i=1;i<=numevents; ++i) {
		sprintf(procs, "%s %s", procname, argv[i]);
		BOOL retval = CreateProcess(NULL, procs, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &startupinfo, &pi);
		if(!retval) {
			printf("CreateProcess failed with %d\n", GetLastError());
			exit(1);
		}
	}

	for(int i=1;i<=numevents;++i) {
		ChildWriteSM(pevent, cevent[i-1],pBuf,i, numops);
	}

	return 0;
}


int main(int argc, char * argv[])
{
	/*if(argc>1) {
		DebugBreak();
		getchar();
	}*/

	//DebugBreak();

		HANDLE hMapFile;
		char* pBuf;
		WProcess wp = P0;
		HANDLE  p1event;
		HANDLE  cevent[numchild];
#ifdef SEMAPHORE
	p1event = CreateSemaphore(NULL, 0, 1, szEv1);
#else
	p1event = CreateEvent(NULL, FALSE, FALSE, szEv1);
#endif
	if(p1event==INVALID_HANDLE_VALUE) {
		printf("Creation of event/semaphore failed with %d\n", GetLastError());
		return 0;
	}
	for(int i=0;i<numchild;++i) {
#ifdef SEMAPHORE
		cevent[i] = CreateSemaphore(NULL, 0, 1, szEv2[i]);
#else
		cevent[i] = CreateEvent(NULL, FALSE, FALSE, szEv2[i]);
#endif
		if(cevent[i]==INVALID_HANDLE_VALUE) {
			printf("Creation of event/semaphore failed with %d\n", GetLastError());
			return 0;
		}
	}

	hMapFile = CreateFileMapping(
				  INVALID_HANDLE_VALUE,    // use paging file
				  NULL,                    // default security
				  PAGE_READWRITE,          // read/write access
				  0,                       // maximum object size (high-order DWORD)
				  BUF_SIZE,                // maximum object size (low-order DWORD)
				  szName);                 // name of mapping object

	if (hMapFile == NULL) {
	   printf(TEXT("Could not create file mapping object (%d).\n"),
			  GetLastError());
	   return 1;
	}

	pBuf = (char *) MapViewOfFile(hMapFile,   // handle to map object
						 FILE_MAP_ALL_ACCESS, // read/write permission
						 0,
						 0,
						 BUF_SIZE);

	if (pBuf == NULL) {
	   printf("Could not map view of file (%d).\n",
			  GetLastError());
	   CloseHandle(hMapFile);
	   return 1;
	}


	if(argc>1) {
		if(!strcmp(argv[1],"P1")) {
			wp = P1;
		} else if(!strcmp(argv[1],"P2")){
			wp = P2;
		} else if(!strcmp(argv[1], "P3")) {
			wp = P3;
		}
	}

	if(wp==P0) {
		RunParentProcess(argv[0], pBuf, p1event, cevent, 3);
	} else {
		//DebugBreak();
		RunChildProcess(pBuf, cevent[(int)wp-1], p1event, (int)wp, numops);
	}

	UnmapViewOfFile(pBuf);
	CloseHandle(hMapFile);
	return 0;
}

#endif

