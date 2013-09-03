#include "Peer.hpp"

#include <set>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
//#include <netinet/tcp.h>

#ifndef DISABLE_LIBNICE
#include <agent.h>
#endif


namespace igcl
{
	//--------------------------------------------------
	// Constructor/destructor
	//--------------------------------------------------

	Peer::Peer(int ownPort, const std::string & coordinatorIp, int coordinatorPort) : Node(ownPort)
	{
		coordinatorAddr.set(coordinatorIp, coordinatorPort);
		ownId = -1;
		started = false;
		ready = false;
		options.doRelayMessages = true;
	}


	Peer::~Peer()
	{
		// see "terminate" method instead
	}

	//--------------------------------------------------
	// Public methods
	//--------------------------------------------------

	void Peer::setAllowRelayedMessages(bool active)
	{
		if (!started) {
			options.doRelayMessages = active;
		}
	}


	result_type Peer::barrier()
	{
		result_type res;
		canExitBarrier = false;

		res = send_type_(coordinatorFd, BARRIER);
		QUIT_IF_UNSUCCESSFUL(res);

		std::unique_lock<std::mutex> lock(barrierMutex);
		while (!canExitBarrier) {
			barrierCondVar.wait(lock);
		}
		lock.unlock();

		return res;
	}


	void Peer::start()
	{
		bindReceivingSocket();
		registerWithCoordinator();
		//preparePeerQueues(descriptor_pair(0, DESCRIPTOR_NONE));		// self-send queue
		establishNextConnectionIfAvailable();
		started = true;
		threadedLoop();
	}


	void Peer::terminate()
	{
		terminateStatusOn();
		terminateReceiverThread();
		terminateQueueReads();

#ifndef DISABLE_LIBNICE
		nice.quit();
#endif

		for (descriptor_pair desc : knownPeers.getAllDescriptors()) {
			if (desc.type == DESCRIPTOR_SOCK) {
				close(desc.desc);
			} else if (desc.type == DESCRIPTOR_NICE) {
				// terminating the NiceAgent object is enough
			}
		}
		if (coordinatorFd >= 0) {
			close(coordinatorFd);
		}
	}

	uint Peer::getNPeers()
	{
		return nPeers;
	}

	//--------------------------------------------------
	// Auxiliar connection and registration methods
	//--------------------------------------------------

	int Peer::connectToPeer(const address & addr, bool retryOnError)
	{
		sockaddr_in sockAddr;
		memset(&sockAddr, 0, sizeof(sockAddr));

		sockAddr.sin_family = AF_INET;
		sockAddr.sin_port = htons(addr.port);
		if (inet_pton(AF_INET, addr.ip.c_str(), &sockAddr.sin_addr) <= 0) {
			std::cout << "\n inet_pton error occurred" << std::endl;
		}

		int fd = socket(AF_INET, SOCK_STREAM, 0);

		// disable Nargle's algorithm
		//int flag = 1;
		//int result = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

		int res;
		while ((res = connect(fd, (sockaddr *) &sockAddr, sizeof(sockAddr))) < 0 and retryOnError) {
			std::cout << "Error: Connect Failed, res: " << res << ", errno: " << errno << std::endl;
			std::cout << "trying again..." << errno << std::endl;
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
		}

		return (res < 0 ? -1 : fd);
	}


	void Peer::setNewPeerStructures(int descriptor, peer_id id, descriptor_type descType)
	{
		descriptor_pair desc(descriptor, descType);
		knownPeers.registerPeer(desc, id);
		preparePeerQueues(desc);
		if (descType == DESCRIPTOR_SOCK) {
			fds.setFd(descriptor);
		}
	}


	void Peer::printIds(peer_id * array, uint size, const std::string & name)
	{
		std::cout << name << ": [";
		for (uint i=0; i<size; ++i) {
			std::cout << " " << array[i];
		}
		std::cout << " ]" << std::endl;
	}

	//--------------------------------------------------
	// Connection and registration methods
	//--------------------------------------------------

