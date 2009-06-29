#include "ThreadPool.h"
#include "System/Log.h"
#include "System/Events/Callable.h"

void* ThreadPool::thread_function(void* param)
{
	ThreadPool* tp = (ThreadPool*) param;
	
	tp->ThreadFunction();
	
	return NULL;
}

void ThreadPool::ThreadFunction()
{	
	Callable* callable;
	Callable** it;
	
	while (running)
	{
		Log_Trace("1");
		pthread_mutex_lock(&mutex);
		Log_Trace("2");
		numActive--;
		
	wait:
		while (numPending == 0 && running)
		{
			Log_Trace("3");		
			pthread_cond_wait(&cond, &mutex);
			Log_Trace("4");
		}
		
		if (!running)
		{
			Log_Trace("5");
			pthread_mutex_unlock(&mutex);
			Log_Trace("6");
			break;
		}
		Log_Trace("7");
		it = callables.Head();
		if (!it)
			goto wait;
		Log_Trace("8");
		callable = *it;
		callables.Remove(it);
		numPending--;
		numActive++;
		
		pthread_mutex_unlock(&mutex);
		
		Call(callable);
	}	
}

ThreadPool::ThreadPool(int numThread) :
numThread(numThread)
{
	numPending = 0;
	numActive = 0;
	running = false;
	
	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);
	
	threads = new pthread_t[numThread];
}

ThreadPool::~ThreadPool()
{
	int i;
	
	if (running)
	{
		running = false;
		for (i = 0; i < numThread; i++)
		{
			pthread_mutex_lock(&mutex);
			pthread_cond_broadcast(&cond);
			pthread_mutex_unlock(&mutex);
		}
		
		for (i = 0; i < numThread; i++)
		{
			pthread_join(threads[i], NULL);
		}
	}
	
	delete[] threads;
}

void ThreadPool::Start()
{
	int i;
	
	running = true;
	numActive = numThread;
	
	for (i = 0; i < numThread; i++)
	{
		pthread_create(&threads[i], NULL, thread_function, this);
	}
}

void ThreadPool::Stop()
{
	running = false;
	
	pthread_mutex_lock(&mutex);
	pthread_cond_broadcast(&cond);
	pthread_mutex_unlock(&mutex);
}

void ThreadPool::Execute(Callable* callable)
{
	pthread_mutex_lock(&mutex);
	
	callables.Append(callable);
	numPending++;
	
	pthread_cond_signal(&cond);
	
	pthread_mutex_unlock(&mutex);
}
