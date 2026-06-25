#ifndef MAC_TOOLS_MACTOOLS_H_H
#define MAC_TOOLS_MACTOOLS_H_H

struct HICommand;

struct HICommand {
    UInt32 attributes;
    UInt32 commandID;
    struct {
        MenuRef menuRef;
        MenuItemIndex menuItemIndex;
    } menu;
};

#endif
