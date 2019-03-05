#include "CentralCache.hpp"
#include "PageCache.hpp"

//对静态成员变量进行初始化
CentralCache CentralCache::_inst;

//打桩（写一段测试一段）
//从中心缓存获取一定数量的对象给thread cache，进行测试直接使用malloc进行分配
//size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t num ,size_t bytes)
//{
//	//分配num个内存大小为byte的内存块
//	start = malloc(bytes * num);
//	end = (char*)start + bytes * (num - 1);
//	void* cur = start;
//
//	//将所有分配好num个内存块连接起来
//	while (cur <= end)
//	{
//		void* next = (char*)cur + bytes;
//		NEXT_OBJ(cur) = next;
//
//		cur = next;
//	}
//
//	NEXT_OBJ(end) = nullptr;
//
//	return num;
//}

//获取的这个span，当Central Cache中存在span的时候，直接从这个里面获取，否则到Page Cache中获取Page的span
//如果是获取Page的span，只是将span进行返回，而不对span进行任何操作
Span* CentralCache::GetOneSpan(SpanList* spanlist, size_t bytes)
{
	//Central Cache中存在span
	//对于该span链表中有span不为空的span直接返回
	Span* span = spanlist->begin();
	while (span != spanlist->end())
	{
		if (span->_objlist != nullptr)
		{
			return span;
		}
		span = span->_next;
	}

	//将所有的span链表中的所有span遍历完都为空，从页缓存中申请span
	//需要进行计算要获取几页npage
	size_t npage = ClassSize::NumMovePage(bytes);
	Span* newSpan = PageCache::GetInstance()->NewSpan(npage);

	//获取到了newSpan之后，这个newSpan是npage数量的页，要将这个页分割成一个个的bytes大小的内存块进行连接起来
	//对于地址设置为char*是为了一会儿可以方便的挂链，每次加数字的时候，可以直接移动这么多的字节
	char* start = (char*)(newSpan->_pageid << PAGE_SHIFT);

	//这里的end是对于当前内存的最后一个地址的下一个地址
	char* end = start + (newSpan->_npage << PAGE_SHIFT);

	char* next = start + bytes;
	char* cur = start;
	while (next < end)
	{
		NEXT_OBJ(cur) = next;
		cur = next;
		next = cur + bytes;
	}

	NEXT_OBJ(cur) = nullptr;
	newSpan->_objlist = start;
	newSpan->_objsize = bytes;//申请的这个span里面的内存大小
	newSpan->_usecount = 0;

	spanlist->PushFront(newSpan);
	return newSpan;
}


//从中心缓存获取内存块给thread cache，中心缓存是从页缓存中申请内存（span）
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t num, size_t bytes)
{
	//求出在span链表数组中的下标
	size_t index = ClassSize::Index(bytes);

	//拿出对应bytes一个span链表,并从该链表中，拿出一个span用来给thread cache进行内存分配
	SpanList* spanlist = &_spanlist[index];

	// 对当前桶进行加锁
	std::unique_lock<std::mutex> lock(spanlist->_mtx);

	Span* span = GetOneSpan(spanlist, bytes);
	
	void* cur = span->_objlist;
	//prev是为了获得获取这个连续内存的最后一个内存块
	void* prev= cur;
	size_t fetchnum = 0;

	//这里必须是小于fetchnum，因为当等于fetchnum的时候已经申请了这么多的内存块了
	while (cur != nullptr && fetchnum < num)
	{
		prev = cur;
		cur = NEXT_OBJ(cur);
		fetchnum++;
	}

	start = span->_objlist;
	//对于end是内存的最后一块，但是对于最后一块的地址存放在最后的一个内存块的前4个或者8个字节
	end = prev;

	//要将取出来的这一段连续的内存最后不需要在记录内存块
	//这是因为对于span里面有可能有多个内存块，根本就没有拿完，所以就进行赋nullptr
	NEXT_OBJ(end) = nullptr;

	//将剩下来的内存块再次链接span上
	span->_objlist = cur;

	//给ThreadCache多少个内存块，这个span会使用计数，记录下来
	span->_usecount += fetchnum;

	//进行改进，每次将span中的内存块拿出来的时候，判断这个span中还有没有内存块，没有就放到最后
	//这是因为每次从span中申请内存块的时候，有可能就会将这个span上的内存块申请完了
	if (span->_objlist == nullptr)
	{
		spanlist->Erase(span);
		spanlist->PushBack(span);
	}

	return fetchnum;
}

void CentralCache::ReleaseListToSpans(void* start, size_t byte)
{
	//首先求出这个span是在那一个CentralCache数组的元素上
	size_t index = ClassSize::Index(byte);
	SpanList* spanlist = &_spanlist[index];

	// 对当前桶进行加锁
	std::unique_lock<std::mutex> lock(spanlist->_mtx);

	while (start)
	{
		void* next = NEXT_OBJ(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		//当一个span为空将这个span移到span链表的尾部上
		//因为我们每次从中心缓存获取内存块的时候，都会采用从头开始遍历来看哪一个span不为空
		//从而到这个span中心获取内存，当我们将已经用光的span放到尾部的时候，再次需要内存的时候，
		//就相当于直接从头部的span拿取内存块，将遍历这个span链表的最好的情况优化到了O(1)的时间复杂度
		if (span->_objlist == nullptr)
		{
			spanlist->Erase(span);
			spanlist->PushBack(span);
		}

		//将内存块采用头插的方式归还给CentralCache的span
		NEXT_OBJ(start) = span->_objlist;
		span->_objlist = start;

		//如果计数变成0了，那就说明对于这个span上的内存块都还回来了，
		//那就将这个span归还给PageCache

		if (--span->_usecount == 0)
		{
			spanlist->Erase(span);

			span->_next = nullptr;
			span->_prev = nullptr;
			span->_objlist = nullptr;
			span->_objsize = 0;

			//对于将一个span从CentralCache归还到PageCache的时候只需要页号和页数不需要其他的东西，所以对于其他的数据进行赋空
			PageCache::GetInstance()->RelaseToPageCache(span);
		}

		start = next;
	}

}
