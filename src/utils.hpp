#ifndef UTILS_HPP
#define UTILS_HPP

#include <utility>
#include <type_traits>

template <typename BlockType>
struct MemoryBlockComparator {
	using is_transparent = void;
	using KeyType =
		typename std::result_of<decltype(&BlockType::getKey)(BlockType)>::type;	

	bool operator()(const BlockType &lhs, const BlockType &rhs) const
	{ return lhs.getKey() < rhs.getKey(); }

	bool operator()(const KeyType &physical, const BlockType &blk) const
	{ return physical < blk.getKey(); }

	bool operator()(const BlockType &blk, const KeyType &physical) const
	{ return blk.getKey() < physical; }
};

template <typename PointerType>
class ComparablePointerAdapter {
private:
	PointerType ptr;
public:
	using ValueType = decltype(*std::declval<PointerType>());

	ComparablePointerAdapter() = delete;

	ComparablePointerAdapter(const PointerType &otherPtr)
	{ ptr = otherPtr; }

	ComparablePointerAdapter(PointerType &&otherPtr)
	{ ptr = std::move(otherPtr); }

	auto getKey() const
	{ return ptr->getKey(); }

	ValueType &operator*()
	{ return *ptr; }

	PointerType operator->() const
	{ return ptr; }
};

#endif
