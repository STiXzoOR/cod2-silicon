/* Platform fixtures exercise transport/presentation without the game shim. */
#include "Mac/Metal/metal_backend.h"
unsigned int MacMetalTest_Cancellations;
void MacMetal_ProducerSubmitted(uint64_t value) { if (!value) ++MacMetalTest_Cancellations; }
void MacMetal_CollectRetired(uint64_t value) { (void)value; }
void MacMetal_TargetResized(unsigned int width, unsigned int height) { (void)width; (void)height; }
