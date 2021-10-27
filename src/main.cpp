#include "editor.hpp"
#include "files.hpp"
#include <iostream>
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

int main(int argc, char* argv[]) {
    Files::Init(argc, argv);

    try {
        Editor editor;
        editor.Run();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return 1;
    }

    return 0;
}
