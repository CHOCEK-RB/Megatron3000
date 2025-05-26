#include "const.cpp"
#include <sys/stat.h>
#include <unistd.h>
#include <utils.hpp>

bool utils::directoryExists(const char *path) {
  struct stat info;
  if (stat(path, &info) != 0) {
    return false;
  }
  return (info.st_mode & S_IFDIR) != 0;
}

int utils::countDigits(int num) {
  if (num == 0)
    return 1;

  int count = 0;
  if (num < 0)
    num = -num;

  while (num > 0) {
    num /= 10;
    count++;
  }
  return count;
}

void utils::writeInt(int num, char *line, int &pos) {
  if (num == 0) {
    line[pos++] = '0';
    return;
  }

  int div = 1;
  int temp = num;

  while (temp >= 10) {
    temp /= 10;
    div *= 10;
  }

  while (div > 0) {
    char digit = '0' + (num / div);
    line[pos++] = digit;

    num %= div;
    div /= 10;
  }
}

void utils::createFullPath(int disk,
                           int superface,
                           int track,
                           int sector,
                           char *fullPath) {
  int pos = 0;

  for (int i = 0; PATH[i]; ++i)
    fullPath[pos++] = PATH[i];
  fullPath[pos++] = '/';

  writeInt(disk, fullPath, pos);
  fullPath[pos++] = '/';

  writeInt(superface, fullPath, pos);
  fullPath[pos++] = '/';

  writeInt(track, fullPath, pos);
  fullPath[pos++] = '/';

  writeInt(sector, fullPath, pos);

  fullPath[pos++] = '.';
  fullPath[pos++] = 'd';
  fullPath[pos++] = 'a';
  fullPath[pos++] = 't';

  fullPath[pos++] = '\0';
}
