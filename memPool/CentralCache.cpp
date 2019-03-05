#include "CentralCache.hpp"
#include "PageCache.hpp"

//�Ծ�̬��Ա�������г�ʼ��
CentralCache CentralCache::_inst;

//��׮��дһ�β���һ�Σ�
//�����Ļ����ȡһ�������Ķ����thread cache�����в���ֱ��ʹ��malloc���з���
//size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t num ,size_t bytes)
//{
//	//����num���ڴ��СΪbyte���ڴ��
//	start = malloc(bytes * num);
//	end = (char*)start + bytes * (num - 1);
//	void* cur = start;
//
//	//�����з����num���ڴ����������
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

//��ȡ�����span����Central Cache�д���span��ʱ��ֱ�Ӵ���������ȡ������Page Cache�л�ȡPage��span
//����ǻ�ȡPage��span��ֻ�ǽ�span���з��أ�������span�����κβ���
Span* CentralCache::GetOneSpan(SpanList* spanlist, size_t bytes)
{
	//Central Cache�д���span
	//���ڸ�span��������span��Ϊ�յ�spanֱ�ӷ���
	Span* span = spanlist->begin();
	while (span != spanlist->end())
	{
		if (span->_objlist != nullptr)
		{
			return span;
		}
		span = span->_next;
	}

	//�����е�span�����е�����span�����궼Ϊ�գ���ҳ����������span
	//��Ҫ���м���Ҫ��ȡ��ҳnpage
	size_t npage = ClassSize::NumMovePage(bytes);
	Span* newSpan = PageCache::GetInstance()->NewSpan(npage);

	//��ȡ����newSpan֮�����newSpan��npage������ҳ��Ҫ�����ҳ�ָ��һ������bytes��С���ڴ�������������
	//���ڵ�ַ����Ϊchar*��Ϊ��һ������Է���Ĺ�����ÿ�μ����ֵ�ʱ�򣬿���ֱ���ƶ���ô����ֽ�
	char* start = (char*)(newSpan->_pageid << PAGE_SHIFT);

	//�����end�Ƕ��ڵ�ǰ�ڴ�����һ����ַ����һ����ַ
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
	newSpan->_objsize = bytes;//��������span������ڴ��С
	newSpan->_usecount = 0;

	spanlist->PushFront(newSpan);
	return newSpan;
}


//�����Ļ����ȡ�ڴ���thread cache�����Ļ����Ǵ�ҳ�����������ڴ棨span��
size_t CentralCache::FetchRangeObj(void*& start, void*& end, size_t num, size_t bytes)
{
	//�����span���������е��±�
	size_t index = ClassSize::Index(bytes);

	//�ó���Ӧbytesһ��span����,���Ӹ������У��ó�һ��span������thread cache�����ڴ����
	SpanList* spanlist = &_spanlist[index];

	// �Ե�ǰͰ���м���
	std::unique_lock<std::mutex> lock(spanlist->_mtx);

	Span* span = GetOneSpan(spanlist, bytes);
	
	void* cur = span->_objlist;
	//prev��Ϊ�˻�û�ȡ��������ڴ�����һ���ڴ��
	void* prev= cur;
	size_t fetchnum = 0;

	//���������С��fetchnum����Ϊ������fetchnum��ʱ���Ѿ���������ô����ڴ����
	while (cur != nullptr && fetchnum < num)
	{
		prev = cur;
		cur = NEXT_OBJ(cur);
		fetchnum++;
	}

	start = span->_objlist;
	//����end���ڴ�����һ�飬���Ƕ������һ��ĵ�ַ���������һ���ڴ���ǰ4������8���ֽ�
	end = prev;

	//Ҫ��ȡ��������һ���������ڴ������Ҫ�ڼ�¼�ڴ��
	//������Ϊ����span�����п����ж���ڴ�飬������û�����꣬���Ծͽ��и�nullptr
	NEXT_OBJ(end) = nullptr;

	//��ʣ�������ڴ���ٴ�����span��
	span->_objlist = cur;

	//��ThreadCache���ٸ��ڴ�飬���span��ʹ�ü�������¼����
	span->_usecount += fetchnum;

	//���иĽ���ÿ�ν�span�е��ڴ���ó�����ʱ���ж����span�л���û���ڴ�飬û�оͷŵ����
	//������Ϊÿ�δ�span�������ڴ���ʱ���п��ܾͻὫ���span�ϵ��ڴ����������
	if (span->_objlist == nullptr)
	{
		spanlist->Erase(span);
		spanlist->PushBack(span);
	}

	return fetchnum;
}

void CentralCache::ReleaseListToSpans(void* start, size_t byte)
{
	//����������span������һ��CentralCache�����Ԫ����
	size_t index = ClassSize::Index(byte);
	SpanList* spanlist = &_spanlist[index];

	// �Ե�ǰͰ���м���
	std::unique_lock<std::mutex> lock(spanlist->_mtx);

	while (start)
	{
		void* next = NEXT_OBJ(start);
		Span* span = PageCache::GetInstance()->MapObjectToSpan(start);

		//��һ��spanΪ�ս����span�Ƶ�span�����β����
		//��Ϊ����ÿ�δ����Ļ����ȡ�ڴ���ʱ�򣬶�����ô�ͷ��ʼ����������һ��span��Ϊ��
		//�Ӷ������span���Ļ�ȡ�ڴ棬�����ǽ��Ѿ��ù��span�ŵ�β����ʱ���ٴ���Ҫ�ڴ��ʱ��
		//���൱��ֱ�Ӵ�ͷ����span��ȡ�ڴ�飬���������span�������õ�����Ż�����O(1)��ʱ�临�Ӷ�
		if (span->_objlist == nullptr)
		{
			spanlist->Erase(span);
			spanlist->PushBack(span);
		}

		//���ڴ�����ͷ��ķ�ʽ�黹��CentralCache��span
		NEXT_OBJ(start) = span->_objlist;
		span->_objlist = start;

		//����������0�ˣ��Ǿ�˵���������span�ϵ��ڴ�鶼�������ˣ�
		//�Ǿͽ����span�黹��PageCache

		if (--span->_usecount == 0)
		{
			spanlist->Erase(span);

			span->_next = nullptr;
			span->_prev = nullptr;
			span->_objlist = nullptr;
			span->_objsize = 0;

			//���ڽ�һ��span��CentralCache�黹��PageCache��ʱ��ֻ��Ҫҳ�ź�ҳ������Ҫ�����Ķ��������Զ������������ݽ��и���
			PageCache::GetInstance()->RelaseToPageCache(span);
		}

		start = next;
	}

}
