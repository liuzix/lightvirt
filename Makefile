CXX = g++
CC = gcc
CFLAGS = -Wall -MMD -MP -g -std=gnu99
CXXFLAGS = -Wall -MMD -MP -g -std=c++17
OUTPUTDIR = build

KERNEL_LDFLAGS = -ffreestanding -nostdlib -T kernel.ld -fPIC

target = lightvirt
csrc = kvm.c

ccsrc = memory.cpp main.cpp fs.cpp

ksrc = kernel.c
kasm = entry.S idt.S


cobj = $(addprefix $(OUTPUTDIR)/,$(patsubst %.c,%.o,$(csrc)))
ccobj = $(addprefix $(OUTPUTDIR)/,$(patsubst %.cpp,%.o,$(ccsrc)))
kobj = $(addprefix $(OUTPUTDIR)/,$(patsubst %.c,%.o,$(ksrc)))
kasmobj = $(addprefix $(OUTPUTDIR)/,$(patsubst %.S,%.o,$(kasm)))

dep = $(cobj:.o=.d)
dep += $(ccobj:.o=.d)
dep += $(kobj:.o=.d)

all: $(target) kernel.bin


$(target): $(cobj) $(ccobj)
	$(CXX) $^ -o $@ -lpthread

$(cobj) : $(OUTPUTDIR)/%.o : src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

$(ccobj) : $(OUTPUTDIR)/%.o : src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

kernel.bin: $(kobj) $(kasmobj)
	$(CC) $(KERNEL_LDFLAGS) $^ -o kernel.bin

$(kobj) : $(OUTPUTDIR)/%.o : src/%.c
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

$(kasmobj) : $(OUTPUTDIR)/%.o : src/%.S
	$(CC) $(CFLAGS) -fPIC -c $< -o $@

src/idt.S: src/idt_gen.pl
	src/idt_gen.pl > src/idt.S

.PHONY: clean

clean:
	rm $(OUTPUTDIR)/* $(target) kernel.bin

-include $(dep)

