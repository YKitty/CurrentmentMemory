/*
* 这是一个内存池，每次创建一个对象的时候，使用该内存池来进行内存的分配
* 比如：申请一个int类型的对象时，分配四个字节给该对象	
* 再分配的时候，首先创建一个内存池，提前分配好内存，采用malloc分配内存块，分配8个类型为T大小的内存，使用链表将提前分配好的内存采用链表来进行连接起来
* 每次获取内存的时候，采用New方法来进行内存的获取，
*	分为两种情况：一种是自由链表上有内存，使用自由链表上的内存进行分配
*				  一种是使用内存池进行内存的分配
*					内存池还有内存直接进行分配	
*					内存池没有内存了再次给内存池增加内存，然后再进行内存的分配
* 对于释放内存的时候，采用Delete方法来进行内存的释放
*   释放的时候采用头插的方法在自由链表上增加内存
*	分为两种情况：一种是自由链表上没有内存，直接使自由链表指向该内存的地址，然后该内存的地址里面的内容置为0
*			      一种是自由链表上有内存，将新来的内存指向自由链表，然后让自由链表指向新来的地址
*/

#ifndef __OBJECT_POOL_H__
#define __OBJECT_POOL_H__

#include <iostream>
using std::cout;
using std::endl;;


//定长对象的内存池
template <class T>
class ObjectPool
{
protected:
	struct Block
	{
		char* _start = nullptr;
		size_t _bytesize = 0;
		size_t _byteleft = 0;//剩余的字节数
		Block* _next = nullptr;

		Block(size_t bytesize)
		{
			_start = (char*)malloc(sizeof(bytesize));
			_bytesize = bytesize;
			_byteleft = bytesize;
			_next = nullptr;
		}
	};

public:
	//初始化内存池的时候，初始化为可以存放8个类型为T的数据
	ObjectPool(size_t initnum = 8)
	{
		_head = _tail = new Block(initnum * sizeof(T));
	}

	T* New()
	{
		T* obj = nullptr;
		if (_freelist != nullptr)
		{
			//空闲链表上有，直接从空闲链表上分配内存
			obj = _freelist;
			_freelist = *(T**)_freelist;
		}
		else
		{
			//Block* tail = _tail;
			//内存块使用完了,进行扩容，进行2倍扩容
			if (_tail->_byteleft == 0)
			{	
				Block* newblock = new Block(_tail->_bytesize * 2);
				_tail->_next = newblock;
				_tail = newblock;
			}

			//进行内存的分配，每次分配可以改变内存地址
			obj = (T*)(_tail->_start + (_tail->_bytesize - _tail->_byteleft));
			_tail->_byteleft -= sizeof(T);
		}

		return obj;
	}

	void Delete(T* ptr)
	{
		//自由链表为空
		if (_freelist == nullptr)
		{
			_freelist = ptr;
			//将ptr这个指针的可以指向的地址，也就是可以接受的变量全部置为0
			//对于32位机下是4个字节，对于64位机下是8个字节
			//也就是可移植性比较高
			(*(T**)ptr) = nullptr;
		}
		//自由链表非空
		else
		{
			//将要释放的地址里面存放的是，下一个内存的地址
			//注意：有可能这里T是char类型或者short类型解引用之后，不够存放一个指针的字节数
			(*(T**)ptr) = _freelist;
			_freelist = ptr;
		}
	}

private:
	//自由链表
	T* _freelist = nullptr;

	//块管理
	Block* _head = nullptr;
	Block* _tail = nullptr;
};

void TestObjectPool()
{
	ObjectPool<int> pool;
	int* p1 = pool.New();
	int* p2 = pool.New();

	cout << p1 << endl;
	cout << p2 << endl;

	pool.Delete(p1);
	pool.Delete(p2);

	cout << pool.New() << endl;
	cout << pool.New() << endl;
}

#endif //__OBJECT_POOL_H__


//#ifndef __OBJECT_POOL_H__
//#define __OBJECT_POOL_H__
//
//// 定长的对象池
//template<class T>
//class ObjectPool
//{
//protected:
//	struct Block
//	{
//		char* _start = nullptr;
//		size_t _bytesize = 0;
//		size_t _byteleft = 0;
//		Block* _next = nullptr;
//
//		Block(size_t bytesize)
//		{
//			_start = (char*)malloc(bytesize);
//			_bytesize = bytesize;
//			_byteleft = bytesize;
//			_next = nullptr;
//		}
//	};
//
//public:
//	ObjectPool(size_t initnum = 8)
//	{
//		_head = _tail = new Block(initnum * sizeof(T));
//	}
//
//	T*& OBJ_NEXT(T* obj)
//	{
//		return *(T**)obj;
//	}
//
//	T* New()
//	{
//		T* obj = nullptr;
//		if (_freelist != nullptr)
//		{
//			obj = _freelist;
//			_freelist = OBJ_NEXT(_freelist);
//		}
//		else
//		{
//			//增容
//			if (_tail->_byteleft == 0)
//			{
//				Block* newblock = new Block(_tail->_bytesize * 2);
//				_tail->_next = newblock;
//				_tail = newblock;
//			}
//
//			obj = (T*)(_tail->_start + (_tail->_bytesize - _tail->_byteleft));
//			_tail->_byteleft -= sizeof(T);
//		}
//
//		return obj;
//	}
//
//	void Delete(T* ptr)
//	{
//		if (_freelist == nullptr)
//		{
//			_freelist = ptr;
//			//(*(T**)ptr) = nullptr;
//			OBJ_NEXT(ptr) = nullptr;
//		}
//		else
//		{
//			//(*(T**)ptr) = _freelist;
//			OBJ_NEXT(ptr) = _freelist;
//			_freelist = ptr;
//		}
//	}
//protected:
//	// 自由链表
//	T * _freelist = nullptr;
//
//	// 块管理
//	Block* _head = nullptr;
//	Block* _tail = nullptr;
//};
//
//void TestObjectPool()
//{
//	ObjectPool<int> pool;
//	int* p1 = pool.New();
//	int* p2 = pool.New();
//
//	cout << p1 << endl;
//	cout << p2 << endl;
//
//	pool.Delete(p1);
//	pool.Delete(p2);
//
//	cout << pool.New() << endl;
//	cout << pool.New() << endl;
//}
//
//#endif
