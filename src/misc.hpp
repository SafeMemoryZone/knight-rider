#ifndef TYPES_HPP
#define TYPES_HPP

#include <cstdint>
#include <iostream>
#include <limits>
#include <mutex>

// types
using Bitboard = uint64_t;
using Score = int32_t;

// constants
constexpr int WHITE = 0;
constexpr int BLACK = 1;

constexpr int PT_PAWN = 0;
constexpr int PT_KNIGHT = 1;
constexpr int PT_BISHOP = 2;
constexpr int PT_ROOK = 3;
constexpr int PT_QUEEN = 4;
constexpr int PT_KING = 5;
constexpr int PT_NULL = 6;

constexpr int WHITE_KING_SIDE_CASTLE = 1;
constexpr int WHITE_QUEEN_SIDE_CASTLE = 1 << 1;
constexpr int BLACK_KING_SIDE_CASTLE = 1 << 2;
constexpr int BLACK_QUEEN_SIDE_CASTLE = 1 << 3;

constexpr Score MATED_SCORE = -100'000'000;
constexpr Score INF = std::numeric_limits<int32_t>::max();

constexpr int MAX_PLY = 256;
constexpr int MAX_MOVES = 256;

// safe printing
extern std::mutex printMutex;

template <typename... Args>
inline void printSafe(Args&&... args) {
	std::lock_guard<std::mutex> guard(printMutex);
	(std::cout << ... << args) << std::endl;
}

#endif  // TYPES_HPP
