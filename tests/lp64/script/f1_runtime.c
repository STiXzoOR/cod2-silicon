/* Reuse the existing real compiler/VM fixture's host services. */
#define main F1_UnusedCallbackTests
#define Scr_GetFunction F1_CallbackResolver
#define RuntimeError F1_UnusedRuntimeError
#include "../../cod2x/stop_gametype_vm.c"
#undef main
#undef Scr_GetFunction
#undef RuntimeError

static int runtimeErrors, observations;
static int interceptError;
static jmp_buf runtimeError;
static unsigned int observedErrorIndex;
static char observedError[256];
static void Observe(void)
{
    assert(Scr_GetNumParam() == 1);
    fprintf(stderr, "observe(%d) at script time %d\n", Scr_GetInt(0), scrVarPub.time);
    assert(Scr_GetInt(0) == 99);
    ++observations;
}
BuiltinFunction Scr_GetFunction(const char **name, int *type)
{
    if (!strcmp(*name, "observe")) { *type = 0; return Observe; }
    return F1_CallbackResolver(name, type);
}
void RuntimeError(const char *pos, unsigned int index, const char *message, const char *dialog)
{
    assert(pos > bytecode && pos < bytecode + emitted);
    (void)dialog;
    ++runtimeErrors;
    observedErrorIndex = index;
    snprintf(observedError, sizeof(observedError), "%s", message);
    fprintf(stderr, "runtime error[%u]: %s\n", index, message);
    if (interceptError) longjmp(runtimeError, 1);
}
int main(int argc, char **argv)
{
    assert(argc == 2);
    Scr_Init(); Scr_InitSystem(1); Scr_BeginLoadScripts();
    if (!strcmp(argv[1], "sentinel"))
        snprintf(scriptText, sizeof(scriptText), "main() { observe(99); }");
    else if (!strcmp(argv[1], "self-endon"))
        snprintf(scriptText, sizeof(scriptText), "main() { level endon(\"done\"); level notify(\"done\"); observe(99); }");
    else if (!strcmp(argv[1], "switch-packed"))
        snprintf(scriptText, sizeof(scriptText), "main() { a=1; switch(a) { case 1: observe(99); break; default: observe(0); } }");
    else if (!strcmp(argv[1], "waittill-type"))
        snprintf(scriptText, sizeof(scriptText), "main() { a=1; a waittill(\"done\"); }");
    else if (!strcmp(argv[1], "notify-type"))
        snprintf(scriptText, sizeof(scriptText), "main() { level notify(1); }");
    else if (!strcmp(argv[1], "endon-type"))
        snprintf(scriptText, sizeof(scriptText), "main() { level endon(1); }");
    else if (!strcmp(argv[1], "switch-range"))
        snprintf(scriptText, sizeof(scriptText), "main() { a=2147483647; switch(a) { case 1: break; } }");
    else if (!strcmp(argv[1], "external-overflow"))
        snprintf(scriptText, sizeof(scriptText), "main() {}");
    else if (!strcmp(argv[1], "function-pointer-type"))
        snprintf(scriptText, sizeof(scriptText), "main() { a=1; [[a]](); }");
    else if (!strcmp(argv[1], "notify-wait"))
        snprintf(scriptText, sizeof(scriptText), "main() { level.flag=0; level thread waiter(); level notify(\"done\",7); waittillframeend; if(level.flag==7) observe(99); else observe(0); } waiter() { level waittill(\"done\",a); level.flag=a; }");
    else if (!strcmp(argv[1], "match-wait"))
        snprintf(scriptText, sizeof(scriptText), "main() { level.flag=0; level thread waiter(); level notify(\"done\",7); waittillframeend; if(level.flag==0) observe(99); else observe(0); level notify(\"done\",7,9); waittillframeend; if(level.flag==1) observe(99); else observe(0); } waiter() { level waittillmatch(\"done\",7,9); level.flag=1; }");
    else if (!strcmp(argv[1], "thread-churn"))
        snprintf(scriptText, sizeof(scriptText), "main() { for(i=0;i<2000;i++) { level thread waiter(); level notify(\"done\",7); waittillframeend; } observe(99); } waiter() { level waittill(\"done\",a); }");
    else if (!strcmp(argv[1], "endon-churn"))
        snprintf(scriptText, sizeof(scriptText), "main() { for(i=0;i<2000;i++) { level thread waiter(); level notify(\"done\"); waittillframeend; } observe(99); } waiter() { level endon(\"done\"); wait 1; }");
    else if (!strcmp(argv[1], "call-churn"))
        snprintf(scriptText, sizeof(scriptText), "main() { for(i=0;i<2000;i++) child(); observe(99); } child() { a=[]; }");
    else if (!strcmp(argv[1], "nested-wait-churn"))
        snprintf(scriptText, sizeof(scriptText), "main() { for(i=0;i<2000;i++) { a=child(i); if(a!=i+1) observe(0); } observe(99); } child(n) { a=n; b=grandchild(n); return a+b; } grandchild(n) { waittillframeend; return 1; }");
    else if (!strcmp(argv[1], "nested-thread-churn"))
        snprintf(scriptText, sizeof(scriptText), "main() { level.count=0; for(i=0;i<2000;i++) { level thread child(i); waittillframeend; } if(level.count==2000) observe(99); else observe(0); } child(n) { a=n; b=grandchild(n); if(a+b!=n+1) observe(0); level.count++; } grandchild(n) { waittillframeend; return 1; }");
    else if (!strcmp(argv[1], "generic-error"))
        snprintf(scriptText, sizeof(scriptText), "main() { a = []; a = !a; observe(99); }");
    else if (!strcmp(argv[1], "too-many-params"))
        snprintf(scriptText, sizeof(scriptText), "main() { child(1,2); observe(99); } child(a) { return 0; }");
    else if (!strcmp(argv[1], "method-error"))
        snprintf(scriptText, sizeof(scriptText), "main() { a=1; a randomint(1); observe(99); }");
    else if (!strcmp(argv[1], "negative-wait"))
        snprintf(scriptText, sizeof(scriptText), "main() { wait -1; observe(99); }");
    else if (!strcmp(argv[1], "inc-type"))
        snprintf(scriptText, sizeof(scriptText), "main() { a=\"bad\"; a++; observe(99); }");
    else abort();
    assert(Scr_LoadScript("f1"));
    scrCompilePub.programLen = emitted;
    scrVarPub.endScriptBuffer = bytecode + emitted;
    scrCompilePub.script_loading = 0;
    Scr_Settings(0, 0, 0);
    scr_func_t handle = Scr_GetFunctionHandle("f1", "main");
    assert(handle);
    if (!strcmp(argv[1], "external-overflow")) {
        extern void Scr_AddInt(int value);
        Scr_AddInt(11); Scr_AddInt(22);
        scrVmPub.function_count = 30;
        Scr_AddExecThread(handle, 1);
        assert(runtimeErrors == 1 && observations == 0);
        assert(scrVmPub.top == scrVmPub.stack + 2 && scrVmPub.top->type == 0);
        assert(scrVmPub.stack[1].u.intValue == 11);
        assert(scrVmPub.inparamcount == 1 && scrVmPub.outparamcount == 1);
        puts(argv[1]); return 0;
    }
    if (!strcmp(argv[1], "sentinel")) {
        scrVmPub.stack[0].type = 6;
        scrVmPub.stack[0].u.intValue = 71;
    }
    int sentinel = scrVmPub.stack[0].type;
    interceptError = !strcmp(argv[1], "too-many-params") || !strcmp(argv[1], "inc-type") ||
        !strcmp(argv[1], "waittill-type") || !strcmp(argv[1], "notify-type") ||
        !strcmp(argv[1], "endon-type") || !strcmp(argv[1], "switch-range") || !strcmp(argv[1], "function-pointer-type");
    if (!setjmp(runtimeError)) {
        Scr_FreeThread(Scr_ExecThread(handle, 0));
        if (!strcmp(argv[1], "notify-wait") || !strcmp(argv[1], "match-wait") || !strcmp(argv[1], "thread-churn") || !strcmp(argv[1], "endon-churn") || !strcmp(argv[1], "nested-wait-churn") || !strcmp(argv[1], "nested-thread-churn")) {
            extern void Scr_RunCurrentThreads(void);
            Scr_RunCurrentThreads();
        }
        assert(!interceptError);
    }
    assert(runtimeErrors == (!strcmp(argv[1], "generic-error") || !strcmp(argv[1], "negative-wait") || interceptError));
    if (!interceptError) {
        assert(observations == (!strcmp(argv[1], "match-wait") ? 2 : strcmp(argv[1], "self-endon") != 0));
        assert(scrVmPub.stack[0].type == sentinel);
    }
    if (!strcmp(argv[1], "too-many-params"))
        assert(!strcmp(observedError, "function called with too many parameters"));
    if (!strcmp(argv[1], "negative-wait"))
        assert(!strcmp(observedError, "negative wait is not allowed") && observedErrorIndex == 2);
    if (!strcmp(argv[1], "waittill-type"))
        assert(!strcmp(observedError, "int is not an object") && observedErrorIndex == 2);
    if (!strcmp(argv[1], "notify-type"))
        assert(!strcmp(observedError, "first parameter of notify must evaluate to a string") && observedErrorIndex == 1);
    if (!strcmp(argv[1], "endon-type"))
        assert(!strcmp(observedError, "first parameter of endon must evaluate to a string") && observedErrorIndex == 0);
    if (!strcmp(argv[1], "switch-range"))
        assert(!strcmp(observedError, "switch index 2147483647 out of range"));
    if (!strcmp(argv[1], "function-pointer-type"))
        assert(!strcmp(observedError, "int is not a function pointer") && observedErrorIndex == 0);
    if (!strcmp(argv[1], "inc-type"))
        assert(!strcmp(observedError, "++ must be applied to an int (applied to string)"));
    if (!interceptError) {
        assert(scrVmPub.top == scrVmPub.stack && scrVmPub.function_count == 0);
        assert(scrVmPub.inparamcount == 0 && scrVmPub.outparamcount == 0);
    }
    puts(argv[1]);
    return 0;
}
