#include "const.cpp"
#include <fcntl.h>
#include <head.hpp>
#include <iostream>
#include <utils.hpp>

void Head::moveTo(int disk, int surface, int track, int sector) {
  currentDisk = disk;
  currentSurface = surface;
  currentTrack = track;
  currentSector = sector;
}

int Head::openCurrentSectorFD() {
  char fullPath[SIZE_FULL_PATH];

  utils::createFullPath(
      currentDisk, currentSurface, currentTrack, currentSector, fullPath);

  currentFd = open(fullPath, O_WRONLY, 0644);

  if (currentFd == -1)
    std::cerr << "No se pudo abrir el sector en la direccion : " << fullPath
              << "\n";

  return currentFd;
}

void Head::resetPosition() {
  currentDisk = currentSurface = currentTrack = currentSector = 0;
}
