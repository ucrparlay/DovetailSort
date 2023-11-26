ifdef GCC
CC = g++
else
CC = clang++
endif

CPPFLAGS = -std=c++20 -Wall -Wextra -Werror

ifdef BREAKDOWN
CPPFLAGS += -DBREAKDOWN
endif

ifdef CILKPLUS
CPPFLAGS += -DPARLAY_CILKPLUS -DCILK -fcilkplus
else ifdef OPENCILK
CPPFLAGS += -DPARLAY_OPENCILK -DCILK -fopencilk
else ifdef SERIAL
CPPFLAGS += -DPARLAY_SEQUENTIAL
else
CPPFLAGS += -pthread
endif

ifdef DEBUG
CC = g++
CPPFLAGS += -Og -mcx16 -DDEBUG -DPARLAY_DEBUG_UNINITIALIZED
else ifdef PERF
CC = g++
CPPFLAGS += -Og -g
else ifdef MEMCHECK
CC = g++
CPPFLAGS += -Og -mcx16 -DPARLAY_SEQUENTIAL
else
CPPFLAGS += -O3 -mcx16 -march=native
endif

ifdef STDALLOC
CPPFLAGS += -DPARLAY_USE_STD_ALLOC
endif

all: integer_sort

integer_sort: integer_sort.h integer_sort.cpp internal/integer_sort.h internal/counting_sort.h generator.h
	$(CC) $(CPPFLAGS) integer_sort.cpp -o integer_sort
