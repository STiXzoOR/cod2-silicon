.text
.globl _Hunk_AllocInternal
_Hunk_AllocInternal:
	jmp *(_ri+12)
.globl _Hunk_AllocAlignInternal
_Hunk_AllocAlignInternal:
	jmp *(_ri+28)
.globl _Hunk_AllocateTempMemoryInternal
_Hunk_AllocateTempMemoryInternal:
	jmp *(_ri+16)
.globl _Hunk_AllocateTempMemoryHighInternal
_Hunk_AllocateTempMemoryHighInternal:
	jmp *(_ri+64)
.globl _Z_MallocInternal
_Z_MallocInternal:
	jmp *(_ri+20)
.globl _Z_FreeInternal
_Z_FreeInternal:
	jmp *(_ri+24)
.globl _Z_VirtualReserveInternal
_Z_VirtualReserveInternal:
	jmp *(_ri+32)
.globl _Z_VirtualCommitInternal
_Z_VirtualCommitInternal:
	jmp *(_ri+36)
.globl _Z_VirtualDecommitInternal
_Z_VirtualDecommitInternal:
	jmp *(_ri+40)
.globl _Z_VirtualFreeInternal
_Z_VirtualFreeInternal:
	jmp *(_ri+44)
.globl _Hunk_FreeTempMemory
_Hunk_FreeTempMemory:
	jmp *(_ri+48)
.globl _Hunk_ClearTempMemory
_Hunk_ClearTempMemory:
	jmp *(_ri+52)
.globl _Hunk_ClearTempMemoryHigh
_Hunk_ClearTempMemoryHigh:
	jmp *(_ri+68)
.globl _Hunk_HideTempMemory
_Hunk_HideTempMemory:
	jmp *(_ri+56)
.globl _Hunk_ShowTempMemory
_Hunk_ShowTempMemory:
	jmp *(_ri+60)
