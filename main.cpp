#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>

#include "simple_tdd.h"

typedef int64_t i64;
typedef uint8_t u8;

typedef int (*JitFunction)();

struct x86_64_Instr {
  static const u8 MOV = 0xB8; // Move immediate to register
  static const u8 RET = 0xC3; // Return from function
};

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

TEST(should_return_42) {
  Jit jit(4096);

  // Example:
  // int x() {
  //   return 42;
  // }
  u8 code[] = {
      x86_64_Instr::MOV, 0x2A, 0x00, 0x00, 0x00, // mov eax, 42
      x86_64_Instr::RET,                         // ret
  };

  // Copy the code to the JIT memory
  std::memcpy(jit.data(), code, sizeof(code));

  // Make the memory executable
  jit.finalize();

  // Cast the memory to a function pointer
  JitFunction func = reinterpret_cast<JitFunction>(jit.data());

  // Call the function
  int result = func();
  
  ASSERT(result == 42);
}

TEST_MAIN();