#include "search.hpp"

#include <algorithm>
#include <cassert>
#include <vector>

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

	for (int depth = 1; depth <= depthLimit; ++depth) {
		Score iterBestScore = -INF;
		Move iterBestMove;

		std::vector<std::pair<Move, Score>> rootScores;
		rootScores.reserve(legalMoves.size());

		bool aborted = false;

		for (const Move &move : legalMoves) {
			if (requestedStop.load(std::memory_order_relaxed)) {
				break;
			}

			position.makeMove(move);
			bool childAborted = false;
			Score childScore = goLimits.nodeLimit > 0
			                       ? -coreSearch<true>(depth - 1, -INF, INF, childAborted)
			                       : -coreSearch<false>(depth - 1, -INF, INF, childAborted);
			position.undoMove();

			if (childAborted) {
				aborted = true;
				break;
			}

			if (childScore > iterBestScore) {
				iterBestScore = childScore;
				iterBestMove = move;
			}

			rootScores.emplace_back(move, childScore);
		}

		// safe to set because we always explore the best move (from previous iteration) first
		if (!iterBestMove.isNull()) {
			bestMove = iterBestMove;
		}

		if (aborted) {
			break;
		}

		std::stable_sort(rootScores.begin(), rootScores.end(),
		                 [](auto &a, auto &b) { return a.second > b.second; });
		for (size_t i = 0; i < rootScores.size(); ++i) {
			legalMoves[i] = rootScores[i].first;
		}
	}
}

template <bool HasNodeLimit>
Score SearchEngine::coreSearch(int depth, Score alpha, Score beta, bool &searchCancelledOut) {
	searchCancelledOut = false;

	if constexpr (HasNodeLimit) {
		if (--nodesRemaining < 0) {
			searchCancelledOut = true;
			return alpha;
		}
	}
	if (requestedStop.load(std::memory_order_relaxed)) {
		searchCancelledOut = true;
		return alpha;
	}

	// quick bailout for terminal nodes
	MoveList legalMoves = moveGenerator.generateLegalMoves();
	if (legalMoves.size() == 0) {
		return legalMoves.inCheck() ? MATED_SCORE + position.ply : 0;
	}

	if (depth == 0) {
		return eval(position);
	}

	Score bestScore = -INF;

	for (const Move &move : legalMoves) {
		position.makeMove(move);
		bool childCancelled = false;
		Score childScore = -coreSearch<HasNodeLimit>(depth - 1, -beta, -alpha, childCancelled);
		position.undoMove();

		if (childCancelled) {
			searchCancelledOut = true;
			return alpha;
		}

		bestScore = std::max(bestScore, childScore);
		alpha = std::max(alpha, childScore);
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
	const bool has_time_controls = goLimits.moveTimeMS > 0 || goLimits.timeLeftMS[0] > 0 ||
	                               goLimits.timeLeftMS[1] > 0 || goLimits.incMS[0] > 0 ||
	                               goLimits.incMS[1] > 0;

	if (!has_time_controls || goLimits.infinite || goLimits.ponder || goLimits.proveMateInN > 0)
		return;

	const double incUse = 0.65;         // fraction of increment to spend per move
	const double maxBudgetFrac = 0.25;  // cap per-move spend as fraction of remaining time
	const int64_t minBudget = 200;
	const std::chrono::milliseconds safetyReserve(80);  // always keep safety reserve
	const std::chrono::milliseconds stopSlack(10);

	auto compute_time_budget_ms = [&]() -> int64_t {
		if (goLimits.moveTimeMS > 0) {
			return (int64_t)goLimits.moveTimeMS;  // fixed movetime from UCI
		}

		int64_t myTime = goLimits.timeLeftMS[engineColor];
		int64_t myInc = goLimits.incMS[engineColor];

		int64_t budget;
		if (goLimits.movesToGo > 0) {
			// spread remaining time across remaining moves + part of increment
			budget = (myTime / goLimits.movesToGo) + (int64_t)(incUse * (double)myInc);
		}
		else {
			budget = (int64_t)(0.03 * (double)myTime + incUse * (double)myInc);
		}

		budget = std::min<int64_t>(budget, (int64_t)(maxBudgetFrac * (double)myTime));
		budget = std::max<int64_t>(budget, minBudget);
		return budget;
	};

	int64_t budgetMS = compute_time_budget_ms();

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
                              std::chrono::time_point<std::chrono::steady_clock> commandReceiveTP,
                              std::function<void(Move)> onFinishCallback) {
	stopSearch();  // stop any ongoing search
	{
		std::lock_guard<std::mutex> lock(timeControlMtx);
		timeControlWakeRequested = false;
	}
	searchEngine.requestedStop.store(false, std::memory_order_relaxed);

	// start search
	timerThread = std::thread(&SearchManager::timeControlManager, this, goLimits, commandReceiveTP,
	                          position.usColor);
	searchThread = std::thread([this, position, goLimits, onFinishCallback]() {
		searchEngine.search(position, goLimits);
		onFinishCallback(searchEngine.fetchBestMove());
	});
}

void SearchManager::stopSearch(void) {
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
	}
}

void SearchManager::blockUntilDone(void) {
	if (searchThread.joinable()) {
		searchThread.join();
		stopSearch();  // stop the timer and clean any state
	}
}
