#ifndef PERFT_HPP
#define PERFT_HPP

#include "position.hpp"

void initPerft(const Position &position);
size_t perft(int depth, bool printPerftLine);

#endif  // PERFT_HPP
