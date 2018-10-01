CXX = g++
CC = gcc
CFLAGS = -Wall -MMD -MP -g -std=gnu99
CXXFLAGS = -Wall -MMD -MP -g -std=c++17
OUTPUTDIR = build

KERNEL_LDFLAGS = -ffreestanding -nostdlib -T kernel.ld

target = lightvirt
csrc = kvm.c

ccsrc = memory.cpp
ccsrc += main.cpp

cobj = $(addprefix $(OUTPUTDIR)/,$(patsubst %.c,%.o,$(csrc)))
ccobj = $(addprefix $(OUTPUTDIR)/,$(patsubst %.cpp,%.o,$(ccsrc)))
dep = $(cobj:.o=.d)
dep += $(ccobj:.o=.d)


all: $(target) kernel


$(target): $(cobj) $(ccobj)
	$(CXX) $^ -o $@ -lpthread

$(cobj) : $(OUTPUTDIR)/%.o : src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(ccobj) : $(OUTPUTDIR)/%.o : src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

kernel: src/kernel.c
	$(CC) $(CFLAGS) -c $< -o $(OUTPUTDIR)/kernel.o
	$(CC) $(KERNEL_LDFLAGS) $(OUTPUTDIR)/kernel.o -o kernel.bin


.PHONY: clean

clean:
	rm $(OUTPUTDIR)/* $(target) kernel.bin

-include $(dep)

