#ifndef UCI_HPP
#define UCI_HPP

#include <string>
#include <vector>

#include "search.hpp"

class UciEngine {
   public:
	UciEngine(void) = default;

	void start(void);

   private:
	void handleUciCmd(void);
	void handleDebugCmd(void);
	void handleIsReadyCmd(void);
	void handleUcinewgameCmd(void);
	void handlePositionCmd(void);
	void handleGoCmd(void);
    void handleStopCmd(void);

	// token buffers
	std::vector<std::string> tokens;
	std::vector<std::string> lowerTokens;

	// parsing state
	size_t tokenPos = 0;

	// engine state
	Position pos;
	MoveGenerator gen = MoveGenerator(&pos);  // move generator bound to pos
	bool isDebugMode = false;

	// search
	SearchManager searchManager;
};

#endif  // UCI_HPP
