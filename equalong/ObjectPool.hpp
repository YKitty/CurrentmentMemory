/*
* ����һ���ڴ�أ�ÿ�δ���һ�������ʱ��ʹ�ø��ڴ���������ڴ�ķ���
* ���磺����һ��int���͵Ķ���ʱ�������ĸ��ֽڸ��ö���	
* �ٷ����ʱ�����ȴ���һ���ڴ�أ���ǰ������ڴ棬����malloc�����ڴ�飬����8������ΪT��С���ڴ棬ʹ��������ǰ����õ��ڴ����������������������
* ÿ�λ�ȡ�ڴ��ʱ�򣬲���New�����������ڴ�Ļ�ȡ��
*	��Ϊ���������һ�����������������ڴ棬ʹ�����������ϵ��ڴ���з���
*				  һ����ʹ���ڴ�ؽ����ڴ�ķ���
*					�ڴ�ػ����ڴ�ֱ�ӽ��з���	
*					�ڴ��û���ڴ����ٴθ��ڴ�������ڴ棬Ȼ���ٽ����ڴ�ķ���
* �����ͷ��ڴ��ʱ�򣬲���Delete�����������ڴ���ͷ�
*   �ͷŵ�ʱ�����ͷ��ķ��������������������ڴ�
*	��Ϊ���������һ��������������û���ڴ棬ֱ��ʹ��������ָ����ڴ�ĵ�ַ��Ȼ����ڴ�ĵ�ַ�����������Ϊ0
*			      һ�����������������ڴ棬���������ڴ�ָ����������Ȼ������������ָ�������ĵ�ַ
*/

#ifndef __OBJECT_POOL_H__
#define __OBJECT_POOL_H__

#include <iostream>
using std::cout;
using std::endl;;


//����������ڴ��
template <class T>
class ObjectPool
{
protected:
	struct Block
	{
		char* _start = nullptr;
		size_t _bytesize = 0;
		size_t _byteleft = 0;//ʣ����ֽ���
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
	//��ʼ���ڴ�ص�ʱ�򣬳�ʼ��Ϊ���Դ��8������ΪT������
	ObjectPool(size_t initnum = 8)
	{
		_head = _tail = new Block(initnum * sizeof(T));
	}

	T* New()
	{
		T* obj = nullptr;
		if (_freelist != nullptr)
		{
			//�����������У�ֱ�Ӵӿ��������Ϸ����ڴ�
			obj = _freelist;
			_freelist = *(T**)_freelist;
		}
		else
		{
			//Block* tail = _tail;
			//�ڴ��ʹ������,�������ݣ�����2������
			if (_tail->_byteleft == 0)
			{	
				Block* newblock = new Block(_tail->_bytesize * 2);
				_tail->_next = newblock;
				_tail = newblock;
			}

			//�����ڴ�ķ��䣬ÿ�η�����Ըı��ڴ��ַ
			obj = (T*)(_tail->_start + (_tail->_bytesize - _tail->_byteleft));
			_tail->_byteleft -= sizeof(T);
		}

		return obj;
	}

	void Delete(T* ptr)
	{
		//��������Ϊ��
		if (_freelist == nullptr)
		{
			_freelist = ptr;
			//��ptr���ָ��Ŀ���ָ��ĵ�ַ��Ҳ���ǿ��Խ��ܵı���ȫ����Ϊ0
			//����32λ������4���ֽڣ�����64λ������8���ֽ�
			//Ҳ���ǿ���ֲ�ԱȽϸ�
			(*(T**)ptr) = nullptr;
		}
		//��������ǿ�
		else
		{
			//��Ҫ�ͷŵĵ�ַ�����ŵ��ǣ���һ���ڴ�ĵ�ַ
			//ע�⣺�п�������T��char���ͻ���short���ͽ�����֮�󣬲������һ��ָ����ֽ���
			(*(T**)ptr) = _freelist;
			_freelist = ptr;
		}
	}

private:
	//��������
	T* _freelist = nullptr;

	//�����
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
//// �����Ķ����
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
//			//����
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
//	// ��������
//	T * _freelist = nullptr;
//
//	// �����
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
