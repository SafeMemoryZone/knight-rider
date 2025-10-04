#ifndef UCI_HPP
#define UCI_HPP

#include <string>
#include <vector>

#include "search.hpp"
#include "tt.hpp"

class UciEngine {
   public:
	UciEngine(void) = default;

	void start(void);

   private:
	void preUciInit(void);

	void handleUciCmd(void);
	void handleDebugCmd(void);
	void handleIsReadyCmd(void);
	void handleUcinewgameCmd(void);
	void handlePositionCmd(void);
	void handleGoCmd(void);
	void handleStopCmd(void);
    void handleSetoptionCmd(void);

	// token buffers
	std::vector<std::string> tokens;
	std::vector<std::string> lowerTokens;

	// parsing state
	size_t tokenPos = 0;

	// engine state
	TranspositionTable tt;
	Position pos;
	MoveGenerator gen = MoveGenerator(&pos);  // move generator bound to pos (for verifying moves)
	bool isDebugMode = false;

	// search
	SearchManager searchManager;
};

#endif  // UCI_HPP
