#include <unistd.h>

// check from path is exists
int is_valid_path(const char *path)
{
  return (access(path, F_OK) == 0);
}
