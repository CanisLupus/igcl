#ifndef NODE_HPP_
#define NODE_HPP_

#include "BlockingQueue.hpp"
#include "Communication.hpp"
#include "Common.hpp"
#include "Debug.hpp"

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <map>
#include <set>
#include <algorithm>
#include <functional>


namespace igcl
{
	class Node : public Communication
	{
		// ======================================================
		// ==================== DEFINITIONS =====================
		// ======================================================

#define LOG_AND_QUIT_IF_UNSUCCESSFUL(res,desc) if ((res) != igcl::SUCCESS) { failedPeers.insert(desc); return (res); }

#ifndef DISABLE_LIBNICE
		struct NiceReceivedData
		{
			msg_type type;
			size_type size;

			char * bytes;
			size_type readBytes;

			char sizeBytes[sizeof(size_type)];
			size_type readSizeBytes;

			NiceReceivedData() : type(NONE), size(0), bytes(NULL), readBytes(0), readSizeBytes(0) {}
		};
#endif

		typedef std::pair<void *, int> QUEUED_TYPE;
		typedef std::pair<peer_id, BlockingQueue<QUEUED_TYPE> *> MAIN_QUEUED_TYPE;

		// ======================================================
		// ==================== ATTRIBUTES ======================
		// ======================================================
	protected:
		peer_id ownId;
		address ownAddr;
		std::string localIp;

		SocketDescriptors fds;
		int listenFd;
		PeerTable knownPeers;
		std::vector<peer_id> prevPeers, nextPeers;
		std::set<descriptor_pair> failedPeers;

		std::thread * receiverThread;
		bool shouldStop;
		std::mutex stopMutex;
		std::condition_variable stopCondVar;

#ifndef DISABLE_LIBNICE
		static std::map<uint, NiceReceivedData> receivedData;
		static Node * instance;
#endif

	private:
		BlockingQueue<MAIN_QUEUED_TYPE> mainQueue;
		std::map<descriptor_pair, BlockingQueue<QUEUED_TYPE> * > queues;
		std::map<BlockingQueue<QUEUED_TYPE> *, uint > invalidReferences;

		// ======================================================
		// ===================== METHODS ========================
		// ======================================================
	public:
		Node(int ownPort);
		virtual ~Node();

		void hang();
		peer_id getId();
		virtual uint getNPeers() = 0;

		virtual void start() = 0;
		virtual void terminate() = 0;

	protected:
		void bindReceivingSocket();
		void threadedLoop();
		void loop();
		result_type doSelect(const timespec & timeout);
		result_type processMessage(const descriptor_pair & sourceDesc);
#ifndef DISABLE_LIBNICE
		static void libniceRecv(NiceAgent * agent, guint stream_id, guint component_id, guint len, gchar * buf, gpointer user_data);
#endif

		void bufferMessage(const descriptor_pair & sourceDesc, peer_id id, char * data, size_type size);
		bool existsInQueues(const descriptor_pair & desc);
		void preparePeerQueues(const descriptor_pair & desc);
		void removePeerQueues(const descriptor_pair & desc);

		void terminateStatusOn();
		void terminateReceiverThread();
		void terminateQueueReads();

		virtual result_type handleMessage(const descriptor_pair & sourceDesc, peer_id id, msg_type type) = 0;
		virtual void deregisterPeer(const descriptor_pair & sourceDesc, peer_id id);
		virtual void actOnFailure(const descriptor_pair & sourceDesc);
		virtual int getCoordinatorFd() = 0;

		//--------------------------------------------------
		// Helpers
		//--------------------------------------------------

	private:
		// removes the front elemnts of the main queue if they reference the queue "q"
		inline void invalidateFrontMainQueueReferencesTo(const BlockingQueue<QUEUED_TYPE> * q, uint & nInvalidRefs)
		{
			nInvalidRefs++;
			while (nInvalidRefs > 0 and mainQueue.peek().second == q) {
				assert(mainQueue.size() > 0);	// TODO: remove
				mainQueue.pop();
				nInvalidRefs--;
			}
		}

		//--------------------------------------------------
		// Public blocking receive methods
		//--------------------------------------------------

