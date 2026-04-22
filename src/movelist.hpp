#ifndef MOVELIST_HPP
#define MOVELIST_HPP

#include <cstddef>

#include "misc.hpp"
#include "move.hpp"

// container for list of moves
class MoveList {
   public:
	MoveList(void) = default;

	// capacity / state
	inline size_t size(void) const noexcept { return count; }
	inline void clear(void) noexcept {
		count = 0;
		isInCheck = false;
	}

	// modifiers
	inline void push_back(Move move) noexcept { moves[count++] = move; }
	inline void setInCheck(bool val) noexcept { isInCheck = val; }

	// accessors
	inline bool inCheck(void) const noexcept { return isInCheck; }
	inline Move& operator[](size_t i) noexcept { return moves[i]; }
	inline const Move& operator[](size_t i) const noexcept { return moves[i]; }

	// iterators
	Move* begin(void) noexcept { return moves; }
	Move* end(void) noexcept { return moves + count; }
	const Move* begin(void) const noexcept { return moves; }
	const Move* end(void) const noexcept { return moves + count; }

   private:
	Move moves[MAX_MOVES];
	size_t count = 0;
	bool isInCheck = false;
};

#endif  // MOVELIST_HPP
