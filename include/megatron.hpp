#ifndef MEGATRON_HPP
#define MEGATRON_HPP

#include <diskController.hpp>

class Megatron {

public:

  DiskController *diskController;
  
  Megatron() = default;
  ~Megatron();

  void init();

  void buildStructure();
  void initializeBootSector(unsigned int numberdisks, unsigned int numbertracks, unsigned int numbersectors, unsigned int numberBytes, unsigned int sectorsBlock);
};

#endif
