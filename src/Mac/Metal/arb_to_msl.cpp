#include "arb_to_msl.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <regex>
#include <set>
#include <sstream>

namespace cod2::arb {
namespace {

std::string Trim(const std::string &s)
{
    auto first = s.find_first_not_of(" \t\n\r");
    return first == std::string::npos ? "" : s.substr(first, s.find_last_not_of(" \t\n\r") - first + 1);
}

std::vector<std::string> Split(const std::string &s, char separator)
{
    std::vector<std::string> parts;
    size_t start = 0;
    int depth = 0;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '[' || s[i] == '{' || s[i] == '(') ++depth;
        if (s[i] == ']' || s[i] == '}' || s[i] == ')') --depth;
        if (depth < 0) throw Error("Unbalanced expression");
        if (!depth && s[i] == separator) {
            parts.push_back(Trim(s.substr(start, i - start)));
            start = i + 1;
        }
    }
    if (depth) throw Error("Unbalanced expression");
    parts.push_back(Trim(s.substr(start)));
    return parts;
}

bool Identifier(const std::string &s)
{
    static const std::regex pattern("[A-Za-z_][A-Za-z_0-9]*");
    return std::regex_match(s, pattern);
}

std::string Number(const std::string &s)
{
    static const std::regex pattern("[+-]?(?:[0-9]+(?:\\.[0-9]*)?|\\.[0-9]+)(?:[eE][+-]?[0-9]+)?");
    if (!std::regex_match(s, pattern)) throw Error("Invalid numeric literal: " + s);
    char *end;
    float value = std::strtof(s.c_str(), &end);
    if (*end || !std::isfinite(value)) throw Error("Non-finite numeric literal");
    return s + (s.find_first_of(".eE") == std::string::npos ? ".0f" : "f");
}

Operand ReadOperand(std::string s)
{
    Operand out;
    s = Trim(s);
    if (!s.empty() && (s[0] == '-' || s[0] == '+')) {
        out.negative = s[0] == '-';
        s = Trim(s.substr(1));
    }
    if (s.size() >= 2 && s.front() == '|' && s.back() == '|') {
        out.absolute = true;
        s = Trim(s.substr(1, s.size() - 2));
    }
    if (std::regex_match(s, std::regex("[0-9.].*"))) {
        out.literal.push_back(Number(s));
        return out;
    }
    if (s.size() >= 2 && s.front() == '{' && s.back() == '}') {
        for (const auto &field : Split(s.substr(1, s.size() - 2), ',')) out.literal.push_back(Number(field));
        if (out.literal.size() != 1 && out.literal.size() != 4) throw Error("Invalid literal source vector");
        return out;
    }
    static const std::regex swizzle("^(.*)\\.([xyzwrgba]{1,4})$");
    std::smatch match;
    if (std::regex_match(s, match, swizzle)) {
        out.swizzle = match[2];
        s = match[1];
        for (char &c : out.swizzle) {
            size_t i = std::string("rgba").find(c);
            if (i != std::string::npos) c = "xyzw"[i];
        }
    }
    size_t bracket = s.find('[');
    if (bracket != std::string::npos) {
        if (s.back() != ']') throw Error("Invalid indexed operand: " + s);
        out.index = Trim(s.substr(bracket + 1, s.size() - bracket - 2));
        s = Trim(s.substr(0, bracket));
    }
    if (!std::regex_match(s, std::regex("[A-Za-z_][A-Za-z_0-9]*(?:\\.[A-Za-z_][A-Za-z_0-9]*)*")))
        throw Error("Invalid operand: " + s);
    out.name = s;
    return out;
}

