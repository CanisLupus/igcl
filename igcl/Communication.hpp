#ifndef COMMUNICATION_HPP_
#define COMMUNICATION_HPP_

#include "Common.hpp"
#include "Debug.hpp"
#include "BlockingQueue.hpp"
#include "LibniceHelper.hpp"

#include <string>
#include <cassert>
#include <cstring>
#include <thread>
#include <sys/socket.h>

#ifdef GMP
#include <gmpxx.h>
#endif


namespace igcl
{
	/*
	 * Class that provides the basic send and receive functionality for inheriting classes.
	 * The class also provides a method to obtain statistics about communication data.
	 */
	class Communication
	{
		// ======================================================
		// ==================== DEFINITIONS =====================
		// ======================================================

#ifdef USE_SEND_QUEUE
		struct SendElement
		{
			int descriptor;
			void * data;
			uint size;

			SendElement() {};
			SendElement(int descriptor, void * data, uint size)
				: descriptor(descriptor), data(data), size(size) {}
		};
#endif

		// ======================================================
		// ==================== ATTRIBUTES ======================
		// ======================================================
	protected:
#ifndef DISABLE_LIBNICE
		LibniceHelper nice;
#endif

	private:
		Stats stats;

#ifdef USE_SEND_QUEUE
		std::thread * senderThread;
		igcl::BlockingQueue< SendElement > sendQueue;
		std::mutex sendingMutex;
		std::condition_variable sendingCondVar;
#endif

		// ======================================================
		// ===================== METHODS ========================
		// ======================================================

	protected:
		Communication();
		virtual ~Communication();

#ifdef USE_SEND_QUEUE
		void sendLoop();
#endif

		result_type send_type_(int fd, msg_type type);
		result_type send_(int socketfd, const std::string & value);
		result_type recv_type_(int fd, flag_type flags, msg_type & type);
		result_type recv_(int socketfd, flag_type flags, std::string & value);

#ifdef GMP
		result_type send_(int socketfd, const mpz_class & value);
		result_type recv_(int socketfd, flag_type flags, mpz_class & value);
#endif

	public:
		Stats getStats() { return stats; }

		// ------------------------------------------------------
		// Internal Send Methods
		// ------------------------------------------------------

	protected:
		// sends an array of data of size "size"
		template<typename T>
		result_type send_(int socketfd, const T * const data, uint size)
		{
			static_assert(!std::is_pointer<T>::value, "T must be non-pointer");

			assert(size*sizeof(T) <= SIZE_TYPE_MAX);
			size_type nBytes = size*sizeof(T);
			dbg("sending", nBytes, "bytes");

			result_type res;
			res = send_size_(socketfd, nBytes);
			QUIT_IF_UNSUCCESSFUL(res);

			res = send_all_(socketfd, data, nBytes);

			return res;
		}


		// sends a (non-pointer) value
		template<typename T>
		result_type send_(int socketfd, const T & value)
		{
			return send_(socketfd, &value, 1);	// send value as T[] of size 1
		}

		// ------------------------------------------------------
		// Internal Recv Methods
		// ------------------------------------------------------

	protected:
		// receives to new array of size "size" (array is allocated with the "new" keyword)
		template<typename T>
		result_type recv_new_(int socketfd, flag_type flags, T * & data, uint & size)
		{
			size_type nBytes;
			result_type res;

			res = recv_size_(socketfd, flags, nBytes);
			QUIT_IF_UNSUCCESSFUL(res);
			//std::cout << "receiving " << nBytes << " bytes" << std::endl;

			size = nBytes / sizeof(T);			// assign values
			data = (T *) malloc(nBytes);

			res = recv_all_(socketfd, 0, data, nBytes);

			return res;
		}


		// receives to existing value
		template<typename T>
		result_type recv_(int socketfd, flag_type flags, T & value)
		{
			static_assert(!std::is_pointer<T>::value, "T must be non-pointer");

			size_type nBytes;
			result_type res;

			res = recv_size_(socketfd, flags, nBytes);
			QUIT_IF_UNSUCCESSFUL(res);
			//std::cout << "receiving " << nBytes << " bytes" << std::endl;
			if (nBytes != sizeof(value))
				return FAILURE;

			res = recv_all_(socketfd, 0, &value, nBytes);

			return res;
		}

		// ------------------------------------------------------
		// Low-level Send and Recv Methods
		// ------------------------------------------------------

	private:
		inline int getSocketBufferSize(int fd)		// TODO: use it!
		{
			int n;
			unsigned int m = sizeof(n);
			getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (void *)&n, &m);
			return n;
		}


		// sends data until "nBytes" have been sent
		inline result_type send_all_(int socketfd, const void * data, size_type nBytes)
		{
#ifdef USE_SEND_QUEUE
			//std::cout << "SEND" << nBytes << std::endl;
			if (nBytes < 85000 and sendQueue.size() == 0) {	// 87380
				std::lock_guard<std::mutex> scopeLock(sendingMutex);
				return send_all_aux_(socketfd, data, nBytes);
			} else {
				void * arr = malloc(nBytes);
				memcpy(arr, data, nBytes);
				//std::cout << "keeping bytes: " << nBytes << std::endl;
				sendQueue.enqueue(SendElement(socketfd, arr, nBytes));
				return SUCCESS;	// TODO: change
			}
#else
			return send_all_aux_(socketfd, data, nBytes);
#endif
		}


