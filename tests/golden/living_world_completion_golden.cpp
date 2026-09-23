#include "RuntimeCompletionObserver.h"
#include <cassert>
#include <iostream>
int main()
{
    lw::RuntimeCompletionObserver observer;
    lw::RuntimeCompletion terminal{42,1,1006,true};
    assert(observer.Notify(terminal)); // Standalone LW has no required consumer.
    unsigned calls=0; bool durable=false;
    observer.Set([&](auto const& outcome) {
        ++calls; assert(outcome.runtimeId==42 && outcome.invasionId==1 && outcome.success);
        return durable;
    });
    assert(!observer.Notify(terminal)); // Caller must retain runtime on failure.
    durable=true; assert(observer.Notify(terminal)); assert(calls==2);
    observer.Set({}); assert(observer.Notify(terminal));
    std::cout << "[FURY][PASS] LW terminal delivery requires durable acknowledgement; standalone/reset unaffected\n";
}
