# MyWebServer
Linux下C++轻量级Web服务器     
* epoll(ET模式) 、模拟Proactor模式的并发模型
* 半同步/半反应堆模式、线程池 
* 有限状态机思想解析Http报文, 支持GET请求
## 测试
环境：2核 2 GiB  Ubuntu  16.04 64位
![alt](https://github.com/weilaikeqixfh/MyServer/blob/master/doc/%E6%B5%8B%E8%AF%95.PNG)

## 详情
* epoll监管套接字的时候用边沿触发+EPOLLONESHOT+非阻塞IO 
* 主线程获取io请求后负责数据读取,然后把请求对象插入请求队列，工作线程从请求队列中获取任务对象进行逻辑处理
* 模拟Proactor模式的并发模型
![alt](https://github.com/weilaikeqixfh/MyServer/blob/master/doc/%E6%A8%A1%E6%8B%9FProactor%E6%A8%A1%E5%BC%8F.PNG)
* 半同步/半反应堆模式
![alt](https://github.com/weilaikeqixfh/MyServer/blob/master/doc/%E5%8D%8A%E5%90%8C%E6%AD%A5%E5%8D%8A%E5%8F%8D%E5%BA%94%E5%A0%86%E6%A8%A1%E5%BC%8F.PNG)

* 使用多线程充分利用多核CPU，并使用线程池避免线程频繁创建销毁的开销
创建一个线程池  线程池中主要包含任务队列 和工作线程集合  使用了一个固定线程数的工作线程
工作线程之间对任务队列的竞争采用条件变量和互斥锁结合使用
一个工作线程先先加互斥锁 当任务队列中任务数量为0时候 阻塞在条件变量,
当任务数量大于0时候 用条件变量通知阻塞在条件变量下的线程 这些线程来继续竞争获取任务
对任务队列中任务的调度采用先来先服务算法

* 锁的使用：
* 任务队列的添加和取操作，都需要加锁，并配合条件变量，跨越了多个线程。

