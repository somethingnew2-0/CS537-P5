#include <stdio.h>
#include <sys/stat.h>
#include "udp.h"

#define BUFFER_SIZE (4096)

#define MFS_DIRECTORY    (0)
#define MFS_REGULAR_FILE (1)

#define MFS_BLOCK_SIZE   (4096)

typedef struct __MFS_DirEnt_t {
    char name[28];  // up to 28 bytes of name in directory (including \0)
    int  iNum;      // inode number of entry (-1 means entry not used)
} DirEnt;

typedef struct __MFS_CheckReg_t {
    int logEnd;
	int iNodeMaps[256];
} CheckReg;

typedef struct __MFS_Stat_t {
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes
    // note: no permissions, access times, etc.
} Stat;

typedef struct __MFS_INode_t {
    int type;   // MFS_DIRECTORY or MFS_REGULAR
    int size;   // bytes
	int blocks[14];
    // note: no permissions, access times, etc.
} INode;

int sd, rc, imageFD, currentINodeNum;
struct sockaddr_in *s;
CheckReg *checkpointRegion;
int *iNodeMap; // The full INode Map stored in memory

int WriteCR() {
	int rc = lseek(imageFD, 0, SEEK_SET);
	if(rc < 0) {
		return -1;
	}
	rc = write(imageFD, checkpointRegion, sizeof(CheckReg));
	if(rc < 0) {
		return -1;
	}
	rc = lseek(imageFD, checkpointRegion->logEnd, SEEK_SET);
	if(rc < 0) {
		return -1;
	}
	return 0;
}

INode* GetINode(int iNodeNum) {
	INode *iNode = malloc(sizeof(INode));
	int iNodePointer = iNodeMap[iNodeNum];
	int rc = lseek(imageFD, iNodePointer, SEEK_SET);
	if(rc < 0) {
		return NULL;
	}
	rc = read(imageFD, iNode, sizeof(INode));
	if(rc < 0) {
		return NULL;
	}
	return iNode;
}