	public:
		template<typename T>
		result_type waitRecvFromAny(peer_id & id, T & value)
		{
			static_assert(!std::is_pointer<T>::value, "T must be non-pointer");
			T * data = NULL; uint size = 0;
			result_type res = waitRecvFromMainQueue(id, data, size);

			if (size != sizeof(value)) {
				free(data);
				return FAILURE;
			}

			value = *data;
			free(data);
			return res;
		}


		template<typename T>
		result_type waitRecvNewFromAny(peer_id & id, T * & data, uint & size)
		{
			static_assert(!std::is_pointer<T>::value, "T must be non-pointer");
			result_type res = waitRecvFromMainQueue(id, data, size);
			size = size / sizeof(T);
			return res;
		}


		template<typename T>
		result_type waitRecvNewFromAny(peer_id & id, T * & data)	// for when size is not needed
		{
			uint size;
			return waitRecvNewFromAny(id, data, size);
		}


		template<typename T>
		result_type waitRecvFrom(peer_id id, T & value)
		{
			static_assert(!std::is_pointer<T>::value, "T must be non-pointer");
			if (!knownPeers.idExists(id))
				return FAILURE;

			const descriptor_pair desc = knownPeers.idToDescriptor(id);
			BlockingQueue<QUEUED_TYPE> * q = queues[desc];

			T * data = NULL; uint size = 0;
			result_type res = waitRecvFromQueue(q, data, size);

			if (size != sizeof(value)) {
				free(data);
				return FAILURE;
			}

			value = *data;
			free(data);

			invalidateFrontMainQueueReferencesTo(q, invalidReferences[q]);

			return res;
		}


		template<typename T>
		result_type waitRecvNewFrom(peer_id id, T * & data, uint & size)
		{
			static_assert(!std::is_pointer<T>::value, "T must be non-pointer");
			if (!knownPeers.idExists(id))
				return FAILURE;

			descriptor_pair desc = knownPeers.idToDescriptor(id);
			BlockingQueue<QUEUED_TYPE> * q = queues[desc];

			result_type res = waitRecvFromQueue(q, data, size);
			size = size / sizeof(T);

			invalidateFrontMainQueueReferencesTo(q, invalidReferences[q]);

			return res;
		}


		template<typename T>
		result_type waitRecvNewFrom(peer_id id, T * & data)	// for when size is not needed
		{
			uint size;
			return waitRecvNewFrom(id, data, size);
		}

		//--------------------------------------------------
		// Public non-blocking receive methods
		//--------------------------------------------------

	public:
		template<typename T>
		result_type tryRecvFromAny(peer_id & id, T & value)
		{
			T * data = NULL; uint size = 0;
			result_type res = tryRecvFromMainQueue(id, data, size);
			QUIT_IF_UNSUCCESSFUL(res);

			if (size != sizeof(value)) {
				free(data);
				return FAILURE;
			}

			value = *data;
			free(data);
			return res;
		}


		template<typename T>
		result_type tryRecvNewFromAny(peer_id & id, T * & data, uint & size)
		{
			result_type res = tryRecvFromMainQueue(id, data, size);
			size = size / sizeof(T);	// no QUIT_IF_UNSUCCESSFUL needed. calculating size works even if size is invalid
			return res;
		}


		template<typename T>
		result_type tryRecvNewFromAny(peer_id & id, T * & data)	// for when size is not needed
		{
			uint size;
			return tryRecvNewFromAny(id, data, size);
		}


		template<typename T>
		result_type tryRecvFrom(peer_id id, T & value)
		{
			if (!knownPeers.idExists(id))
				return FAILURE;

			descriptor_pair desc = knownPeers.idToDescriptor(id);
			BlockingQueue<QUEUED_TYPE> * q = queues[desc];

			T * data; uint size;
			result_type res = tryRecvFromQueue(q, data, size);
			QUIT_IF_UNSUCCESSFUL(res);

			if (size != sizeof(value)) {
				free(data);
				return FAILURE;
			}

			value = *data;
			free(data);

			invalidateFrontMainQueueReferencesTo(q, invalidReferences[q]);

			return res;
		}


