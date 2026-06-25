static const short kV60ToV59[181] = {
    0,
    1,
    2,
    3,
    4,
    5,
    6,
    7,
    8,
    9,
    10,
    11,
    12,
    13,
    14,
    15,
    16,
    17,
    18,
    19,
    20,
    21,
    -1,
    -1,
    -1,
    -1,
    22,
    23,
    24,
    25,
    26,
    27,
    28,
    29,
    30,
    31,
    32,
    33,
    34,
    35,
    36,
    37,
    38,
    39,
    40,
    41,
    42,
    43,
    44,
    45,
    46,
    47,
    48,
    49,
    50,
    51,
    52,
    53,
    54,
    55,
    56,
    57,
    58,
    59,
    60,
    61,
    62,
    63,
    64,
    65,
    66,
    67,
    68,
    69,
    70,
    71,
    72,
    73,
    74,
    75,
    76,
    77,
    78,
    79,
    80,
    81,
    82,
    83,
    84,
    85,
    86,
    87,
    88,
    89,
    90,
    91,
    92,
    93,
    94,
    95,
    96,
    97,
    98,
    99,
    100,
    101,
    102,
    103,
    104,
    105,
    106,
    107,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    108,
    109,
    110,
    111,
    112,
    113,
    114,
    115,
    116,
    117,
    118,
    119,
    120,
    121,
    123,
    122,
    124,
    125,
    126,
    127,
    128,
    129,
    132,
    -1,
    -1,
    133,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
    -1,
};
extern void Com_Printf(const char *, ...);
static int gfxV60Missing;
static int gfxV60Report(int off)
{
    if (!getenv("NOMISS"))
        Com_Printf("[v60] UNMAPPED ri+0x%x (m%d)\n", off * 4, off);
    gfxV60Missing = off;
    return 0;
}
static int gfxV60T22(void)
{
    return gfxV60Report(22);
}
static int gfxV60T23(void)
{
    return gfxV60Report(23);
}
static int gfxV60T24(void)
{
    return gfxV60Report(24);
}
static int gfxV60T25(void)
{
    return gfxV60Report(25);
}
static int gfxV60T112(void)
{
    return gfxV60Report(112);
}
static int gfxV60T113(void)
{
    return gfxV60Report(113);
}
static int gfxV60T114(void)
{
    return gfxV60Report(114);
}
static int gfxV60T115(void)
{
    return gfxV60Report(115);
}
static int gfxV60T116(void)
{
    return gfxV60Report(116);
}
static int gfxV60T117(void)
{
    return gfxV60Report(117);
}
static int gfxV60T118(void)
{
    return gfxV60Report(118);
}
static int gfxV60T119(void)
{
    return gfxV60Report(119);
}
static int gfxV60T120(void)
{
    return gfxV60Report(120);
}
static int gfxV60T121(void)
{
    return gfxV60Report(121);
}
static int gfxV60T122(void)
{
    return gfxV60Report(122);
}
static int gfxV60T123(void)
{
    return gfxV60Report(123);
}
static int gfxV60T124(void)
{
    return gfxV60Report(124);
}
static int gfxV60T125(void)
{
    return gfxV60Report(125);
}
static int gfxV60T149(void)
{
    return gfxV60Report(149);
}
static int gfxV60T150(void)
{
    return gfxV60Report(150);
}
static int gfxV60T152(void)
{
    return gfxV60Report(152);
}
static int gfxV60T153(void)
{
    return gfxV60Report(153);
}
static int gfxV60T154(void)
{
    return gfxV60Report(154);
}
static int gfxV60T155(void)
{
    return gfxV60Report(155);
}
static int gfxV60T156(void)
{
    return gfxV60Report(156);
}
static int gfxV60T157(void)
{
    return gfxV60Report(157);
}
static int gfxV60T158(void)
{
    return gfxV60Report(158);
}
static int gfxV60T159(void)
{
    return gfxV60Report(159);
}
static int gfxV60T160(void)
{
    return gfxV60Report(160);
}
static int gfxV60T161(void)
{
    return gfxV60Report(161);
}
static int gfxV60T162(void)
{
    return gfxV60Report(162);
}
static int gfxV60T163(void)
{
    return gfxV60Report(163);
}
static int gfxV60T164(void)
{
    return gfxV60Report(164);
}
static int gfxV60T165(void)
{
    return gfxV60Report(165);
}
static int gfxV60T166(void)
{
    return gfxV60Report(166);
}
static int gfxV60T167(void)
{
    return gfxV60Report(167);
}
static int gfxV60T168(void)
{
    return gfxV60Report(168);
}
static int gfxV60T169(void)
{
    return gfxV60Report(169);
}
static int gfxV60T170(void)
{
    return gfxV60Report(170);
}
static int gfxV60T171(void)
{
    return gfxV60Report(171);
}
static int gfxV60T172(void)
{
    return gfxV60Report(172);
}
static int gfxV60T173(void)
{
    return gfxV60Report(173);
}
static int gfxV60T174(void)
{
    return gfxV60Report(174);
}
static int gfxV60T175(void)
{
    return gfxV60Report(175);
}
static int gfxV60T176(void)
{
    return gfxV60Report(176);
}
static int gfxV60T177(void)
{
    return gfxV60Report(177);
}
static int gfxV60T178(void)
{
    return gfxV60Report(178);
}
static int gfxV60T179(void)
{
    return gfxV60Report(179);
}
static int gfxV60T180(void)
{
    return gfxV60Report(180);
}
static void *const gfxV60Thunks[181] = {
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gfxV60T22,
    gfxV60T23,
    gfxV60T24,
    gfxV60T25,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gfxV60T112,
    gfxV60T113,
    gfxV60T114,
    gfxV60T115,
    gfxV60T116,
    gfxV60T117,
    gfxV60T118,
    gfxV60T119,
    gfxV60T120,
    gfxV60T121,
    gfxV60T122,
    gfxV60T123,
    gfxV60T124,
    gfxV60T125,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    0,
    gfxV60T149,
    gfxV60T150,
    0,
    gfxV60T152,
    gfxV60T153,
    gfxV60T154,
    gfxV60T155,
    gfxV60T156,
    gfxV60T157,
    gfxV60T158,
    gfxV60T159,
    gfxV60T160,
    gfxV60T161,
    gfxV60T162,
    gfxV60T163,
    gfxV60T164,
    gfxV60T165,
    gfxV60T166,
    gfxV60T167,
    gfxV60T168,
    gfxV60T169,
    gfxV60T170,
    gfxV60T171,
    gfxV60T172,
    gfxV60T173,
    gfxV60T174,
    gfxV60T175,
    gfxV60T176,
    gfxV60T177,
    gfxV60T178,
    gfxV60T179,
    gfxV60T180,
};
