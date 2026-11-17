#include "shader_setup.h"
#include <iostream>

static void Result(const char *name, int status, const char *detail, void *)
{
    std::cout << name << '\t' << status << '\t' << detail << '\n';
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::cerr << "Usage: prepare_shaders external-input-cache external-output-cache\n";
        return 2;
    }
    return MacMetal_PrepareShaderCache(argv[1], argv[2], Result, nullptr);
}
