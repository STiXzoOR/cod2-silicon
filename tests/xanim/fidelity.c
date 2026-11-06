#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "f1_xanim_source.h"

static unsigned char testStrings[32][256];
static unsigned int numStrings;
static unsigned int serverNames[32], serverNameCount;
static unsigned int pushedName;
static dvar_t developer;
const dvar_t *com_developer = &developer;

unsigned int SL_GetStringOfLen(const char *value, unsigned int user, unsigned int len, int type)
{
    (void)user; (void)type;
    assert(numStrings + 1 < 32 && len <= 256);
    memcpy(testStrings[++numStrings], value, len);
    return numStrings;
}
unsigned int SL_GetString_(const char *value, unsigned int user, int type)
{
    return SL_GetStringOfLen(value, user, (unsigned int)strlen(value) + 1, type);
}
const char *SL_ConvertToString(unsigned int value) { assert(value <= numStrings); return (const char *)testStrings[value]; }
void SL_AddRefToString(unsigned int value) { (void)value; }
void SL_RemoveRefToString(unsigned int value) { (void)value; }
void SL_RemoveRefToStringOfLen(unsigned int value, int len) { (void)value; (void)len; }
void Scr_AddConstString(unsigned int value) { pushedName = value; }
void Scr_NotifyNum(int entnum, int classnum, unsigned int value, unsigned int count)
{
    assert(entnum == 0 && classnum == 0 && count == 1 && value == 20 && serverNameCount < 32);
    serverNames[serverNameCount++] = pushedName;
}
void Com_Error(int code, const char *fmt, ...) { (void)code; (void)fmt; abort(); }
void Com_Printf(const char *fmt, ...) { (void)fmt; abort(); }

/* Only these rendering accessors are needed by the real DObj routines. */
const char *XModelGetName(const XModel *model) { return model->name; }
int XModelGetSurfaces(const XModel *model, struct XSurface_s ***surfaces, int lod, int **partBits)
{
    const XModelLodInfo *l = &model->lodInfo[lod];
    *surfaces = l->surfs->surfs; *partBits = l->surfs->partBits; return l->numsurfs;
}
void TestSkeleton(void);
void TestSurfaces(void);
void TestTrace(void);
void TestAngles(void);
void TestNames(void);

static void Near(float x, float expected) { assert(fabsf(x - expected) < 0.00001f); }
static XAnimTree_s *Tree(int count)
{
    XAnimTree_s *tree = calloc(1, offsetof(XAnimTree_s, infoArray) + count * 5 + 1);
    tree->anims = calloc(1, offsetof(XAnim, entries) + count * sizeof(XAnimEntry));
    tree->anims->size = count;
    memset(g_xAnimInfo, 0, sizeof(g_xAnimInfo));
    for (int i = 0; i < count; ++i) {
        tree->infoArray[i] = i + 1;
        g_xAnimInfo[i + 1].s.weight = 1;
        g_xAnimInfo[i + 1].s.goalWeight = 1;
        g_xAnimInfo[i + 1].s.rate = 1;
        g_xAnimInfo[i + 1].notifyIndex = -1;
    }
    return tree;
}
static void FreeTree(XAnimTree_s *tree) { free(tree->anims); free(tree); }

