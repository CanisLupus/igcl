#include "Coordinator.hpp"
#include "Utils.hpp"

#include <iostream>
#include <cassert>
#include <fstream>
#include <algorithm>
#include <utility>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>


namespace igcl
{
	//--------------------------------------------------
	// Constructor/destructor
	//--------------------------------------------------

	Coordinator::Coordinator(int ownPort) : Node(ownPort)
	{
		ownId = 0;
		currentId = 1;
		readyPeers = 0;
		layout = GroupLayout::getFreeMasterWorkersLayout();
		usingDfltCallbacks = true;
		callbacks = new CoordinatorCallbacks();
		callbacks->setOwner(this);
		preparePeerQueues(descriptor_pair(ownId, DESCRIPTOR_NONE));		// self-send queue
	}


	Coordinator::~Coordinator()
	{
		if (usingDfltCallbacks) {
			usingDfltCallbacks = false;
			delete callbacks;
		}
	}

	//--------------------------------------------------
	// Public methods
	//--------------------------------------------------

	void Coordinator::setCallbacks(CoordinatorCallbacks * callbacks)
	{
		if (usingDfltCallbacks) {
			usingDfltCallbacks = false;
			delete callbacks;
		}
		this->callbacks = callbacks;
		callbacks->setOwner(this);
	}


	void Coordinator::setLayout(const GroupLayout & layout)
	{
		if (!layout.isFreeformed()) {
			prevPeers = layout.getPreviousOf(0);
			nextPeers = layout.getNextOf(0);
		}
		this->layout = layout;
	}


	uint Coordinator::getNPeers()
	{
		return layout.size();
	}


	result_type Coordinator::waitForNodes(uint n)
	{
		std::unique_lock<std::mutex> uniqueLock(nPeersMutex);
		while (readyPeers < n-1 and !shouldStop) {
			nPeersCondVar.wait(uniqueLock);
		}
		uniqueLock.unlock();

		return shouldStop ? FAILURE : SUCCESS;
	}


	void Coordinator::start()
	{
		bindReceivingSocket();
		threadedLoop();
		callbacks->cbStart();
	}


	void Coordinator::terminate()
	{
		terminateStatusOn();
		terminateReceiverThread();
		terminateQueueReads();

		nPeersCondVar.notify_one();

		for (descriptor_pair desc : knownPeers.getAllDescriptors())	// ask every peer to shutdown
		{
			if (desc.type == DESCRIPTOR_SOCK) {
				send_type_(desc.desc, SHUTDOWN);
			} else {
				// it never happens in the coordinator :)
			}
		}
		callbacks->cbTerminate();
	}

	//--------------------------------------------------
	// Message handling methods
	//--------------------------------------------------

	result_type Coordinator::whenPeerRegisters(const descriptor_pair & sourceDesc)
	{
		TEST() std::cout << "handlePeerRegister" << std::endl;
		result_type res;
		int sourceFd = sourceDesc.desc;

		peer_id id = currentId++;
		knownPeers.registerPeer(descriptor_pair(sourceFd, DESCRIPTOR_SOCK), id);
		res = send_(sourceFd, id);	// respond with id
		LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);

		TEST() std::cout << "gave ID " << id << std::endl;
		preparePeerQueues(sourceDesc);

		// disable Nargle's algorithm
		//int flag = 1;
		//int result = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (char *) &flag, sizeof(int));

