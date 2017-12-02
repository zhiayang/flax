// gluecode.h
// Copyright (c) 2017, zhiayang@gmail.com
// Licensed under the Apache License Version 2.0.

#include <stdint.h>

#define DEBUG_RUNTIME_GLUE_MASTER	1

#define DEBUG_STRING_MASTER			(1 & DEBUG_RUNTIME_GLUE_MASTER)
#define DEBUG_STRING_ALLOCATION		(1 & DEBUG_STRING_MASTER)
#define DEBUG_STRING_REFCOUNTING	(1 & DEBUG_STRING_MASTER)

#define DEBUG_ARRAY_MASTER			(1 & DEBUG_RUNTIME_GLUE_MASTER)
#define DEBUG_ARRAY_ALLOCATION		(1 & DEBUG_ARRAY_MASTER)
#define DEBUG_ARRAY_REFCOUNTING		(1 & DEBUG_ARRAY_MASTER)


