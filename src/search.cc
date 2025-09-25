#include "search.hpp"

#include <cassert>

#include "eval.hpp"

void SearchEngine::search(const Position &searchPosition, const GoLimits &goLimits) {
	position = searchPosition;
	position.resetPly();  // no matter the state, start from ply 0
	nodesRemaining = goLimits.nodeLimit;
	bestMove = Move();

	MoveList legalMoves =
	    goLimits.searchMoves.size() > 0 ? goLimits.searchMoves : moveGenerator.generateLegalMoves();

	// no legal move
	if (legalMoves.size() == 0) {
		return;
	}

	int depthLimit = goLimits.depthLimit > 0 ? std::min(MAX_PLY, goLimits.depthLimit) : MAX_PLY;

	if (goLimits.proveMateInN > 0) {
		depthLimit = std::min(depthLimit, goLimits.proveMateInN * 2);
	}

	for (int depth = 1; depth <= depthLimit; depth++) {
		Score bestSearchScore = -INF;
		Score alpha = -INF;
		Score beta = INF;

		std::vector<std::pair<Move, Score>> rootScores;
		rootScores.reserve(legalMoves.size());

		for (Move move : legalMoves) {
			if (requestedStop.load(std::memory_order_relaxed)) {
				return;
			}

			position.makeMove(move);
			Score childScore = goLimits.nodeLimit > 0
			                       ? -coreSearch<true>(depth - 1, -beta, -alpha)
			                       : -coreSearch<false>(depth - 1, -beta, -alpha);
			position.undoMove();

			if (childScore == -INF) {
				goto search_end;  // if he haven't searched all children we can't know the true
				                  // value of this node
			}

			if (childScore > bestSearchScore) {
				bestSearchScore = childScore;
				bestMove = move;
			}

			alpha = std::max<Score>(alpha, childScore);
			rootScores.emplace_back(move, childScore);
		}

		// order moves so they can be explored in a better order in the next iteration
		assert(rootScores.size() == legalMoves.size());

		std::stable_sort(rootScores.begin(), rootScores.end(),
		                 [](auto &a, auto &b) { return a.second > b.second; });
		for (size_t i = 0; i < rootScores.size(); ++i) {
			legalMoves[i] = rootScores[i].first;
		}
	}

search_end:
	return;
}

template <bool HasNodeLimit>
Score SearchEngine::coreSearch(int depth, Score alpha, Score beta) {
	// stop on node limit
	if constexpr (HasNodeLimit) {
		if (--nodesRemaining < 0) {
			return INF;
		}
	}

	// stop on time
	if (requestedStop.load(std::memory_order_relaxed)) {
		return INF;
	}

	MoveList legalmoves = moveGenerator.generateLegalMoves();
	const bool isCheckmateOrStalemate = legalmoves.size() == 0;

	// stop on depth
	// TODO: stop on other terminal conditions besides checkmate and stalemate
	if (depth == 0) {
		return isCheckmateOrStalemate ? (legalmoves.inCheck() ? MATED_SCORE : 0) : eval(position);
	}

	// stop on checkmate or stalemate
	if (isCheckmateOrStalemate) {
		return legalmoves.inCheck() ? MATED_SCORE : 0;
	}

	Score bestScore = -INF;

	for (Move move : legalmoves) {
		position.makeMove(move);
		Score childScore = -coreSearch<HasNodeLimit>(depth - 1, -beta, -alpha);
		position.undoMove();

		if (childScore == -INF) {
			return INF;  // if he haven't searched all children we can't know the true value of this
			             // node
		}

		bestScore = std::max(bestScore, childScore);

		alpha = std::max<Score>(alpha, childScore);
		if (alpha >= beta) {
			break;
		}
	}

	return bestScore;
}

Move SearchEngine::fetchBestMove(void) const { return bestMove; }

SearchManager::~SearchManager(void) { stopSearch(); }

void SearchManager::timeControlManager(
    const GoLimits &goLimits, std::chrono::time_point<std::chrono::steady_clock> commandReceiveTP,
    int engineColor) {
	if (goLimits.infinite || goLimits.ponder || goLimits.proveMateInN > 0) {
		return;  // no timer needed
	}

	auto compute_time_budget_ms = [&](void) -> int64_t {
		if (goLimits.moveTimeMS > 0) {
			return static_cast<int64_t>(goLimits.moveTimeMS);  // fixed time budget
		}

		int64_t myTime = goLimits.timeLeftMS[engineColor];
		int64_t myInc = goLimits.incMS[engineColor];

		const double incUse = 0.6;
		const double maxBudgetFrac = 0.20;
		const int64_t minBudget = 20;

		int64_t budget;
		if (goLimits.movesToGo > 0) {
			budget = static_cast<int64_t>(static_cast<double>(myTime) /
			                              static_cast<double>(goLimits.movesToGo)) +
			         static_cast<int64_t>(incUse * static_cast<double>(myInc));
		}
		else {
			budget = static_cast<int64_t>(0.03 * static_cast<double>(myTime) +
			                              incUse * static_cast<double>(myInc));
		}
		budget = std::min<int64_t>(
		    budget, static_cast<int64_t>(maxBudgetFrac * static_cast<double>(myTime)));
		return std::max<int64_t>(budget, minBudget);
	};

	int64_t budgetMS = compute_time_budget_ms();

	const std::chrono::milliseconds safetyReserve(30);  // always keep this in the bank
	const std::chrono::milliseconds stopSlack(5);       // stop a bit sooner

	// don't exceed budget and keep a reserve
	auto effective = std::chrono::milliseconds(std::max<int64_t>(10, budgetMS)) - safetyReserve;
	if (effective <= std::chrono::milliseconds(10)) {
		effective = std::chrono::milliseconds(10);
	}

	auto deadline = commandReceiveTP + effective - stopSlack;

	if (std::chrono::steady_clock::now() < deadline) {
		std::unique_lock<std::mutex> lock(timeControlMtx);
		timeControlCV.wait_until(lock, deadline, [this] { return timeControlWakeRequested; });
	}

	searchEngine.requestedStop.store(true, std::memory_order_relaxed);
}

void SearchManager::runSearch(const Position &position, const GoLimits &goLimits,
                              std::chrono::time_point<std::chrono::steady_clock> commandReceiveTP) {
	stopSearch();  // stop any ongoing search
	{
		std::lock_guard<std::mutex> lock(timeControlMtx);
		timeControlWakeRequested = false;
	}
	searchEngine.requestedStop.store(false, std::memory_order_relaxed);

	// start search
	timerThread = std::thread(&SearchManager::timeControlManager, this, goLimits, commandReceiveTP,
	                          position.usColor);
	searchThread = std::thread(&SearchEngine::search, &searchEngine, position, goLimits);
}

Move SearchManager::stopSearch(void) {
	if (timerThread.joinable()) {
		{
			std::lock_guard<std::mutex> lock(timeControlMtx);
			timeControlWakeRequested = true;
		}
		timeControlCV.notify_all();
		timerThread.join();
	}
	if (searchThread.joinable()) {
		searchEngine.requestedStop.store(true, std::memory_order_relaxed);
		searchThread.join();
		return searchEngine.fetchBestMove();
	}

	return Move();
}
