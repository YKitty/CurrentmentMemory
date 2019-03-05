/*
	为了解决对于线程获取内存时对于锁的竞争，提高了并发场景下的作用
*/

#pragma once
#include "Common.hpp"
#include "CentralCache.hpp"
#include <iostream>
#include <stdlib.h>

class ThreadCache
{
public:
	//给线程分配内存
	void* Allocate(size_t size);

	//释放内存
	void Deallocate(void* ptr, size_t size);

	//从中心缓存中拿取内存，index是数组下标，size是要拿取内存的字节大小
	void* FetchFromCentralCache(size_t index, size_t size);

	//当链表中的对象太多的时候，开始回收
	void ListTooLong(FreeList* freelist, size_t byte);

private:
	FreeList _freelist[NLISTS];//创建了一个自由链表数组，长度为NLISTS是240
	/*int _tid;
	ThreadCache* _next;*/
};

//静态的tls变量，对于每一个ThreadCache对象都有着自己的一个tls_threadcache
//产生了对于每一个线程都有自己的threadcache
//_declspec(thread)相当于每一个线程都有一个线程
static _declspec(thread) ThreadCache* tls_threadcache = nullptr;