std::vector<Parameter> ReadParameter(std::string s, Program &program)
{
    s = Trim(s);
    if (s.size() >= 2 && s.front() == '{' && s.back() == '}') {
        auto fields = Split(s.substr(1, s.size() - 2), ',');
        if (!fields.empty() && std::regex_match(fields[0], std::regex("[+\\-.0-9].*"))) {
            if (fields.size() != 1 && fields.size() != 4) throw Error("Literal vector requires 1 or 4 components");
            Parameter p;
            for (const auto &field : fields) p.literal.push_back(Number(field));
            return {p};
        }
        std::vector<Parameter> result;
        for (const auto &field : fields) {
            auto part = ReadParameter(field, program);
            result.insert(result.end(), part.begin(), part.end());
        }
        return result;
    }
    static const std::regex binding("program\\.(env|local)\\[\\s*([0-9]+)\\s*(?:\\.\\.\\s*([0-9]+)\\s*)?\\]");
    std::smatch match;
    if (std::regex_match(s, match, binding)) {
        int start = std::stoi(match[2]), end = match[3].matched ? std::stoi(match[3]) : start;
        if (start > end || end >= 256) throw Error("Parameter register outside [0,255]");
        std::vector<Parameter> result;
        for (int i = start; i <= end; ++i) result.push_back({"constants." + match[1].str() + "[" + std::to_string(i) + "]", {}});
        return result;
    }
    if (s.rfind("state.", 0) == 0) {
        /* Backend supplies each opaque ARB state binding at this ordered slot. */
        if (!std::regex_match(s, std::regex("state\\.[A-Za-z0-9_.\\[\\]]+"))) throw Error("Invalid state binding");
        auto it = std::find(program.stateBindings.begin(), program.stateBindings.end(), s);
        if (it == program.stateBindings.end()) {
            if (program.stateBindings.size() == 64) throw Error("Too many state bindings");
            program.stateBindings.push_back(s);
            it = program.stateBindings.end() - 1;
        }
        return {{"constants.state[" + std::to_string(it - program.stateBindings.begin()) + "]", {}}};
    }
    throw Error("Unsupported parameter binding: " + s);
}

const std::map<std::string, unsigned> arities = {
    {"ABS",1},{"ADD",2},{"ARL",1},{"CMP",3},{"DP2ADD",3},{"DP3",2},{"DP4",2},
    {"DPH",2},{"DST",2},{"EX2",1},{"EXP",1},{"FLR",1},{"FRC",1},{"KIL",1},
    {"LG2",1},{"LIT",1},{"LOG",1},{"LRP",3},{"MAD",3},{"MAX",2},{"MIN",2},
    {"MOV",1},{"MUL",2},{"NRM",1},{"POW",2},{"RCP",1},{"RSQ",1},
    {"SGE",2},{"SIN",1},{"COS",1},{"SCS",1},{"SLT",2},{"SUB",2},{"SWZ",5},
    {"DDX",1},{"DDY",1},{"TEX",1},{"TXB",1},{"TXP",1},{"TXL",1},{"TXD",3}
};

class Emitter {
    const Program &p;
    std::ostringstream code;

    std::string Index(const std::string &s, size_t size) const
    {
        if (std::regex_match(s, std::regex("[0-9]+"))) {
            unsigned long n = std::stoul(s);
            if (n >= size) throw Error("Register index outside binding");
            return s;
        }
        std::smatch m;
        if (std::regex_match(s, m, std::regex("([A-Za-z_][A-Za-z_0-9]*)\\.x\\s*([+-]\\s*[0-9]+)?")) &&
            std::find(p.addresses.begin(), p.addresses.end(), m[1]) != p.addresses.end()) {
            std::string offset = m[2];
            offset.erase(std::remove_if(offset.begin(), offset.end(), [](char c) { return std::isspace(c); }), offset.end());
            return "a_" + m[1].str() + ".x" + offset;
        }
        throw Error("Unsupported register index: " + s);
    }

