#ifndef COORDINATOR_HPP_
#define COORDINATOR_HPP_

#include "Common.hpp"
#include "GroupLayout.hpp"
#include "Peer.hpp"
#include "Debug.hpp"

#include <vector>
#include <mutex>
#include <condition_variable>


namespace igcl
{
	class Coordinator : public Node
	{
		// ======================================================
		// =================== DEFINITIONS ======================
		// ======================================================

		struct peer_credentials
		{
		};

		struct peer_credentials_nice
		{
			std::string candidates;
		};

		// ======================================================
		// ==================== ATTRIBUTES ======================
		// ======================================================
	private:
		peer_id currentId;
		std::set<peer_id> barrierBlockedPeers;
		uint readyPeers;

		std::mutex nPeersMutex;
		std::condition_variable nPeersCondVar;

		GroupLayout layout;

		std::map<std::pair<peer_id, peer_id>, peer_credentials>      credentialsRequests;
		std::map<std::pair<peer_id, peer_id>, peer_credentials_nice> credentialsRequestsNice;

		CoordinatorCallbacks * callbacks;
		bool usingDfltCallbacks;

		// ======================================================
		// ===================== METHODS ========================
		// ======================================================
	public:
		Coordinator(int ownPort);
		virtual ~Coordinator();

		void setCallbacks(CoordinatorCallbacks * callbacks);
		void setLayout(const GroupLayout & layout);

		virtual uint getNPeers();
		result_type waitForNodes(uint n);

		virtual void start();
		virtual void terminate();

	private:
		result_type whenPeerRegisters(const descriptor_pair & desc);
		result_type whenPeerDeregisters(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenPeerRequestsTargetCredentials(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenTargetProvidesCredentials(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenPeerRequestsTargetNiceCredentials(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenTargetProvidesNiceCredentials(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenReceivedSetRelayedConnection(const descriptor_pair & sourceDesc, peer_id sourceId);
		result_type whenPeerIsReady(const descriptor_pair & desc, peer_id id);

		result_type whenReceivedRelayedSendTo(const descriptor_pair & desc, peer_id id);
		result_type whenReceivedRelayedSendToAll(const descriptor_pair & desc, peer_id id);
		result_type whenReceivedBarrierArrival(const descriptor_pair & desc, peer_id id);

		virtual result_type handleMessage(const descriptor_pair & desc, peer_id id, msg_type type);
		virtual void deregisterPeer(const descriptor_pair & sourceDesc, peer_id id);
		virtual void actOnFailure(const descriptor_pair & sourceDesc);
		virtual int getCoordinatorFd();

	public:
		// ------------------------------------------------------
		// Public messaging methods
		// ------------------------------------------------------

		template <typename ...T>
		result_type sendToAll(T && ...data)
		{
			result_type final = SUCCESS;

			for (descriptor_pair desc : knownPeers.getAllDescriptors()) {
				if (desc.type == DESCRIPTOR_SOCK) {
					result_type res;
					res = send_type_(desc.desc, SEND_TO_PEER);
					res = send_(desc.desc, std::forward<T>(data)...);
					if (res != SUCCESS) final = res;
				} else {
					// it never happens in the coordinator :)
				}
			}

			return final;
		}

		// ------------------------------------------------------
		// Private messaging methods
		// ------------------------------------------------------

	private:
		template <typename ...T>
		result_type sendToPeerRelayed(peer_id id, peer_id sourceId, T && ...data)
		{
			if (!knownPeers.idExists(id))
				return FAILURE;

			const descriptor_pair & desc = knownPeers.idToDescriptor(id);
			result_type res = SUCCESS;

			if (desc.type == DESCRIPTOR_SOCK) {
				res = send_type_(desc.desc, SEND_TO_PEER_RELAYED);
				res = send_(desc.desc, sourceId);	// send identifier of source
				res = send_(desc.desc, std::forward<T>(data)...);
				LOG_AND_QUIT_IF_UNSUCCESSFUL(res, desc);
			} else {
				// it never happens in the coordinator :)
			}

			return res;
		}


		template <typename ...T>
		result_type sendToAllRelayed(const descriptor_pair & sourceDesc, peer_id sourceId, T && ...data)
		{
			result_type final = SUCCESS;

			for (const descriptor_pair & desc : knownPeers.getAllDescriptors()) {
				if (desc == sourceDesc)		//do not send to requester
					continue;

				std::cout << "sendToAllRelayed " << desc.desc << std::endl;

				if (desc.type == DESCRIPTOR_SOCK) {
					result_type res;
					res = send_type_(desc.desc, SEND_TO_PEER_RELAYED);
					res = send_(desc.desc, sourceId);	// send identifier of source
					res = send_(desc.desc, std::forward<T>(data)...);
					if (res != SUCCESS) {
						final = res;
						failedPeers.insert(desc);
					}
				} else {
					// it never happens in the coordinator :)
				}
			}

			return final;
		}


		result_type barrierReplyToAll()
		{
			result_type final = SUCCESS;

			for (descriptor_pair desc : knownPeers.getAllDescriptors())
			{
				if (desc.type == DESCRIPTOR_SOCK) {
					result_type res = send_type_(desc.desc, BARRIER_REPLY);
					if (res != SUCCESS) {
						final = res;
						failedPeers.insert(desc);
					}
				} else {
					// it never happens in the coordinator :)
				}
			}

			return final;
		}

		// ------------------------------------------------------
		//
		// ------------------------------------------------------
	};
}

#endif /* COORDINATOR_HPP_ */
