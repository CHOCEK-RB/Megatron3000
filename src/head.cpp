#include "const.cpp"
#include <fcntl.h>
#include <head.hpp>
#include <iostream>
#include <unistd.h>
#include <utils.hpp>

void Head::moveTo(unsigned int track, unsigned int sector) {
  if (currentTrack == track && currentSector == sector)
    return;

  currentTrack = track;
  currentSector = sector;

  for (size_t i = 0; i < numberDisks; ++i) {
    if (heads[i] != -1) {
      close(heads[i]);
      heads[i] = -1;
    }
  }
}

bool Head::openCurrentSectorFD() {
  for (size_t i = 0; i < numberDisks; ++i) {
    if (heads[i] != -1) {
      close(heads[i]);
      heads[i] = -1;
    }
  }
  // Abrir todos los archivos en el cinlidro actual
  for (size_t i = 0; i < numberDisks; ++i) {
    for (int s = 0; s < 2; ++s) {
      std::string fullPath = utils::createFullPath(i, s, currentTrack, currentSector);

      int fd = open(fullPath.c_str(), O_RDWR , 0644);
      if (fd == -1) {
        std::cerr << "Error opening file: " << fullPath << std::endl;
        return false;
      }

      heads[i * 2 + s] = fd;
    }
  }
  return true;
}

void Head::resetPosition() {
  currentTrack = 0;
  currentSector = 0;
}
