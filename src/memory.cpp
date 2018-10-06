#include "memory.hpp"

#include <sys/mman.h>
#include "kvm.h"
#include "archflags.h"
#include "log.hpp"

DefaultHostMemoryMapper DefaultHostMemoryMapper::instance;

void *DefaultHostMemoryMapper::operator()(size_t len)
{
	void *hostVirtualAddr = mmap(NULL, len, PROT_READ | PROT_WRITE,
		MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);

	if (hostVirtualAddr == MAP_FAILED) {
		console->error("Cannot mmap memory, length = {}", len);
		return nullptr;
	}

	return hostVirtualAddr;
}

MemoryPool::MemoryPool(vm_t *_vm, addr_t _physBase, size_t _size, Mapper &mapper)
	: vm(_vm), size(_size), physBase(_physBase)
{
	virtBase = mapper(size);
	if (!virtBase) std::abort();

	mem = vm_map_guest_physical(vm, virtBase, physBase, size);
	if (!mem) {
		console->error("Cannot map physical memory, exit");
		std::abort();
	}
}

addr_t MemoryPool::getPhysicalMemoryBlock(size_t len)
{
	std::lock_guard<std::recursive_mutex> guard(lock);

	addr_t lastEnd = physBase;
	auto blk = blocks.begin();

	for (; blk != blocks.end(); blk++) {
		if (blk->guestPhysical - lastEnd >= len) break;
		lastEnd = blk->guestPhysical + blk->len;
	}

	if (blk == blocks.begin()) {
		/* create a new block in the beginning */
		blocks.emplace_hint(blk, lastEnd, len);
	} else {
		/* extend the previous block */
		auto prev = std::prev(blk);
		prev->len += len;
	}

	console->trace("Allocate physical block addr = 0x{:x}, size = {}", lastEnd, len);
	return lastEnd;
}

auto MemoryPool::getBlockIterator(addr_t addr, size_t len)
{
	std::lock_guard<std::recursive_mutex> guard(lock);
	
	auto blk = blocks.lower_bound(addr);
	if (blk == blocks.end() && blocks.empty()) {
		console->warn("Cannot find block 0x{:x}. Before first block", addr);
		return blk;
	}

	if (blk->guestPhysical == addr) return blk;
	
	if (blk == blocks.begin()) {
		console->warn("Cannot find block 0x{:x}. Before first block", addr);
		return blk;
	}

	blk--;
	if (blk->guestPhysical > addr ||
		blk->guestPhysical + blk->len < addr + len) {
	
		console->warn("Cannot find block 0x{:x}", addr);
		return blocks.end();
	}

	return blk;
}

void MemoryPool::freePhysicalMemoryBlock(addr_t addr, size_t len)
{
	std::lock_guard<std::recursive_mutex> guard(lock);

	auto blk = getBlockIterator(addr, len);

	if (blk == blocks.end()) {
		console->error("Cannot find guest physical memory 0x{:x}",
				addr);
		std::abort();
	}

	auto temp = *blk;
	blocks.erase(blk);

	if (temp.guestPhysical < addr) {
		blocks.emplace(temp.guestPhysical, addr - temp.guestPhysical);
	}

	if (temp.guestPhysical + temp.len > addr + len) {
		blocks.emplace(addr + len,
			temp.guestPhysical + temp.len - addr - len);
	}
}

void *MemoryPool::getHostVirtualFromPhysical(addr_t addr) const
{
	uint64_t offset = addr - physBase;
	if (offset < 0 || offset >= size) {
		console->error("Physical address 0x{:x} out of bound",
				addr);
		std::abort();
	}

	return (char *)virtBase + offset;
}

addr_t MemoryPool::getPhysicalFromHostVirtual(void *hostVirtual) const
{
	uintptr_t offset = (char *)hostVirtual - (char *)virtBase;
	if (offset < 0 || offset >= size) {
		console->error("Physical address 0x{:x} out of bound",
				(uintptr_t)hostVirtual);
		std::abort();
	}

	return physBase + offset;
}

MemoryRegion::MemoryRegion(addr_t guestVirt, size_t _len)
	: guestVirtualAddr(guestVirt), len(_len), isKernel(false)
{

	checkPageMultiple(len);
	size_t nPages = len / PAGE_SIZE;
	physicalPages.resize(nPages);
}

MemorySpace::MemorySpace(AbstractMemoryPool *_memoryPool)
	: memoryPool(_memoryPool)
{
	pageTableP = memoryPool->getPhysicalMemoryBlock(PAGE_SIZE);
	pageTableV = memoryPool->getHostVirtualFromPhysical(pageTableP);
	pageTablePages.emplace_back(pageTableP, pageTableV);
}

bool MemorySpace::fault(addr_t guestVirtualPage, uint32_t errorcode)
{
	std::lock_guard<std::recursive_mutex> guard(lock);

	auto region = regions.upper_bound(guestVirtualPage);
	if (region == regions.begin()) {
		console->warn("Unresolved page fault at 0x{:x}",
				guestVirtualPage);
		return false;
	}

	region--;
	
	std::lock_guard regionGuard = std::lock_guard((*region)->lock);

}


PageTableEntry *MemorySpace::getPTE(addr_t guestVirtual, bool create)
{
	std::lock_guard<std::recursive_mutex> guard(lock);

	auto *cur = castGuestPhysical<PageTableEntry>(guestVirtual);
	uint64_t nBits = 39;

	while ((1ULL << nBits) >= PAGE_SIZE) {
		uint64_t index = (guestVirtual >> nBits) & 0b111111111;

		if ((1ULL << nBits) == PAGE_SIZE)
			return &cur[index];

		nBits -= 9;
		PageTableEntry &entry = cur[index]; 
		if (!entry.present) {
			if (!create) return nullptr;

			/* allocate a new page */
			addr_t newPage =
				memoryPool->getPhysicalMemoryBlock(PAGETABLE_SIZE);
			entry = DEFAULT_PTE;
			entry.address = newPage / PAGETABLE_SIZE;
			cur = castGuestPhysical<PageTableEntry>(newPage);

			pageTablePages.emplace_back(newPage, cur);
		} else {
			cur = castGuestPhysical<PageTableEntry>(entry.address * PAGETABLE_SIZE);
		}
	}
	
	console->error("shouldn't have reached here!");
	std::abort();
}
