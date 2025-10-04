#ifndef TT_HPP
#define TT_HPP

#include <cstdint>

#include "misc.hpp"
#include "move.hpp"

enum TTFlag : uint8_t { TT_EXACT, TT_LOWER, TT_UPPER };

struct TTEntry {
	Move bestMove;    // best move from this position
	Score value;      // value of the node that depends on TTFlag
	uint16_t age;     // age to replace old entries with newer ones
	uint16_t keyTag;  // higher 16 bits of the zobrist key
	int8_t depth;     // how much 'deeper' we searched to compute the value
	uint8_t flag;     // exact value or alpha/beta cutoff
};

class TranspositionTable {
   public:
	TranspositionTable(void) = default;
    ~TranspositionTable(void);

	void newSearch(void);
	void resize(size_t mb);
	bool probe(uint64_t key, TTEntry& out) const;
	void store(uint64_t key, int depth, Score value, TTFlag flag, Move bestMove);

   private:
	inline size_t getClusterBase(uint64_t key) const {
		const size_t numClusters = capacity / CLUSTER_SIZE;
		const size_t clusterIdx = (size_t)(key % numClusters);
		return clusterIdx * CLUSTER_SIZE;
	}

	static inline uint16_t getKeyTag(uint64_t key) { return key >> 48; }

	static constexpr int CLUSTER_SIZE = 4;

	TTEntry* table = nullptr;
	size_t capacity = 0;
	uint16_t age = 0;
};

#endif  // TT_HPP
