#include "eval.hpp"

#include <bit>
#include <cstdint>

#include "position.hpp"

static constexpr Score PAWN_VAL = 100;
static constexpr Score KNIGHT_VAL = 320;
static constexpr Score BISHOP_VAL = 330;
static constexpr Score ROOK_VAL = 500;
static constexpr Score QUEEN_VAL = 900;

static_assert(WHITE == 0 && BLACK == 1);
static_assert(PT_PAWN == 0 && PT_KNIGHT == 1 && PT_BISHOP == 2 && PT_ROOK == 3 && PT_QUEEN == 4 &&
              PT_KING == 5 && PT_NULL == 6);

// precomputed rank mirror for LERF
alignas(64) static constexpr uint8_t MIRROR[64] = {
    // sq ^ 56 precomputed
    56, 57, 58, 59, 60, 61, 62, 63, 48, 49, 50, 51, 52, 53, 54, 55, 40, 41, 42, 43, 44, 45,
    46, 47, 32, 33, 34, 35, 36, 37, 38, 39, 24, 25, 26, 27, 28, 29, 30, 31, 16, 17, 18, 19,
    20, 21, 22, 23, 8,  9,  10, 11, 12, 13, 14, 15, 0,  1,  2,  3,  4,  5,  6,  7};

// PST for white only
alignas(64) static constexpr int16_t PST[6][64] = {
    // 0: pawn
    {
        0,  0,  0,  0,  0,  0,  0,  0,  5,  10, 10, -20, -20, 10, 10, 5,  5, -5, -10, 0,  0,  -10,
        -5, 5,  0,  0,  0,  20, 20, 0,  0,  0,  5,  5,   10,  25, 25, 10, 5, 5,  10,  10, 20, 30,
        30, 20, 10, 10, 50, 50, 50, 50, 50, 50, 50, 50,  0,   0,  0,  0,  0, 0,  0,   0,
    },
    // 1: knight
    {
        -50, -40, -30, -30, -30, -30, -40, -50, -40, -20, 0,   5,   5,   0,   -20, -40,
        -30, 5,   10,  15,  15,  10,  5,   -30, -30, 0,   15,  20,  20,  15,  0,   -30,
        -30, 5,   15,  20,  20,  15,  5,   -30, -30, 0,   10,  15,  15,  10,  0,   -30,
        -40, -20, 0,   0,   0,   0,   -20, -40, -50, -40, -30, -30, -30, -30, -40, -50,
    },
    // 2: bishop
    {
        -20, -10, -10, -10, -10, -10, -10, -20, -10, 5,   0,   0,   0,   0,   5,   -10,
        -10, 10,  10,  10,  10,  10,  10,  -10, -10, 0,   10,  10,  10,  10,  0,   -10,
        -10, 5,   5,   10,  10,  5,   5,   -10, -10, 0,   5,   10,  10,  5,   0,   -10,
        -10, 0,   0,   0,   0,   0,   0,   -10, -20, -10, -10, -10, -10, -10, -10, -20,

    },
    // 3: rook
    {
        0, 0,  0,  5,  5, 0,  0,  0,  -5, 0,  0,  0, 0, 0, 0, -5, -5, 0,  0,  0, 0, 0,
        0, -5, -5, 0,  0, 0,  0,  0,  0,  -5, -5, 0, 0, 0, 0, 0,  0,  -5, -5, 0, 0, 0,
        0, 0,  0,  -5, 5, 10, 10, 10, 10, 10, 10, 5, 0, 0, 0, 0,  0,  0,  0,  0,
    },
    // 4: queen
    {
        -20, -10, -10, -5, -5, -10, -10, -20, -10, 0,   5,   0,  0,  0,   0,   -10,
        -10, 5,   5,   5,  5,  5,   0,   -10, 0,   0,   5,   5,  5,  5,   0,   -5,
        -5,  0,   5,   5,  5,  5,   0,   -5,  -10, 0,   5,   5,  5,  5,   0,   -10,
        -10, 0,   0,   0,  0,  0,   0,   -10, -20, -10, -10, -5, -5, -10, -10, -20,
    },
    // 5: king (middlegame)
    {
        20,  30,  10,  0,   0,   10,  30,  20,  20,  20,  0,   0,   0,   0,   20,  20,
        -10, -20, -20, -20, -20, -20, -20, -10, -20, -30, -30, -40, -40, -30, -30, -20,
        -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30,
        -30, -40, -40, -50, -50, -40, -40, -30, -30, -40, -40, -50, -50, -40, -40, -30,
    },
};

static inline Score materialScore(const Position& pos) {
	const Bitboard* p = pos.pieces;
	Score w = 0, b = 0;

	w += std::popcount(p[PT_PAWN + WHITE * 6]) * PAWN_VAL;
	w += std::popcount(p[PT_KNIGHT + WHITE * 6]) * KNIGHT_VAL;
	w += std::popcount(p[PT_BISHOP + WHITE * 6]) * BISHOP_VAL;
	w += std::popcount(p[PT_ROOK + WHITE * 6]) * ROOK_VAL;
	w += std::popcount(p[PT_QUEEN + WHITE * 6]) * QUEEN_VAL;

	b += std::popcount(p[PT_PAWN + BLACK * 6]) * PAWN_VAL;
	b += std::popcount(p[PT_KNIGHT + BLACK * 6]) * KNIGHT_VAL;
	b += std::popcount(p[PT_BISHOP + BLACK * 6]) * BISHOP_VAL;
	b += std::popcount(p[PT_ROOK + BLACK * 6]) * ROOK_VAL;
	b += std::popcount(p[PT_QUEEN + BLACK * 6]) * QUEEN_VAL;

	return w - b;  // white - black
}

static inline Score pstScore(const Position& pos) {
	Score score = 0;
	const Bitboard* p = pos.pieces;

	for (int pt = 0; pt < 6; ++pt) {
		const int16_t* pst = PST[pt];

		// white
		Bitboard bbW = p[pt + WHITE * 6];
		while (bbW) {
			int sq = std::countr_zero(bbW);
			score += pst[sq];
			bbW &= bbW - 1;
		}

		Bitboard bbB = p[pt + BLACK * 6];
		while (bbB) {
			int sq = std::countr_zero(bbB);
			score -= pst[MIRROR[sq]];
			bbB &= bbB - 1;
		}
	}
	return score;  // white - black
}

Score eval(const Position& position) {
	Score score = 0;

	score += materialScore(position);
	score += pstScore(position);

	if (position.usColor == BLACK) score = -score;

	score += 10;  // tempo

	return score;
}