		template<typename T>
		result_type tryRecvNewFrom(peer_id id, T * & data, uint & size)
		{
			if (!knownPeers.idExists(id))
				return FAILURE;

			descriptor_pair desc = knownPeers.idToDescriptor(id);
			BlockingQueue<QUEUED_TYPE> * q = queues[desc];

			result_type res = tryRecvFromQueue(q, data, size);
			QUIT_IF_UNSUCCESSFUL(res);
			size = size / sizeof(T);

			invalidateFrontMainQueueReferencesTo(q, invalidReferences[q]);

			return res;
		}


		template<typename T>
		result_type tryRecvNewFrom(peer_id id, T * & data)	// for when size is not needed
		{
			uint size;
			return tryRecvNewFrom(id, data, size);
		}

		//--------------------------------------------------
		// Public send methods
		//--------------------------------------------------

	public:
		template <typename T>
		result_type sendToSelf(const T value)
		{
			uint nBytes = sizeof(value);
			char * newData = (char *) malloc(nBytes);
			memcpy(newData, &value, nBytes);
			bufferMessage(descriptor_pair(0, DESCRIPTOR_NONE), ownId, newData, nBytes);
			return SUCCESS;
		}


		template <typename T>
		result_type sendToSelf(const T * data, const uint size)
		{
			uint nBytes = size * sizeof(T);
			char * newData = (char *) malloc(nBytes);
			memcpy(newData, data, nBytes);
			bufferMessage(descriptor_pair(0, DESCRIPTOR_NONE), ownId, newData, nBytes);
			return SUCCESS;
		}


		template <typename ...T>
		result_type sendTo(peer_id id, T && ...data)
		{
			if (!knownPeers.idExists(id))
				return FAILURE;

			const descriptor_pair & desc = knownPeers.idToDescriptor(id);
			result_type res = auxiliarySendTo(desc, std::forward<T>(data)...);
			//LOG_AND_QUIT_IF_UNSUCCESSFUL(res, desc);
			return res;
		}


		template <typename ...T>
		result_type sendToAll(T && ...data)
		{
			result_type res, final = SUCCESS;

			for (const descriptor_pair & desc : knownPeers.getAllDescriptors()) {
				res = auxiliarySendTo(desc, std::forward<T>(data)...);
				if (res != SUCCESS) {
					final = res;
					//failedPeers.insert(desc);
				}
			}

			return final;
		}


		template <typename ...T>
		result_type sendToAllDownstream(T && ...data)
		{
			result_type res, final = SUCCESS;

			for (const peer_id id : downstreamPeers()) {
				const descriptor_pair & desc = knownPeers.idToDescriptor(id);
				res = auxiliarySendTo(desc, std::forward<T>(data)...);
				if (res != SUCCESS) {
					final = res;
				}
			}

			return final;
		}


		template <typename ...T>
		result_type sendToAllUpstream(T && ...data)
		{
			result_type res, final = SUCCESS;

			for (const peer_id id : upstreamPeers()) {
				const descriptor_pair & desc = knownPeers.idToDescriptor(id);
				res = auxiliarySendTo(desc, std::forward<T>(data)...);
				if (res != SUCCESS) {
					final = res;
				}
			}

			return final;
		}


	private:
		template <typename ...T>
		result_type auxiliarySendTo(const descriptor_pair & desc, T && ...data)
		{
			result_type res;

			if (desc.type == DESCRIPTOR_SOCK)
			{
				int fd = desc.desc;
				res = send_type_(fd, SEND_TO_PEER);
				res = send_(fd, std::forward<T>(data)...);
			}
#ifndef DISABLE_LIBNICE
			else if (desc.type == DESCRIPTOR_NICE)
			{
				uint streamId = desc.desc;
				res = nice_send_type_(streamId, SEND_TO_PEER);
				res = nice_send_(streamId, data...);
			}
#endif
			else {
				const int & coordinatorFd = getCoordinatorFd();
				peer_id id = knownPeers.descriptorToId(desc);
				res = send_type_(coordinatorFd, SEND_TO_PEER_RELAYED);
				res = send_(coordinatorFd, id);
				res = send_(coordinatorFd, std::forward<T>(data)...);
			}

			return res;
		}

		//--------------------------------------------------
		// Main-queue message receive methods
		//--------------------------------------------------

