/*
 * FDIOStream.cpp
 *
 *  Created on: Sep 27, 2012
 *      Author: Mitchell Wills
 */

#include "networktables2/stream/FDIOStream.h"
#include "networktables2/util/IOException.h"
#include "networktables2/util/EOFException.h"

#include <errno.h>
#include <stdlib.h>
#ifdef _WRS_KERNEL
#include <ioLib.h>
#else
#include <unistd.h>
#endif
#include <stdio.h>


FDIOStream::FDIOStream(int _fd){
  fd = _fd;
  //	f = fdopen(_fd, "rbwb");
  //	if(f==NULL)
  //		throw IOException("Could not open stream from file descriptor", errno);
}
FDIOStream::~FDIOStream(){
	close();
}

int FDIOStream::read(void* ptr, int numbytes){
	if(numbytes==0)
		return 0;
	char* bufferPointer = (char*)ptr;
	int totalRead = 0;
	while (totalRead < numbytes) {
		int numRead = ::read(fd, bufferPointer, numbytes-totalRead);
		if(numRead == 0){
			throw EOFException();
		}
		else if (numRead < 0) {
			perror("read error: ");
			fflush(stderr);
			throw IOException("Error on FDIO read");
		}
		bufferPointer += numRead;
		totalRead += numRead;
	}
	return totalRead;
}
int FDIOStream::write(const void* ptr, int numbytes){
  int numWrote = ::write(fd, (char*)ptr, numbytes);//TODO: this is bad
  //int numWrote = fwrite(ptr, 1, numbytes, f);
  if(numWrote==numbytes)
    return numWrote;
  perror("write error: ");
  fflush(stderr);
  throw IOException("Could not write all bytes to fd stream");
	
}
void FDIOStream::flush(){
  //if(fflush(f)==EOF)
  //  throw EOFException();
}
void FDIOStream::close(){
  //fclose(f);//ignore any errors closing
  ::close(fd);
}