	result_type Peer::registerWithCoordinator()
	{
		result_type res;

		coordinatorFd = connectToPeer(coordinatorAddr, true);

		res = send_type_(coordinatorFd, REGISTER);
		QUIT_IF_UNSUCCESSFUL(res);

		res = recv_(coordinatorFd, 0, ownId);			// receive registration ID
		QUIT_IF_UNSUCCESSFUL(res);
		std::cout << "ID: " << this->ownId << std::endl;

		setNewPeerStructures(coordinatorFd, 0, DESCRIPTOR_SOCK);	// the coordinator has always ID = 0

		res = recv_(coordinatorFd, 0, nPeers);
		QUIT_IF_UNSUCCESSFUL(res);

		this->usingFreeformLayout = (nPeers == 0);

		if (!this->usingFreeformLayout)
		{
			// receive all peers that are upstream or downstream of this
			peer_id * prev = NULL, * next = NULL;
			uint prevSize = 0, nextSize = 0;

			res = recv_new_(coordinatorFd, 0, prev, prevSize);
			QUIT_IF_UNSUCCESSFUL(res);

			res = recv_new_(coordinatorFd, 0, next, nextSize);
			QUIT_IF_UNSUCCESSFUL(res);

			printIds(prev, prevSize, "previous");
			printIds(next, nextSize, "next");

			this->prevPeers.assign(prev+0, prev+prevSize);
			this->nextPeers.assign(next+0, next+nextSize);

			free(prev);
			free(next);
		}

		// receive which next/prev IDs are already registered
		peer_id * connectable_array;
		uint connectableSize = 0;

		res = recv_new_(coordinatorFd, 0, connectable_array, connectableSize);
		QUIT_IF_UNSUCCESSFUL(res);
		connectable.insert(connectable_array+0, connectable_array+connectableSize);

		if (this->usingFreeformLayout) {
			this->nextPeers.assign(connectable.begin(), connectable.end());		// when there's no layout, all connectable peers are considered to be "next"
		}

		free(connectable_array);

		TEST() {
			std::cout << "connectable peers:" << std::endl;
			for (peer_id id : connectable)
				std::cout << "id: " << id << std::endl;
			std::cout << "---------------------" << std::endl;
		}

		return res;
	}


	result_type Peer::establishNextConnectionIfAvailable()
	{
		if (connectable.size() > 0) {
			auto it = connectable.begin();
			peer_id next = *it;
			connectable.erase(it);
#ifdef FORCE_LIBNICE
	#ifndef DISABLE_LIBNICE
			return requestNiceConnectionTo(next);	// this forces libnice for tests
	#endif
#else
	#ifdef FORCE_RELAYED
			return requestRelayedConnectionTo(next);
	#else
			return requestNormalConnectionTo(next);
	#endif
#endif
		} else {
			if (!this->ready) {
				return finishEstablishingConnections();
			}
			return SUCCESS;
		}
	}


	result_type Peer::finishEstablishingConnections()
	{
		result_type res;

#ifndef DISABLE_LIBNICE
		for (auto & pair : knownPeers.getAllDescriptors()) {
			if (pair.type == DESCRIPTOR_NICE) {
				nice.waitWritable(pair.desc);
			}
		}
#endif

		this->ready = true;
		res = send_type_(coordinatorFd, READY);
		std::cout << "ready" << std::endl;

		return res;
	}


	result_type Peer::requestNormalConnectionTo(peer_id id)
	{
		result_type res;

		// try to connect with normal sockets
		TEST() std::cout << "requestNormalConnectionTo " << id << std::endl;
		res = send_type_(coordinatorFd, REQUEST_PEER_CREDENTIALS);
		res = send_(coordinatorFd, id);
		TEST() std::cout << "sent request for creds" << std::endl;

		return res;
	}


#ifndef DISABLE_LIBNICE
	result_type Peer::requestNiceConnectionTo(peer_id id)
	{
		result_type res;

		// try to connect with libnice
		TEST() std::cout << "requestNiceConnectionTo " << id << std::endl;
		res = send_type_(coordinatorFd, REQUEST_NICE_PEER_CREDENTIALS);
		res = send_(coordinatorFd, id);

		uint streamId;
		std::string localInfo;
		nice.startNewStream(streamId, localInfo);
		res = send_(coordinatorFd, localInfo);
		TEST() std::cout << "sent request for creds" << std::endl;

		streamsForConnections[id] = streamId;

		return res;
	}
#endif