    std::string Register(const Operand &o, bool write = false, unsigned recursion = 0) const
    {
        if (recursion > p.aliases.size()) throw Error("Cyclic alias");
        auto alias = p.aliases.find(o.name);
        if (alias != p.aliases.end()) {
            Operand resolved = alias->second;
            if (!o.index.empty()) resolved.index = o.index;
            return Register(resolved, write, recursion + 1);
        }
        if (std::find(p.temporaries.begin(), p.temporaries.end(), o.name) != p.temporaries.end()) {
            if (!o.index.empty()) throw Error("Indexed temporary");
            return "r_" + o.name;
        }
        if (std::find(p.addresses.begin(), p.addresses.end(), o.name) != p.addresses.end()) return "a_" + o.name;
        auto param = p.parameters.find(o.name);
        if (param != p.parameters.end()) {
            if (write) throw Error("Write to parameter");
            return "p_" + o.name + "[" + (o.index.empty() ? "0" : Index(o.index, param->second.size())) + "]";
        }
        auto array = [&](const std::string &name, unsigned size) {
            return name + "[" + (o.index.empty() ? "0" : Index(o.index, size)) + "]";
        };
        if (o.name == "program.env" || o.name == "program.local") {
            if (write || o.index.empty()) throw Error("Invalid program parameter");
            return array("constants." + o.name.substr(8), 256);
        }
        if (o.name.rfind("state.", 0) == 0) {
            if (write) throw Error("Write to state binding");
            std::string binding = o.name + (o.index.empty() ? "" : "[" + o.index + "]");
            auto it = std::find(p.stateBindings.begin(), p.stateBindings.end(), binding);
            if (it == p.stateBindings.end()) throw Error("Undeclared state binding");
            return "constants.state[" + std::to_string(it - p.stateBindings.begin()) + "]";
        }
        if (write && o.name.rfind("result.", 0) != 0) throw Error("Read-only input: " + o.name);
        if (o.name == "vertex.attrib" && p.stage == Stage::Vertex) return array("input.attrib", 16);
        if ((o.name == "vertex.texcoord" && p.stage == Stage::Vertex) || (o.name == "fragment.texcoord" && p.stage == Stage::Fragment)) return array("input.texcoord", 8);
        if (o.name == "result.texcoord" && p.stage == Stage::Vertex) return array("output.texcoord", 8);
        if (o.name == "vertex.position" && p.stage == Stage::Vertex) return "input.attrib[0]";
        if (o.name == "vertex.normal" && p.stage == Stage::Vertex) return "input.attrib[2]";
        if ((o.name == "vertex.color" || o.name == "vertex.color.primary") && p.stage == Stage::Vertex) return "input.attrib[3]";
        if (o.name == "vertex.color.secondary" && p.stage == Stage::Vertex) return "input.attrib[4]";
        if (o.name == "vertex.fogcoord" && p.stage == Stage::Vertex) return "input.attrib[5]";
        if (o.name == "fragment.position" && p.stage == Stage::Fragment) return "input.position";
        if (o.name == "fragment.color" || o.name == "fragment.color.primary") return "input.color[0]";
        if (o.name == "fragment.color.secondary") return "input.color[1]";
        if (o.name == "fragment.fogcoord") return "input.fogcoord";
        if (o.name == "result.position" && p.stage == Stage::Vertex) return "output.position";
        if (o.name == "result.color" || o.name == "result.color.primary" || o.name == "result.color.front.primary") return "output.color[0]";
        if (o.name == "result.color.secondary" || o.name == "result.color.front.secondary") return "output.color[1]";
        if (o.name == "result.fogcoord" && p.stage == Stage::Vertex) return "output.fogcoord";
        if (o.name == "result.depth" && p.stage == Stage::Fragment) return "output.depth";
        throw Error("Unknown register: " + o.name);
    }

    std::string Source(const Operand &o) const
    {
        std::string s;
        if (!o.literal.empty()) {
            s = "float4(";
            for (size_t i = 0; i < o.literal.size(); ++i) {
                if (i) s += ", ";
                s += o.literal[i];
            }
            s += ")";
        } else s = Register(o);
        if (!o.swizzle.empty()) {
            if (o.swizzle.size() != 1 && o.swizzle.size() != 4) throw Error("Source swizzle requires 1 or 4 components");
            s = "(" + s + ")." + o.swizzle;
            if (o.swizzle.size() == 1) s = "float4(" + s + ")";
        }
        if (o.absolute) s = "abs(" + s + ")";
        return o.negative ? "(-" + s + ")" : s;
    }

    std::string TextureType(const std::string &target) const
    {
        if (target == "2D") return "texture2d<float>";
        if (target == "3D") return "texture3d<float>";
        return "texturecube<float>";
    }

    std::string TextureParameters(bool attributes) const
    {
        std::string result;
        for (const auto &[unit, target] : p.textures) {
            std::string n = std::to_string(unit);
            result += ", " + TextureType(target) + " tex" + n + (attributes ? " [[texture(" + n + ")]]" : "");
            result += ", sampler smp" + n + (attributes ? " [[sampler(" + n + ")]]" : "");
        }
        return result;
    }

    std::string TextureArguments() const
    {
        std::string result;
        for (const auto &[unit, target] : p.textures) {
            (void)target;
            std::string n = std::to_string(unit);
            result += ", tex" + n + ", smp" + n;
        }
        return result;
    }

    bool WritesDepth() const
    {
        for (const auto &instruction : p.instructions)
            if (instruction.opcode != "KIL" && Register(instruction.destination, true) == "output.depth")
                return true;
        return false;
    }

    std::string StreamParameters(bool attributes) const
    {
        std::string result;
        for (unsigned i = 0; i < 16; ++i)
            result += ", device const uchar *stream" + std::to_string(i) +
                (attributes ? " [[buffer(" + std::to_string(i + 2) + ")]]" : "");
        return result;
    }

    std::string StreamArguments() const
    {
        std::string result;
        for (unsigned i = 0; i < 16; ++i) result += ", stream" + std::to_string(i);
        return result;
    }

