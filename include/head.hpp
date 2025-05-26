#ifndef HEAD_HPP
#define HEAD_HPP

class Head {
public:
  int currentDisk;
  int currentSurface;
  int currentTrack;
  int currentSector;

  int currentFd;

  Head() : currentDisk(0), currentSurface(0), currentTrack(0), currentSector(0) {};

  void moveTo(int disk, int surface, int track, int sector);

  int openCurrentSectorFD();
  void resetPosition();
};

#endif // HEAD_HPP
