#include "arb_to_msl.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <set>

int main(int argc, char **argv)
{
    if (argc != 3) {
        std::cerr << "Usage: arb_to_msl input.vsa|input.pse output.metal\n";
        return 2;
    }
    try {
        std::ifstream in(argv[1]);
        if (!in) throw cod2::arb::Error("Cannot read input");
        auto result = cod2::arb::Translate(std::string(std::istreambuf_iterator<char>(in), {}));
        std::ofstream out(argv[2]);
        if (!out || !(out << result.source)) throw cod2::arb::Error("Cannot write output");
        std::set<std::string> ops;
        for (const auto &i : result.program.instructions) ops.insert(i.opcode + (i.saturate ? "_SAT" : ""));
        std::cout << "{\"stage\":\"" << (result.program.stage == cod2::arb::Stage::Vertex ? "vertex" : "fragment") << "\",\"instructions\":[";
        bool first = true;
        for (const auto &op : ops) { std::cout << (first ? "" : ",") << '"' << op << '"'; first = false; }
        std::cout << "],\"state_bindings\":[";
        first = true;
        for (const auto &state : result.program.stateBindings) { std::cout << (first ? "" : ",") << '"' << state << '"'; first = false; }
        std::cout << "],\"assumptions\":[";
        first = true;
        for (const auto &assumption : result.program.assumptions) { std::cout << (first ? "" : ",") << '"' << assumption << '"'; first = false; }
        std::cout << "],\"textures\":{";
        first = true;
        for (const auto &[unit, target] : result.program.textures) {
            std::cout << (first ? "" : ",") << '"' << unit << "\":\"" << target << '"'; first = false;
        }
        std::cout << "}}\n";
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