int WriteParentDirEnt(int pINum, int cINum, char *cName) {
	INode *parentINode = GetINode(pINum);
	if(parentINode != NULL && parentINode->type == MFS_DIRECTORY) {
		// Read Old Dir Entry Data
		int blockIndex;		
		DirEnt *parentBlock = malloc(MFS_BLOCK_SIZE);
		DirEnt *emptyBlock = malloc(MFS_BLOCK_SIZE);
		for(blockIndex = 0; blockIndex < 14; blockIndex++) {
			int parentBlockPointer = parentINode->blocks[blockIndex];
			if(parentBlockPointer == 0) {
				// Create Directory Entry Block
				memcpy(parentBlock, emptyBlock, MFS_BLOCK_SIZE);
				int i;
				for (i = 0; i < MFS_BLOCK_SIZE/sizeof(DirEnt); i++) {
					parentBlock[i].iNum = -1;
				}
			} 
			else {
				// Read existing Directory Entry Block
				rc = lseek(imageFD, parentBlockPointer, SEEK_SET);
				if(rc < 0) {
					return -1;
				}
				rc = read(imageFD, parentBlock, MFS_BLOCK_SIZE);
				if(rc < 0) {
					return -1;
				}	
			}
			// Write Dir Entry Data
			int dirEntIndex;			
			for(dirEntIndex = 0; dirEntIndex < MFS_BLOCK_SIZE/sizeof(DirEnt); dirEntIndex++) {
				if((!strcmp(parentBlock[dirEntIndex].name, cName) && parentBlock[dirEntIndex].iNum >= 0) || (parentBlock[dirEntIndex].iNum == -1 && cINum >= 0)) {
					int iNodePointer = 0;
					int iNodeMapPointer = checkpointRegion->iNodeMaps[pINum/16];
					int iNodeMapOffset = pINum % 16;
					int *iNodeMapPiece = malloc(16*sizeof(int));

					// Read IMap Piece
					rc = lseek(imageFD, iNodeMapPointer, SEEK_SET);
					if(rc < 0) {
						return -1;
					}
					rc = read(imageFD, iNodeMapPiece, 16*sizeof(int));
					if(rc < 0) {
						return -1;
					}
					// Update INodeMap Piece
					iNodeMapPiece[iNodeMapOffset] = iNodePointer = checkpointRegion->logEnd + 16*sizeof(int);
					// Write INodeNum to IMap Piece
					rc = lseek(imageFD, checkpointRegion->logEnd, SEEK_SET);
					if(rc < 0) {
						return -1;
					}
					rc = write(imageFD, iNodeMapPiece, 16*sizeof(int));
					if(rc < 0) {
						return -1;
					}
					// Update Checkpoint Region with IMap Piece
					checkpointRegion->iNodeMaps[pINum/16] = checkpointRegion->logEnd;
					checkpointRegion->logEnd += 16*sizeof(int);
					rc = WriteCR();
					if(rc < 0) {
						return -1;
					}

					// Update in Memory INodeMap
					iNodeMap[pINum] = iNodePointer;
					// Update INode
					parentINode->blocks[blockIndex] = checkpointRegion->logEnd + sizeof(INode);

					// Write INode
					rc = lseek(imageFD, iNodePointer, SEEK_SET);
					if(rc < 0) {
						return -1;
					}
					rc = write(imageFD, parentINode, sizeof(INode));
					if(rc < 0) {
						return -1;
					}
					checkpointRegion->logEnd += sizeof(INode);
					rc = WriteCR();
					if(rc < 0) {
						return -1;
					}
					// Write Dir Entries Block
					parentBlock[dirEntIndex].iNum = cINum;
					sprintf(parentBlock[dirEntIndex].name, cName);
					rc = lseek(imageFD, checkpointRegion->logEnd, SEEK_SET);
					if(rc < 0) {
						return -1;
					}
					rc = write(imageFD, parentBlock, MFS_BLOCK_SIZE);
					if(rc < 0) {
						return -1;
					}
					// Update checkpoint region
					checkpointRegion->logEnd += MFS_BLOCK_SIZE;
					rc = WriteCR();
					if(rc < 0) {
						return -1;
					}
					return 0;
				}
				else if(blockIndex == 13 && dirEntIndex == (MFS_BLOCK_SIZE/sizeof(DirEnt))-1) {
					return -1;
				}
			}
			//}
		}	
	}
	else {
		return -1;
	}
	return 0;
}

int GetParentDirEnt(int pINum, char *cName) {
	INode *parentINode = GetINode(pINum);
	if(parentINode != NULL && parentINode->type == MFS_DIRECTORY) {
		int blockIndex;		
		DirEnt *parentBlock = malloc(MFS_BLOCK_SIZE);
		for(blockIndex = 0; blockIndex < 14; blockIndex++) {
			int parentBlockPointer = parentINode->blocks[blockIndex];
			if(parentBlockPointer > 0) {
				int rc = lseek(imageFD, parentBlockPointer, SEEK_SET);
				if(rc < 0) {
					return -1;
				}
				rc = read(imageFD, parentBlock, MFS_BLOCK_SIZE);
				if(rc < 0) {
					return -1;
				}
				int dirEntIndex;			
				for(dirEntIndex = 0; dirEntIndex < MFS_BLOCK_SIZE/sizeof(DirEnt); dirEntIndex++) {
					DirEnt parentDirEnt = parentBlock[dirEntIndex];
					if(!strcmp(parentDirEnt.name, cName)) {
						return parentDirEnt.iNum;
					}
				}
			}
		}	
	}
	return -1;
}

