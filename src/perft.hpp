#ifndef PERFT_HPP
#define PERFT_HPP

#include "position.hpp"

void initPerft(Position position);
size_t perft(int depth, bool printCountAfterMoves);

#endif  // PERFT_HPP