static void TestEnd(void)
{
    XAnimPartQuat full = {0}, simple = {0};
    XAnimPartTrans trans = {0};
    XQuat q[3] = {{1,2,3,4}, {5,6,7,8}, {9,10,11,12}};
    XQuat2 q2[3] = {{1,2}, {3,4}, {5,6}};
    vec3_t t[3] = {{1,2,3}, {4,5,6}, {7,8,9}};
    XAnimTime time = {1.0f, 2, 1.0f, 1};
    DObjAnimMat mat = {0};
    full.size = simple.size = trans.size = 2;
    full.u.frames.u.frames = q; simple.u.frames.u.frames2 = q2; trans.u.frames.frames = t;
    /* Mac XAnimCalc end path selects frame[size] and never the preceding frame. */
    XAnimCalcAccumulateFullQuatLocal(&mat, &full, &time, 1);
    Near(mat.quat[0],9); Near(mat.quat[1],10); Near(mat.quat[2],11); Near(mat.quat[3],12);
    memset(&mat,0,sizeof(mat));
    XAnimCalcAccumulateSimpleQuatLocal(&mat,&simple,&time,1);
    Near(mat.quat[2],5); Near(mat.quat[3],6);
    XAnimCalcAccumulateTransLocal(&mat,&trans,&time,1);
    Near(mat.trans[0],7); Near(mat.trans[1],8); Near(mat.trans[2],9);
    {
        vec3_t *longFrames=calloc(5001,sizeof(vec3_t));
        XAnimTime longTime={.5f,5000,2500,2500};
        for (int i=0;i<5001;++i) longFrames[i][0]=2;
        trans.size=5000; trans.u.frames.frames=longFrames;
        memset(&mat,0,sizeof(mat));
        XAnimCalcAccumulateTransLocal(&mat,&trans,&longTime,1);
        Near(mat.trans[0],2); free(longFrames);
    }

    XAnimTree_s *tree=Tree(1); DObj object={0}; XAnimCalcAnimInfo info={0};
    XAnimParts parts={0}; XAnimPart part={0}; unsigned short name=17;
    XModel model={0}; XModelParts modelParts={0}; XBoneHierarchy hierarchy={0}; char simpleBits=1;
    hierarchy.names=&name; modelParts.numBones=1; modelParts.hierarchy=&hierarchy;
    model.parts=(void (*)())&modelParts; object.numModels=1; object.numBones=1; object.models[0]=&model;
    object.tree=tree; tree->anims->entries[0].u.parts=&parts;
    parts.boneCount=1; parts.names=&name; parts.parts=&part; parts.simpleQuatBits=&simpleBits;
    memset(&mat,0,sizeof(mat));
    /* Missing simple rotation is identity with the unscaled animation weight. */
    XAnimCalc(&object,0,0.25f,(XAnimPart (*)())&mat,1,0,&info,0);
    Near(mat.quat[3],0.25f); Near(mat.transWeight,0.25f);
    FreeTree(tree);
}

static void TestRoot(void)
{
    XAnimDeltaPartQuat q={0}; XAnimDeltaPartTrans t={0}; XAnimDeltaPart delta={0}; XAnimParts parts={0};
    XQuat2 frames[2]={{0,32767},{32767,0}}; vec3_t trans[2]={{0,0,0},{2,0,0}};
    XAnimSimpleRotPos rp={0};
    q.size=t.size=1; q.u.frames.frames=frames; t.u.frames.frames=trans;
    delta.quat=&q; delta.trans=&t; parts.deltaPart=&delta; parts.numframes=1;
    /* Mac 0x39b53 indexes Q[quatIndex]. Q[1] is the 180 degree final frame. */
    XAnimCalcRelDeltaParts(&parts,&rp,1,1,0,1);
    Near(rp.pos[0],-2); Near(rp.pos[1],0); Near(rp.pos[2],0);
}

static void TestGoals(void)
{
    XAnimTree_s *tree=Tree(5); XAnim *anims=tree->anims;
    anims->entries[0].numAnims=2; anims->entries[0].u.s.children=1;
    anims->entries[1].numAnims=2; anims->entries[1].u.s.children=3;
    anims->entries[1].parent=anims->entries[2].parent=0;
    anims->entries[3].parent=anims->entries[4].parent=1;
    g_xAnimInfo[4].s.weight=.9f; g_xAnimInfo[4].s.goalWeight=.1f;
    g_xAnimInfo[5].s.weight=.1f; g_xAnimInfo[5].s.goalWeight=.9f;
    assert(XAnimGetDescendantWithGreatestWeight(tree,1)==4);
    g_xAnimInfo[2].s.weight=.7f; g_xAnimInfo[2].s.goalWeight=0;
    XAnimSetCompleteGoalWeight(tree,3,1,1,1,0,0,0);
    Near(g_xAnimInfo[2].s.goalWeight,1);
    g_xAnimInfo[3].s.weight=.8f; g_xAnimInfo[3].s.goalWeight=.1f;
    Near(XAnimSiblingGoalBlendTime(tree,1,1,1),.8f);
    XAnimClearGoalWeightForBlend(&g_xAnimInfo[3],0);
    Near(g_xAnimInfo[3].s.weight,0); Near(g_xAnimInfo[3].s.rate,1);
    g_xAnimInfo[2].s.time=0; g_xAnimInfo[2].s.timeCount=0; anims->entries[1].u.s.flags=4;
    g_xAnimInfo[4].s.time=.5f;
    XAnimRestart(tree,3); Near(g_xAnimInfo[4].s.time,.5f);
    {
        unsigned int bits;
        g_xAnimInfo[3].s.weight=.1f;
        XAnimSetGoalWeightInternal(tree,2,0,.9f,.3f,1,0,0);
        memcpy(&bits,&g_xAnimInfo[3].s.goalTime,sizeof(bits));
        /* Mac SUBSS, MULSS, DIVSS at 0x3b678..683, each rounded to float. */
        assert(bits==0x3e888889u);
    }
    FreeTree(tree);
}

