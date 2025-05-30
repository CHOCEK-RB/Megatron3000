#ifndef HEAD_HPP
#define HEAD_HPP

class Head {
public:
  unsigned int numberDisks;

  unsigned int currentTrack;
  unsigned int currentSector;
  int *heads;

  Head(int disks) : numberDisks(disks), currentTrack(0), currentSector(0) {
    heads = new int[disks * 2];
    for (int i = 0; i < disks * 2; ++i) {
      heads[i] = -1;
    }
  };

  void moveTo(unsigned int track, unsigned int sector);

  bool openCurrentSectorFD();
  void resetPosition();
};

#endif // HEAD_HPP
