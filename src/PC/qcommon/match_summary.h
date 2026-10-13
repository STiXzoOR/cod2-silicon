#ifndef COD2_MATCH_SUMMARY_H
#define COD2_MATCH_SUMMARY_H

#if defined(COD2_X64)
void MatchSummary_Init(void);
void MatchSummary_Frame(int rawMsec, int frameBudgetMsec, int capFPS);
void MatchSummary_Snapshot(int ping, int pingMeasured, int intermission);
void MatchSummary_Packet(int dropped);
void MatchSummary_Finish(const char *reason);
#endif

#endif