	private:
		template <bool BLOCKING, typename T>
		inline result_type recvFromMainQueue(peer_id & id, T * & data, uint & size)
		{
			MAIN_QUEUED_TYPE elem;
			BlockingQueue<QUEUED_TYPE> * q;
			result_type res = SUCCESS;

			while (1) {
				if (BLOCKING) {
					res = mainQueue.blockingDequeue(elem);
				} else {
					res = mainQueue.dequeue(elem);
				}
				QUIT_IF_UNSUCCESSFUL(res);

				q = elem.second;
				if (invalidReferences[q] == 0) {
					break;
				}
				invalidReferences[q]--;	// removed elem from main queue -> invalidate reference to q
			};

			id = elem.first;

			waitRecvFromQueue(q, data, size);
			return res;
		}


		template <typename T>
		result_type waitRecvFromMainQueue(peer_id & id, T * & data, uint & size)
		{
			return recvFromMainQueue<true>(id, data, size);
		}


		template <typename T>
		result_type tryRecvFromMainQueue(peer_id & id, T * & data, uint & size)
		{
			return recvFromMainQueue<false>(id, data, size);
		}

		//--------------------------------------------------
		// Individual-queue message receive methods
		//--------------------------------------------------

		template <bool BLOCKING, typename T>
		inline result_type recvFromQueue(BlockingQueue<QUEUED_TYPE> * q, T * & data, uint & size)
		{
			result_type res;
			QUEUED_TYPE elem;

			if (BLOCKING) {
				res = q->blockingDequeue(elem);
			} else {
				res = q->dequeue(elem);
			}
			QUIT_IF_UNSUCCESSFUL(res);

			data = (T *) elem.first;
			size = elem.second;

			return res;
		}


		template <typename T>
		result_type waitRecvFromQueue(BlockingQueue<QUEUED_TYPE> * q, T * & data, uint & size)
		{
			return recvFromQueue<true>(q, data, size);
		}


		template <typename T>
		result_type tryRecvFromQueue(BlockingQueue<QUEUED_TYPE> * q, T * & data, uint & size)
		{
			return recvFromQueue<false>(q, data, size);
		}

		// ------------------------------------------------------
		// Layout Methods
		// ------------------------------------------------------

	public:
		inline uint nDownstreamPeers()
		{
			return nextPeers.size();
		}


		inline uint nUpstreamPeers()
		{
			return prevPeers.size();
		}


		inline const std::vector<peer_id> & downstreamPeers()
		{
			return nextPeers;
		}


		inline const std::vector<peer_id> & upstreamPeers()
		{
			return prevPeers;
		}

		// ------------------------------------------------------
		// Master-workers layout methods
		// ------------------------------------------------------

		template<class T>
		result_type distribute(T * data, uint sizeInUnits, uint unitSize, uint & startIndex, uint & endIndex)
		{
			std::vector<peer_id> peers = downstreamPeers();
			uint nPeers = peers.size();
			uint sizePerPeer = sizeInUnits / (nPeers+1);		// calculate portion for each node
			int remainder = sizeInUnits % (nPeers+1);

			uint ini = startIndex = 0;
			uint end = endIndex = sizePerPeer + (remainder-- > 0 ? 1 : 0);	// coordinator gets first piece

			for (igcl::peer_id sendId : peers)
			{
				ini = end;
				end = end + sizePerPeer + (remainder-- > 0 ? 1 : 0);	// gets a unit off the remainder if it still exists

				//printf("process %d takes %d rows (from %d to %d)\n", sendId, end-ini, ini, end);

				result_type res;
				res = sendTo(sendId, ini);
				res = sendTo(sendId, end);
				res = sendTo(sendId, data+ini*unitSize, (end-ini)*unitSize);
				QUIT_IF_UNSUCCESSFUL(res);
			}

			return SUCCESS;
		}


		template<class T>
		result_type recvSection(T * & data, uint & startIndex, uint & endIndex, peer_id & masterId)
		{
			waitRecvFromAny(masterId, startIndex);		// sets masterId
			waitRecvFrom(masterId, endIndex);
			return waitRecvNewFrom(masterId, data);
		}


		template<class T>
		result_type sendResult(T * data, uint sizeInUnits, uint unitSize, uint index, peer_id masterId)
		{
			sendTo(masterId, index);
			return sendTo(masterId, data, sizeInUnits * unitSize);
		}


