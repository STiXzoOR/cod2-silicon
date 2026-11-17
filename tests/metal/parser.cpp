#include "arb_to_msl.h"
#include <cassert>
#include <iostream>

using namespace cod2::arb;

static void Reject(const char *source)
{
    bool rejected = false;
    try { Translate(source); } catch (const Error &) { rejected = true; }
    assert(rejected);
}

int main()
{
    auto result = Translate(
        "!!ARBvp1.0\n# synthetic, independently written\n"
        "ATTRIB v = vertex.attrib[0]; OUTPUT p = result.position;"
        "PARAM c[2] = {program.env[3..4]}; TEMP r; ADDRESS a;"
        "ARL a.x, v.x; MOV r, -v.wzyx; ADD r.xy, r, c[a.x+1]; MOV p, r; END");
    assert(result.program.stage == Stage::Vertex);
    assert(result.program.instructions.size() == 4);
    assert(result.program.parameters.at("c").size() == 2);
    assert(result.source.find("floor(") != std::string::npos);
    assert(result.source.find(".wzyx") != std::string::npos);
    assert(result.program.instructions[2].destination.swizzle == "xy");
    auto fragment = Translate(
        "!!ARBfp1.0\nOPTION ARB_precision_hint_fastest;"
        "PARAM fog = state.fog.color; PARAM c = {1,2,3,4};"
        "ATTRIB t = fragment.texcoord[0]; TEMP r;"
        "TXP r, t, texture[2], 2D; MAD_SAT result.color, r, c, fog; END");
    assert(fragment.program.textures.at(2) == "2D");
    assert(fragment.program.stateBindings.at(0) == "state.fog.color");
    assert(fragment.source.find("clamp(") != std::string::npos);
    assert(fragment.source.find("tex2.sample") != std::string::npos);
    assert(fragment.source.find("bias(fetch.samplerLODBias[2])") != std::string::npos);
    auto modifiers = Translate("!!ARBfp1.0\nTEMP r; MOV r, {-1, 0, 1, 2};"
        "MAD_SAT result.color.xy, -r_abs.wzyx, 0.5, state.fog.color; END");
    assert(modifiers.program.instructions[1].sources[0].absolute);
    assert(modifiers.program.instructions[1].sources[0].negative);
    assert(modifiers.program.instructions[1].sources[1].literal.size() == 1);
    assert(modifiers.program.stateBindings == std::vector<std::string>{"state.fog.color"});
    assert(modifiers.program.assumptions.size() == 1);
    // The shaders generated from the game's own source keep sine and cosine as one instruction.
    auto sincos = Translate("!!ARBfp1.0\nTEMP r; SCS r.y, fragment.texcoord[0].y; MOV result.color, r.y; END");
    assert(sincos.source.find("float4(cos(") != std::string::npos && sincos.source.find("sin(") != std::string::npos);
    assert(sincos.program.instructions[0].destination.swizzle == "y");
    auto defined = Translate("!!ARBfp1.0\nTEMP r_abs; MOV r_abs, 1; MOV result.color, r_abs; END");
    assert(!defined.program.instructions[1].sources[0].absolute);
    assert(defined.program.assumptions.empty());
    /* Raster wrappers consume the ABI-1 fetch descriptor, not packed compute
     * inputs. Color/UV/fog interpolation must share one linked interface. */
    assert(result.source.find("constant ARBRasterDescriptor &fetch [[buffer(0)]]") != std::string::npos);
    assert(result.source.find("stream15 [[buffer(17)]]") != std::string::npos);
    assert(result.source.find("center_perspective") != std::string::npos);
    auto position = Translate("!!ARBfp1.0\nMOV result.color, fragment.position; END");
    assert(position.source.find("fetch.framebufferHeight - v.position.y") != std::string::npos);
    assert(position.source.find("arb_alpha_pass(output.color[0].w, fetch)") != std::string::npos);
    auto depth = Translate("!!ARBfp1.0\nOUTPUT d = result.depth; MOV d.z, 0.25; END");
    assert(depth.source.find("[[depth(any)]]") != std::string::npos);
    assert(depth.source.find("clamp(output.depth.z, 0.0f, 1.0f)") != std::string::npos);
    auto unusedDepth = Translate("!!ARBfp1.0\nOUTPUT d = result.depth; MOV result.color, 1; END");
    assert(unusedDepth.source.find("[[depth(any)]]") == std::string::npos);
    auto derivative = Translate("!!ARBfp1.0\nTEMP x,y; DDX x, fragment.texcoord[0];"
        "DDY y, fragment.texcoord[0]; ADD result.color, x, y; END");
    assert(derivative.source.find("dfdx(") != std::string::npos);
    assert(derivative.source.find("-dfdy(") != std::string::npos);
    auto lod = Translate("!!ARBfp1.0\nTEMP a,b;"
        "TXL a, fragment.texcoord[0], texture[0], 2D;"
        "TXD b, fragment.texcoord[0], {0.1,0,0,0}, {0,0.1,0,0}, texture[0], 2D;"
        "ADD result.color, a, b; END");
    assert(lod.source.find("gradient2d(") != std::string::npos);
    Reject("!!ARBvp1.0\nDDX result.position, vertex.position; END");
    Reject("!!ARBvp1.0\nFOOBAR result.position, vertex.position; END");
    Reject("!!ARBvp1.0\nMOV result.position, missing; END");
    Reject("!!ARBfp1.0\nMOV result.color.xx, fragment.color; END");
    Reject("!!ARBfp1.0\nMOV result.color, program.env[256]; END");
    Reject("!!ARBvp1.0\nPARAM c[3] = {program.env[0..1]}; END");
    Reject("!!ARBfp1.0\nTEX result.color, fragment.texcoord[0], texture[0], BAD; END");
    Reject("!!ARBvp1.0\nMOV result.position, vertex.position;");
    std::cout << "ARB parser/emitter synthetic tests passed\n";
}
