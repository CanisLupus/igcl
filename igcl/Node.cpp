#include "Node.hpp"
#include "Common.hpp"

#include <netinet/in.h>
#include <sys/select.h>
#include <arpa/inet.h>
#include <cstring>
#include <unistd.h>
#include <netdb.h>


namespace igcl
{
#ifndef DISABLE_LIBNICE
	std::map<uint, Node::NiceReceivedData> Node::receivedData;
	Node * Node::instance;
#endif

	//--------------------------------------------------
	// Constructor/destructor
	//--------------------------------------------------

	Node::Node(int ownPort) : Communication()
	{
		ownAddr.set("127.0.0.1", ownPort);
		shouldStop = false;
		receiverThread = NULL;
#ifndef DISABLE_LIBNICE
		instance = this;
		nice.cb_nice_recv = libniceRecv;
		nice.start();
#endif
	}


	Node::~Node()
	{
		// see "terminate" method instead
	}

	//--------------------------------------------------
	// Implemented public methods
	//--------------------------------------------------

	// hangs node forever (until a call to "terminate")
	// avoids quitting from main thread (the one calling hang())
	void Node::hang()
	{
		std::unique_lock<std::mutex> uniqueLock(stopMutex);
		while (!shouldStop) {
			stopCondVar.wait(uniqueLock);
		}
		uniqueLock.unlock();
	}


	peer_id Node::getId()
	{
		return ownId;
	}

	//--------------------------------------------------
	// Listen, receive and process messages
	//--------------------------------------------------

