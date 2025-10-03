#ifndef PERFT_HPP
#define PERFT_HPP

#include <cstddef>

#include "position.hpp"

size_t perft(const Position &pos, int depth, bool printPerftLine);

#endif  // PERFT_HPP
