#ifndef PEER_HPP_
#define PEER_HPP_

#include "Node.hpp"
#include "Common.hpp"
#include "Debug.hpp"

#include <string>
#include <set>
#include <map>
#include <mutex>
#include <condition_variable>


namespace igcl
{
	class Peer : public Node
	{
		// ======================================================
		// ==================== DEFINITIONS =====================
		// ======================================================

		struct peer_options
		{
			bool doRelayMessages;
		};

		// ======================================================
		// ==================== ATTRIBUTES ======================
		// ======================================================
	protected:
		peer_options options;
		bool started;
		bool ready;

		int coordinatorFd;
		address coordinatorAddr;

		bool usingFreeformLayout;
		std::set<peer_id> connectable;
		std::map<peer_id, int> streamsForConnections;
		uint nPeers;

		std::mutex barrierMutex;
		std::condition_variable barrierCondVar;
		bool canExitBarrier;

		// ======================================================
		// ===================== METHODS ========================
		// ======================================================
	public:
		Peer(int ownPort, const std::string & coordinatorIp, int coordinatorPort);
		virtual ~Peer();

		void setAllowRelayedMessages(bool active);
		result_type barrier();

		virtual void start();
		virtual void terminate();
		virtual uint getNPeers();

	protected:
		int connectToPeer(const address & addr, bool retryOnError=false);
		void setNewPeerStructures(int descriptor, peer_id id, descriptor_type descType);
		void printIds(peer_id * array, uint size, const std::string & name);

		result_type registerWithCoordinator();		// peer registering with coordinator
		result_type establishNextConnectionIfAvailable();
		result_type finishEstablishingConnections();

		result_type requestNormalConnectionTo(peer_id id);
		result_type requestNiceConnectionTo(peer_id id);
		result_type requestRelayedConnectionTo(peer_id id);

		result_type whenPeerRegisters(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenCredentialsAreRequested(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenCredentialsAreProvided(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenNiceCredentialsAreRequested(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenNiceCredentialsAreProvided(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenReceivedSetRelayedConnection(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenCoordinatorDeregistersPeer(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenReceivedRelayedMessage(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenReceivedBarrierUnblock(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenReceivedShutdownRequest(const descriptor_pair & sourceDesc, peer_id sourceId);

		virtual result_type handleMessage(const descriptor_pair & desc, peer_id id, msg_type type);
		virtual void deregisterPeer(const descriptor_pair & sourceDesc, peer_id id);
		virtual void actOnFailure(const descriptor_pair & sourceDesc);
		virtual int getCoordinatorFd();

	public:
		// ------------------------------------------------------
		// Public messaging methods
		// ------------------------------------------------------

		template <typename ...T>
		result_type sendToPeerThroughCoordinator(peer_id id, T && ...data)
		{
			result_type res;

			res = send_type_(coordinatorFd, SEND_TO_PEER_RELAYED);
			QUIT_IF_UNSUCCESSFUL(res);

			res = send_(coordinatorFd, id);
			QUIT_IF_UNSUCCESSFUL(res);

			res = send_(coordinatorFd, std::forward<T>(data)...);
			return res;
		}


		template <typename ...T>
		result_type sendToAllPeersThroughCoordinator(T && ...data)
		{
			result_type res;

			res = send_type_(coordinatorFd, SEND_TO_ALL_RELAYED);
			QUIT_IF_UNSUCCESSFUL(res);

			res = send_(coordinatorFd, std::forward<T>(data)...);
			return res;
		}

		// ------------------------------------------------------
		//
		// ------------------------------------------------------
	};
}

#endif /* PEER_HPP_ */
