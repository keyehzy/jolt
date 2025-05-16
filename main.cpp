#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

typedef int64_t i64;
typedef uint8_t u8;

typedef int (*JitFunction)();

class Jit {
private:
  void *data_;
  size_t size_;

public:
  Jit(size_t size) : size_(size) {
    // Allocate memory with read, write, and execute permissions
    data_ = mmap(nullptr, size, PROT_READ | PROT_WRITE | PROT_EXEC,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (data_ == MAP_FAILED) {
      throw std::runtime_error("Memory allocation failed");
    }
  }

  ~Jit() {
    if (data_ != MAP_FAILED) {
      munmap(data_, size_);
    }
  }

  void *data() { return data_; }

  size_t size() { return size_; }

  void finalize() {
    if (mprotect(data_, size_, PROT_READ | PROT_EXEC) != 0) {
      throw std::runtime_error("Failed to change memory protection");
    }
  }
};

int main() {
  size_t size = 4096;
  Jit jit(size);

  // Example:
  // int x() {
  //   return 42;
  // }
  unsigned char code[] = {
      0xB8,  0x2A, 0x00, 0x00, 0x00, // mov eax, 42
      0xC3,
  };

  // Copy the code to the JIT memory
  std::memcpy(jit.data(), code, sizeof(code));

  // Make the memory executable
  jit.finalize();

  // Cast the memory to a function pointer
  JitFunction func = reinterpret_cast<JitFunction>(jit.data());

  // Call the function
  int result = func();

  // Print the result
  std::cout << "Result: " << result << std::endl;
  return 0;
}
