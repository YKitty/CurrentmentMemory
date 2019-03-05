#include "PageCache.hpp"

//对于静态的成员变量必须在类外进行初始化
PageCache PageCache::_inst;

Span* PageCache::NewSpan(size_t npage)
{
	//加锁，防止多个线程同时到PageCache中申请大于64k的内存
	std::unique_lock<std::mutex> lock(_mtx);
	if (npage >= NPAGES)
	{
		//申请的是大于64K的页的大小
		void* ptr = SystemAlloc(npage);
		Span* span = new Span();
		span->_pageid = (PageID)ptr >> PAGE_SHIFT;
		span->_npage = npage;
		span->_objsize = npage << PAGE_SHIFT;

		//这里只需要将申请的大的内存块的第一个页号插入进去就好了
		_id_span_map[span->_pageid] = span;

		return span;
	}

	//从系统申请大于64K小于128页的内存的时候，需要将span的objsize进行一个设置为了释放的时候进行合并
	Span* span = _NewSpan(npage);
	//这个就是对于一个从PageCache申请span的时候，来记录申请这个span的所要分割的时候
	span->_objsize = span->_npage << PAGE_SHIFT;
	return span;
}

//从PageCache出拿取一个span用来给CentralCache
Span* PageCache::_NewSpan(size_t npage)
{
	//如果对于page的span不为空，则直接返回一个span
	if (!_pagelist[npage].Empty())
	{
		return _pagelist[npage].PopFront();
	}

	//如果对于npage的span为空，接下来检测比他大的span是不是为空的，如果不是空的就进行切割大的span
	for (size_t i = npage + 1; i < NPAGES; i++)
	{
		if (!_pagelist[i].Empty())
		{
			//进行切割
			Span* span = _pagelist[i].PopFront();
			Span* split = new Span();

			//页号，从span的后面进行切割
			split->_pageid = span->_pageid + span->_npage - npage;
			//页数
			split->_npage = npage;
			span->_npage = span->_npage - npage;

			//将新分割出来的页都映射到新的span上
			for (size_t i = 0; i < npage; i++)
			{
				_id_span_map[split->_pageid + i] = split;
			}

			_pagelist[span->_npage].PushFront(span);

			return split;
		}
	}

	//到这里也就是，PageCache里面也没有大于申请的npage的页，要去系统申请内存
	//对于从系统申请内存，一次申请128页的内存，这样的话，提高效率，一次申请够不需要频繁申请
	void* ptr = SystemAlloc(npage);

	Span* largespan = new Span();
	largespan->_pageid = (PageID)(ptr) >> PAGE_SHIFT;
	largespan->_npage = NPAGES - 1;
	_pagelist[NPAGES - 1].PushFront(largespan);

	//将从系统申请的页都映射到同一个span
	for (size_t i = 0; i < largespan->_npage; i++)
	{
		_id_span_map[largespan->_pageid + i] = largespan;
	}

	return _NewSpan(npage);
}

Span* PageCache::MapObjectToSpan(void* obj)
{
	//取出该内存的页号
	PageID pageid = (PageID)(obj) >> PAGE_SHIFT;
	
	auto it = _id_span_map.find(pageid);

	assert(it != _id_span_map.end());

	//返回的是这个内存地址页号为那个的span中拿出来的
	return it->second;
}

//将CentralCache的span归还给PageCache
void PageCache::RelaseToPageCache(Span* span)
{
	std::unique_lock<std::mutex> lock(_mtx);
	//当释放的内存是大于128页
	if (span->_npage >= NPAGES)
	{
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		_id_span_map.erase(span->_pageid);
		SystemFree(ptr);
		delete span;

		return;
	}

	//找到这个span前面的span
	auto previt = _id_span_map.find(span->_pageid - 1);

	while (previt != _id_span_map.end())
	{
		Span* prevspan = previt->second;
		//判断前面的span的计数是不是0
		if (prevspan->_usecount != 0)
		{
			break;
		}

		//判断前面的span加上后面的span有没有超出NPAGES
		if (prevspan->_npage + span->_npage >= NPAGES)
		{
			break;
		}

		//进行合并
		_pagelist[prevspan->_npage].Erase(prevspan);
		prevspan->_npage += span->_npage;
		delete(span);
		span = prevspan;

		previt = _id_span_map.find(span->_pageid - 1);
	}

	//找到这个span后面的span
	auto nextvit = _id_span_map.find(span->_pageid + span->_npage);

	while (nextvit != _id_span_map.end())
	{
		Span* nextspan = nextvit->second;
		//判断前面的span的计数是不是0
		if (nextspan->_usecount != 0)
		{
			break;
		}

		//判断前面的span加上后面的span有没有超出NPAGES
		if (nextspan->_npage + span->_npage >= NPAGES)
		{
			break;
		}

		//进行合并,将后面的span从span链中删除，合并到前面的span上
		_pagelist[nextspan->_npage].Erase(nextspan);
		span->_npage += nextspan->_npage;
		delete(nextspan);

		nextvit = _id_span_map.find(span->_pageid + span->_npage);
	}

	//将合并好的页都映射到新的span上
	for (size_t i = 0; i < span->_npage; i++)
	{
		_id_span_map[span->_pageid + i] = span;
	}

	//最后将合并好的span插入到span链中
	_pagelist[span->_npage].PushFront(span);
}