		template<class T>
		result_type collect(T * data, uint sizeInUnits, uint unitSize)
		{
			std::vector<peer_id> peers = downstreamPeers();
			uint nPeers = peers.size();

			while(nPeers--)	// receive from all slaves
			{
				uint index, size;
				T * tmp_data;
				peer_id sourceId;

				result_type res;
				res = waitRecvFromAny(sourceId, index);
				res = waitRecvNewFrom(sourceId, tmp_data, size);
				QUIT_IF_UNSUCCESSFUL(res);

				memcpy(data+index*unitSize, tmp_data, size * sizeof(T));
				free(tmp_data);
			}

			return SUCCESS;
		}

		// ------------------------------------------------------
		// Tree layout methods
		// ------------------------------------------------------

		template<uint DEGREE=2, class T>
		result_type branch(T * data, uint sizeInUnits, uint unitSize, uint & ownSize)
		{
			uint branchSize, sendSize, sendPos;
			int branchRem;
			int curr = DEGREE;

			ownSize = sizeInUnits;
			uint nRemainingPeers = nDownstreamPeers()+1;

			for (igcl::peer_id sendId : downstreamPeers())
			{
				if (curr == DEGREE) {	// correct division of data even if nDownstreamPeers != DEGREE
					curr = 1;
					uint div = DEGREE;
					if (nRemainingPeers < DEGREE) {
						div = nRemainingPeers;
					}
					branchSize = ownSize / div;
					branchRem  = ownSize % div;

					ownSize = branchSize + (branchRem-- > 0 ? 1:0);
					sendPos = ownSize;
				} else {
					sendPos += sendSize;
				}

				sendSize = branchSize + (branchRem-- > 0 ? 1:0);

				//std::cout << "process " << sendId << " takes " << sendSize << " rows (from " << sendPos << " to " << (sendPos+sendSize) << ")" << std::endl;

				result_type res;
				res = sendTo(sendId, sendSize);
				res = sendTo(sendId, data+sendPos*unitSize, sendSize*unitSize);
				QUIT_IF_UNSUCCESSFUL(res);

				++curr;
				--nRemainingPeers;
			}

			return SUCCESS;
		}


		template<class T>
		result_type recvBranch(T * & data, uint & sizeInUnits, peer_id & masterId)
		{
			waitRecvFromAny(masterId, sizeInUnits);		// sets masterId
			return waitRecvNewFrom(masterId, data);
		}


		template<class T>
		result_type returnBranch(T * data, uint sizeInUnits, uint unitSize, peer_id masterId)
		{
			sendTo(masterId, sizeInUnits);
			return sendTo(masterId, data, sizeInUnits * unitSize);
		}


		template<class T, class Func=std::function<void (T*, uint, T*, uint, T*)> >
		result_type merge(T * data, uint sizeInUnits, uint unitSize, T * ownData, uint ownSizeInUnits, Func merger)
		{
			if (nDownstreamPeers() == 0)
				return SUCCESS;

			auto peers = downstreamPeers();
			std::reverse(peers.begin(), peers.end());

			for (igcl::peer_id peerId : peers)
			{
				T * branchData = NULL;
				uint branchSizeInUnits = 0;

				result_type res;
				res = waitRecvFrom(peerId, branchSizeInUnits);
				res = waitRecvNewFrom(peerId, branchData);
				QUIT_IF_UNSUCCESSFUL(res);

				T * mergePlace = data + (sizeInUnits - ownSizeInUnits - branchSizeInUnits) * unitSize;	// merge at end of "data" array
				merger(ownData, ownSizeInUnits, branchData, branchSizeInUnits, mergePlace);

				free(branchData);
				ownData = mergePlace;
				ownSizeInUnits += branchSizeInUnits;
			}

			return SUCCESS;
		}

		//--------------------------------------------------
		// All-IDs public access wrapper
		//--------------------------------------------------

	public:
		inline std::vector<peer_id> getAllIds()
		{
			return knownPeers.getAllIds();
		}

		// ------------------------------------------------------
		//
		// ------------------------------------------------------
	};
}

#endif /* NODE_HPP_ */
