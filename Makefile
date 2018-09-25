CXX = g++
CC = gcc
CFLAGS = -Wall -MMD -MP -g -std=gnu99
CXXFLAGS = -Wall -MMD -MP -g -std=c++17
OUTPUTDIR = build


target = lightvirt
csrc = kvm.c
ccsrc = main.cpp

cobj = $(csrc:.c=.o)
ccobj = $(ccsrc:.cpp=.o)
dep = $(obj:.o=.d)


$(target): $(OUTPUTDIR)/$(cobj) $(OUTPUTDIR)/$(ccobj)
	$(CXX) $^ -o $@

$(OUTPUTDIR)/$(cobj): src/$(csrc)
	$(CC) $(CFLAGS) -c $< -o $@

$(OUTPUTDIR)/$(ccobj): src/$(ccsrc)
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean

clean:
	rm $(OUTPUTDIR)/* $(target)

-include $(dep)

