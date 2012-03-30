#include "leagueOverSeer.h"
#include "PlayerInfo.h"
#include "TimeKeeper.h"
#include <list>

class RejoinDB : public leagueOverSeer {
  public:
    RejoinDB ();
    ~RejoinDB ();

    bool add (int playerIndex);
    void del (void);
    bool inListAlready( const char* ip);
    char* getCallsignByIP(const char* ip);

  private:
    std::list<struct RejoinEntry*> queue;
};

