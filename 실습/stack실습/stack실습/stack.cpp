#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>

using namespace std;
using namespace chrono;

class NODE {
public:
	int key;
	NODE* volatile next;

	NODE() :next{ nullptr } {}
	NODE(int key_value) : key{ key_value }, next{ nullptr }{}
	~NODE() {}
};

// 성긴동기화 스택
class CSTACK {
	NODE* top;
	mutex s_lock;

public:
	CSTACK() {
		top = nullptr;
	}
	~CSTACK() {
		clear();
	}

	void clear()
	{
		while (nullptr != top) {
			NODE* to_delete = top;
			top = top->next;
			delete to_delete;
		}
	}

	void Push(int key)
	{
		NODE* e = new NODE(key);
		s_lock.lock();
		e->next = top;
		top = e;
		s_lock.unlock();
	}

	int Pop()
	{
		s_lock.lock();
		if (nullptr == top) {
			s_lock.unlock();
			return -1;
		}
		NODE* p = top;
		int value = p->key;
		top = top->next;
		s_lock.unlock();
		delete p;
		return value;
	}

	void display20()
	{
		NODE* ptr = top;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == ptr) break;
			cout << ptr->key << ", ";
			ptr = ptr->next;
		}
		cout << endl;
	}
};

class LFSTACK {
	NODE* volatile top;

public:
	LFSTACK() {
		top = nullptr;
	}
	~LFSTACK() {
		clear();
	}

	void clear()
	{
		while (nullptr != top) {
			NODE* to_delete = top;
			top = top->next;
			delete to_delete;
		}
	}

	bool CAS(NODE* volatile* ptr, NODE* o_node, NODE* n_node)
	{
		return atomic_compare_exchange_strong(
			reinterpret_cast<volatile atomic_int*>(ptr),
			reinterpret_cast<int*>(&o_node),
			reinterpret_cast<int>(n_node));
	}

	void Push(int key)
	{
		NODE* e = new NODE(key);

		while (true) {
			NODE* first = top;
			e->next = first;
			if (true == CAS(&top, first, e))
				return;
		}
	}

	int Pop()
	{
		while (true) {
			NODE* first = top;
			if (nullptr == top) {
				return -1;
			}
			NODE* next = first->next;
			int value = first->key;
			if (top != first) continue;
			if (true == CAS(&top, first, next))
				return value;
			// ABA문제 때문에 delete는 하지 않는다
		}
	}

	void display20()
	{
		NODE* ptr = top;
		for (int i = 0; i < 20; ++i) {
			if (nullptr == ptr) break;
			cout << ptr->key << ", ";
			ptr = ptr->next;
		}
		cout << endl;
	}
};

constexpr int NUM_TEST = 10000000;
constexpr int MAX_THREAD = 8;

LFSTACK my_stack;

void benchmark(int num_threads)
{

	for (int i = 0; i < NUM_TEST / num_threads; ++i) {
		if (rand() % 2 == 0 || (i < 2 / num_threads)) {
			my_stack.Push(i);
		}
		else {
			my_stack.Pop();
		}
	}
}

int main()
{
	for (int num = 1; num <= MAX_THREAD; num = num * 2) {
		vector <thread> threads;
		my_stack.clear();
		auto start_t = high_resolution_clock::now();
		for (int i = 0; i < num; ++i)
			threads.emplace_back(benchmark, num);
		for (auto& th : threads) th.join();
		auto end_t = high_resolution_clock::now();
		auto du = end_t - start_t;

		cout << num << " Threads,  ";
		cout << "Exec time " <<
			duration_cast<milliseconds>(du).count() << "ms  ";
		my_stack.display20();
	}
}