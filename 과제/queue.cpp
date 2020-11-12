#include <iostream>
#include <thread>
#include <mutex>
#include <chrono>
#include <vector>
#include <set>

using namespace std;
using namespace chrono;

constexpr int MAX_THREAD = 8;


class NODE {
public:
	int key;
	NODE* volatile next;

	NODE(){
		next = nullptr;
	}
	NODE(int x){
		key = x;
		next = nullptr;
	}
	~NODE()
	{
	}
};

class null_mutex
{
public:
	void lock()
	{
	}
	void unlock()
	{
	}
};

// 성긴동기화
class CQUEUE {
	NODE *head, *tail;
	mutex deq_lock, enq_lock;
	//null_mutex m_lock;
public:
	CQUEUE()
	{
		head = tail = new NODE();
	}
	~CQUEUE()
	{
		clear();
		delete head;
	}

	void clear()
	{
		while (head != tail) {
			NODE* to_delete = head;
			head = head->next;
			delete to_delete;
		}
	}

	void Enq(int x)
	{
		NODE* new_node = new NODE(x);
		enq_lock.lock();
		tail->next = new_node;
		tail = new_node;
		enq_lock.unlock();
	}

	int Deq()
	{
		int result;
		deq_lock.lock();
		if (head->next == nullptr) {
			deq_lock.unlock();
			return -1;
		}
		else {
			NODE* to_delete = head;
			result = head->next->key;
			head = head->next;
			deq_lock.unlock();
			delete to_delete;
			return result;
		}
	}

	void display20()
	{
		NODE* ptr = head->next;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == ptr) break;
			cout << ptr->key << ", ";
			ptr = ptr->next;
		}
		cout << endl;
	}
};

// 락프리 무제한 큐
class LFQUEUE {
	NODE* volatile head;
	NODE* volatile tail;
	mutex deq_lock, enq_lock;
	//null_mutex m_lock;
public:
	LFQUEUE()
	{
		head = tail = new NODE();
	}
	~LFQUEUE()
	{
		clear();
		delete head;
	}

	void clear()
	{
		while (head != tail) {
			NODE* to_delete = head;
			head = head->next;
			delete to_delete;
		}
	}

	bool CAS(NODE* volatile* next, NODE* old_p, NODE* new_p) {
		return atomic_compare_exchange_strong(reinterpret_cast<volatile atomic_int*>(next),
			reinterpret_cast<int*>(&old_p), reinterpret_cast<int>(new_p));
	}

	void Enq(int x)
	{
		NODE* new_node = new NODE(x);
		while (true) {
			NODE* last = tail;
			NODE* next = last->next;
			if (last != tail) continue;
			if (nullptr != next) {
				CAS(&tail, last, next);
				continue;
			}
			if (false == CAS(&(tail->next), next, new_node))
				continue;
			CAS(&tail, last, next);
			return;
		}

	}

	int Deq()
	{
		if (nullptr == head->next) {
			return -1; //
		}
		while (true) {
			NODE* first = head;
			NODE* last = tail;
			NODE* next = first->next;
			if (head != first) continue;
			if (nullptr == next) return -1; //empty
			if (last == first) {
				CAS(&tail, last, next);
				continue;
			}
			int value = next->key;
			if (false == CAS(&head, first, next)) continue;
			//delete first;
			return value;
		}
	}

	void display20()
	{
		NODE* ptr = head->next;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == ptr) break;
			cout << ptr->key << ", ";
			ptr = ptr->next;
		}
		cout << endl;
	}
};


constexpr int NUM_TEST = 1000'0000;


LFQUEUE my_queue;

void benchmark(int num_threads)
{

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		if (rand() % 2 == 0 || (i < 2 / num_threads)) {
			my_queue.Enq(i);
		}
		else {
			my_queue.Deq();
		}
	}
}

int main()
{
	for (int num = 1; num <= MAX_THREAD; num = num * 2) {
		vector <thread> threads;
		my_queue.clear();
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num; ++i)
			threads.emplace_back(benchmark, num);
		for (auto& th : threads) th.join();
		auto end_t = high_resolution_clock::now();
		auto du = end_t - start_t;

		cout << num << " Threads,  ";
		cout << "Exec time " <<
			duration_cast<milliseconds>(du).count() << "ms  ";
		my_queue.display20();
	}
}