		res = send_(sourceFd, (layout.isFreeformed() ? 0 : getNPeers()));
		LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);

		TEST() std::cout << "layout is not NULL" << std::endl;
		const std::vector<peer_id> & prev = layout.getPreviousOf(id);
		const std::vector<peer_id> & next = layout.getNextOf(id);

		if (!layout.isFreeformed()) {
			res = send_(sourceFd, &prev[0], prev.size());	// send as arrays
			res = send_(sourceFd, &next[0], next.size());
			LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
		} else {
			layout.addNode(id);
		}

		std::set<peer_id> connectableSet;

		for (peer_id id : prev) {
			if (knownPeers.idExists(id)) {
				connectableSet.insert(id);
			}
		}
		for (peer_id id : next) {
			if (knownPeers.idExists(id)) {
				connectableSet.insert(id);
			}
		}

		std::vector<peer_id> connectable(connectableSet.begin(), connectableSet.end());
		res = send_(sourceFd, &connectable[0], connectable.size());
		LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);

		return res;
	}


	result_type Coordinator::whenPeerDeregisters(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		actOnFailure(sourceDesc);
		return SUCCESS;
	}


	result_type Coordinator::whenPeerRequestsTargetCredentials(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		TEST() std::cout << "normal connection (requested from " << sourceId << ")";
		int sourceFd = sourceDesc.desc;
		result_type res = SUCCESS;
		peer_id targetId;

		res = recv_(sourceFd, 0, targetId);
		LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
		TEST() std::cout << " to " << targetId << std::endl;
		TEST() std::cout << "received source creds (from " << sourceId << ")" << std::endl;

		if (knownPeers.idExists(targetId) and layout.areConnected(sourceId, targetId))
		{
			TEST() std::cout << "requested ID is registered. proceeding" << std::endl;
			const descriptor_pair & targetDesc = knownPeers.idToDescriptor(targetId);
			int targetFd = targetDesc.desc;
			res = send_type_(targetFd, GET_PEER_CREDENTIALS);
			res = send_(targetFd, sourceId);
			QUIT_IF_UNSUCCESSFUL(res);
			LOG_AND_QUIT_IF_UNSUCCESSFUL(res, targetDesc);

			std::pair<peer_id, peer_id> idPair = {sourceId, targetId};
			credentialsRequests[idPair] = peer_credentials();
			TEST() std::cout << "sent request for credentials (to " << targetId << ")" << std::endl;
		} else {
			TEST() std::cout << "requested ID is NOT registered" << std::endl;
			res = send_type_(sourceFd, GIVE_PEER_CREDENTIALS);
			res = send_(sourceFd, std::string(""));
			LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
		}

		TEST() std::cout << "-----------------------------" << std::endl;
		return res;
	}


	result_type Coordinator::whenTargetProvidesCredentials(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		peer_id targetId = sourceId;	// the source of this message is the target of the request for credentials
		int targetFd = sourceDesc.desc;
		result_type res = SUCCESS;

		int targetAddrPort;
		peer_id requesterId;
		res = recv_(targetFd, 0, targetAddrPort);	// receive target credentials
		res = recv_(targetFd, 0, requesterId);
		LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
		TEST() std::cout << "received target creds (from " << targetId << ")" << std::endl;

		std::pair<peer_id, peer_id> idPair = {requesterId, targetId};

		if (credentialsRequests.count(idPair) > 0) {		// if request for this data indeed existed
			credentialsRequests.erase(idPair);

			sockaddr addr;
			socklen_t addrlen = sizeof(addr);
			int rc;

			rc = getpeername(targetFd, &addr, &addrlen);
			assert(rc == 0);
			std::string targetIp(inet_ntoa((*(sockaddr_in*)&addr).sin_addr));
			TEST() std::cout << "IP address of target (from getpeername): " << targetIp << std::endl;

			const descriptor_pair & requesterDesc = knownPeers.idToDescriptor(requesterId);
			int requesterFd = requesterDesc.desc;

			rc = getpeername(requesterFd, &addr, &addrlen);
			assert(rc == 0);
			std::string requesterIp(inet_ntoa((*(sockaddr_in*)&addr).sin_addr));

			res = send_type_(requesterFd, GIVE_PEER_CREDENTIALS);
			if (requesterIp == targetIp) {
				TEST() std::cout << "target is in the same network as requester" << std::endl;
				res = send_(requesterFd, std::string("local"));
			}
			res = send_(requesterFd, targetIp);		// give target credentials to source
			res = send_(requesterFd, targetAddrPort);
			res = send_(requesterFd, targetId);
			LOG_AND_QUIT_IF_UNSUCCESSFUL(res, requesterDesc);
			TEST() std::cout << "sent target creds to source (" << requesterId << ")" << std::endl;
		}

		return res;
	}


