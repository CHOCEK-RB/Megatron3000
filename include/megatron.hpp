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

  void readCSV(int lines = -1);
  void generateSchema();

  void createSchema();
};
#endif
