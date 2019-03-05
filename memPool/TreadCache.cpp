#include "ThreadCache.hpp"
#include "CentralCache.hpp"

//�����Ļ�����ȡ����
//ÿһ��ȡ���������ݣ���Ϊÿ�ε�CentralCache�����ڴ��ʱ������Ҫ�����ģ�����һ�ξͶ�����һЩ�ڴ�飬
//��ֹÿ�ε�CentralCacheȥ�ڴ���ʱ��,��μ������Ч������
void* ThreadCache::FetchFromCentralCache(size_t index, size_t byte)
{
	//��ȡ���ڴ�����������������
	FreeList* freelist = &_freelist[index];

	//ÿ�δ����Ļ���ȡ��10��������С���ڴ��
	//size_t num = 10;

	//����ÿ������10�������ǽ����������Ĺ���
	//��������ԽС�������ڴ�������Խ��
	//��������Խ�������ڴ�������ԽС
	size_t num_to_move = min(ClassSize::NumMoveSize(byte), freelist->MaxSize());


	//���ﲻ��_maxsize�������ӣ�ÿ������Ķ���һ�����ڴ������
	//�����������16byte��ʱ��ÿ�������ʱ��������1���ڴ��

	//start��end�ֱ��ʾȡ�������ڴ�Ŀ�ʼ��ַ�ͽ�����ַ
	//ȡ�������ڴ���һ���������ڴ�
	void* start, *end;

	//fetchnum��ʾʵ��ȡ�������ڴ�ĸ�����fetchnum�п���С��num����ʾ���Ļ���û����ô���С���ڴ��
	size_t fetchnum = CentralCache::GetInstance()->FetchRangeObj(start, end, num_to_move, byte);
	if (fetchnum > 1)
	{
		freelist->PushRange(NEXT_OBJ(start), end, fetchnum - 1);
	}

	if (num_to_move == freelist->MaxSize())
	{
		freelist->SetMaxSize(num_to_move + 1);
	}


	////ֻ�����������з���(fetchnum - 1)���ڴ�飬������Ϊ��һ���ڴ���Ѿ���ʹ����
	////�������Ļ����Ѿ����ڴ�����һ��һ�����Ӻ��ˣ���ֻ��Ҫ����һ����ڴ���뵽��������Ϳ�����
	//freelist->PushRange(NEXT_OBJ(start), end, fetchnum - 1);

	return start;
}

//���������������������������ȡ�ڴ�
void* ThreadCache::Allocate(size_t byte)
{
	assert(byte < MAXBYTES);
	//size��Ҫ��ȡ���ٸ��ֽڵ��ڴ�
	byte = ClassSize::RoundUp(byte);
	//index�����������������±�
	size_t index = ClassSize::Index(byte);

	//����ȷ��Ҫ��ȡ��size�Ǵ������������������һ����������
	FreeList* freelist = &_freelist[index];

	//�ж������Ƿ�Ϊ��
	//��Ϊ�գ�ֱ��ȡ�ڴ�
	if (!freelist->Empty())
	{
		//Ҳ���Ǵ��������������ڴ�
		return freelist->Pop();
	}
	//���������ǿյ�Ҫ�����Ķ�����ȡ�ڴ棬һ��ȡ�����ֹ�ظ�ȡ
	else 
	{
		return FetchFromCentralCache(index, byte);
	}
}

void ThreadCache::Deallocate(void* ptr, size_t byte)
{
	//����������������е���������黹�ڴ�
	assert(byte < MAXBYTES);
	size_t index = ClassSize::Index(byte);
	
	FreeList* freelist = &_freelist[index];

	freelist->Push(ptr);

	//�������������������һ�δ�central cache������ڴ�������ʱ
	//��ʼ�����ڴ�鵽���Ļ���
	if (freelist->Size() >= freelist->MaxSize())
	{
		ListTooLong(freelist, byte);
	}

	//�����������е������ڴ�����������2����ÿ�δ�Page������ڴ�ʱ���ͷ�
	/*if (freelist->Size() >= ClassSize::NumMoveSize(byte) * 2)
	{
		ListTooLong(freelist, byte);
	}*/

	// thread cache�ܵ��ֽ�������2M����ʼ�ͷ�
}

void ThreadCache::ListTooLong(FreeList* freelist, size_t byte)
{
	void* start = freelist->Clear();

	//����start��ʼ���ڴ�黹�����Ļ���
	CentralCache::GetInstance()->ReleaseListToSpans(start, byte);
}



