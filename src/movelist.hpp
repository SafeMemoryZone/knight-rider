#ifndef MOVELIST_HPP
#define MOVELIST_HPP

#include <cstddef>

#include "misc.hpp"
#include "move.hpp"

class MoveList {
   public:
	MoveList(void) = default;

	inline size_t size() const noexcept { return count; }
	inline void push_back(Move move) noexcept { moves[count++] = move; }

	inline void setInCheck(bool val) noexcept { isInCheck = val; }
	inline bool inCheck() const noexcept { return isInCheck; }

	Move* begin() noexcept { return moves; }
	Move* end() noexcept { return moves + count; }
	const Move* begin() const noexcept { return moves; }
	const Move* end() const noexcept { return moves + count; }
	Move& operator[](size_t i) noexcept { return moves[i]; }
	const Move& operator[](size_t i) const noexcept { return moves[i]; }

   private:
	Move moves[MAX_MOVES];
	size_t count = 0;
	bool isInCheck = false;
};

#endif  // MOVELIST_HPP
