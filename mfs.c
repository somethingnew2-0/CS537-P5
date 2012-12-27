#include "udp.h"
#include "mfs.h"
#include <ctype.h>

#define BUFFER_SIZE (4096)

//char *test;

int sd = -1;
struct sockaddr_in saddr;

int MFS_Init(char *hostname, int port) {
    sd = UDP_Open(-1);
	if (sd < 0) {
		return sd;
	} 
	int rc = UDP_FillSockAddr(&saddr, hostname, port);
	if (rc < 0) {
		return rc;
	} 
	return 0;
}

int MFS_Lookup(int pinum, char *name) {
	if (sd < 0) {
		return sd;
	} 
	char message[BUFFER_SIZE];
    sprintf(message, "0~%d~%s", pinum, name);
    int rc = UDP_Write(sd, &saddr, message, BUFFER_SIZE);
    printf("CLIENT:: sent message (%d) \"%s\"\n", rc, message);
	if (rc < 0) {
	    sd = UDP_Open(-1);
		//return rc;
	} 

   	fd_set fds;
	struct timeval timeout;
	/* Set time limit. */
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	/* Create a descriptor set containing our two sockets.  */
	FD_ZERO(&fds);
	FD_SET(sd, &fds);
	rc = select(sd+1, &fds, NULL, NULL, &timeout);
	
	/* select error */
	if(rc < 0) {
		return -1;
	}
	/* No data in five seconds */	
	else if (rc == 0) {
		return MFS_Lookup(pinum, name);
	}
	/* Data is available */
	else {
		struct sockaddr_in raddr;
		char buffer[BUFFER_SIZE];
		int rc = UDP_Read(sd, &raddr, buffer, BUFFER_SIZE);
		printf("CLIENT:: read %d bytes (message: '%s')\n", rc, buffer);
		return atoi(buffer);
	}

	return 0;
}

int MFS_Stat(int inum, MFS_Stat_t *m) {
	if (sd < 0) {
		return sd;
	} 
	char message[BUFFER_SIZE];
    sprintf(message, "1~%d", inum);
    int rc = UDP_Write(sd, &saddr, message, BUFFER_SIZE);
    printf("CLIENT:: sent message (%d) \"%s\"\n", rc, message);
	if (rc < 0) {
		sd = UDP_Open(-1);		
		//return rc;
	} 

   	fd_set fds;
	struct timeval timeout;
	/* Set time limit. */
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	/* Create a descriptor set containing our two sockets.  */
	FD_ZERO(&fds);
	FD_SET(sd, &fds);
	rc = select(sd+1, &fds, NULL, NULL, &timeout);
	
	/* select error */
	if(rc < 0) {
		return -1;
	}
	/* No data in five seconds */	
	else if (rc == 0) {
		return MFS_Stat(inum, m);
	}
	/* Data is available */
	else {
		char buffer[BUFFER_SIZE];
		struct sockaddr_in raddr;
		int rc = UDP_Read(sd, &raddr, buffer, BUFFER_SIZE);
		printf("CLIENT:: read %d bytes (message: '%s')\n", rc, buffer);
		int retCode = atoi(strtok(buffer, "~"));
		m->type = atoi(strtok(NULL, "~"));
		m->size = atoi(strtok(NULL, "~"));
		return retCode;
	}

	return 0;
}

int MFS_Write(int inum, char *buffer, int block) {
	if (sd < 0) {
		return sd;
	} 
	char message[BUFFER_SIZE];
    sprintf(message, "2~%d~%d", inum, block);
	//memcpy(message, buffer, BUFFER_SIZE);
    int rc = UDP_Write(sd, &saddr, message, BUFFER_SIZE);
    printf("CLIENT:: sent message (%d) \"%s\"\n", rc, message);
	if (rc < 0) {
		sd = UDP_Open(-1);		
		//return rc;
	} 

    rc = UDP_Write(sd, &saddr, buffer, BUFFER_SIZE);
    printf("CLIENT:: sent message (%d) \"BUFFER\"\n", rc);
	/*int i;	
	for (i = 0; i < BUFFER_SIZE; i++ ) {
  		putc( isprint(buffer[i]) ? buffer[i] : '.' , stdout );
	}
	printf("\n");*/
	if (rc < 0) {
		return rc;
	} 

   	fd_set fds;
	struct timeval timeout;
	/* Set time limit. */
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	/* Create a descriptor set containing our two sockets.  */
	FD_ZERO(&fds);
	FD_SET(sd, &fds);
	rc = select(sd+1, &fds, NULL, NULL, &timeout);
	
	/* select error */
	if(rc < 0) {
		return -1;
	}
	/* No data in five seconds */	
	else if (rc == 0) {
		return MFS_Write(inum, buffer, block);
	}
	/* Data is available */
	else {
		struct sockaddr_in raddr;
		int rc = UDP_Read(sd, &raddr, message, BUFFER_SIZE);
		printf("CLIENT:: read %d bytes (message: '%s')\n", rc, message);
		return atoi(message);
	}

	return 0;
}

