#pragma once
#include "Common.hpp"
#include "ThreadCache.hpp"
#include "PageCache.hpp"

void* ConcurrentAlloc(size_t size)
{
	if (size > MAXBYTES)
	{
		//大于64K，小于128页，就到PageCache中申请内存
		size_t roundsize = ClassSize::_RoundUp(size, 1 << PAGE_SHIFT);
		size_t npage = roundsize >> PAGE_SHIFT;
		Span* span = PageCache::GetInstance()->NewSpan(npage);
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);

		return ptr;

		//大于64K直接malloc
		//return malloc(size);
	}
	//在64K之内直接在线程缓存中申请内存
	else
	{
		//通过tls，获取线程自己的tls
		if (tls_threadcache == nullptr)
		{
			tls_threadcache = new ThreadCache;
			//std::cout << std::this_thread::get_id() << "->"  << tls_threadcache << std::endl;
			//std::cout << tls_threadcache << std::endl;
		}

		//返回获取的内存块的地址
		return tls_threadcache->Allocate(size);
		//return nullptr;
	}
}

//将获取到的内存块归还给自由链表
void ConcurrentFree(void* ptr)
{

	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objsize;
	//拿出这个要释放的内存块的大小，来判断要进行何种释放
	//是大于64K的还是小于64K的
	if (size > MAXBYTES)
	{
		//大于64K
		PageCache::GetInstance()->RelaseToPageCache(span);
	}
	else
	{
		return tls_threadcache->Deallocate(ptr, size);
	}
}