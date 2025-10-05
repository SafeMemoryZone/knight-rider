#include "tt.hpp"

#include <algorithm>
#include <cstring>
#include <limits>

TTEntry TTEntry::makeEmptyEntry(void) {
	TTEntry entry;
	entry.bestMove = Move();
	entry.value = 0;
	entry.age = std::numeric_limits<uint16_t>::max();
	entry.keyTag = std::numeric_limits<uint16_t>::max();
	entry.depth = -1;
	entry.flag = static_cast<uint8_t>(TT_UPPER);
	return entry;
}

TranspositionTable::~TranspositionTable(void) {
	if (table) {
		delete[] table;
	}
}

void TranspositionTable::clear(void) {
	if (table && capacity != 0) {
		const TTEntry empty = TTEntry::makeEmptyEntry();
		std::fill(table, table + capacity, empty);
	}
}

void TranspositionTable::newSearch(void) { age++; }

void TranspositionTable::resize(size_t mb) {
	size_t bytes = mb * 1024ULL * 1024ULL;
	capacity = bytes / sizeof(TTEntry);
	capacity = std::max<size_t>(CLUSTER_SIZE * 1024, (capacity / CLUSTER_SIZE) * CLUSTER_SIZE);

	if (table) {
		delete[] table;
	}

	table = new TTEntry[capacity];
	clear();
	age = 0;
}

bool TranspositionTable::probe(uint64_t key, TTEntry &out) const {
	if (!table || capacity == 0) {
		return false;
	}
	const size_t base = getClusterBase(key);
	const uint16_t tag = getKeyTag(key);

	for (unsigned i = 0; i < CLUSTER_SIZE; i++) {
		const TTEntry &entry = table[base + i];
		if (entry.depth >= 0 && entry.keyTag == tag) {
			out = entry;
			return true;
		}
	}
	return false;
}

void TranspositionTable::store(uint64_t key, int depth, Score value, TTFlag flag, Move bestMove) {
	if (!table || capacity == 0) return;

	const uint16_t tag = getKeyTag(key);
	const size_t base = getClusterBase(key);

	int emptyIdx = -1;
	int sameIdx = -1;

	for (int i = 0; i < CLUSTER_SIZE; i++) {
		TTEntry &entry = table[base + static_cast<unsigned>(i)];
		if (entry.depth < 0 && emptyIdx < 0) {  // pick first empty
			emptyIdx = i;
		}
		if (entry.keyTag == tag) {  // same position tag
			sameIdx = i;
			break;
		}
	}

	int victimIdx = -1;
	auto flagPriority = [](TTFlag entryFlag) {
		switch (entryFlag) {
			case TT_EXACT:
				return 2;
			case TT_LOWER:
				return 1;
			case TT_UPPER:
			default:
				return 0;
		}
	};
	if (sameIdx >= 0) {
		TTEntry &existing = table[base + static_cast<unsigned>(sameIdx)];
		const int existingDepth = static_cast<int>(existing.depth);
		const bool betterFlag =
		    flagPriority(flag) > flagPriority(static_cast<TTFlag>(existing.flag));

		if (!betterFlag && depth < existingDepth) {
			return;  // keep deeper entry
		}

		victimIdx = sameIdx;
	}
	else if (emptyIdx >= 0) {
		victimIdx = emptyIdx;
	}
	else {
		int bestScoreIdx = 0;
		int bestScore = -1;  // higher = more replaceable
		for (int i = 0; i < CLUSTER_SIZE; i++) {
			TTEntry &e = table[base + static_cast<unsigned>(i)];
			const int depthTerm = (127 - static_cast<int>(e.depth)) * 256;
			const int ageTerm = static_cast<int>(static_cast<uint16_t>(age - e.age));
			const int repScore = depthTerm + ageTerm;

			if (repScore > bestScore) {
				bestScore = repScore;
				bestScoreIdx = i;
			}
		}
		victimIdx = bestScoreIdx;
	}

	TTEntry &v = table[base + static_cast<unsigned>(victimIdx)];
	v.bestMove = bestMove;
	v.value = value;
	v.age = age;
	v.keyTag = tag;
	v.depth = static_cast<int8_t>(depth);
	v.flag = static_cast<uint8_t>(flag);
}
