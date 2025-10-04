#ifndef MOVE_HPP
#define MOVE_HPP

#include <bit>
#include <cstdint>
#include <string>

#include "misc.hpp"

class Move {
   public:
	Move(void) = default;
	inline Move(Bitboard from, Bitboard to, int movingPt, int promoPt, bool isCastling, bool isEp);

	inline bool isNull(void) const;

	inline std::string toLan(void) const;
	inline Bitboard getFrom(void) const;
	inline Bitboard getTo(void) const;
	inline int getMovingPt(void) const;
	inline int getPromoPt(void) const;
	inline bool getIsCastling(void) const;
	inline bool getIsEp(void) const;

   private:
	uint32_t move = 0;
};

inline Move::Move(Bitboard from, Bitboard to, int movingPt, int promoPt, bool isCastling, bool isEp)
    : move(0) {
	move |= std::countr_zero(from) & 0x3F;
	move |= static_cast<uint32_t>(std::countr_zero(to) & 0x3F) << 6;
	move |= static_cast<uint32_t>(movingPt & 7) << 12;
	move |= static_cast<uint32_t>(promoPt & 7) << 15;
	move |= static_cast<uint32_t>(isCastling & 1) << 18;
	move |= static_cast<uint32_t>(isEp & 1) << 19;
}

inline std::string Move::toLan(void) const {
	if (!move) {
		return "0000";  // null move
	}

	int fromSq = move & 0x3F;
	int toSq = (move >> 6) & 0x3F;
	int promoPt = (move >> 15) & 0x7;

	char fromFile = char('a' + (fromSq & 7));
	char fromRank = char('1' + (fromSq >> 3));
	char toFile = char('a' + (toSq & 7));
	char toRank = char('1' + (toSq >> 3));

	std::string s;
	s.reserve(5);
	s.push_back(fromFile);
	s.push_back(fromRank);
	s.push_back(toFile);
	s.push_back(toRank);

	if (promoPt != PT_NULL) {
		static constexpr char promoChar[7] = {0, 'n', 'b', 'r', 'q', 0, 0};
		s.push_back(promoChar[promoPt]);
	}

	return s;
}

inline bool Move::isNull(void) const { return move == 0; }

inline Bitboard Move::getFrom(void) const { return 1ULL << (move & 0x3F); }
inline Bitboard Move::getTo(void) const { return 1ULL << ((move >> 6) & 0x3F); }
inline int Move::getMovingPt(void) const { return (move >> 12) & 7; }
inline int Move::getPromoPt(void) const { return (move >> 15) & 7; }
inline bool Move::getIsCastling(void) const { return (move >> 18) & 1; }
inline bool Move::getIsEp(void) const { return (move >> 19) & 1; }

#endif  // MOVE_HPP
