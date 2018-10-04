#include <cstdio>
#include <cstdlib>
#include <string>

int main(int argc, char* argv[]) {
  int count = std::atoi(argv[1]);
  auto msg = std::string(count, 'x');
  auto handle = msg.c_str();
  while(true) {
    std::puts(handle);
  }
}