int IsDirectoryEmpty(int iNum) {
	INode *iNode = GetINode(iNum);
	if(iNode != NULL && iNode->type == MFS_DIRECTORY) {
		int blockIndex;		
		DirEnt *block = malloc(MFS_BLOCK_SIZE);
		for(blockIndex = 0; blockIndex < 14; blockIndex++) {
			int blockPointer = iNode->blocks[blockIndex];
			if(blockPointer > 0) {
				//int blockPointer = iNode->blocks[0];
				int rc = lseek(imageFD, blockPointer, SEEK_SET);
				if(rc < 0) {
					return -1;
				}
				rc = read(imageFD, block, MFS_BLOCK_SIZE);
				if(rc < 0) {
					return -1;
				}
				int dirEntIndex;			
				for(dirEntIndex = 0; dirEntIndex < MFS_BLOCK_SIZE/sizeof(DirEnt); dirEntIndex++) {
					DirEnt dirEnt = block[dirEntIndex];
					if(!(dirEnt.iNum < 0 || !strcmp(dirEnt.name, ".") || !strcmp(dirEnt.name, ".."))) {
						return -1;
					}
				}
			}
		}	
	}
	return 0;
}

int WriteFile(int iNum, char *buffer, int block) {
	INode *iNode = GetINode(iNum);
	if(iNode != NULL && iNode->type == MFS_REGULAR_FILE) {
		int iNodePointer = 0;
		int iNodeMapPointer = checkpointRegion->iNodeMaps[iNum/16];
		int iNodeMapOffset = iNum % 16;
		int *iNodeMapPiece = malloc(16*sizeof(int));

		// Read IMap Piece
		rc = lseek(imageFD, iNodeMapPointer, SEEK_SET);
		if(rc < 0) {
			return -1;
		}
		rc = read(imageFD, iNodeMapPiece, 16*sizeof(int));
		if(rc < 0) {
			return -1;
		}
		// Update INodeMap Piece
		iNodeMapPiece[iNodeMapOffset] = iNodePointer = checkpointRegion->logEnd + 16*sizeof(int);
		// Write INodeNum to IMap Piece
		rc = lseek(imageFD, checkpointRegion->logEnd, SEEK_SET);
		if(rc < 0) {
			return -1;
		}
		rc = write(imageFD, iNodeMapPiece, 16*sizeof(int));
		if(rc < 0) {
			return -1;
		}
		// Update Checkpoint Region with IMap Piece
		checkpointRegion->iNodeMaps[iNum/16] = checkpointRegion->logEnd;
		checkpointRegion->logEnd += 16*sizeof(int);
		rc = WriteCR();
		if(rc < 0) {
			return -1;
		}

		// Update in Memory INodeMap
		iNodeMap[iNum] = iNodePointer;

		// Calculate Size of File
		int i; 
		for(i = MFS_BLOCK_SIZE - 1; i >= 0; i--) {
			if(buffer[i] != '\0') {
				iNode->size = (MFS_BLOCK_SIZE * block) + i + 1;
				break;
			}
		}
		// Update INode
		iNode->blocks[block] = checkpointRegion->logEnd + sizeof(INode);

		// Write INode
		int rc = lseek(imageFD, iNodePointer, SEEK_SET);
		if(rc < 0) {
			return -1;
		}
		rc = write(imageFD, iNode, sizeof(INode));
		if(rc < 0) {
			return -1;
		}
		checkpointRegion->logEnd += sizeof(INode);
		rc = WriteCR();
		if(rc < 0) {
			return -1;
		}
		// Write Buffer Data
		rc = lseek(imageFD, checkpointRegion->logEnd, SEEK_SET);
		if(rc < 0) {
			return -1;
		}
		rc = write(imageFD, buffer, MFS_BLOCK_SIZE);
		if(rc < 0) {
			return -1;
		}
		// Update checkpoint region
		checkpointRegion->logEnd += MFS_BLOCK_SIZE;
		rc = WriteCR();
		if(rc < 0) {
			return -1;
		}
	}
	else {
		return -1;
	}
	return 0;
}

