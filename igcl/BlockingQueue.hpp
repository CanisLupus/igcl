#ifndef BLOCKING_QUEUE_HPP_
#define BLOCKING_QUEUE_HPP_

#include "Common.hpp"

#include <queue>
#include <mutex>
#include <condition_variable>


namespace igcl		// Internet Group-Communication Library
{
	template <class T>
	class BlockingQueue
	{
	private:
		std::queue<T> q;
		std::mutex queueAccessMutex;
		std::condition_variable queueAccessCondVar;
		bool shouldQuit;

	public:
		BlockingQueue() : shouldQuit(false)
		{
		}

		inline void enqueue(const T & elem)
		{
			std::lock_guard<std::mutex> scopeLock(queueAccessMutex);
			q.push(elem);
			queueAccessCondVar.notify_one();
		}

		inline result_type dequeue(T & elem)
		{
			std::lock_guard<std::mutex> scopeLock(queueAccessMutex);
			if (shouldQuit)
				return FAILURE;
			if (q.empty())
				return NOTHING;
			internalDequeue(elem);
			return SUCCESS;
		}

		inline result_type blockingDequeue(T & elem)
		{
			if (shouldQuit)
				return FAILURE;

			std::unique_lock<std::mutex> uniqueLock(queueAccessMutex);
			while (q.empty()) {
				queueAccessCondVar.wait(uniqueLock);
				if (shouldQuit) {
					uniqueLock.unlock();
					return FAILURE;
				}
			}
			internalDequeue(elem);
			uniqueLock.unlock();
			return SUCCESS;
		}

		inline const T & peek()
		{
			std::lock_guard<std::mutex> scopeLock(queueAccessMutex);
			return q.front();
		}

		inline void pop()
		{
			std::lock_guard<std::mutex> scopeLock(queueAccessMutex);
			q.pop();
		}

		inline uint size()
		{
			std::lock_guard<std::mutex> scopeLock(queueAccessMutex);
			return q.size();
		}

		inline void forceQuit()		// makes all waiting and subsequent calls to dequeue methods return failure,
		{								// effectively forcing all waiting threads to quit if a program abort is received
			std::lock_guard<std::mutex> scopeLock(queueAccessMutex);
			shouldQuit = true;
			queueAccessCondVar.notify_all();
		}

	private:
		inline void internalDequeue(T & elem)
		{
			elem = q.front();
			q.pop();
		}
	};
}

#endif /* BLOCKING_QUEUE_HPP_ */