	void Node::bindReceivingSocket()
	{
		int rc, on = 1;
		sockaddr_in addr;
		socklen_t addrlen = sizeof(addr);

		memset(&addr, 0, addrlen);
		addr.sin_family = AF_INET;
		addr.sin_port = htons(ownAddr.port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		int listenSd = socket(AF_INET, SOCK_STREAM, 0);
		rc = setsockopt(listenSd, SOL_SOCKET, SO_REUSEADDR, (char *) &on, sizeof(on));
		assert(rc == 0);
		//rc = ioctl(listenSd, FIONBIO, (char *) &on);
		//assert(rc == 0);
		rc = bind(listenSd, (sockaddr *) &addr, sizeof(addr));
		assert(rc == 0);
		rc = listen(listenSd, 10);
		assert(rc == 0);

		this->listenFd = listenSd;
		fds.setFd(listenFd);

		char hostname[128];
		gethostname(hostname, sizeof hostname);
		std::cout << "hostname: " << hostname << std::endl;

		hostent * hp = gethostbyname(hostname);
		std::cout << hp->h_name << " = ";

		for (uint i=0; hp->h_addr_list[i] != NULL; i++) {	// for cycle but we only want first result
			this->localIp.assign(inet_ntoa(*(struct in_addr *)(hp->h_addr_list[i])));
			std::cout << localIp << " ";
			break;
		}
		std::cout << std::endl;
	}


	void Node::threadedLoop()
	{
		receiverThread = new std::thread(&Node::loop, this);
		receiverThread->detach();
	}


	// repeatedly calls select with a timeout and checks if the function should quit
	void Node::loop()
	{
		timespec timeout;
		timeout.tv_sec = 2;
		timeout.tv_nsec = 0;

		for(;;) {
			// lock scope
			{
				std::lock_guard<std::mutex> lockWhileInsideScope(stopMutex);
				if (shouldStop)
					break;
			}
			result_type res;
			res = doSelect(timeout);
			if (res == FAILURE) {
				std::cout << "FAILURE IN SELECT" << std::endl;
				std::cout << "errno: " << errno << std::endl;
			}
		}
	}


	result_type Node::doSelect(const timespec & timeout)
	{
		fd_set working_set = fds.copyDescriptorSet();	// copy set (will be overwritten by select)

		int desc_ready = pselect(fds.maxFd+1, &working_set, NULL, NULL, &timeout, NULL);
		dbg("got out of select");

		if (desc_ready == 0)	// did timeout
			return NOTHING;
		else if (desc_ready < 0)
			return FAILURE;

		result_type res = SUCCESS;

		for (int fd = 0;  desc_ready > 0 and fd <= fds.maxFd;  ++fd)
		{
			if (FD_ISSET(fd, &working_set))
			{
				if (fd == listenFd) {		// new connections
					int new_fd = accept(listenFd, NULL, NULL);
					dbg("Listening socket has stuff");

					if (new_fd < 0) {
						if (errno != EWOULDBLOCK) {
							perror("accept() failed");
						}
					}
					fds.setFd(new_fd);
				} else {					// known connections
					result_type processRes = processMessage(descriptor_pair(fd, DESCRIPTOR_SOCK));

					if (processRes == FAILURE) {
						std::cout << "FAILURE WHILE PROCESSING MESSAGE" << std::endl;
						for (const descriptor_pair & desc : failedPeers) {
							actOnFailure(desc);	// virtual call
						}
						failedPeers.clear();
					}
				}
				--desc_ready;
			}
		}

		return res;
	}


#ifndef DISABLE_LIBNICE
	void Node::libniceRecv(NiceAgent * agent, guint stream_id, guint component_id, guint len, gchar * buf, gpointer user_data)
	{
		//std::cout << "cb_nice_recv with len " << len << std::endl;

		NiceReceivedData & data = receivedData[stream_id];

		while (len > 0)
		{
			if (data.type == NONE) {
				data.type = buf[0];
				buf++;
				len--;
			}

			if (len == 0) {
				return;
			}

			uint nNewBytes = 0;

			if (data.size == 0)		// size not yet set
			{
				if (data.readSizeBytes == 0 and len >= sizeof(size_type))		// buf contains whole size -> read directly
				{
					data.size = ((size_type *) buf)[0];
					data.bytes = (char *) malloc(data.size);
					nNewBytes = sizeof(size_type);
				}
				else							// incomplete read (either some bytes of "size" were already read or buf doesn't contain all bytes of "size")
				{
					char * address = data.sizeBytes + data.readSizeBytes;

					if (data.readSizeBytes + len >= sizeof(size_type))		// buf contains enough bytes to fill remaining size value
					{
						nNewBytes = sizeof(size_type) - data.readSizeBytes;
						memcpy(address, buf, nNewBytes);		// read remaining bytes
						data.readSizeBytes += nNewBytes;

						// NODE: the following line gives a warning and will not work for big-endian systems
						data.size = *((size_type *) data.sizeBytes);	// set size and allocate space for data
						data.bytes = (char *) malloc(data.size);
					}
					else
					{
						nNewBytes = len;
						memcpy(address, buf, nNewBytes);				// else save partial bytes of size
						data.readSizeBytes += nNewBytes;
					}
				}

				buf += nNewBytes;
				len -= nNewBytes;
			}

			/*if (! ((data.size != 0) == (len > 0))) {
				std::cout << "size: " << data.size << std::endl;
				std::cout << "len: " << len << std::endl;
				uint a;
				std::cin >> a;
			}*/

			if (data.size != 0)		// size is set -> read message
			{
				char * address = data.bytes + data.readBytes;

				if (data.readBytes + len >= data.size)		// buf contains enough bytes to fill remaining data
				{
					nNewBytes = data.size - data.readBytes;
					memcpy(address, buf, nNewBytes);		// read remaining bytes
					data.readBytes += nNewBytes;

					//std::cout << "processMessage through libnice" << std::endl;

					result_type processRes = instance->processMessage(descriptor_pair(stream_id, DESCRIPTOR_NICE));

					if (processRes == FAILURE) {
						std::cout << "FAILURE WHILE PROCESSING MESSAGE" << std::endl;
						for (const descriptor_pair & desc : instance->failedPeers) {
							instance->actOnFailure(desc);	// virtual call
						}
						instance->failedPeers.clear();
					}

					data = NiceReceivedData();
				}
				else
				{
					nNewBytes = len;
					memcpy(address, buf, nNewBytes);				// else save partial bytes of size
					data.readBytes += nNewBytes;
				}

				buf += nNewBytes;
				len -= nNewBytes;
			}
		}
	}
#endif


	result_type Node::processMessage(const descriptor_pair & sourceDesc)
	{
		dbg_f();
		result_type res;

		msg_type type = NONE;
		if (sourceDesc.type == DESCRIPTOR_SOCK) {
			res = recv_type_(sourceDesc.desc, 0, type);	// receive msg type (present in all messages)
			LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
		}
#ifndef DISABLE_LIBNICE
		else {
			type = instance->receivedData[sourceDesc.desc].type;
			res = SUCCESS;
		}
		if (res == FAILURE)
			return res;
#endif

		peer_id id = knownPeers.descriptorToId(sourceDesc);

		res = handleMessage(sourceDesc, id, type);		// virtual call

		if (res == NOTHING) {
			char * bytes = NULL;
			size_type size = 0;

			if (sourceDesc.type == DESCRIPTOR_SOCK) {
				res = recv_new_(sourceDesc.desc, 0, bytes, size);
				LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
			}
#ifndef DISABLE_LIBNICE
			else {
				NiceReceivedData & data = instance->receivedData[sourceDesc.desc];
				bytes = data.bytes;
				size = data.size;
			}
#endif
			bufferMessage(sourceDesc, id, bytes, size);
			res = SUCCESS;
		}

		return res;
	}

	//--------------------------------------------------
	// Queue messages
	//--------------------------------------------------

	void Node::bufferMessage(const descriptor_pair & sourceDesc, peer_id id, char * data, size_type size)
	{
		auto * q = queues[sourceDesc];
		MAIN_QUEUED_TYPE mainElem(id, q);
		mainQueue.enqueue(mainElem);

		QUEUED_TYPE elem(data, size);
		q->enqueue(elem);
	}


	bool Node::existsInQueues(const descriptor_pair & desc)
	{
		return queues.count(desc) > 0;
	}


	void Node::preparePeerQueues(const descriptor_pair & desc)
	{
		auto * q = new BlockingQueue<QUEUED_TYPE>();
		queues[desc] = q;
		invalidReferences[q] = 0;
	}


	void Node::removePeerQueues(const descriptor_pair & desc)
	{
		auto * q = queues[desc];
		q->forceQuit();

		// inefficient process, but is only done on peer de-registers
		std::queue<MAIN_QUEUED_TYPE> backup;

		while (mainQueue.size() > 0) {	// copy all main queue elements that are not of peer
			MAIN_QUEUED_TYPE elem;
			mainQueue.dequeue(elem);
			if (elem.second != q) {
				backup.push(elem);
			}
		}

		while (backup.size() > 0) {	// copy everything back
			MAIN_QUEUED_TYPE elem;
			mainQueue.enqueue(backup.front());
			backup.pop();
		}

		invalidReferences.erase(q);
		delete q;
		queues.erase(desc);

		//std::cout << "erased " << desc.desc << " " << (int)desc.type << std::endl;
		//std::cout << "qs size " << queues.size() << std::endl;
	}

	//--------------------------------------------------
	// Termination methods
	//--------------------------------------------------

	void Node::terminateStatusOn()
	{
		// lock scope
		{
			std::lock_guard<std::mutex> lockWhileInsideScope(stopMutex);
			shouldStop = true;
			stopCondVar.notify_all();
		}
	}


	void Node::terminateReceiverThread()
	{
		if (receiverThread != NULL) {
			//receiverThread->join();
			delete receiverThread;
			receiverThread = NULL;
		}
	}


	void Node::terminateQueueReads()
	{
		mainQueue.forceQuit();
		for (auto & q : queues) {
			q.second->forceQuit();
		}
	}

	//--------------------------------------------------
	// Miscellaneous methods
	//--------------------------------------------------

	void Node::deregisterPeer(const descriptor_pair & sourceDesc, peer_id id)
	{
		if (knownPeers.idExists(id)) {
			std::cout << "Node::deregisterPeer" << std::endl;
			removePeerQueues(sourceDesc);

			auto it = std::find(prevPeers.begin(), prevPeers.end(), id);
			if (it != prevPeers.end()) {
				prevPeers.erase(it);
			}

			it = std::find(nextPeers.begin(), nextPeers.end(), id);
			if (it != nextPeers.end()) {
				nextPeers.erase(it);
			}

			knownPeers.deregisterPeer(sourceDesc);
		}
	}


	void Node::actOnFailure(const descriptor_pair & sourceDesc)
	{
		std::cout << "Node::actOnFailure" << std::endl;
		if (sourceDesc.type == DESCRIPTOR_SOCK)
		{
			close(sourceDesc.desc);
			fds.unsetFd(sourceDesc.desc);
		}
		else if (sourceDesc.type == DESCRIPTOR_SOCK)
		{
#ifndef DISABLE_LIBNICE
			instance->receivedData.erase(sourceDesc.desc);
#endif
		}
	}

	//--------------------------------------------------
	//
	//--------------------------------------------------
}
