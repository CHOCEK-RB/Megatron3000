#include "const.cpp"
#include <fcntl.h>
#include <head.hpp>
#include <iostream>
#include <unistd.h>
#include <utils.hpp>

void Head::moveTo(unsigned int disk, unsigned int surface, unsigned int track, unsigned int sector) {
  if (currentDisk == disk && currentSurface == surface && currentTrack == track && currentSector == sector)
    return;
  
  currentDisk = disk;
  currentSurface = surface;
  currentTrack = track;
  currentSector = sector;

  close(currentFd);
  currentFd = -1;
}

int Head::openCurrentSectorFD() {
  if (currentFd != -1)
    close (currentFd);

  char fullPath[SIZE_FULL_PATH];

  utils::createFullPath(
      currentDisk, currentSurface, currentTrack, currentSector, fullPath);

  currentFd = open(fullPath, O_RDWR, 0644);

  if (currentFd == -1)
    std::cerr << "No se pudo abrir el sector en la direccion : " << fullPath
              << "\n";

  return currentFd;
}

void Head::resetPosition() {
  currentDisk = currentSurface = currentTrack = currentSector = 0;

  if(currentFd != -1){
    close(currentFd);
    currentFd = -1;
  }
}
