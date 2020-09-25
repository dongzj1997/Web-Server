#include <iostream>
#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <boost/thread/thread.hpp>


//Using a timer synchronously 以synchronously的方式使用计时器
int synchronously_example()
{
	// 所有使用asio的程序都必须至少具有一个 I/O execution context，可以是io_context或thread_pool对象。 
	// 以完成提供对io功能的访问。这里首先声明一个io_context类型的对象。
	boost::asio::io_context io(1);

	// 有了io_context以后，使用其他对象完成相应的IO功能，比如此处用一个steady_timer来完成计时器功能
	// steady_timer 的第一个参数为io_context的引用，第二个参数为计时器到期时间。
	boost::asio::steady_timer t(io, boost::asio::chrono::seconds(5));
	// 我们对计时器执行阻塞等待。直到计时器到期（在创建计时器后5秒）（而不是wait开始时起），对steady_timer :: wait（）的调用才会返回。
	t.wait();
	std::cout << "Hello, world!" << std::endl;

	return 0;
}

void print1(const boost::system::error_code& /*e*/)
{
	std::cout << "Hello, world!" << std::endl;
}

// 使用asio的异步回调功能，从而对计时器执行异步等待
int asynchronously_example()
{
	boost::asio::io_context io;
	boost::asio::steady_timer t(io, boost::asio::chrono::seconds(5));

	// async_wait函数执行异步等待。print作为回调处理程序（传入函数的指针）
	t.async_wait(&print1);

	// 必须在io_context对象上调用io_context::run()成员函数。
	// asio库可确保仅从当前正在调用io_context::run()的线程中调用回调处理程序。 
	// 因此，除非调用io_context::run()函数，否则永远不会调用异步等待完成的回调。
	std::cout << "I'm Hear! " << std::endl;
	io.run();
	// 在这个例子中run的作用是对计时器的异步等待，所以一直会运行到计时器t到期且回调完成才会返回
	// 所以要在调用run之前给io_context塞一些工作，如果没有工作，run会立即返回。

	return 0;
}




void print2(const boost::system::error_code& /*e*/, boost::asio::steady_timer* t, int* count)
{
	if (*count < 5)
	{
		std::cout << *count << std::endl;
		++(*count);

		//将计时器的到期时间从上一个到期时间起移一秒。
		//通过计算新的过期时间（相对于旧的过期时间），
		//我们可以确保计时器不会由于处理程序的任何延迟而偏离整秒的标记。
		t->expires_at(t->expiry() + boost::asio::chrono::seconds(1));
		t->async_wait(boost::bind(print2,boost::asio::placeholders::error, t, count));
	}
}

//将计时器设为每秒触发一次
int main2()
{
	boost::asio::io_context io;

	int count = 0;
	boost::asio::steady_timer t(io, boost::asio::chrono::seconds(1));

	//回调函数中更改计时器的过期时间，然后启动新的异步等待 (回调函数中调用wait) 。
	//print需要添加两个参数，一个是timer object，另一个是count计数器
	t.async_wait(boost::bind(print2, boost::asio::placeholders::error, &t, &count));
	// placeholders 就是一个参数占位符而已

	//run函数在没有更多work可完成时就返回。
	io.run();

	std::cout << "Final count is " << count << std::endl;

	return 0;
}

class printer1
{
public:
	printer1(boost::asio::io_context& io) : timer_(io, boost::asio::chrono::seconds(1)), count_(0)
	{
		timer_.async_wait(boost::bind(&printer1::print, this));
	}

	~printer1()
	{
		std::cout << "Final count is " << count_ << std::endl;
	}

	void print()
	{
		if (count_ < 5)
		{
			std::cout << count_ << std::endl;
			++count_;

			timer_.expires_at(timer_.expiry() + boost::asio::chrono::seconds(1));

			// 由于所有非静态类成员函数都有一个隐式this参数，因此我们需要将此参数绑定到该函数。
			// bind将成员函数 转换为可以调用的函数对象（传入this指针）
			// 和上一个例子相比，可以不用 boost :: asio :: placeholders :: error占位符
			timer_.async_wait(boost::bind(&printer1::print, this));
		}
	}

private:
	// 下划线是 Google Coding Style 里主张的： Data members are lowercase and always end with a trailing underscore
	boost::asio::steady_timer timer_;
	int count_;
};


//如何使用类的成员函数作为回调处理程序。

// 效果和上一个例子一样，不同之处在于，它现在对类数据成员进行操作，而不是将计时器和计数器作为参数传入。
int main3()
{
	boost::asio::io_context io;
	// steady_timer 和 count都是类的成员
	printer1 p(io);
	io.run();

	return 0;
	// 退出时p析构
}



class printer
{
public:
	printer(boost::asio::io_context& io)
		: strand_(boost::asio::make_strand(io)),
		timer1_(io, boost::asio::chrono::seconds(3)),
		timer2_(io, boost::asio::chrono::seconds(1)),
		count_(0)
	{
		// boost :: asio :: bind_executor（）函数返回一个新的handler，该handler通过strand对象自动分派其包含的handler。
		// 通过将handler绑定到同一strand，我们确保它们不能同时执行( 没有规定先后顺序，只保证不同时)。
		timer1_.async_wait(boost::asio::bind_executor(strand_, boost::bind(&printer::print1, this)));
		timer2_.async_wait(boost::asio::bind_executor(strand_, boost::bind(&printer::print2, this)));
	}

	~printer()
	{
		std::cout << "Final count is " << count_ << std::endl;
	}

	void print1()
	{
		if (count_ < 10)
		{
			std::cout << "Timer 1: " << count_ << std::endl;
			++count_;

			timer1_.expires_at(timer1_.expiry() + boost::asio::chrono::seconds(3));
			timer1_.async_wait(boost::asio::bind_executor(strand_, boost::bind(&printer::print1, this)));
		}
	}

	void print2()
	{
		if (count_ < 10)
		{
			std::cout << "Timer 2: " << count_ << std::endl;
			++count_;

			timer2_.expires_at(timer2_.expiry() + boost::asio::chrono::seconds(1));
			timer2_.async_wait(boost::asio::bind_executor(strand_, boost::bind(&printer::print2, this)));
		}
	}

private:
	boost::asio::strand<boost::asio::io_context::executor_type> strand_;
	boost::asio::steady_timer timer1_;
	boost::asio::steady_timer timer2_;
	// count_ 和 std::out 为共享资源
	int count_;
};

// 演示如何使用strand类模板在多线程程序中同步回调处理程序
// 使用多线程很简单，需要一个调用io_context :: run（）的线程池。
// 但是，由于这允许处理程序并发执行，因此当处理程序可能正在访问共享的线程不安全资源时，我们需要一种同步方法。

// strand类模板是一个执行程序(executor)适配器，该适配器确保对于通过它调度的那些函数(handler)，以一定的先后顺序执行。
// 无论调用io_context :: run（）的线程数如何，都可以保证这一点。
// 当然，这些handler可能仍可与未通过strand分配或通过其他strand分配的其他handler并发执行。
int main()
{
	boost::asio::io_context io;
	printer p(io);
	// 注意只有一个io_context 和一个printer 所以使用多线程并不会加快程序的等待时间
	// 只是处理回调函数print的时候可以由不同的线程执行(加一个strand保证不是同时执行,但是也能在不同线程中执行)
	boost::thread t(boost::bind(&boost::asio::io_context::run, &io));
	io.run();
	t.join();

	return 0;
}