int MkINode(int type, int size) {
	int iNodeMapPointer = checkpointRegion->iNodeMaps[currentINodeNum/16];
	int iNodeMapOffset = currentINodeNum % 16;
	int *iNodeMapPiece = malloc(16*sizeof(int));
	int iNodePointer = 0;
	int rc;
	if(iNodeMapOffset == 0) {
		// Write IMap Piece with initial INode
		iNodeMapPiece[0] = iNodePointer = checkpointRegion->logEnd + 16*sizeof(int);
		rc = lseek(imageFD, checkpointRegion->logEnd, SEEK_SET);
		if(rc < 0) {
			return -1;
		}
		rc = write(imageFD, iNodeMapPiece, 16*sizeof(int));
		if(rc < 0) {
			return -1;
		}
		// Update Checkpoint Region with IMap Piece
		checkpointRegion->iNodeMaps[currentINodeNum/16] = checkpointRegion->logEnd;
		checkpointRegion->logEnd += 16*sizeof(int);
		rc = WriteCR();
		if(rc < 0) {
			return -1;
		}
	}
	else {
		// Read IMap Piece
		rc = lseek(imageFD, iNodeMapPointer, SEEK_SET);
		if(rc < 0) {
			return -1;
		}
		rc = read(imageFD, iNodeMapPiece, 16*sizeof(int));
		if(rc < 0) {
			return -1;
		}
		// Write INodeNum to IMap Piece
		iNodeMapPiece[iNodeMapOffset] = iNodePointer = checkpointRegion->logEnd + 16*sizeof(int);
		rc = lseek(imageFD, checkpointRegion->logEnd, SEEK_SET);
		if(rc < 0) {
			return -1;
		}
		rc = write(imageFD, iNodeMapPiece, 16*sizeof(int));
		if(rc < 0) {
			return -1;
		}
		// Update Checkpoint Region with IMap Piece
		checkpointRegion->iNodeMaps[currentINodeNum/16] = checkpointRegion->logEnd;
		checkpointRegion->logEnd += 16*sizeof(int);
		rc = WriteCR();
		if(rc < 0) {
			return -1;
		}
	}
	iNodeMap[currentINodeNum] = iNodePointer;

	INode *iNode = malloc(sizeof(INode));
	iNode->type = type;
	iNode->size = size;
	if(type == MFS_DIRECTORY) {
		iNode->blocks[0] = iNodePointer + sizeof(INode);
	}
	rc = lseek(imageFD, iNodePointer, SEEK_SET);
	if(rc < 0) {
		return -1;
	}
	rc = write(imageFD, iNode, sizeof(INode));
	if(rc < 0) {
		return -1;
	}

	checkpointRegion->logEnd += sizeof(INode);
	rc = WriteCR();
	if(rc < 0) {
		return -1;
	}

	currentINodeNum++;
	return currentINodeNum-1;
}

int MkDir(int pINum, char *name) {
	int iNodeNum = MkINode(MFS_DIRECTORY, 2*sizeof(DirEnt));
	if(iNodeNum < 0) {
		return -1;
	}
	DirEnt *block = malloc(MFS_BLOCK_SIZE);
	sprintf(block[0].name,".");
	block[0].iNum = iNodeNum;
	sprintf(block[1].name, "..");
	block[1].iNum = pINum;
	int i;
	for (i = 2; i < MFS_BLOCK_SIZE/sizeof(DirEnt); i++) {
		block[i].iNum = -1;
	}
	int rc = lseek(imageFD, checkpointRegion->logEnd, SEEK_SET);
	if(rc < 0) {
		return -1;
	}
	rc = write(imageFD, block, MFS_BLOCK_SIZE);
	if(rc < 0) {
		return -1;
	}
	// Update checkpoint region
	checkpointRegion->logEnd += MFS_BLOCK_SIZE;
	rc = WriteCR();
	if(rc < 0) {
		return -1;
	}
	// Update parent directory entry
	// Only update directories that aren't the root dir	
	if(pINum != iNodeNum) {
		rc = WriteParentDirEnt(pINum, iNodeNum, name);
		if(rc < 0) {
			return -1;
		}
	}
	return 0;
}

