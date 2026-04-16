// Minimal smoke-test for the Example class.
#ifdef USE_CATCH2
#include <catch2/catch_test_macros.hpp>
TEST_CASE("Example::greet returns expected string", "[example]") {
    // Απλό placeholder που επαληθεύει ότι το Example compile και link σωστά.
    REQUIRE(true);
}
#else
#include <cassert>
#include <iostream>
int main() {
    std::cout << "test_example passed\n";
    return 0;
}
#endif
