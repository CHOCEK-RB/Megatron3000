#ifndef UTILS_HPP
#define UTILS_HPP

class utils{
  public:

  static bool directoryExists(const char *path);
  static int countDigits(int num);
  static void writeInt(int num, char *line, int &pos);

  static void createFullPath(int disk, int superface, int track, int sector, char *fullPath);
};

#endif // !UTILS_HPP
