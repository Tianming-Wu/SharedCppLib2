#pragma once

# define EVL_ANNOTATE_CLASS(type, ...)
# define EVL_ANNOTATE_CLASS2(type, a1, a2)
# define EVL_ANNOTATE_FUNCTION(x)
# define EVL_ANNOTATE_ACCESS_SPECIFIER(x)

# define EVL_SLOTS EVL_ANNOTATE_ACCESS_SPECIFIER(evl_slot)
# define EVL_SIGNALS public EVL_ANNOTATE_ACCESS_SPECIFIER(evl_signal)
# define EVL_PRIVATE_SLOT(d, signature) EVL_ANNOTATE_CLASS2(evl_private_slot, d, signature)
# define EVL_EMIT

# define slots EVL_SLOTS
# define signals EVL_SIGNALS
// # define emit

#define EVL_SIGNAL QT_ANNOTATE_FUNCTION(evl_signal)
#define EVL_SLOT QT_ANNOTATE_FUNCTION(evl_slot)