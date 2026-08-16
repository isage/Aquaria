#include "MT.h"
#include "Base.h"


// --------- Lockable ----------

Lockable::Lockable()
: _mtx(NULL)
{
	_mtx = SDL_CreateMutex();
}

Lockable::~Lockable()
{
	SDL_DestroyMutex((SDL_Mutex*)_mtx);
}

void Lockable::lock()
{
	SDL_LockMutex((SDL_Mutex*)_mtx);
}

void Lockable::unlock()
{
	SDL_UnlockMutex((SDL_Mutex*)_mtx);
}

// --------- Waitable ----------

Waitable::Waitable()
: _cond(NULL)
{
	_cond = SDL_CreateCondition();
}

Waitable::~Waitable()
{
	SDL_DestroyCondition((SDL_Condition*)_cond);
}

void Waitable::wait()
{
	SDL_WaitCondition((SDL_Condition*)_cond, (SDL_Mutex*)mutex());
}

void Waitable::signal()
{
	SDL_SignalCondition((SDL_Condition*)_cond);
}

void Waitable::broadcast()
{
	SDL_BroadcastCondition((SDL_Condition*)_cond);
}

