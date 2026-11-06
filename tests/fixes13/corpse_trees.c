#include "common_types.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static bgs_t level_bgs;
static scr_data_t g_scr_data;
static void *imp_g_scr_data = &g_scr_data;
static uint64_t storage[72][32];
static int allocations, loads, frees;
static unsigned char freed[72];

void *Hunk_AllocXAnimServer(int size) { (void)size; assert(0); return NULL; }
static int scriptLoads;
static XAnim_s *humanAnims = (XAnim_s *)&storage[0][1];
snd_alias_list_t *Com_FindSoundAlias(const char *name) { (void)name; return NULL; }
int G_AnimScriptSound(int client, snd_alias_list_t *aliasList) { (void)client; (void)aliasList; return 0; }
void Scr_BeginLoadScripts(void) { assert(scriptLoads == 0); }
void GScr_LoadScripts(int level) { assert(level == 1); ++scriptLoads; }
void Scr_EndLoadScripts(void) { assert(scriptLoads == 1); }
void BG_LoadAnim(void) { assert(scriptLoads == 1); level_bgs.generic_human.tree.anims = humanAnims; ++loads; }
XAnimTree *XAnimCreateTree(XAnim *anims, Alloc_t allocator)
{
    /* Trees are built after BG_LoadAnim from the generic human anim tree. */
    assert(anims == (XAnim *)humanAnims);
    assert(allocator == (Alloc_t)Hunk_AllocXAnimServer && allocations < 72);
    return (XAnimTree *)storage[allocations++];
}
void XAnimFreeTree(XAnimTree_s *tree, void (*release)(void *, int))
{
    assert(!release);
    int i;
    for (i = 64; i < 72; ++i) {
        if (tree == (XAnimTree_s *)storage[i]) {
            assert(!freed[i]);
            freed[i] = 1;
            ++frees;
            return;
        }
    }
    assert(!"released a player tree instead of the corpse-owned tree");
}
#include "corpse_trees_source.h"

int main(void)
{
    initialize_trees(0);
    initialize_corpse_slots();
    for (int i = 0; i < 8; ++i)
        assert(g_scr_data.playerCorpseInfo[i].entnum == -1);
    if (allocations != 72) {
        fprintf(stderr, "FAIL: cold level requires 64 player and 8 corpse trees; got %d\n", allocations);
        return 1;
    }
    for (int i = 0; i < 64; ++i)
        assert(level_bgs.clientinfo[i].pXAnimTree == (XAnimTree_s *)storage[i]);
    for (int i = 0; i < 8; ++i) {
        assert(g_scr_data.playerCorpseInfo[i].tree == (XAnimTree_s *)storage[64 + i]);
        /* Unused slots have no copied clientinfo; used slots alias their own tree. */
        if (i & 1)
            g_scr_data.playerCorpseInfo[i].ci.pXAnimTree = g_scr_data.playerCorpseInfo[i].tree;
    }
    for (int i = 0; i < 8; ++i)
        g_scr_data.playerCorpseInfo[i].entnum = 64 + i;
    initialize_trees(1);
    initialize_corpse_slots();
    for (int i = 0; i < 8; ++i)
        assert(g_scr_data.playerCorpseInfo[i].entnum == -1);
    assert(loads == 1 && allocations == 72);
    release_corpse_trees();
    assert(frees == 8);
    for (int i = 0; i < 8; ++i) {
        assert(!g_scr_data.playerCorpseInfo[i].tree);
        /* Mac shutdown frees the slot-owned tree and leaves ci untouched
           (see tests/lp64/game/main_shutdown.c). */
        assert(g_scr_data.playerCorpseInfo[i].ci.pXAnimTree ==
               (i & 1 ? (XAnimTree_s *)storage[64 + i] : NULL));
    }
    release_corpse_trees();
    assert(frees == 8);
    assert(scriptLoads == 1);
    puts("PASS cold allocation, restart retention, unused/used corpse shutdown");
    return 0;
}
