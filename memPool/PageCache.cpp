#include "PageCache.hpp"

//���ھ�̬�ĳ�Ա����������������г�ʼ��
PageCache PageCache::_inst;

Span* PageCache::NewSpan(size_t npage)
{
	//��������ֹ����߳�ͬʱ��PageCache���������64k���ڴ�
	std::unique_lock<std::mutex> lock(_mtx);
	if (npage >= NPAGES)
	{
		//������Ǵ���64K��ҳ�Ĵ�С
		void* ptr = SystemAlloc(npage);
		Span* span = new Span();
		span->_pageid = (PageID)ptr >> PAGE_SHIFT;
		span->_npage = npage;
		span->_objsize = npage << PAGE_SHIFT;

		//����ֻ��Ҫ������Ĵ���ڴ��ĵ�һ��ҳ�Ų����ȥ�ͺ���
		_id_span_map[span->_pageid] = span;

		return span;
	}

	//��ϵͳ�������64KС��128ҳ���ڴ��ʱ����Ҫ��span��objsize����һ������Ϊ���ͷŵ�ʱ����кϲ�
	Span* span = _NewSpan(npage);
	//������Ƕ���һ����PageCache����span��ʱ������¼�������span����Ҫ�ָ��ʱ��
	span->_objsize = span->_npage << PAGE_SHIFT;
	return span;
}

//��PageCache����ȡһ��span������CentralCache
Span* PageCache::_NewSpan(size_t npage)
{
	//�������page��span��Ϊ�գ���ֱ�ӷ���һ��span
	if (!_pagelist[npage].Empty())
	{
		return _pagelist[npage].PopFront();
	}

	//�������npage��spanΪ�գ����������������span�ǲ���Ϊ�յģ�������ǿյľͽ����и���span
	for (size_t i = npage + 1; i < NPAGES; i++)
	{
		if (!_pagelist[i].Empty())
		{
			//�����и�
			Span* span = _pagelist[i].PopFront();
			Span* split = new Span();

			//ҳ�ţ���span�ĺ�������и�
			split->_pageid = span->_pageid + span->_npage - npage;
			//ҳ��
			split->_npage = npage;
			span->_npage = span->_npage - npage;

			//���·ָ������ҳ��ӳ�䵽�µ�span��
			for (size_t i = 0; i < npage; i++)
			{
				_id_span_map[split->_pageid + i] = split;
			}

			_pagelist[span->_npage].PushFront(span);

			return split;
		}
	}

	//������Ҳ���ǣ�PageCache����Ҳû�д��������npage��ҳ��Ҫȥϵͳ�����ڴ�
	//���ڴ�ϵͳ�����ڴ棬һ������128ҳ���ڴ棬�����Ļ������Ч�ʣ�һ�����빻����ҪƵ������
	void* ptr = SystemAlloc(npage);

	Span* largespan = new Span();
	largespan->_pageid = (PageID)(ptr) >> PAGE_SHIFT;
	largespan->_npage = NPAGES - 1;
	_pagelist[NPAGES - 1].PushFront(largespan);

	//����ϵͳ�����ҳ��ӳ�䵽ͬһ��span
	for (size_t i = 0; i < largespan->_npage; i++)
	{
		_id_span_map[largespan->_pageid + i] = largespan;
	}

	return _NewSpan(npage);
}

Span* PageCache::MapObjectToSpan(void* obj)
{
	//ȡ�����ڴ��ҳ��
	PageID pageid = (PageID)(obj) >> PAGE_SHIFT;
	
	auto it = _id_span_map.find(pageid);

	assert(it != _id_span_map.end());

	//���ص�������ڴ��ַҳ��Ϊ�Ǹ���span���ó�����
	return it->second;
}

//��CentralCache��span�黹��PageCache
void PageCache::RelaseToPageCache(Span* span)
{
	std::unique_lock<std::mutex> lock(_mtx);
	//���ͷŵ��ڴ��Ǵ���128ҳ
	if (span->_npage >= NPAGES)
	{
		void* ptr = (void*)(span->_pageid << PAGE_SHIFT);
		_id_span_map.erase(span->_pageid);
		SystemFree(ptr);
		delete span;

		return;
	}

	//�ҵ����spanǰ���span
	auto previt = _id_span_map.find(span->_pageid - 1);

	while (previt != _id_span_map.end())
	{
		Span* prevspan = previt->second;
		//�ж�ǰ���span�ļ����ǲ���0
		if (prevspan->_usecount != 0)
		{
			break;
		}

		//�ж�ǰ���span���Ϻ����span��û�г���NPAGES
		if (prevspan->_npage + span->_npage >= NPAGES)
		{
			break;
		}

		//���кϲ�
		_pagelist[prevspan->_npage].Erase(prevspan);
		prevspan->_npage += span->_npage;
		delete(span);
		span = prevspan;

		previt = _id_span_map.find(span->_pageid - 1);
	}

	//�ҵ����span�����span
	auto nextvit = _id_span_map.find(span->_pageid + span->_npage);

	while (nextvit != _id_span_map.end())
	{
		Span* nextspan = nextvit->second;
		//�ж�ǰ���span�ļ����ǲ���0
		if (nextspan->_usecount != 0)
		{
			break;
		}

		//�ж�ǰ���span���Ϻ����span��û�г���NPAGES
		if (nextspan->_npage + span->_npage >= NPAGES)
		{
			break;
		}

		//���кϲ�,�������span��span����ɾ�����ϲ���ǰ���span��
		_pagelist[nextspan->_npage].Erase(nextspan);
		span->_npage += nextspan->_npage;
		delete(nextspan);

		nextvit = _id_span_map.find(span->_pageid + span->_npage);
	}

	//���ϲ��õ�ҳ��ӳ�䵽�µ�span��
	for (size_t i = 0; i < span->_npage; i++)
	{
		_id_span_map[span->_pageid + i] = span;
	}

	//��󽫺ϲ��õ�span���뵽span����
	_pagelist[span->_npage].PushFront(span);
}
