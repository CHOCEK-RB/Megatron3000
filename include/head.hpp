#ifndef HEAD_HPP
#define HEAD_HPP

class Head {
public:
  unsigned int currentDisk;
  unsigned int currentSurface;
  unsigned int currentTrack;
  unsigned int currentSector;
  unsigned int currentBlock;
  int currentFd;

  Head() : currentDisk(0), currentSurface(0), currentTrack(0), currentSector(0), currentBlock(0) {};

  void moveTo(unsigned int disk, unsigned int surface, unsigned int track, unsigned int sector);

  int openCurrentSectorFD();
  void resetPosition();
};

#endif // HEAD_HPP
