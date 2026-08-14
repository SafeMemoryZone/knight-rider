#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest/doctest.h>

#include <cstddef>
#include <mutex>
#include <string>

#include "bitboards.hpp"
#include "misc.hpp"
#include "perft.hpp"
#include "position.hpp"
#include "zobrist.hpp"

std::mutex printMutex;

struct PerftCase {
	std::string fen;
	int depth;
	size_t nodes;
};

TEST_CASE("deep perft" * doctest::skip()) {
    static const PerftCase PERFT_CASES[] = {
        {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 7, 3'195'901'860},
        {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 6, 8'031'647'685},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 8, 3'009'794'393},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 6, 706'045'033},
        {"r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1", 6, 706'045'033},
        {"rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", 5, 89'941'194},
        {"r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", 6, 6'923'051'137}
    };

    for (const PerftCase &perftCase : PERFT_CASES) {
        std::string name = std::to_string(perftCase.depth) + " " + perftCase.fen;

        SUBCASE(name.c_str()) {
            bool success = false;
            Position pos = Position::fromFen(perftCase.fen, success);
            REQUIRE(success);
            CHECK(perft(pos, perftCase.depth, false) == perftCase.nodes);
        }
    }
}

int main(int argc, char **argv) {
    initBitboards();
    initZobristTables();

    return doctest::Context(argc, argv).run();
}