	result_type Peer::requestRelayedConnectionTo(peer_id id)
	{
		result_type res = SUCCESS;

		if (options.doRelayMessages) {
			// No connection. All data to this peer will be sent through the coordinator
			TEST() std::cout << "requestRelayedConnectionTo " << id << std::endl;
			descriptor_pair desc(id, DESCRIPTOR_NONE);
			knownPeers.registerPeer(desc, id);
			preparePeerQueues(desc);

			res = send_type_(coordinatorFd, SET_RELAYED_CONNECTION);
			res = send_(coordinatorFd, id);
		} else if (!this->usingFreeformLayout) {
			res = send_type_(coordinatorFd, DEREGISTER);
			res = FAILURE;
		}
		std::cout << knownPeers.toString() << std::endl;
		establishNextConnectionIfAvailable();

		return res;
	}

	//--------------------------------------------------
	// Message handling methods
	//--------------------------------------------------

	result_type Peer::whenPeerRegisters(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res = SUCCESS;
		int sourceFd = sourceDesc.desc;

		peer_id actualId;
		res = recv_(sourceFd, 0, actualId);
		TEST() std::cout << "peer " << actualId << " registering with this node " << std::endl;
		res = send_(sourceFd, this->ownId);
		LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
		setNewPeerStructures(sourceFd, actualId, DESCRIPTOR_SOCK);

		if (this->usingFreeformLayout) {
			this->nextPeers.push_back(actualId);
		}

		std::cout << knownPeers.toString() << std::endl;

		return res;
	}


	result_type Peer::whenCredentialsAreRequested(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res = SUCCESS;

		peer_id requesterId;
		res = recv_(coordinatorFd, 0, requesterId);			// receive source credentials
		QUIT_IF_UNSUCCESSFUL(res);
		TEST() std::cout << "peer " << requesterId << " requested credentials" << std::endl;

		res = send_type_(coordinatorFd, PROVIDE_PEER_CREDENTIALS);
		res = send_(coordinatorFd, ownAddr.port);
		res = send_(coordinatorFd, requesterId);

		return res;
	}