int MFS_Read(int inum, char *buffer, int block) {
	if (sd < 0) {
		return sd;
	} 
	char message[BUFFER_SIZE];
    sprintf(message, "3~%d~%d", inum, block);
    int rc = UDP_Write(sd, &saddr, message, BUFFER_SIZE);
    printf("CLIENT:: sent message (%d) \"%s\"\n", rc, message);
	if (rc < 0) {
		sd = UDP_Open(-1);		
		//return rc;
	} 

   	fd_set fds;
	struct timeval timeout;
	/* Set time limit. */
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	/* Create a descriptor set containing our two sockets.  */
	FD_ZERO(&fds);
	FD_SET(sd, &fds);
	rc = select(sd+1, &fds, NULL, NULL, &timeout);
	
	/* select error */
	if(rc < 0) {
		return -1;
	}
	/* No data in five seconds */	
	else if (rc == 0) {
		return MFS_Read(inum, buffer, block);
	}
	/* Data is available */
	else {
		struct sockaddr_in raddr;
		int rc = UDP_Read(sd, &raddr, message, BUFFER_SIZE);
		printf("CLIENT:: read %d bytes (message: '%s')\n", rc, message);
		int retCode = atoi(message);
		//memcpy(buffer, message, BUFFER_SIZE);
		rc = UDP_Read(sd, &raddr, buffer, BUFFER_SIZE);
		printf("CLIENT:: read %d bytes (message: 'BUFFER')\n", rc);
		/*int i;		
		for (i = 0; i < BUFFER_SIZE; i++ ) {
	  		putc( isprint(buffer[i]) ? buffer[i] : '.' , stdout );
		}
		printf("\n");
		printf("CLIENT:: test 'BUFFER'\n\"");
		for (i = 0; i < BUFFER_SIZE; i++ ) {
			if(buffer[i] != test[i]) {
	  			putc( isprint(buffer[i]) ? buffer[i] : '.' , stdout );
			}
		}
		printf("\"\n");*/
		return retCode;
	}

	return 0;
}

int MFS_Creat(int pinum, int type, char *name) {
	if (sd < 0) {
		return sd;
	} 
	char message[BUFFER_SIZE];
    sprintf(message, "4~%d~%d~%s", pinum, type, name);
    int rc = UDP_Write(sd, &saddr, message, BUFFER_SIZE);
    printf("CLIENT:: sent message (%d) \"%s\"\n", rc, message);
	if (rc < 0) {
		sd = UDP_Open(-1);		
		//return rc;
	} 

   	fd_set fds;
	struct timeval timeout;
	/* Set time limit. */
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	/* Create a descriptor set containing our two sockets.  */
	FD_ZERO(&fds);
	FD_SET(sd, &fds);
	rc = select(sd+1, &fds, NULL, NULL, &timeout);

	/* select error */
	if(rc == -1) {
		return -1;
	}
	/* Data is available */
	else if (rc) {
		char buffer[BUFFER_SIZE];
		struct sockaddr_in raddr;
		int rc = UDP_Read(sd, &raddr, buffer, BUFFER_SIZE);
		printf("CLIENT:: read %d bytes (message: '%s')\n", rc, buffer);
		return atoi(buffer);
	}
	/* No data in five seconds */	
	else {
		return MFS_Creat(pinum, type, name);
	}

	return 0;
}

int MFS_Unlink(int pinum, char *name) {
	if (sd < 0) {
		return sd;
	} 
	char message[BUFFER_SIZE];
    sprintf(message, "5~%d~%s", pinum, name);
    int rc = UDP_Write(sd, &saddr, message, BUFFER_SIZE);
    printf("CLIENT:: sent message (%d) \"%s\"\n", rc, message);
	if (rc < 0) {
		sd = UDP_Open(-1);		
		//return rc;
	} 

   	fd_set fds;
	struct timeval timeout;
	/* Set time limit. */
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;
	/* Create a descriptor set containing our two sockets.  */
	FD_ZERO(&fds);
	FD_SET(sd, &fds);
	rc = select(sd+1, &fds, NULL, NULL, &timeout);
	
	/* select error */
	if(rc < 0) {
		return -1;
	}
	/* No data in five seconds */	
	else if (rc == 0) {
		return MFS_Unlink(pinum, name);
	}
	/* Data is available */
	else {
		char buffer[BUFFER_SIZE];
		struct sockaddr_in raddr;
		int rc = UDP_Read(sd, &raddr, buffer, BUFFER_SIZE);
		printf("CLIENT:: read %d bytes (message: '%s')\n", rc, buffer);
		return atoi(buffer);
	}

	return 0;
}

int MFS_Shutdown() {
	if (sd < 0) {
		return sd;
	} 
	char message[BUFFER_SIZE];
    sprintf(message, "6");
    int rc = UDP_Write(sd, &saddr, message, BUFFER_SIZE);
    printf("CLIENT:: sent message (%d) \"%s\"\n", rc, message);
	if (rc < 0) {
		return rc;
	} 

	UDP_Close(sd);
	return 0;
}
