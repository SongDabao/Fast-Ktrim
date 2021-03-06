/*
  This file is a part of DSRC software distributed under GNU GPL 2 licence.
  The homepage of the DSRC project is http://sun.aei.polsl.pl/dsrc
  
  Authors: Lucas Roguski and Sebastian Deorowicz
  
  Version: 2.00
*/

#ifndef H_DATAQUEUE
#define H_DATAQUEUE

#include "Globals.h"

#include <queue>

#ifdef USE_BOOST_THREAD
#include <boost/thread.hpp>
namespace th = boost;
#else
#include <mutex>
#include <condition_variable>
namespace th = std;
#endif

namespace mash
{

namespace core
{

template <class _TDataType>
class TDataQueue
{
	typedef _TDataType DataType;
	typedef std::queue<std::pair<int64, DataType*> > part_queue;

	const uint32 threadNum;
	const uint32 maxPartNum;
	uint64 completedThreadMask;
	uint32 partNum;
	uint64 currentThreadMask;
	part_queue parts;

	th::mutex mutex;
	th::condition_variable queueFullCondition;
	th::condition_variable queueEmptyCondition;

public:
	static const uint32 DefaultMaxPartNum = 64;
	static const uint32 DefaultMaxThreadtNum = 64;

	TDataQueue(uint32 maxPartNum_ = DefaultMaxPartNum, uint32 threadNum_ = 1)
		:	threadNum(threadNum_)
		,	maxPartNum(maxPartNum_)
		,	partNum(0)
		,	currentThreadMask(0)
	{
		ASSERT(maxPartNum_ > 0);
		ASSERT(threadNum_ >= 1);
		ASSERT(threadNum_ < 64);

		completedThreadMask = ((uint64)1 << threadNum) - 1;
	}

	~TDataQueue()
	{}

	bool IsEmpty()
	{
		return parts.empty();
	}

	bool IsCompleted()
	{
		return parts.empty() && currentThreadMask == completedThreadMask;
	}

	void SetCompleted()
	{
		th::lock_guard<th::mutex> lock(mutex);

		ASSERT(currentThreadMask != completedThreadMask);
		currentThreadMask = (currentThreadMask << 1) | 1;

		queueEmptyCondition.notify_all();
	}

	void Push(int64 partId_, const DataType* part_)
	{
		th::unique_lock<th::mutex> lock(mutex);

		while (partNum > maxPartNum)
		{
			//printf("pushing, %d\n", completedThreadMask);
			queueFullCondition.wait(lock);
		}

		parts.push(std::make_pair(partId_, (DataType*)part_));
		partNum++;

		queueEmptyCondition.notify_one();
	}

	bool Pop(int64 &partId_, DataType* &part_)
	{
		th::unique_lock<th::mutex> lock(mutex);

		while ((parts.size() == 0) && currentThreadMask != completedThreadMask)
		{
			//printf("poping, %d\n", completedThreadMask);
			queueEmptyCondition.wait(lock);
		}

		if (parts.size() != 0)
		{
			partId_ = parts.front().first;
			part_ = parts.front().second;
			partNum--;
			parts.pop();
			queueFullCondition.notify_one();
			return true;
		}

		// assure this is impossible
		ASSERT(currentThreadMask == completedThreadMask);
		ASSERT(parts.size() == 0);
		return false;
	}

	void Reset()
	{
		ASSERT(currentThreadMask == completedThreadMask);

		partNum = 0;
		currentThreadMask = 0;
	}
};

} // namespace core

} // namespace mash

#endif // DATA_QUEUE_H