    void FetchWrapper()
    {
        code << R"MSL(
struct ARBRasterAttribute { uint stream, offset, type, colorOrder; };
struct ARBRasterStream { ulong offset, length; uint stride, reserved; };
struct ARBRasterDescriptor {
    ARBRasterAttribute attributes[16]; ARBRasterStream streams[16];
    float framebufferHeight; uint reserved[3]; float samplerLODBias[16];
    uint alphaEnabled, alphaFunction, alphaReference, reservedAlpha;
};
inline uint arb_u16(device const uchar *p) { return uint(p[0]) | (uint(p[1]) << 8); }
inline uint arb_u32(device const uchar *p) { return arb_u16(p) | (arb_u16(p + 2) << 16); }
inline float4 arb_unpack(device const uchar *p, uint type, uint colorOrder)
{
    float4 value = float4(0.0f, 0.0f, 0.0f, 1.0f);
    if (type <= 3) {
        for (uint c = 0; c <= type; ++c) value[c] = as_type<float>(arb_u32(p + c * 4));
    } else if (type == 4 || type == 5 || type == 8) {
        value = float4(p[0], p[1], p[2], p[3]);
        if (type != 5) value /= 255.0f;
        if (type == 4 && colorOrder == 1) value = value.zyxw;
        if (type == 4 && colorOrder == 2) value = value.yzwx;
    } else if (type >= 6 && type <= 12) {
        uint count = (type == 6 || type == 9 || type == 11) ? 2 : 4;
        for (uint c = 0; c < count; ++c) {
            uint raw = arb_u16(p + c * 2);
            if (type >= 11) value[c] = float(raw) / 65535.0f;
            else {
                int signedValue = (raw & 0x8000u) ? int(raw) - 65536 : int(raw);
                value[c] = type >= 9 ? max(float(signedValue) / 32767.0f, -1.0f) : float(signedValue);
            }
        }
    } else if (type == 13 || type == 14) {
        uint raw = arb_u32(p);
        for (uint c = 0; c < 3; ++c) {
            uint component = (raw >> (c * 10)) & 1023u;
            int signedValue = (component & 512u) ? int(component) - 1024 : int(component);
            value[c] = type == 13 ? float(component) : max(float(signedValue) / 511.0f, -1.0f);
        }
    } else if (type == 15 || type == 16) {
        for (uint c = 0; c < (type == 15 ? 2u : 4u); ++c)
            value[c] = float(as_type<half>(ushort(arb_u16(p + c * 2))));
    }
    return value;
}
)MSL";
        if (p.stage == Stage::Fragment) {
            code << R"MSL(
inline bool arb_alpha_pass(float alpha, constant ARBRasterDescriptor &fetch)
{
    if (!fetch.alphaEnabled) return true;
    float reference = float(min(fetch.alphaReference, 255u)) / 255.0f;
    switch (fetch.alphaFunction) {
    case 1: return false;
    case 2: return alpha < reference;
    case 3: return alpha == reference;
    case 4: return alpha <= reference;
    case 5: return alpha > reference;
    case 6: return alpha != reference;
    case 7: return alpha >= reference;
    case 8: return true;
    default: return false;
    }
}
)MSL";
            return;
        }
        code << "inline ARBInput arb_fetch(constant ARBRasterDescriptor &fetch, uint id" << StreamParameters(false) << ")\n{\n    ARBInput input = {};\n";
        code << R"MSL(
    for (uint n = 0; n < 16; ++n) {
        input.attrib[n] = float4(0.0f, 0.0f, 0.0f, 1.0f);
        ARBRasterAttribute a = fetch.attributes[n];
        if (a.stream >= 16 || a.type >= 17) continue;
        ARBRasterStream s = fetch.streams[a.stream];
        uint size = a.type <= 3 ? (a.type + 1) * 4 :
            (a.type == 7 || a.type == 10 || a.type == 12 || a.type == 16) ? 8 : 4;
        ulong relative = ulong(id) * ulong(s.stride) + ulong(a.offset);
        if (s.offset > s.length || relative > s.length - s.offset ||
            ulong(size) > s.length - s.offset - relative) continue;
        device const uchar *bytes = nullptr;
        switch (a.stream) {
)MSL";
        for (unsigned i = 0; i < 16; ++i)
            code << "        case " << i << ": bytes = stream" << i << "; break;\n";
        code << R"MSL(
        }
        if (bytes) input.attrib[n] = arb_unpack(bytes + s.offset + relative, a.type, a.colorOrder);
    }
    for (uint n = 0; n < 8; ++n) input.texcoord[n] = input.attrib[n + 8];
    input.color[0] = input.attrib[3]; input.color[1] = input.attrib[4];
    input.position = input.attrib[0]; input.fogcoord = input.attrib[5];
    return input;
}
)MSL";
    }

    void Operation(const Instruction &i, unsigned number)
    {
        std::vector<std::string> s;
        for (const auto &o : i.sources) s.push_back(Source(o));
        auto x = [&](unsigned n) { return "(" + s.at(n) + ").x"; };
        auto xyz = [&](unsigned n) { return "(" + s.at(n) + ").xyz"; };
        const std::string &op = i.opcode;
        std::string value;
        if (op == "KIL") {
            code << "    if (any(" << s[0] << " < float4(0.0f))) { output.killed = 1; return output; }\n";
            return;
        }
        if (op == "MOV") value = s[0];
        else if (op == "ABS") value = "abs(" + s[0] + ")";
        else if (op == "ADD" || op == "SUB" || op == "MUL") value = s[0] + (op == "ADD" ? " + " : op == "SUB" ? " - " : " * ") + s[1];
        else if (op == "MAD") value = "(" + s[0] + " * " + s[1] + ") + " + s[2];
        else if (op == "MIN" || op == "MAX") value = (op == "MIN" ? "min(" : "max(") + s[0] + ", " + s[1] + ")";
        else if (op == "DP3" || op == "DP4") value = "float4(dot(" + (op == "DP3" ? xyz(0) : s[0]) + ", " + (op == "DP3" ? xyz(1) : s[1]) + "))";
        else if (op == "DP2ADD") value = "float4(dot((" + s[0] + ").xy, (" + s[1] + ").xy) + " + x(2) + ")";
        else if (op == "DPH") value = "float4(dot(float4(" + xyz(0) + ", 1.0f), " + s[1] + "))";
        else if (op == "DST") value = "float4(1.0f, (" + s[0] + ").y * (" + s[1] + ").y, (" + s[0] + ").z, (" + s[1] + ").w)";
        else if (op == "RCP") value = "float4(1.0f / " + x(0) + ")";
        else if (op == "RSQ") value = "float4(rsqrt(abs(" + x(0) + ")))";
        else if (op == "EX2") value = "float4(exp2(" + x(0) + "))";
        else if (op == "LG2") value = "float4(log2(abs(" + x(0) + ")))";
        else if (op == "POW") value = "float4(pow(abs(" + x(0) + "), " + x(1) + "))";
        else if (op == "FLR" || op == "ARL") value = "floor(" + s[0] + ")";
        else if (op == "FRC") value = "(" + s[0] + " - floor(" + s[0] + "))";
        else if (op == "LRP") value = "(" + s[0] + " * " + s[1] + ") + ((1.0f - " + s[0] + ") * " + s[2] + ")";
        else if (op == "CMP") value = "select(" + s[2] + ", " + s[1] + ", " + s[0] + " < float4(0.0f))";
        else if (op == "SLT" || op == "SGE") value = "select(float4(0.0f), float4(1.0f), " + s[0] + (op == "SLT" ? " < " : " >= ") + s[1] + ")";
        else if (op == "NRM") value = "float4(normalize(" + xyz(0) + "), (" + s[0] + ").w)";
        else if (op == "SIN" || op == "COS") value = "float4(" + (op == "SIN" ? std::string("sin(") : std::string("cos(")) + x(0) + "))";
        else if (op == "SCS") value = "float4(cos(" + x(0) + "), sin(" + x(0) + "), 0.0f, 0.0f)";
        else if (op == "LIT") value = "float4(1.0f, max(" + x(0) + ", 0.0f), " + x(0) + " > 0.0f ? pow(max((" + s[0] + ").y, 0.0f), clamp((" + s[0] + ").w, -128.0f, 128.0f)) : 0.0f, 1.0f)";
        else if (op == "DDX" || op == "DDY") value = (op == "DDX" ? "dfdx(" : "-dfdy(") + s[0] + ")";
        else if (op == "TEX" || op == "TXP" || op == "TXB" || op == "TXL" || op == "TXD") {
            std::string n = std::to_string(i.texture);
            std::string coords = "(" + s[0] + ")." + (i.target == "2D" ? "xy" : "xyz");
            if (op == "TXP" && i.target != "CUBE") coords = "(" + coords + " / (" + s[0] + ").w)";
            if (op == "TXL" || op == "TXD") {
                if (op == "TXD") {
                    code << "#ifdef ARB_SEMANTIC_TEST\n    float gradientScale" << number << " = 1.0f;\n#else\n    float gradientScale" << number << " = exp2(fetch.samplerLODBias[" << n << "]);\n#endif\n";
                }
                std::string scale = " * gradientScale" + std::to_string(number);
                std::string options = op == "TXL" ? "level((" + s[0] + ").w)" :
                    std::string(i.target == "2D" ? "gradient2d(" : i.target == "3D" ? "gradient3d(" : "gradientcube(") +
                    "(" + s[1] + ")." + (i.target == "2D" ? "xy" : "xyz") + scale + ", -(" + s[2] + ")." +
                    (i.target == "2D" ? "xy" : "xyz") + scale + ")";
                if (op == "TXL") code << "#ifdef ARB_SEMANTIC_TEST\n";
                code << "    float4 sample" << number << " = tex" << n << ".sample(smp" << n << ", " << coords << ", " << options << ");\n";
                if (op == "TXL") code << "#else\n    float4 sample" << number << " = tex" << n << ".sample(smp" << n << ", " << coords << ", level((" << s[0] << ").w + fetch.samplerLODBias[" << n << "]));\n#endif\n";
            } else {
                code << "#ifdef ARB_SEMANTIC_TEST\n";
                code << "    float4 sample" << number << " = tex" << n << ".sample(smp" << n << ", " << coords << ", level(" << (op == "TXB" ? "(" + s[0] + ").w" : "0.0f") << "));\n";
                code << "#else\n    float4 sample" << number << " = tex" << n << ".sample(smp" << n << ", " << coords << ", bias(fetch.samplerLODBias[" << n << "]" << (op == "TXB" ? " + (" + s[0] + ").w" : "") << "));\n#endif\n";
            }
            value = "sample" + std::to_string(number);
        } else throw Error("Emitter does not support instruction: " + op);
        std::string dest = Register(i.destination, true);
        std::string mask = i.destination.swizzle;
        if (i.destination.negative || i.destination.absolute) throw Error("Signed destination");
        if (!mask.empty()) {
            size_t previous = 0;
            bool first = true;
            for (char c : mask) {
                size_t current = std::string("xyzw").find(c);
                if (!first && current <= previous) throw Error("Invalid destination mask");
                first = false;
                previous = current;
            }
        }
        if (op == "ARL") {
            if (std::find(p.addresses.begin(), p.addresses.end(), i.destination.name) == p.addresses.end() || mask != "x") throw Error("ARL requires address.x");
        } else if (dest.rfind("a_", 0) == 0) throw Error("Only ARL writes address registers");
        if (i.saturate) value = "clamp(" + value + ", float4(0.0f), float4(1.0f))";
        std::string name = "value" + std::to_string(number);
        code << "    float4 " << name << " = " << value << ";\n";
        code << "    " << dest << (mask.empty() ? "" : "." + mask) << " = " << (op == "ARL" ? "int((" + name + ").x)" : mask.empty() ? name : "(" + name + ")." + mask) << ";\n";
    }

