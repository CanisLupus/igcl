#include "Communication.hpp"


namespace igcl
{
	//--------------------------------------------------
	// Constructor/destructor
	//--------------------------------------------------

	Communication::Communication()
	{
#ifdef USE_SEND_QUEUE
		senderThread = new std::thread(&Communication::sendLoop, this);
		senderThread->detach();
#endif
	}


	Communication::~Communication()
	{
#ifdef USE_SEND_QUEUE
		delete senderThread;
#endif
	}

	//--------------------------------------------------
	// Sending methods
	//--------------------------------------------------

#ifdef USE_SEND_QUEUE
	void Communication::sendLoop()
	{
		SendElement elem;

		while (1)
		{
			sendQueue.blockingDequeue(elem);
			//std::cout << "thread is sending... " << elem.descriptor << " " << elem.data << " " << elem.size << " bytes" << std::endl;

			//std::lock_guard<std::mutex> scopeLock(sendingMutex);
			send_all_aux_(elem.descriptor, elem.data, elem.size);
			free(elem.data);
			//std::cout << "thread sent " << elem.size << " bytes" << std::endl;
		}
	}
#endif


	result_type Communication::send_type_(int fd, msg_type type)
	{
		return send_all_(fd, &type, sizeof(type));
	}


	// sends a std::string
	result_type Communication::send_(int socketfd, const std::string & value)
	{
		return send_(socketfd, value.c_str(), value.length());	// send value as char[] with a certain size
	}


#ifdef GMP
	// sends an mpz_class value
	result_type Communication::send_(int socketfd, const mpz_class & value)
	{
		size_t size = mpz_sizeinbase(value.get_mpz_t(), 10) + 2;
		char str[size];
		mpz_get_str(str, 10, value.get_mpz_t());
		//std::cout << "sending " << size << " bytes" << std::endl;
		return send_(socketfd, str, size);	// send value as char[] with a certain size
	}
#endif

	//--------------------------------------------------
	// Receive methods
	//--------------------------------------------------

	result_type Communication::recv_type_(int fd, flag_type flags, msg_type & type)
	{
		return recv_all_(fd, 0, &type, sizeof(type));
	}


	// receives to existing std::string
	result_type Communication::recv_(int socketfd, flag_type flags, std::string & value)
	{
		size_type nBytes;
		result_type res;

		res = recv_size_(socketfd, flags, nBytes);
		QUIT_IF_UNSUCCESSFUL(res);

		char data[nBytes];
		res = recv_all_(socketfd, 0, data, nBytes);
		QUIT_IF_UNSUCCESSFUL(res);
		//std::cout << "receiving " << nBytes << " bytes" << std::endl;

		value.assign(static_cast<char*>(data), nBytes);	// copy from buffer to string

		return res;
	}


#ifdef GMP
	// receives an mpz_class value
	result_type Communication::recv_(int socketfd, flag_type flags, mpz_class & value)
	{
		size_type nBytes;
		result_type res;

		res = recv_size_(socketfd, flags, nBytes);
		QUIT_IF_UNSUCCESSFUL(res);

		char data[nBytes];
		res = recv_all_(socketfd, 0, data, nBytes);
		QUIT_IF_UNSUCCESSFUL(res);
		//std::cout << "receiving " << nBytes << " bytes" << std::endl;

		value.set_str(data, 10);

		return SUCCESS;
	}
#endif

	//--------------------------------------------------
	//
	//--------------------------------------------------
}