#ifndef DISABLE_LIBNICE
	result_type Coordinator::whenPeerRequestsTargetNiceCredentials(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		std::cout << "libnice connection (requested from " << sourceId << ")";
		int sourceFd = sourceDesc.desc;
		result_type res = SUCCESS;

		peer_id targetId;
		std::string sourceCandidates;
		res = recv_(sourceFd, 0, targetId);
		res = recv_(sourceFd, 0, sourceCandidates);			// receive source credentials and candidates
		LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
		std::cout << " to " << targetId << std::endl;

		if (knownPeers.idExists(targetId))		// is registered (exists)
		{
			TEST() std::cout << "requested ID is registered. proceeding" << std::endl;
			const descriptor_pair & targetDesc = knownPeers.idToDescriptor(targetId);
			int targetFd = targetDesc.desc;
			res = send_type_(targetFd, GET_NICE_PEER_CREDENTIALS);
			res = send_(targetFd, sourceId);
			LOG_AND_QUIT_IF_UNSUCCESSFUL(res, targetDesc);

			std::pair<peer_id, peer_id> idPair = {sourceId, targetId};//std::make_pair(sourceId, targetId);
			peer_credentials_nice sourceCredentials;
			sourceCredentials.candidates = sourceCandidates;
			credentialsRequestsNice[idPair] = sourceCredentials;
			TEST() std::cout << "sent request for nice credentials (to " << targetId << ")" << std::endl;
		} else {
			TEST() std::cout << "requested ID is NOT registered" << std::endl;
			res = send_type_(sourceFd, GIVE_NICE_PEER_CREDENTIALS);
			res = send_(sourceFd, std::string(""));
			LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
		}

		return res;
	}


	result_type Coordinator::whenTargetProvidesNiceCredentials(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		peer_id targetId = sourceId;	// the source of this message is the target of the request for credentials
		int targetFd = sourceDesc.desc;
		result_type res = SUCCESS;

		std::string targetCandidates;
		peer_id requesterId;
		res = recv_(targetFd, 0, targetCandidates);			// receive target credentials and candidates
		res = recv_(targetFd, 0, requesterId);
		LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
		TEST() std::cout << "received target creds (from " << targetId << ")" << std::endl;

		std::pair<peer_id, peer_id> idPair = {requesterId, targetId};

		if (credentialsRequestsNice.count(idPair) > 0) {		// if request for this data indeed existed
			std::string requesterCandidates = credentialsRequestsNice[idPair].candidates;
			credentialsRequestsNice.erase(idPair);

			const descriptor_pair & requesterDesc = knownPeers.idToDescriptor(requesterId);
			int requesterFd = requesterDesc.desc;
			res = send_type_(requesterFd, GIVE_NICE_PEER_CREDENTIALS);
			res = send_(requesterFd, targetCandidates);		// give target credentials to source (includes ID)
			res = send_(requesterFd, targetId);
			LOG_AND_QUIT_IF_UNSUCCESSFUL(res, requesterDesc);
			TEST() std::cout << "sent target creds to source (" << requesterId << ")" << std::endl;

			res = send_type_(targetFd, GIVE_NICE_PEER_CREDENTIALS);
			res = send_(targetFd, requesterCandidates);		// give source credentials to target (includes ID)
			res = send_(targetFd, requesterId);
			LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
			TEST() std::cout << "sent source creds to target (" << targetId << ")" << std::endl;
		}

		return res;
	}
