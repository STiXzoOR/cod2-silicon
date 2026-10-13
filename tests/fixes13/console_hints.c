#include "common_types.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
enum { DVAR_TYPE_BOOL, DVAR_TYPE_FLOAT, DVAR_TYPE_VEC2, DVAR_TYPE_VEC3, DVAR_TYPE_VEC4,
       DVAR_TYPE_INT, DVAR_TYPE_ENUM, DVAR_TYPE_STRING, DVAR_TYPE_COLOR };
#if defined(COD2_CODX) && COD2_CODX
#define COD2X_CON_NAME_CHARS 40
#define COD2X_CON_VALUE_OFFSET 340.0f
#else
#define COD2X_CON_NAME_CHARS 24
#define COD2X_CON_VALUE_OFFSET 200.0f
#endif
static ConDrawInputGlob conDrawInputGlob;
static dvar_t variable;
static int latched, argument_count = 2, completions;
static const vec4_t con_inputDvarMatchColor={1}, con_inputDvarValueColor={2}, con_inputDvarInfoColor={3}, con_inputDvarInactiveValueColor={4};
static struct { char text[2048]; float x,y; int max; const float *color; } draws[16];
static struct { int lines; float x,y; } boxes[4];
static int draw_count, box_count, failures;
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr,"failed line %d: %s\n",__LINE__,#condition); ++failures; } } while (0)
int I_strnicmp(const char *a,const char *b,size_t n) { return strncasecmp(a,b,n); }
const dvar_t *Dvar_FindVar(const char *name) { return !strcmp(name,"sample") ? &variable : NULL; }
Bool Dvar_HasLatchedValue(const dvar_t *dvar) { return latched; }
const char *Dvar_DisplayableValue(const dvar_t *dvar) { return "current"; }
const char *Dvar_DisplayableResetValue(const dvar_t *dvar) { return "reset"; }
const char *Dvar_DisplayableLatchedValue(const dvar_t *dvar) { return "pending"; }
int Cmd_Argc(void) { return argument_count; }
static void ConDrawInput_DrawText(const char *text,int max,const float *color)
{
 assert(draw_count<16); assert(strlen(text)<sizeof draws[0].text);
 strcpy(draws[draw_count].text,text);draws[draw_count].x=conDrawInputGlob.x;
 draws[draw_count].y=conDrawInputGlob.y;draws[draw_count].max=max;draws[draw_count++].color=color;
}
static void ConDrawInput_NextLine(void) { conDrawInputGlob.y+=conDrawInputGlob.fontHeight;conDrawInputGlob.x=conDrawInputGlob.leftX; }
static void ConDrawInput_DrawHintBox(int lines,float width)
{
 assert(box_count<4);assert(width==0);boxes[box_count].lines=lines;
 boxes[box_count].x=conDrawInputGlob.x;boxes[box_count++].y=conDrawInputGlob.y;
}
static void ConDrawInput_AutoCompleteArg(const char **strings,int count) { ++completions; }
#include "console_hints_source.h"
int main(int argc,char **argv)
{
 const char *choices[]={"alpha","beta","gamma"};
 if(argc>1) {
  char huge[3000];char *out=malloc(64);assert(out);memset(huge,'x',sizeof huge-1);huge[sizeof huge-1]=0;
  const char *long_choices[]={huge,huge};DvarLimits domain={0};int lines=-1;
  domain.enumeration.strings=long_choices;domain.enumeration.stringCount=2;
  const char *end=Dvar_DomainToString_GetLines(DVAR_TYPE_ENUM,domain,out,64,&lines);
  assert(out[63]==0);assert(lines==1);assert(end>=out&&end<out+64);
  for(int size=1;size<=32;++size) {
   memset(out,0x55,64);lines=-1;
   end=Dvar_DomainToString_GetLines(DVAR_TYPE_ENUM,domain,out,size,&lines);
   assert(out[size-1]==0&&out[size]==0x55);assert(end>=out&&end<out+size);
  }
  out[0]='z';lines=-1;Dvar_DomainToString_GetLines(DVAR_TYPE_ENUM,domain,out,0,&lines);
  assert(out[0]=='z'&&lines==0);free(out);puts("PASS bounded enum domain");return 0;
 }
 int types[]={DVAR_TYPE_BOOL,DVAR_TYPE_INT,DVAR_TYPE_FLOAT,DVAR_TYPE_ENUM,DVAR_TYPE_STRING};
 const char *domains[]={"Domain is 0 or 1","Domain is any integer from -2 to 8","Domain is any number from 0.5 to 2.5","Domain is one of the following:\n   0: alpha\n   1: beta\n   2: gamma","Domain is any text"};
 for(int t=0;t<5;++t) for(latched=0;latched<2;++latched) {
  memset(&variable,0,sizeof variable);variable.type=types[t];
  if(types[t]==DVAR_TYPE_INT) { variable.domain.integer.min=-2;variable.domain.integer.max=8; }
  if(types[t]==DVAR_TYPE_FLOAT) { variable.domain.value.min=.5f;variable.domain.value.max=2.5f; }
  if(types[t]==DVAR_TYPE_ENUM) { variable.domain.enumeration.strings=choices;variable.domain.enumeration.stringCount=3; }
  conDrawInputGlob=(ConDrawInputGlob){.inputText="sample",.inputTextLen=6,.hasExactMatch=1,.x=10,.y=20,.leftX=10,.fontHeight=16};
  draw_count=box_count=completions=0;
  ConDrawInput_DetailedDvarMatch("sample");
  CHECK(box_count==2);CHECK(boxes[0].lines==2+latched);
  CHECK(draw_count==5+2*latched);
  CHECK(!strcmp(draws[0].text,"sample")&&draws[0].max==COD2X_CON_NAME_CHARS);
  CHECK(draws[1].x==10+COD2X_CON_VALUE_OFFSET&&draws[1].y==20);
  int default_index=latched?4:2;
  CHECK(!strcmp(draws[default_index].text,"  default"));
  CHECK(!strcmp(draws[default_index+1].text,"reset"));
  CHECK(draws[default_index+1].color==con_inputDvarInactiveValueColor);
  if(latched) { CHECK(!strcmp(draws[2].text,"  latched value"));CHECK(!strcmp(draws[3].text,"pending")); }
  CHECK(!strcmp(draws[draw_count-1].text,domains[t]));
  CHECK(boxes[1].lines==(types[t]==DVAR_TYPE_ENUM?4:1));
  CHECK(boxes[1].y==20+(3+latched)*16);
  CHECK(completions==(types[t]==DVAR_TYPE_ENUM));
 }
 draw_count=box_count=completions=0;ConDrawInput_DetailedDvarMatch("other");CHECK(!draw_count&&!box_count&&!completions);
 ConDrawInput_DetailedDvarMatch("sampleExtra");CHECK(!draw_count&&!box_count&&!completions);
 puts(failures?"FAIL console hints":"PASS console hints");return failures!=0;
}
