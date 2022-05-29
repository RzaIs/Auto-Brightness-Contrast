#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <getopt.h>
#include <fcntl.h>

typedef union IntUnion {
  int nNumber;
  char array[4];
} UIntUnion;

typedef struct Color {
  unsigned char alpha, blue, green, red;
} SColor;

typedef struct ImageInfo {
	int nHeaderSize, nWidth, nHeight, nNumberOfBytePerPixel;
} SImageInfo;

SImageInfo getImageInfo(int fd);

int findDarkest(SColor * pColors, int nSize);
int findBrightest(SColor * pColors, int nSize);

void shiftBrightness(SColor * pColors, int nSize);
void addContrast(SColor * pColors, int nSize);
void convertToBigEndian(UIntUnion * pData);
void writeResilt(int outFD, SColor * pColors, unsigned char * pHeader, SImageInfo info);

SColor * readColors(int fd, SImageInfo info);

int main(int argc, char * argv[]) {

  int opt;
  int output = 1; // stdout

  if (argc == 1) {
    printf("ERROR: you must provide a .bmp file name\n  for help: -h\n");
    exit(1);
  }

  if (argv[1][0] != '-'){
    argv++;
    argc--;
  }

  while ((opt = getopt(argc, argv, "o:h")) != -1) {
    switch (opt) {
      case 'o':
        output = open(optarg, O_WRONLY | O_TRUNC | O_CREAT, 0666);
        if (output == -1) {
          printf("ERROR: failed to write into file: %s\n", optarg);
          exit(2);
        }
        break;
      case 'h':
        printf("usage: autoadjust srcname.bmp [-o <OutputFile>]\nexample: autoadjust srcname.bmp -o dstname.bmp\n");
        exit(0);
      default:
        exit(5);
        break;
    }
  }

  char * szEnding = strrchr(argv[0], '.');

  if (szEnding == NULL) {
    printf("ERROR: you must provide a .bmp file name\n  for help: -h\n");
    exit(1);
  }

  if (strcmp(szEnding, ".bmp") != 0) {
    printf("ERROR: you must provide a .bmp file name\n  for help: -h\n");
    exit(1);
  }

  char * szFileName = argv[0];

  int fd = open(szFileName, O_RDONLY);
  if (fd == -1) {
    printf("ERROR: failed to open file: %s\n", szFileName);
    exit(3);
  }

	SImageInfo info = getImageInfo(fd);
	close(fd);

  fd = open(szFileName, O_RDONLY);
	
  unsigned char * pHeader = (unsigned char *)calloc(info.nHeaderSize, sizeof(unsigned char));
  if (pHeader == NULL) {
    printf("ERROR: cannot alloc pHeader\n");
    exit(4);
  }

	read(fd, pHeader, sizeof(char) * info.nHeaderSize);
  
  int nSize = info.nHeight * info.nWidth;
  SColor * pColors = readColors(fd, info);
	
  close(fd);

  shiftBrightness(pColors, nSize);
  addContrast(pColors, nSize);

  writeResilt(output, pColors, pHeader, info);

  if (output != 1) close(output);
  free(pColors);
  free(pHeader);

  return 0;
}

void writeResilt(int outFD, SColor * pColors, unsigned char * pHeader, SImageInfo info) {
  write(outFD, pHeader, info.nHeaderSize);
  int nSize = info.nHeight * info.nWidth;
  
  if (info.nNumberOfBytePerPixel == 4) {
    for (int i = 0; i < nSize; i++) {
			write(1, &pColors[i], sizeof(SColor));
    }
  } else {
    for (int i = 0; i < nSize; i++) {
			write(outFD, &pColors[i].blue, sizeof(char) * 3);
    }
  }
}

SColor * readColors(int fd, SImageInfo info) {
	int nSize = info.nWidth * info.nHeight;

	SColor * pColors = (SColor *)calloc(nSize, sizeof(SColor));
  if (pColors == NULL) {
    printf("ERROR: cannot alloc pColors\n");
    exit(4);
  }
  if (info.nNumberOfBytePerPixel == 4) {
    for (int i = 0; i < nSize; i++) {
			read(fd, &pColors[i], sizeof(SColor));
    }
  } else {
    for (int i = 0; i < nSize; i++) {
			read(fd, &pColors[i].blue, sizeof(char) * 3);
    }
  }
  return pColors;
}

SImageInfo getImageInfo(int fd) {
	SImageInfo info;
  UIntUnion uBuffer;
	char buffer[4];

  // B M
	read(fd, buffer, sizeof(char) * 2);

  // Size of file
	read(fd, &uBuffer, sizeof(int));
  convertToBigEndian(&uBuffer);

  // reserve skipped
	read(fd, buffer, sizeof(int));

	// size of header
	read(fd, &uBuffer, sizeof(int));
  convertToBigEndian(&uBuffer);
  info.nHeaderSize = uBuffer.nNumber;

	// rest size of header
	read(fd, buffer, sizeof(int));

  // width
	read(fd, &uBuffer, sizeof(int));
  convertToBigEndian(&uBuffer);
  info.nWidth = uBuffer.nNumber;

  // height
	read(fd, &uBuffer, sizeof(int));
  convertToBigEndian(&uBuffer);
  info.nHeight = uBuffer.nNumber;

	// nb of layer
	read(fd, buffer, sizeof(char) * 2);

	// number of byte per pixel
  unsigned char nbbtprpx;
	read(fd, &nbbtprpx, sizeof(char));
	info.nNumberOfBytePerPixel = nbbtprpx / 8;

	return info;
}


int findDarkest(SColor * pColors, int nSize) {
	int nMin = 255;
	
	for (int i = 0; i < nSize; i++) {
		SColor color = pColors[i];
		if (color.red < nMin) {
			nMin = color.red;
		}
		if (color.green < nMin) {
			nMin = color.green;
		}
		if (color.blue < nMin) {
			nMin = color.blue;
		}
	}
	return nMin;
}

int findBrightest(SColor * pColors, int nSize) {
	int nMax = 0; 

	for (int i = 0; i < nSize; i++) {
		SColor color = pColors[i];
		if (color.red > nMax) {
			nMax = color.red;
		}
		if (color.green > nMax) {
			nMax = color.green;
		}
		if (color.blue > nMax) {
			nMax = color.blue;
		}
	}
	return nMax;
}

void shiftBrightness(SColor * pColors, int nSize) {
	int nMin = findDarkest(pColors, nSize);

	for (int i = 0; i < nSize; i++) {
    pColors[i].red -= nMin + log(1.0 + (pColors[i].red - nMin));
    pColors[i].green -= nMin + log(1.0 + (pColors[i].green - nMin));
    pColors[i].blue -= nMin + log(1.0 + (pColors[i].blue - nMin));
	}
}

void addContrast(SColor * pColors, int nSize) {
	int nMax = findBrightest(pColors, nSize);

	double dCoeff = 255.0 / nMax;

	for (int i = 0; i < nSize; i++) {
    pColors[i].red *= dCoeff;
    pColors[i].green *= dCoeff;
    pColors[i].blue *= dCoeff;
	} 
}

void convertToBigEndian(UIntUnion * pData) {
  unsigned int x = 1;
  if ((int)*((char*) &x) == 0) {
    char temp = pData->array[0];
    pData->array[0] = pData->array[3];
    pData->array[3] = temp;

    temp = pData->array[1];
    pData->array[1] = pData->array[2];
    pData->array[2] = temp;
  }
}