int MkFile(int pINum, char *name) {
	int iNodeNum = MkINode(MFS_REGULAR_FILE, 0);
	if(iNodeNum < 0) {
		return -1;
	}
	// Update parent directory entry
	//// Only update directories that aren't the root dir	
	//if(pINum != iNodeNum) {
	int rc = WriteParentDirEnt(pINum, iNodeNum, name);
	if(rc < 0) {
		return -1;
	}
	//}
	return 0;
}

int ReadFile(int iNum, char *buffer, int block) {
	INode *iNode = GetINode(iNum);
	if(iNode == NULL) {
		return -1;
	}
	int blockPointer = iNode->blocks[block];
	// Read INode Block
	rc = lseek(imageFD, blockPointer, SEEK_SET);
	if(rc < 0) {
		return -1;
	}
	rc = read(imageFD, buffer, MFS_BLOCK_SIZE);
	if(rc < 0) {
		return -1;
	}
	return 0;
}

void MFS_Lookup(int pINum, char *name) {
	int retCode = -1;		
	INode *iNode = GetINode(pINum);
	if(iNode != NULL && iNode->type == MFS_DIRECTORY) {
		retCode = GetParentDirEnt(pINum, name);
	}

	fsync(imageFD);
    char reply[BUFFER_SIZE];
    sprintf(reply, "%d", retCode);
    rc = UDP_Write(sd, s, reply, BUFFER_SIZE);
}

void MFS_Stat(int iNum){
	int retCode = -1;
    Stat *stat = malloc(sizeof(Stat));
	INode *iNode = GetINode(iNum);
	if(iNode != NULL) {
		stat = (Stat *)iNode;		
		retCode = 0;
	}

	fsync(imageFD);
    char reply[BUFFER_SIZE];
    sprintf(reply, "%d~%d~%d", retCode, stat->type, stat->size);
    rc = UDP_Write(sd, s, reply, BUFFER_SIZE);
}

void MFS_Write(int iNum, char *buffer, int block) {
	int retCode = -1;		
	
	if(block >= 0 && block < 14) {
		retCode = WriteFile(iNum, buffer, block);
	}

	fsync(imageFD);
    char reply[BUFFER_SIZE];
    sprintf(reply, "%d", retCode);

    rc = UDP_Write(sd, s, reply, BUFFER_SIZE);

}

void MFS_Read(int iNum, int block) {
	int retCode = -1;
	char *buffer = malloc(BUFFER_SIZE);
	if(block >= 0 && block < 14) {
		retCode = ReadFile(iNum, buffer, block);
	}

	fsync(imageFD);
    char reply[BUFFER_SIZE];
    sprintf(reply, "%d", retCode);
	//memcpy(reply, buffer, BUFFER_SIZE);
    rc = UDP_Write(sd, s, reply, BUFFER_SIZE);
    rc = UDP_Write(sd, s, buffer, BUFFER_SIZE);
}

void MFS_Creat(int pINum, int type, char *name) {
	int retCode = -1;	
	if(strlen(name) <= 28) {
		if(type == MFS_REGULAR_FILE) {
			retCode = MkFile(pINum, name);
		}
		else {
			retCode = MkDir(pINum, name);
		}
	}
	fsync(imageFD);

    char reply[BUFFER_SIZE];
    sprintf(reply, "%d", retCode);
    rc = UDP_Write(sd, s, reply, BUFFER_SIZE);
}

