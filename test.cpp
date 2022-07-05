#include "filec.hpp"
#include <iostream>
#include <fstream>

#include <cstdlib>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define CHUNK_SIZE 8

int main() {
  // Get the size of the file for testing
  struct stat s;
  stat("testfile.txt", &s);
  int fsize = s.st_size;

  // Server
  std::fstream servfile;
  servfile.open("testfile.txt", std::ios::binary | std::ios::ate | std::ios::in);
  filec::chunker serv = filec::chunker(servfile, 0, fsize, CHUNK_SIZE);

  // Client
  std::fstream clientfile;
  clientfile.open("client.txt", std::ios::binary | std::ios::ate | std::ios::out);
  filec::chunker cl = filec::chunker(clientfile, 0, fsize, CHUNK_SIZE);

  // Let client read from server
  while (true) {
    // Client sends fread for each chunk
    while(!serv.complete()) {
      filec::chunk b;
      serv >> b;

      // 50-50 chance of discarding a packet
      if (!(rand() & 0b1))
        continue;

      cl << b;
    }

    // If all chunks have been read, break
    if (cl.complete())
      break;

    // Client sends the pagemap to the server
    uint8_t *clpm = cl.get_pagemap();
    serv.set_pagemap(clpm);
  }

  // Close files
  servfile.close();
  clientfile.close();

  return 0;
}
