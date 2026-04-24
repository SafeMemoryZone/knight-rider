#ifndef SEARCH_HPP
#define SEARCH_HPP

#include <atomic>
#include <cstdint>
#include <thread>

#include "movegen.hpp"
#include "movelist.hpp"
#include "position.hpp"
#include "tt.hpp"

// search limits & options
struct GoLimits {
	int64_t timeLeftMS[2];
	int64_t nodeLimit;
	int incMS[2], movesToGo, depthLimit, proveMateInN, moveTimeMS;
	bool infinite, ponder;
	MoveList searchMoves;
};

// single-threaded search engine
class Engine {
   public:
	void startSearch(const Position &pos, TranspositionTable *tt, const GoLimits &limits,
	                 std::chrono::time_point<std::chrono::steady_clock> commandReceiveTime);
	void stopSearch();
	Move fetchBestMove();  // blocks and returns resulting best move

   private:
	void rootNegamax(const GoLimits &limits);
	Score negamax(int depth, Score alpha, Score beta, bool &searchCancelledOut);
	Score quiescence(Score alpha, Score beta, bool &searchCancelledOut);

	Move bestMove;

	// current search state
	Position searchPos;            // WARN: will be modified during search
	TranspositionTable *searchTt;  // NOTE: lifetime managed exteranlly by UCI engine
	MoveGenerator gen = MoveGenerator(&searchPos);
	uint64_t maxNodes;       // set if nodeLimit is set in GoLimits, otherwise UINT64_MAX
	uint64_t nodesSearched;  // how many nodes were explored until now

	// thread & time management
	std::chrono::time_point<std::chrono::steady_clock> deadline;
	std::thread searchThread;
	std::atomic<bool> searchStopRequested;
	bool hasDeadline;
};

#endif  // SEARCH_HPP
