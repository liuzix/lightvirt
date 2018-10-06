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
#include "utils.hpp"

#define PAGE_SIZE 4096
#define PAGETABLE_SIZE 4096

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
	virtual void *getHostVirtualFromPhysical(addr_t addr) const = 0;
	virtual addr_t getPhysicalFromHostVirtual(void *hostVirtual) const = 0;
};

struct GuestPhysicalPage {
	void *hostVirtual;
	addr_t guestPhysical;

	GuestPhysicalPage(addr_t guest, void *host)
	{
		guestPhysical = guest;
		hostVirtual = host;
	}
};

using GuestPhysicalPagePtr = std::shared_ptr<GuestPhysicalPage>;

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


	virtual void *getHostVirtualFromPhysical(addr_t addr) const;

	virtual addr_t getPhysicalFromHostVirtual(void *hostVirtual) const;

private:
	auto getBlockIterator(addr_t addr, size_t len);
};


class MemorySpace;

class MemoryRegion {
private:
	std::recursive_mutex lock;
	addr_t guestVirtualAddr;
	size_t len;
	
	std::vector<GuestPhysicalPagePtr> physicalPages;
	MemorySpace *memorySpace;
	bool isKernel;

public:
	MemoryRegion(addr_t guestVirt, size_t _len);

	MemoryRegion(MemoryRegion &) = delete; // TODO: delete for now

	virtual	void fault(addr_t guestVirtualPage, uint32_t errorcode) = 0;

	void setMemorySpace(MemorySpace *_memorySpace)
	{ memorySpace = _memorySpace; }
	
	void setIsKernel(bool _isKernel)
	{ isKernel = _isKernel; }

	addr_t getKey() const
	{ return guestVirtualAddr; }

private:
	virtual PageTableEntry *mapPage(size_t offset) = 0;

	friend class MemorySpace;

};

class MemorySpace {
private:
	std::recursive_mutex lock;
	void *pageTableV;
	addr_t pageTableP;

	using MemoryRegionPtr = ComparablePointerAdapter<std::shared_ptr<MemoryRegion>>;
	std::set<MemoryRegionPtr, MemoryBlockComparator<MemoryRegionPtr>> regions;
	AbstractMemoryPool *memoryPool;
	std::vector<GuestPhysicalPage> pageTablePages;
	// TODO: we need a task list here to flush tlbs
	
public:
	MemorySpace(AbstractMemoryPool *_memoryPool);

	void apply(vcpu_t *vcpu);

	void flushTlb(addr_t guestVirtualPage);

	bool fault(addr_t guestVirtualPage, uint32_t errorcode);
private:
	PageTableEntry *getPTE(addr_t guestVirtual, bool create = false);

	template <typename T>
	T *castGuestPhysical(addr_t addr)
	{
		return static_cast<T *>
			(memoryPool->getHostVirtualFromPhysical(addr));
	}

	friend class MemoryRegion;
};

#endif