		inline result_type send_all_aux_(int socketfd, const void * data, size_type nBytes)
		{
			std::cout << "inside send_all_ bytesToSend: " << nBytes << std::endl;

			uint bytesSentTotal = 0;
			while (bytesSentTotal < nBytes)
			{
				int bytesSent = send(socketfd, ((char *)data)+bytesSentTotal, nBytes-bytesSentTotal, 0);
				std::cout << "inside send_all_ bytesSent: " << bytesSent<< std::endl;
				if (bytesSent < 0) {
					if (errno != EAGAIN) {
						return FAILURE;
					}
				} else {
					bytesSentTotal += bytesSent;
				}
			}
			//stats.incNSends(1);
			//stats.incNBytesSent(bytesSentTotal);
			return SUCCESS;
		}


		// sends the message size (header)
		inline result_type send_size_(int socketfd, size_type nBytes)
		{
			//stats.incNSizeSends(1);
			//stats.incNBytesSent(bytesSent);
			return send_all_(socketfd, &nBytes, sizeof(nBytes));
		}


		// receives data until "nBytes" have been read (usually called after the message size is known)
		inline result_type recv_all_(int socketfd, flag_type flags, void * data, size_type nBytes)
		{
			uint bytesReadTotal = 0;
			while (bytesReadTotal < nBytes)
			{
				// TODO: what about non-blocking?
				int bytesRead = recv(socketfd, ((char *)data)+bytesReadTotal, nBytes-bytesReadTotal, flags);

				if (bytesRead <= 0) {
					if (errno != EAGAIN) {
						return FAILURE;
					}
				} else {
					bytesReadTotal += bytesRead;
				}
			}
			//stats.incNReceives(1);
			//stats.incNBytesReceived(bytesReadTotal);
			dbg("received", bytesReadTotal, "bytes");
			return SUCCESS;
		}


		// receives the message size (header)
		inline result_type recv_size_(int socketfd, int flags, size_type & nBytes)
		{
			int bytesRead = recv(socketfd, &nBytes, sizeof(nBytes), flags);
			dbg(nBytes, "bytes coming");

			if (bytesRead <= 0) {
				if (errno == EWOULDBLOCK) {
					return NOTHING;
				} else {
					return FAILURE;
				}
			}

			//stats.incNSizeReceives(1);
			//stats.incNBytesReceived(bytesRead);
			return SUCCESS;
		}

#ifndef DISABLE_LIBNICE
		// ------------------------------------------------------
		// Internal Nice send methods
		// ------------------------------------------------------

	protected:
		// sends the message type
		result_type nice_send_type_(uint streamId, msg_type type)
		{
			return nice_send_all_(streamId, &type, sizeof(type));
		}


		// sends an array of data of size "size"
		template<typename T>
		result_type nice_send_(uint streamId, const T * data, uint size)
		{
			//std::cout << "nice_send_ data of size " << size << " (" << size*sizeof(T) << " bytes)" << std::endl;
			assert(size*sizeof(T) <= SIZE_TYPE_MAX);
			size_type nBytes = size*sizeof(T);
			dbg("sending", nBytes, "bytes");

			result_type res;
			res = nice_send_size_(streamId, nBytes);
			QUIT_IF_UNSUCCESSFUL(res);

			res = nice_send_all_(streamId, data, nBytes);
			return res;
		}


		// sends a (non-pointer) value
		template<typename T>
		result_type nice_send_(uint streamId, const T & value)
		{
			static_assert(!std::is_pointer<T>::value, "T must be non-pointer");
			return nice_send_(streamId, &value, 1);	// send value as T[] of size 1
		}


		// sends a std::string
		result_type nice_send_(uint streamId, const std::string & value)
		{
			// send value as char[] with a certain size
			return nice_send_(streamId, value.c_str(), value.length());
		}

		// ------------------------------------------------------
		// Low-level Nice send methods
		// ------------------------------------------------------

	private:
		// sends the message size (header)
		inline result_type nice_send_size_(uint streamId, size_type nBytes)
		{
			//stats.incNSizeSends(1);
			//stats.incNBytesSent(bytesSent);
			return nice_send_all_(streamId, (void *) &nBytes, sizeof(size_type));
		}


		// sends data until "nBytes" have been sent
		inline result_type nice_send_all_(uint streamId, const void * data, size_type nBytes)
		{
			uint totalBytesSent = 0;
			//std::cout << "total to send: " << nBytes << std::endl;
			while (totalBytesSent < nBytes)
			{
				nice.waitWritable(streamId);
				int bytesSent = nice.send(streamId, ((char *)data)+totalBytesSent, nBytes-totalBytesSent);
				//std::cout << "nBytes sent: " << bytesSent << std::endl;
				if (bytesSent < 0) {
					if (errno != 0 and errno != 92) {
						std::cout << "fail with errno == " << errno << std::endl;
						//return FAIL;
					}
				} else {
					totalBytesSent += bytesSent;
				}
			}
			//stats.incNSends(1);
			//stats.incNBytesSent(bytesSentTotal);
			return SUCCESS;
		}
#endif

		// ------------------------------------------------------
		//
		// ------------------------------------------------------
	};
}

#endif /* COMMUNICATION_HPP_ */
