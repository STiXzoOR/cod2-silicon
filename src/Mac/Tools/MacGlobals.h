#ifndef MAC_TOOLS_MACGLOBALS_H_H
#define MAC_TOOLS_MACGLOBALS_H_H

struct AlertStdCFStringAlertParamRec;

struct AlertStdCFStringAlertParamRec {
    UInt32 version;
    Boolean movable;
    Boolean helpButton;
    CFStringRef defaultText;
    CFStringRef cancelText;
    CFStringRef otherText;
    SInt16 defaultButton;
    SInt16 cancelButton;
    UInt16 position;
    OptionBits flags;
};

#endif
