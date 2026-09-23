#include "content/defias/DefiasPresence.h"
#include <cassert>
#include <chrono>
#include <iostream>
int main()
{
    using namespace Fury::Defias;
    PresenceTracker tracker; Fury::FuryEvent event; event.type="player.login";
    auto start=std::chrono::steady_clock::time_point{};
    tracker.Observe(1,event,true,start);
    event.type="player.zone.changed";
    tracker.Observe(1,event,true,start+std::chrono::seconds(110));
    assert(tracker.Entries().at(1).since==start);
    // Disconnect/reconnect between ticks must not inherit the old timer.
    event.type="player.login";
    auto login=start+std::chrono::seconds(119);
    tracker.Observe(1,event,true,login);
    assert(tracker.Entries().at(1).since==login);
    event.type="player.logout";
    tracker.Observe(1,event,true,login);
    assert(tracker.Entries().empty());
    event.type="player.login"; tracker.Observe(1,event,true,login);
    event.type="player.zone.changed"; tracker.Observe(1,event,false,login);
    assert(tracker.Entries().empty());
    std::cout << "[FURY][PASS] production presence tracker resets on new sessions, logout, ineligibility\n";
}