static void TestTime(void)
{
    XAnimTree_s *tree=Tree(1); XAnimParts parts={0};
    tree->anims->entries[0].u.parts=&parts; parts.bLoop=1; parts.frequency=1;
    XAnimState *s=&g_xAnimInfo[1].s;
    s->oldTime=.9f; s->time=.1f; s->oldTimeCount=4; s->timeCount=5;
    /* A second notify slice starts from oldTime, but advances the current cycle count. */
    XAnimUpdateInfoInternal(tree,.25f,0,0);
    Near(s->time,.15f); assert(s->timeCount==6);
    FreeTree(tree);
}

static void TestNotify(void)
{
    XAnimTree_s *tree=Tree(1); XAnimParts parts={0};
    XAnimNotifyInfo notes[3]={{0}};
    tree->entnum=1; tree->anims->entries[0].u.parts=&parts; parts.notify=notes; parts.notifyCount=3;
    notes[0].name=11; notes[0].time=.1f; notes[1].name=12; notes[1].time=.9f;
    notes[2].name=13; notes[2].time=1;
    XAnimInfo *info=&g_xAnimInfo[1]; info->notifyName=20; info->s.time=.8f;
    serverNameCount=0;
    XAnimProcessServerNotify(tree,info,&tree->anims->entries[0],.2f);
    assert(serverNameCount==3); assert(serverNames[0]==12 && serverNames[1]==13 && serverNames[2]==11);
    {
        XAnimNotifyInfo clientNotes[5]={{0}};
        const char *names[5]={"first", "skipped", "third", "tail", "end"};
        const float times[5]={.1f,.2f,.3f,.9f,1};
        for (int i=0;i<5;++i) {
            clientNotes[i].name=SL_GetString_(names[i],0,3); clientNotes[i].time=times[i];
        }
        parts.notify=clientNotes; parts.notifyCount=5;
        info->s.oldTime=.8f; info->s.time=.4f; info->notifyType=1; g_notifyListSize=0;
        XAnimProcessClientNotify(info,&tree->anims->entries[0],.6f);
        assert(g_notifyListSize==4);
        assert(!strcmp(g_notifyList[0].name,"tail") && !strcmp(g_notifyList[1].name,"end"));
        assert(!strcmp(g_notifyList[2].name,"first") && !strcmp(g_notifyList[3].name,"third"));
    }
    FreeTree(tree);
    {
        XAnimTree_s *syncTree=Tree(2); XAnimParts childParts={0}; XAnimNotifyInfo note={0};
        XAnimState oldState={0}, nextState={0};
        syncTree->entnum=1; syncTree->anims->entries[0].numAnims=1;
        syncTree->anims->entries[0].u.s.children=1;
        syncTree->anims->entries[1].u.parts=&childParts;
        childParts.notify=&note; childParts.notifyCount=1;
        g_xAnimInfo[2].notifyName=20; g_xAnimInfo[2].notifyIndex=0;
        nextState.time=1;
        /* Mac UCOMISS/JBE retains the existing finite minimum when a child's
         * fraction is unordered; MINSS's source operand does likewise. */
        Near(XAnimGetServerNotifyFracSyncTotal(syncTree,&g_xAnimInfo[1],&syncTree->anims->entries[0],
                                               &oldState,&nextState,0),1);
        FreeTree(syncTree);
    }
}

static void TestMapping(void)
{
    DObj object={0}; XModel model={0}; XModelParts parts={0}; XBoneHierarchy hierarchy={0};
    unsigned short names[2]={17,17}, boneNames[2]={17,17}; XAnimParts anim={0};
    parts.numBones=2; parts.hierarchy=&hierarchy; hierarchy.names=boneNames;
    model.parts=(void (*)())&parts; object.numModels=1; object.models[0]=&model;
    anim.boneCount=2; anim.names=names;
    unsigned int id=XAnimCalcBuildAnimToModelLocal(&object,&anim);
    const unsigned char *map=(const unsigned char *)SL_ConvertToString(id)+16;
    assert(map[0]==127 && map[1]==0);
}

int main(int argc,char **argv)
{
    const char *which=argc>1?argv[1]:NULL;
#define RUN(name, fn) if (!which || !strcmp(which,name)) fn()
    RUN("end",TestEnd); RUN("root",TestRoot); RUN("goals",TestGoals); RUN("time",TestTime);
    RUN("notify",TestNotify); RUN("skeleton",TestSkeleton); RUN("surfaces",TestSurfaces); RUN("mapping",TestMapping);
    RUN("trace",TestTrace);
    RUN("angles",TestAngles);
    RUN("names",TestNames);
    puts("Mac 1.3 animation, root motion, blending, notify, skeleton and surface fixtures passed");
    return 0;
}
