#ifndef COD2_ARB_TO_MSL_H
#define COD2_ARB_TO_MSL_H

#include <map>
#include "arb_raster.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace cod2::arb {

enum class Stage { Vertex, Fragment };

class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

/* No game shader text is embedded here. Bindings retain ARB register indices. */
struct Operand {
    std::string name, index, swizzle;
    std::vector<std::string> literal;
    bool negative = false, absolute = false;
};

struct Instruction {
    std::string opcode;
    Operand destination;
    std::vector<Operand> sources;
    bool saturate = false;
    int texture = -1;
    std::string target;
};

struct Parameter {
    std::string binding;
    std::vector<std::string> literal;
};

struct Program {
    Stage stage;
    std::vector<std::string> temporaries, addresses;
    std::map<std::string, Operand> aliases;
    std::map<std::string, std::vector<Parameter>> parameters;
    std::vector<std::string> stateBindings;
    std::vector<std::string> assumptions;
    std::map<unsigned, std::string> textures;
    std::vector<Instruction> instructions;
};

struct Translation {
    Program program;
    std::string source;
};

Program Parse(const std::string &source);
std::string Emit(const Program &program);
Translation Translate(const std::string &source);

} // namespace cod2::arb
#endif