public:
    explicit Emitter(const Program &program) : p(program) {}
    std::string Run()
    {
        code << "#include <metal_stdlib>\nusing namespace metal;\n";
        for (const auto &instruction : p.instructions)
            if (instruction.opcode == "DDX" || instruction.opcode == "DDY") {
                code << "#ifdef ARB_SEMANTIC_TEST\n#error DDX/DDY require raster neighbors; use the linked raster fixture\n#endif\n";
                break;
            }
        code << "struct ARBInput { float4 attrib[16]; float4 texcoord[8]; float4 color[2]; float4 position; float4 fogcoord; };\n";
        code << "struct ARBConstants { float4 env[256]; float4 local[256]; float4 state[64]; };\n";
        code << "struct ARBOutput { float4 position; float4 color[2]; float4 texcoord[8]; float4 fogcoord; float4 depth; uint killed; };\n";
        code << "#ifndef ARB_SEMANTIC_TEST\n";
        FetchWrapper();
        code << "#endif\n";
        code << "inline ARBOutput arb_evaluate(ARBInput input, constant ARBConstants &constants" << TextureParameters(false) << "\n#ifndef ARB_SEMANTIC_TEST\n    , constant ARBRasterDescriptor &fetch\n#endif\n)\n{\n    ARBOutput output = {};\n";
        for (const auto &name : p.temporaries) code << "    float4 r_" << name << " = float4(0.0f);\n";
        for (const auto &name : p.addresses) code << "    int4 a_" << name << " = int4(0);\n";
        for (const auto &[name, values] : p.parameters) {
            code << "    float4 p_" << name << "[" << values.size() << "] = {";
            for (size_t i = 0; i < values.size(); ++i) {
                if (i) code << ", ";
                if (values[i].literal.empty()) code << values[i].binding;
                else {
                    code << "float4(";
                    for (size_t j = 0; j < values[i].literal.size(); ++j) {
                        if (j) code << ", ";
                        code << values[i].literal[j];
                    }
                    code << ")";
                }
            }
            code << "};\n";
        }
        for (unsigned n = 0; n < p.instructions.size(); ++n) Operation(p.instructions[n], n);
        code << "    return output;\n}\n";
        code << "#ifdef ARB_SEMANTIC_TEST\nkernel void arb_test(device const ARBInput *inputs [[buffer(0)]], constant ARBConstants &constants [[buffer(1)]], device ARBOutput *outputs [[buffer(2)]]" << TextureParameters(true) << ", uint id [[thread_position_in_grid]])\n{\n    outputs[id] = arb_evaluate(inputs[id], constants" << TextureArguments() << ");\n}\n#else\n";
        code << "struct ARBVaryings { float4 position [[position]]; float4 primary [[user(color0), center_perspective]]; float4 secondary [[user(color1), center_perspective]]; float4 fog [[user(fog), center_perspective]];\n";
        for (int i = 0; i < 8; ++i) code << "    float4 texcoord" << i << " [[user(texcoord" << i << "), center_perspective]];\n";
        code << "};\n";
        if (p.stage == Stage::Vertex) {
            code << "vertex ARBVaryings arb_vertex(constant ARBRasterDescriptor &fetch [[buffer(0)]], constant ARBConstants &constants [[buffer(1)]]" << StreamParameters(true) << ", uint id [[vertex_id]])\n{\n    ARBOutput output = arb_evaluate(arb_fetch(fetch, id" << StreamArguments() << "), constants, fetch);\n    ARBVaryings v;\n    v.position = output.position;\n    v.position.z = (v.position.z + v.position.w) * 0.5f;\n    v.primary = output.color[0]; v.secondary = output.color[1]; v.fog = output.fogcoord;\n";
            for (int i = 0; i < 8; ++i) code << "    v.texcoord" << i << " = output.texcoord[" << i << "];\n";
            code << "    return v;\n}\n";
        } else {
            bool depth = WritesDepth();
            if (depth) code << "struct ARBFragmentOutput { float4 color [[color(0)]]; float depth [[depth(any)]]; };\n";
            code << "fragment " << (depth ? "ARBFragmentOutput" : "float4") << " arb_fragment(ARBVaryings v [[stage_in]], constant ARBRasterDescriptor &fetch [[buffer(0)]], constant ARBConstants &constants [[buffer(1)]]" << TextureParameters(true) << ")\n{\n    ARBInput input = {};\n    input.position = float4(v.position.x, fetch.framebufferHeight - v.position.y, v.position.z, v.position.w); input.color[0] = v.primary; input.color[1] = v.secondary; input.fogcoord = float4(v.fog.x, 0.0f, 0.0f, 1.0f);\n";
            for (int i = 0; i < 8; ++i) code << "    input.texcoord[" << i << "] = v.texcoord" << i << ";\n";
            code << "    ARBOutput output = arb_evaluate(input, constants" << TextureArguments() << ", fetch);\n    if (output.killed || !arb_alpha_pass(output.color[0].w, fetch)) discard_fragment();\n";
            if (depth) code << "    return { output.color[0], clamp(output.depth.z, 0.0f, 1.0f) };\n}\n";
            else code << "    return output.color[0];\n}\n";
        }
        code << "#endif\n";
        return code.str();
    }
};
} // namespace

