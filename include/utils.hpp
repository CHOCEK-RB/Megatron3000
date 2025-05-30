#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>
class utils{
  public:

  static bool directoryExists(const char *path);
  static int countDigits(int num);
  static void writeInt(int num, char *line, int &pos);
  static std::string inputSchema();

  static std::string createFullPath(int disk, int surface, int track, int sector);
};

#endif // !UTILS_HPP
