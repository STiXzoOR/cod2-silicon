# system/stl_tree.h attribution reconciliation

Updated 2026-11-05.

2 equal, 4 fixed, 5 intentional (11 entries).

The CVAOPacket lower/upper bounds match. Missing native node insertion and subtree erasure are restored, using actual native field sizes and the existing native rebalance ABI. The packet global destructor no longer calls Shutdown and resets live header/status fields that Mac does not reset. The string-key tree and its global lifetime are replaced by the native external shader cache, separately from the still-active packet tree.

Changed or unresolved examples: `ZNSt8_Rb_treeIjSt4pairIKj10CVAOPacketESt10_Select1stIS3_ESt4lessIjESaIS3_EE12insert_equalERKS3_` at 0x002f131e, `ZNSt8_Rb_treeIjSt4pairIKj10CVAOPacketESt10_Select1stIS3_ESt4lessIjESaIS3_EE8_M_eraseEPSt13_Rb_tree_nodeIS3_E` at 0x002f1370, `ZNSt8_Rb_treeIjSt4pairIKj10CVAOPacketESt10_Select1stIS3_ESt4lessIjESaIS3_EE9_M_insertEPSt18_Rb_tree_node_baseSB_RKS3_` at 0x002f12a0, `__static_initialization_and_destruction_0` at 0x00113a6e.

The Mac binary and actual native owners were read directly; no other worker’s verdict replaced binary evidence. Build/guard verification and runtime limitations are recorded in [system/basic_string.h](system-basic_string.md).
