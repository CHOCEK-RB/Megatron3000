#ifndef MEGATRON_HPP
#define MEGATRON_HPP

#include <diskController.hpp>
#include <string>

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
  std::string getSchema(const std::string& name);
};
#endif