Program Parse(const std::string &source)
{
    Program program;
    std::ostringstream stripped;
    std::istringstream lines(source);
    std::string line;
    while (std::getline(lines, line)) stripped << line.substr(0, line.find('#')) << '\n';
    std::string text = Trim(stripped.str());
    if (text.rfind("!!ARBvp1.0", 0) == 0) program.stage = Stage::Vertex;
    else if (text.rfind("!!ARBfp1.0", 0) == 0) program.stage = Stage::Fragment;
    else throw Error("Expected ARBvp1.0 or ARBfp1.0 header");
    text = Trim(text.substr(10));
    std::set<std::string> symbols;
    bool ended = false;
    for (const auto &statement : Split(text, ';')) {
        if (statement.empty()) continue;
        if (ended) throw Error("Content after END");
        if (statement == "END") { ended = true; continue; }
        auto space = statement.find_first_of(" \t\n\r");
        if (space == std::string::npos) throw Error("Missing instruction operands");
        std::string word = statement.substr(0, space), rest = Trim(statement.substr(space));
        auto addSymbol = [&](const std::string &name) {
            if (!Identifier(name) || !symbols.insert(name).second) throw Error("Invalid or duplicate symbol: " + name);
        };
        if (word == "OPTION") {
            if (rest != "ARB_precision_hint_fastest" && rest != "ARB_precision_hint_nicest") throw Error("Unsupported option: " + rest);
        } else if (word == "TEMP" || word == "ADDRESS") {
            for (const auto &name : Split(rest, ',')) {
                addSymbol(name);
                (word == "TEMP" ? program.temporaries : program.addresses).push_back(name);
            }
        } else if (word == "ATTRIB" || word == "OUTPUT" || word == "ALIAS" || word == "PARAM") {
            auto equal = rest.find('=');
            if (equal == std::string::npos) throw Error("Missing declaration binding");
            std::string name = Trim(rest.substr(0, equal)), value = Trim(rest.substr(equal + 1));
            if (word == "PARAM") {
                std::smatch m;
                if (!std::regex_match(name, m, std::regex("([A-Za-z_][A-Za-z_0-9]*)(?:\\[\\s*([0-9]+)\\s*\\])?"))) throw Error("Invalid parameter declaration");
                size_t size = m[2].matched ? std::stoul(m[2]) : 1;
                name = m[1];
                addSymbol(name);
                auto values = ReadParameter(value, program);
                if (size != values.size() || !size) throw Error("Parameter array size does not match binding");
                program.parameters[name] = values;
            } else {
                addSymbol(name);
                Operand binding = ReadOperand(value);
                if (binding.negative || binding.absolute || !binding.swizzle.empty()) throw Error("Invalid alias binding");
                if (word == "OUTPUT" && binding.name.rfind("result.", 0) != 0) throw Error("OUTPUT requires result binding");
                if (word == "ATTRIB" && binding.name.rfind(program.stage == Stage::Vertex ? "vertex." : "fragment.", 0) != 0) throw Error("ATTRIB requires input binding");
                program.aliases[name] = binding;
            }
        } else {
            Instruction i;
            if (word.size() > 4 && word.substr(word.size() - 4) == "_SAT") {
                i.saturate = true;
                word.resize(word.size() - 4);
            }
            auto arity = arities.find(word);
            if (arity == arities.end()) throw Error("Unsupported instruction: " + word);
            i.opcode = word;
            auto operands = Split(rest, ',');
            bool texture = word == "TEX" || word == "TXP" || word == "TXB" || word == "TXL" || word == "TXD";
            if ((word == "DDX" || word == "DDY") && program.stage != Stage::Fragment)
                throw Error("Derivatives require a fragment program");
            size_t expected = arity->second + (word == "KIL" ? 0 : 1) + (texture ? 2 : 0);
            if (operands.size() != expected) throw Error("Wrong operand count: " + word);
            size_t start = 0;
            if (word != "KIL") i.destination = ReadOperand(operands[start++]);
            for (size_t n = 0; n < arity->second; ++n) {
                Operand operand = ReadOperand(operands[start++]);
                if (operand.name.size() > 4 && operand.name.substr(operand.name.size() - 4) == "_abs" && !symbols.count(operand.name)) {
                    operand.name.resize(operand.name.size() - 4);
                    operand.absolute = true;
                    const std::string assumption = "Undeclared _abs operand interpreted as absolute value";
                    if (std::find(program.assumptions.begin(), program.assumptions.end(), assumption) == program.assumptions.end()) program.assumptions.push_back(assumption);
                }
                if (operand.name.rfind("state.", 0) == 0) ReadParameter(operand.name + (operand.index.empty() ? "" : "[" + operand.index + "]"), program);
                i.sources.push_back(operand);
            }
            if (texture) {
                std::smatch m;
                if (program.stage != Stage::Fragment || !std::regex_match(operands[start++], m, std::regex("texture\\[\\s*([0-9]+)\\s*\\]"))) throw Error("Invalid texture binding");
                i.texture = std::stoi(m[1]);
                i.target = operands[start];
                if (i.texture >= 16 || (i.target != "2D" && i.target != "3D" && i.target != "CUBE")) throw Error("Unsupported texture unit or target");
                auto existing = program.textures.find(i.texture);
                if (existing != program.textures.end() && existing->second != i.target) throw Error("Conflicting texture targets");
                program.textures[i.texture] = i.target;
            }
            if (word == "NRM") program.assumptions.push_back("NRM extension interpreted as XYZ normalization");
            program.instructions.push_back(i);
        }
    }
    if (!ended) throw Error("Missing END");
    return program;
}

std::string Emit(const Program &program)
{
    return Emitter(program).Run();
}

Translation Translate(const std::string &source)
{
    Translation result;
    result.program = Parse(source);
    result.source = Emit(result.program);
    return result;
}
} // namespace cod2::arb
