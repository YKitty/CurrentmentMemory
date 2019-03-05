#pragma once
#include "Common.hpp"
#include "ThreadCache.hpp"
#include "PageCache.hpp"

void* ConcurrentAlloc(size_t size)
{
	if (size > MAXBYTES)
	{
		//����64K��С��128ҳ���͵�PageCache�������ڴ�
		size_t roundsize = ClassSize::_RoundUp(size, 1 << PAGE_SHIFT);
		size_t npage = roundsize >> PAGE_SHIFT;
		Span* span = PageCache::GetInstance()->NewSpan(npage);
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);

		return ptr;

		//����64Kֱ��malloc
		//return malloc(size);
	}
	//��64K֮��ֱ�����̻߳����������ڴ�
	else
	{
		//ͨ��tls����ȡ�߳��Լ���tls
		if (tls_threadcache == nullptr)
		{
			tls_threadcache = new ThreadCache;
			//std::cout << std::this_thread::get_id() << "->"  << tls_threadcache << std::endl;
			//std::cout << tls_threadcache << std::endl;
		}

		//���ػ�ȡ���ڴ��ĵ�ַ
		return tls_threadcache->Allocate(size);
		//return nullptr;
	}
}

//����ȡ�����ڴ��黹����������
void ConcurrentFree(void* ptr)
{

	Span* span = PageCache::GetInstance()->MapObjectToSpan(ptr);
	size_t size = span->_objsize;
	//�ó����Ҫ�ͷŵ��ڴ��Ĵ�С�����ж�Ҫ���к����ͷ�
	//�Ǵ���64K�Ļ���С��64K��
	if (size > MAXBYTES)
	{
		//����64K
		PageCache::GetInstance()->RelaseToPageCache(span);
	}
	else
	{
		return tls_threadcache->Deallocate(ptr, size);
	}
}