	result_type Peer::whenCredentialsAreProvided(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res = SUCCESS;

		address otherAddr;
		peer_id targetId;
		std::string targetIp;

		res = recv_(coordinatorFd, 0, targetIp);
		if (targetIp.length() == 0) {
			TEST() std::cout << "peer is not yet registered" << std::endl;
			establishNextConnectionIfAvailable();
			return NOTHING;
		}

		if (targetIp == "local") {
			TEST() std::cout << "peer is in the local network" << std::endl;
			res = recv_(coordinatorFd, 0, targetIp);	// get public IP, in case local connection fails and its needed
			otherAddr.ip = this->localIp;
		} else {
			otherAddr.ip = targetIp;
		}

		res = recv_(coordinatorFd, 0, otherAddr.port);
		res = recv_(coordinatorFd, 0, targetId);
		QUIT_IF_UNSUCCESSFUL(res);

		TEST() std::cout << "received creds " << targetId << " " << otherAddr << " (public IP: " << targetIp << ")"<<  std::endl;
		TEST() std::cout << "connecting using sockets" << std::endl;

		int fd = connectToPeer(otherAddr, false);	// connect (here, otherAddr can be a local or public IP)

		if (fd == -1 and otherAddr.ip == localIp) {		// failed when connecting locally
			TEST() std::cout << "local connection failed. trying public IP" << std::endl;
			otherAddr.ip == targetIp;
			fd = connectToPeer(otherAddr, false);
		}

		if (fd == -1) {
#ifndef DISABLE_LIBNICE
			res = requestNiceConnectionTo(targetId);
#else
			res = requestRelayedConnectionTo(targetId);
#endif
			return res;
		}
		TEST() std::cout << "connected" << std::endl;

		res = send_type_(fd, REGISTER);
		res = send_(fd, this->ownId);
		QUIT_IF_UNSUCCESSFUL(res);

		peer_id arguedId;
		res = recv_(fd, 0, arguedId);
		QUIT_IF_UNSUCCESSFUL(res);
		if (arguedId != targetId) {
			return FAILURE;
		}
		TEST() std::cout << "registered with peer" << std::endl;

		setNewPeerStructures(fd, targetId, DESCRIPTOR_SOCK);
		std::cout << knownPeers.toString() << std::endl;
		TEST() std::cout << "end establishConnection" << std::endl;

		establishNextConnectionIfAvailable();

		return res;
	}


#ifndef DISABLE_LIBNICE
	result_type Peer::whenNiceCredentialsAreRequested(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res = SUCCESS;

		peer_id requesterId;
		res = recv_(coordinatorFd, 0, requesterId);			// receive source credentials
		QUIT_IF_UNSUCCESSFUL(res);
		TEST() std::cout << "peer " << requesterId << " requested nice credentials" << std::endl;

		uint streamId;
		std::string localInfo;
		nice.startNewStream(streamId, localInfo);

		res = send_type_(coordinatorFd, PROVIDE_NICE_PEER_CREDENTIALS);
		res = send_(coordinatorFd, localInfo);
		res = send_(coordinatorFd, requesterId);
		TEST() std::cout << "sent creds, as requested" << std::endl;

		streamsForConnections[requesterId] = streamId;

		return res;
	}

	result_type Peer::whenNiceCredentialsAreProvided(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res = SUCCESS;

		peer_id targetId;
		std::string remoteInfo;
		res = recv_(coordinatorFd, 0, remoteInfo);	// receive other peer credentials
		if (remoteInfo.length() == 0) {
			TEST() std::cout << "peer is not yet registered" << std::endl;
			establishNextConnectionIfAvailable();
			return NOTHING;
		}
		res = recv_(coordinatorFd, 0, targetId);
		TEST() std::cout << "received nice creds " << targetId << " " << remoteInfo <<  std::endl;

		int streamId = streamsForConnections[targetId];
		streamsForConnections.erase(targetId);
		TEST() std::cout << "connecting using libnice" << std::endl;

		if (nice.connect(streamId, remoteInfo)) {
			setNewPeerStructures(streamId, targetId, DESCRIPTOR_NICE);
			std::cout << knownPeers.toString() << std::endl;
		} else {
			res = requestRelayedConnectionTo(targetId);
		}
		res = establishNextConnectionIfAvailable();

		return res;
	}
#endif


	result_type Peer::whenReceivedSetRelayedConnection(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res;
		if (sourceDesc.desc != coordinatorFd) {
			return SUCCESS;
		}
		peer_id id;
		res = recv_(sourceDesc.desc, 0, id);
		LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
		TEST() std::cout << "received \"set relayed connection\" (from " << id << ")" << std::endl;

		descriptor_pair desc(id, DESCRIPTOR_NONE);	// yes, it is sourceId indeed
		knownPeers.registerPeer(desc, id);
		preparePeerQueues(desc);

		return res;
	}


	result_type Peer::whenCoordinatorDeregistersPeer(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		peer_id id;
		recv_(sourceDesc.desc, 0, id);
		if (knownPeers.idExists(id)) {
			actOnFailure(knownPeers.idToDescriptor(id));
		}
		return SUCCESS;
	}


