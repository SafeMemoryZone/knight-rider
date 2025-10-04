#ifndef ZOBRIST_HPP
#define ZOBRIST_HPP

#include <cstdint>

extern uint64_t Z_PSQ[12][64];
extern uint64_t Z_CASTLING[16];
extern uint64_t Z_EP_FILE[8];
extern uint64_t Z_BLACK_TO_MOVE;

void initZobristTables(uint64_t seed = 0x9E3779B97F4A7C15ULL);

#endif  // ZOBRIST_HPP