#endif


	result_type Coordinator::whenReceivedSetRelayedConnection(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res;
		peer_id targetId;
		res = recv_(sourceDesc.desc, 0, targetId);
		LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);
		TEST() std::cout << "received \"set relayed connection\" (from " << sourceId << " to " << targetId << ")" << std::endl;

		if (knownPeers.idExists(targetId)) {
			descriptor_pair targetDesc = knownPeers.idToDescriptor(targetId);
			if (layout.areConnected(sourceId, targetId)) {
				res = send_type_(targetDesc.desc, SET_RELAYED_CONNECTION);
				res = send_(targetDesc.desc, sourceId);
			}
		}

		return res;
	}


	result_type Coordinator::whenPeerIsReady(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		TEST() std::cout << "peer is ready " << sourceId << std::endl;
		result_type res = SUCCESS;
		++readyPeers;
		nPeersCondVar.notify_all();
		callbacks->cbNewPeerReady(sourceId);
		if (layout.isFreeformed()) {
			nextPeers.push_back(sourceId);
		}
		return res;
	}


	result_type Coordinator::whenReceivedRelayedSendTo(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res;
		int sourceFd = sourceDesc.desc;

		peer_id id = 0;
		res = recv_(sourceFd, 0, id);

		char * bytes = NULL; uint size = 0;
		res = recv_new_(sourceFd, 0, bytes, size);
		LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);

		if (layout.areConnected(sourceId, id)) {
			res = sendToPeerRelayed(id, sourceId, bytes, size);
		}
		free(bytes);

		return res;
	}


	result_type Coordinator::whenReceivedRelayedSendToAll(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res = SUCCESS;
		int sourceFd = sourceDesc.desc;

		char * bytes = NULL;
		uint size = 0;
		res = recv_new_(sourceFd, 0, bytes, size);
		LOG_AND_QUIT_IF_UNSUCCESSFUL(res, sourceDesc);

		if (layout.isFreeformed()) {
			res = sendToAllRelayed(sourceDesc, sourceId, bytes, size);
		}
		free(bytes);

		return res;
	}


	result_type Coordinator::whenReceivedBarrierArrival(const descriptor_pair & sourceDesc, peer_id sourceId)
	{
		result_type res = SUCCESS;

		barrierBlockedPeers.insert(sourceId);

		if (barrierBlockedPeers.size() == knownPeers.size())
		{
			res = barrierReplyToAll();
			barrierBlockedPeers.clear();
		}

		return res;
	}


	result_type Coordinator::handleMessage(const descriptor_pair & sourceDesc, peer_id sourceId, msg_type type)
	{
		switch (type)
		{
			case REGISTER:
			{
				dbg("msg type -> REGISTER");
				return whenPeerRegisters(sourceDesc);
			}

			case DEREGISTER:
			{
				dbg("msg type -> DEREGISTER");
				return whenPeerDeregisters(sourceDesc, sourceId);
			}

			case REQUEST_PEER_CREDENTIALS:
			{
				dbg("msg type -> REQUEST_PEER_CREDENTIALS");
				return whenPeerRequestsTargetCredentials(sourceDesc, sourceId);
			}

			case PROVIDE_PEER_CREDENTIALS:
			{
				dbg("msg type -> PROVIDE_PEER_CREDENTIALS");
				return whenTargetProvidesCredentials(sourceDesc, sourceId);
			}

#ifndef DISABLE_LIBNICE
			case REQUEST_NICE_PEER_CREDENTIALS:
			{
				dbg("msg type -> REQUEST_NICE_PEER_CREDENTIALS");
				return whenPeerRequestsTargetNiceCredentials(sourceDesc, sourceId);
			}

			case PROVIDE_NICE_PEER_CREDENTIALS:
			{
				dbg("msg type -> PROVIDE_NICE_PEER_CREDENTIALS");
				return whenTargetProvidesNiceCredentials(sourceDesc, sourceId);
			}
#endif

			case SET_RELAYED_CONNECTION:
			{
				dbg("msg type -> SET_RELAYED_CONNECTION");
				return whenReceivedSetRelayedConnection(sourceDesc, sourceId);
			}

			case READY:
			{
				dbg("msg type -> READY");
				return whenPeerIsReady(sourceDesc, sourceId);
			}

			case SEND_TO_PEER_RELAYED:
			{
				dbg("msg type -> SEND_TO_PEER_RELAYED");
				return whenReceivedRelayedSendTo(sourceDesc, sourceId);
			}

			case SEND_TO_ALL_RELAYED:
			{
				dbg("msg type -> SEND_TO_ALL_RELAYED");
				return whenReceivedRelayedSendToAll(sourceDesc, sourceId);
			}

			case BARRIER:
			{
				dbg("msg type -> BARRIER");
				return whenReceivedBarrierArrival(sourceDesc, sourceId);
			}

			default:
			{
				dbg("msg type -> no known type. message passed to user");
				return NOTHING;
			}
		}
	}

	//--------------------------------------------------
	// Virtual methods' implementations
	//--------------------------------------------------

	void Coordinator::deregisterPeer(const descriptor_pair & sourceDesc, peer_id id)
	{
		std::cout << "Coordinator::deregisterPeer" << std::endl;
		Node::deregisterPeer(sourceDesc, id);

		layout.removeNode(id);

		std::vector< std::pair<peer_id, peer_id> > toDelete;

		for (auto & elem : credentialsRequests)
			if (elem.first.second == id)
				toDelete.push_back(elem.first);

		for (auto & elem : toDelete)
			credentialsRequests.erase(elem);

		toDelete.clear();

		for (auto & elem : credentialsRequestsNice)
			if (elem.first.second == id)
				toDelete.push_back(elem.first);

		for (auto & elem : toDelete)
			credentialsRequestsNice.erase(elem);

		barrierBlockedPeers.erase(id);
	}


	void Coordinator::actOnFailure(const descriptor_pair & sourceDesc)
	{
		std::cout << "Coordinator::actOnFailure" << std::endl;
		Node::actOnFailure(sourceDesc);

		if (!layout.isFreeformed()) {
			terminate();
			return;
		}

		peer_id id = knownPeers.descriptorToId(sourceDesc);

		if (knownPeers.idExists(id)) {
			deregisterPeer(sourceDesc, id);

			const std::vector<peer_id> & next = layout.getNextOf(id);
			std::vector<peer_id> connected = layout.getPreviousOf(id);
			connected.insert(connected.end(), next.begin(), next.end());

			for (peer_id targetId : connected)
			{
				if (!knownPeers.idExists(targetId)) {
					continue;
				}
				descriptor_pair desc = knownPeers.idToDescriptor(targetId);

				if (desc.type == DESCRIPTOR_SOCK) {
					send_type_(desc.desc, DEREGISTER_PEER);
					send_(desc.desc, id);
				} else {
					// it never happens in the coordinator :)
				}
			}
		}
	}


	int Coordinator::getCoordinatorFd()
	{
		return 0;
	}

	// ------------------------------------------------------
	//
	// ------------------------------------------------------
}
