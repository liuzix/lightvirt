#ifndef MEMORY_HPP
#define MEMORY_HPP

#include <mutex>
#include <shared_mutex>
#include <memory>
#include <set>
#include <vector>
#include <type_traits>
#include "kvm.h"
#include "archflags.h"
#include "log.hpp"

#define PAGE_SIZE 4096

inline void checkPageMultiple(size_t len)
{
	if (len % PAGE_SIZE != 0) {
		console->error("{} is not a multiple of {}", len, PAGE_SIZE);
		std::abort();
	}
}

class AbstractHostMemoryMapper {
public:
	virtual void *operator()(size_t len) = 0;
};

class DefaultHostMemoryMapper: public AbstractHostMemoryMapper {
public:
	void *operator()(size_t len);
	static DefaultHostMemoryMapper instance;
};

class AbstractMemoryPool {
public:
	virtual addr_t getPhysicalMemoryBlock(size_t len) = 0;
	virtual void freePhysicalMemoryBlock(addr_t addr, size_t len) = 0;
};

struct GuestPhysicalPage {
	void *hostVirtual;
	addr_t guestPhysical;
};

using GuestPhysicalPagePtr = std::shared_ptr<GuestPhysicalPage>;

template <typename BlockType>
struct MemoryBlockComparator {
	using is_transparent = void;
	using KeyType =
		typename std::result_of<decltype(&BlockType::getKey)(BlockType)>::type;	

	bool operator()(const BlockType &lhs, const BlockType &rhs) const
	{ return lhs.guestPhysical < rhs.guestPhysical; }

	bool operator()(const KeyType &physical, const BlockType &blk) const
	{ return physical < blk.guestPhysical; }

	bool operator()(const BlockType &blk, const KeyType &physical) const
	{ return blk.guestPhysical < physical; }
};

class MemoryPool: public AbstractMemoryPool {
private:
	using Mapper = AbstractHostMemoryMapper;

	struct MemoryBlock {
		addr_t guestPhysical;
		mutable size_t len;
		MemoryBlock(addr_t guestPhys, size_t len)
			: guestPhysical(guestPhys), len(len) {}
		addr_t getKey() const
		{ return guestPhysical; }
	};

	std::recursive_mutex lock;
	vm_t *vm;
	mem_t *mem;
	size_t size;
	void *virtBase;
	addr_t physBase;
	std::set<MemoryBlock, MemoryBlockComparator<MemoryBlock>> blocks;
public:
	MemoryPool(vm_t *_vm, addr_t _physBase, size_t _size,
		Mapper &mapper = DefaultHostMemoryMapper::instance);

	virtual addr_t getPhysicalMemoryBlock(size_t len);

	virtual void freePhysicalMemoryBlock(addr_t addr, size_t len);

private:
	auto getBlockIterator(addr_t addr, size_t len);
};


class MemorySpace;

class MemoryRegion {
private:
	addr_t guestVirtualAddr;
	size_t len;
	
	std::vector<GuestPhysicalPagePtr> physicalPages;
	MemorySpace *memorySpace;
	bool isKernel;

public:
	MemoryRegion(addr_t guestVirt, size_t _len);

	MemoryRegion(MemoryRegion &) = delete; // TODO: delete for now

	bool fault(addr_t guestVirtualPage);

	void setMemorySpace(MemorySpace *_memorySpace)
	{ memorySpace = _memorySpace; }
	
	void setIsKernel(bool _isKernel)
	{ isKernel = _isKernel; }

	addr_t getKey() const
	{ return guestVirtualAddr; }
};

class MemorySpace {
private:
	void *pageTableV;
	addr_t pageTableP;
	std::set<MemoryRegion, MemoryBlockComparator<MemoryRegion>> regions;
	// TODO: we need a task list here to flush tlbs
	
public:
	void apply(vcpu_t *vcpu);
	void flushTlb(addr_t guestVirtualPage);
};

#endif