	result_type Peer::whenReceivedRelayedMessage(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res = SUCCESS;

		peer_id senderId;
		char * bytes = NULL;
		size_type size = 0;

		res = recv_(sourceDesc.desc, 0, senderId);
		res = recv_new_(sourceDesc.desc, 0, bytes, size);
		QUIT_IF_UNSUCCESSFUL(res);

		descriptor_pair senderDesc = knownPeers.idToDescriptor(senderId);
		//descriptor_pair senderDesc(senderId, DESCRIPTOR_NONE);
		if (!existsInQueues(senderDesc)) {
			std::cout << "NOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO" << std::endl;
			preparePeerQueues(senderDesc);
		}

		bufferMessage(senderDesc, senderId, bytes, size);

		return res;
	}


	result_type Peer::whenReceivedBarrierUnblock(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res = SUCCESS;
		std::lock_guard<std::mutex> scoped_lock(barrierMutex);
		canExitBarrier = true;
		barrierCondVar.notify_one();
		return res;
	}


	result_type Peer::whenReceivedShutdownRequest(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res = SUCCESS;
		terminateStatusOn();
		terminateQueueReads();
		return res;
	}


	result_type Peer::handleMessage(const descriptor_pair & sourceDesc, peer_id id, msg_type type)
	{
		switch (type)
		{
			case REGISTER:
			{
				dbg("msg type -> REGISTER");
				return whenPeerRegisters(sourceDesc, id);
			}

			case GET_PEER_CREDENTIALS:
			{
				dbg("msg type -> GET_PEER_CREDENTIALS");
				return whenCredentialsAreRequested(sourceDesc, id);
			}

			case GIVE_PEER_CREDENTIALS:
			{
				dbg("msg type -> GIVE_PEER_CREDENTIALS");
				return whenCredentialsAreProvided(sourceDesc, id);
			}

#ifndef DISABLE_LIBNICE
			case GET_NICE_PEER_CREDENTIALS:
			{
				dbg("msg type -> GET_NICE_PEER_CREDENTIALS");
				return whenNiceCredentialsAreRequested(sourceDesc, id);
			}

			case GIVE_NICE_PEER_CREDENTIALS:
			{
				dbg("msg type -> GIVE_NICE_PEER_CREDENTIALS");
				return whenNiceCredentialsAreProvided(sourceDesc, id);
			}
#endif

			case SET_RELAYED_CONNECTION:
			{
				dbg("msg type -> SET_RELAYED_CONNECTION");
				return whenReceivedSetRelayedConnection(sourceDesc, id);
			}

			case DEREGISTER_PEER:
			{
				dbg("msg type -> DEREGISTER_PEER");
				return whenCoordinatorDeregistersPeer(sourceDesc, id);
			}

			case SEND_TO_PEER_RELAYED:
			{
				dbg("msg type -> SEND_TO_PEER_RELAYED");
				return whenReceivedRelayedMessage(sourceDesc, id);
			}

			case BARRIER_REPLY:
			{
				dbg("msg type -> BARRIER_REPLY");
				return whenReceivedBarrierUnblock(sourceDesc, id);
			}

			case SHUTDOWN:
			{
				dbg("msg type -> SHUTDOWN");
				return whenReceivedShutdownRequest(sourceDesc, id);
			}

			default:
			{
				dbg("msg type -> no type known to Peer class");
				return NOTHING;
			}
		}
	}

	//--------------------------------------------------
	// Virtual methods' implementations
	//--------------------------------------------------

	void Peer::deregisterPeer(const descriptor_pair & sourceDesc, peer_id id)
	{
		std::cout << "Peer::deregisterPeer" << std::endl;
		Node::deregisterPeer(sourceDesc, id);
		streamsForConnections.erase(id);
	}


	void Peer::actOnFailure(const descriptor_pair & sourceDesc)
	{
		std::cout << "Peer::actOnFailure" << std::endl;
		Node::actOnFailure(sourceDesc);

		peer_id id = knownPeers.descriptorToId(sourceDesc);

		if (!this->usingFreeformLayout or id == 0) {
			terminate();
			return;
		}

		deregisterPeer(sourceDesc, id);
	}


	int Peer::getCoordinatorFd()
	{
		return coordinatorFd;
	}

	// ------------------------------------------------------
	//
	// ------------------------------------------------------
}