void MFS_Unlink(int pINum, char *name) {
	int retCode = -1;
	int iNum = GetParentDirEnt(pINum, name);
	if(iNum >= 0) {
		retCode = IsDirectoryEmpty(iNum);
		if(retCode == 0) {
			retCode = WriteParentDirEnt(pINum, -1, name);
		}
	}
	else {
		retCode = 0;
	}

	fsync(imageFD);
    char reply[BUFFER_SIZE];
    sprintf(reply, "%d", retCode);
    rc = UDP_Write(sd, s, reply, BUFFER_SIZE);
}

void MFS_Shutdown(fd) {
	fsync(fd);
	close(fd);
	exit(0);
}

int
main(int argc, char *argv[])
{
	int port;
	char *image, *args[3];

	port = atoi(argv[1]);
	image = argv[2];

    sd = UDP_Open(port);
    assert(sd > -1);

	imageFD = open(image, O_RDWR|O_CREAT, S_IRWXU);
	assert(imageFD >= 0);

	currentINodeNum = 0;
	s = malloc(sizeof(struct sockaddr_in));
	checkpointRegion = malloc(sizeof(CheckReg));
	iNodeMap = malloc(4096*sizeof(int));
	struct stat imageStat;
	stat(image, &imageStat);
	if (!imageStat.st_size) {
		checkpointRegion->logEnd = sizeof(CheckReg);
		WriteCR();
		MkDir(currentINodeNum, "");
		fsync(imageFD);
	} else {
		read(imageFD, checkpointRegion, sizeof(CheckReg));
		int i;
		for (i = 0; i < 256; i++) {
			int iNodeMapPointer = checkpointRegion->iNodeMaps[i];
			if(iNodeMapPointer > 0) {			
				lseek(imageFD, iNodeMapPointer, SEEK_SET);
				read(imageFD, iNodeMap + (16*i), 16*sizeof(int));
			}
			else {
				int j;
				for(j = 0; j < 16; j++) {
					if(iNodeMap[(16*(i-1)) + j] == 0) {
						currentINodeNum = (16*(i-1)) + j;
						break;
					}
				}
				if(currentINodeNum == 0) {
					currentINodeNum = 16*i;
				}
				break;
			}
		}
	}

    printf("waiting in loop\n");

    while (1) {
		char buffer[2*BUFFER_SIZE];
		int rc = UDP_Read(sd, s, buffer, BUFFER_SIZE);
		if (rc > 0) {
			printf("SERVER:: read %d bytes (message: '%s')\n", rc, buffer);
			switch(atoi(strtok(buffer, "~"))) {
				case 0:
					args[0] = strtok(NULL, "~");
					args[1] = strtok(NULL, "~");
					MFS_Lookup(atoi(args[0]), args[1]);
					break;
				case 1:
					args[0] = strtok(NULL, "~");
					MFS_Stat(atoi(args[0]));
					break;
				case 2:
					args[0] = strtok(NULL, "~");
					args[2] = strtok(NULL, "~");
					char *writeBuffer = malloc(BUFFER_SIZE);
					//memcpy(writeBuffer, buffer, BUFFER_SIZE);
					rc = UDP_Read(sd, s, writeBuffer, BUFFER_SIZE);
					if (rc > 0) {
						args[1] = writeBuffer;
						MFS_Write(atoi(args[0]), args[1], atoi(args[2]));
					}
					break;
				case 3: 
					args[0] = strtok(NULL, "~");
					args[1] = strtok(NULL, "~");
					MFS_Read(atoi(args[0]), atoi(args[1]));
					break;
				case 4:
					args[0] = strtok(NULL, "~");
					args[1] = strtok(NULL, "~");
					args[2] = strtok(NULL, "~");
					MFS_Creat(atoi(args[0]), atoi(args[1]), args[2]);
					break;
				case 5: 
					args[0] = strtok(NULL, "~");
					args[1] = strtok(NULL, "~");
					MFS_Unlink(atoi(args[0]), args[1]);
					break;
				case 6:
					MFS_Shutdown(imageFD);
					break;
			}
		}
    }

    return 0;
}
