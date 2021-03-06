#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdio.h>
#include <list>
#include <cstdio>
#include <exception>
#include <pthread.h>
#include "locker.h"
//线程池类定义成模板类为了代码的复用
template <typename T>
class threadpool
{
public:
    /*thread_number是线程池中线程的数量，max_requests是请求队列中最多允许的、等待处理的请求的数量*/
    threadpool( int thread_number = 8, int max_request = 10000);
    ~threadpool();
    //向请求队列中添加任务；
    bool append(T *request);

private:
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
    static void *worker(void *arg);
    void run();

private:
    int m_thread_number;        //线程池中的线程数
    int m_max_requests;         //请求队列中允许的最大请求数
    pthread_t *m_threads;       //描述线程池的数组，其大小为m_thread_number
    std::list<T *> m_workqueue; //请求队列,用一个链表表示
    locker m_queuelocker;       //保护请求队列的互斥锁
    sem m_queuestat;            //信号量判断是否有任务需要处理
    bool m_stop;		//是否结束线程
};
template <typename T>
threadpool<T>::threadpool( int thread_number, int max_requests): m_thread_number(thread_number), m_max_requests(max_requests),m_stop(false),m_threads(NULL)
{
    if (thread_number <= 0 || max_requests <= 0)
        throw std::exception();
    m_threads = new pthread_t[m_thread_number];//创建数组
    if (!m_threads)
       { throw std::exception();}
    //创建thread_number个线程，并将他们都设置为脱离线程
    for (int i = 0; i < thread_number; ++i)
    {
	printf("创建第%d个线程\n",i);
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}
template <typename T>
threadpool<T>::~threadpool()
{
    delete[] m_threads;
    m_stop=true;
}
//向请求队列中添加任务
template <typename T>
bool threadpool<T>::append(T *request)
{
    m_queuelocker.lock();
    if (m_workqueue.size() >= m_max_requests)
    {
        m_queuelocker.unlock();//如果请求队列中线程数量已经比最大请求数多了，释放互斥锁
        return false;
    }
    
    m_workqueue.push_back(request);//插入请求
    m_queuelocker.unlock();
    m_queuestat.post();//根据信号量判断线程是否阻塞在这里
    return true;
}
    /*工作线程运行的函数，它不断从工作队列中取出任务并执行之*/
template <typename T>
void *threadpool<T>::worker(void *arg)
{
    //worker是一个静态函数，不能访问成员变量，通过传递的this指针访问
    threadpool *pool = (threadpool *)arg;//*右结合；强制类型转换，把arg转换成一个指向（threadpool类型的指针(即一个内存，内存没有名字，内存地址是arg)；整个表达式的意思是指针pool指向位置arg;
    pool->run();
    return pool;
}
template <typename T>
void threadpool<T>::run()
{
    while (!m_stop)
    {
	//线程池从队列中取出任务
        m_queuestat.wait();//没有阻塞说明有数据
        m_queuelocker.lock();//上锁
        if (m_workqueue.empty())
        {
            m_queuelocker.unlock();
            continue;
        }
        T *request = m_workqueue.front();//取出
        m_workqueue.pop_front();//队列中删除请求任务
        m_queuelocker.unlock();//解锁队列
        if (!request)
           { continue;}
            request->process();
        }
    }
#endif
