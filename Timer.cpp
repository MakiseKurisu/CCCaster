#include "Event.h"
#include "Timer.h"

using namespace std;

Timer::Timer ( Owner& owner ) : owner ( owner ), delay ( 0 ), expiry ( 0 )
{
}

Timer::~Timer()
{
    EventManager::get().removeTimer ( this );
}

void Timer::start ( double delay )
{
    if ( delay <= 0 )
        return;

    this->delay = delay;

    EventManager::get().addTimer ( this );
}