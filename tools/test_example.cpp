#include <cassert>
#include <iostream>
#include "../include/example.h"

int main() {
    Example ex;
    assert(ex.greet() == "Hello from Example!");
    std::cout << "test_example passed\n";
    return 0;
}
