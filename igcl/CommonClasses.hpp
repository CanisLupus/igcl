#ifndef COMMONCLASSES_HPP_
#define COMMONCLASSES_HPP_

#include <string>
#include <map>
#include <list>
#include <queue>
#include <functional>
#include <mutex>


namespace igcl		// Internet Group-Communication Library
{
	class Coordinator;
	typedef int peer_id;

	// ======================================================
	// ================== ADDRESS CLASS =====================
	// ======================================================

	// struct that keeps address information
	struct address
	{
		std::string ip;
		int port;

		address() {}
		address(const std::string & ip, int port) : ip(ip), port(port) {}

		inline void set(const std::string & ip, int port)
		{
			this->ip = ip;
			this->port = port;
		}
	};

	std::ostream & operator << (std::ostream & os, const address & addr);

	// ======================================================
	// ============== DESCRIPTOR PAIR CLASS =================
	// ======================================================

	struct descriptor_pair
	{
		int desc;
		descriptor_type type;

		descriptor_pair() {}
		descriptor_pair(int desc, descriptor_type type) : desc(desc), type(type) {}

		bool operator < (const descriptor_pair & other) const
		{
			return (desc < other.desc or (desc == other.desc and type < other.type));
		}

		bool operator == (const descriptor_pair & other) const
		{
			return (desc == other.desc and type == other.type);
		}
	};

	// ======================================================
	// ================= PEER TABLE CLASS ===================
	// ======================================================

	// thread safe table that relates peer location to peer information
	class PeerTable
	{
	private:
		std::map<peer_id, descriptor_pair> idToFd;
		std::map<descriptor_pair, peer_id> fdToId;
		std::mutex mutex;

	public:
		inline bool registerPeer(const descriptor_pair & desc, peer_id id)
		{
			std::lock_guard<std::mutex> lockWhileInsideScope(mutex);
			auto it = idToFd.find(id);
			if (it == idToFd.end()) {	// what normally happens is that the ID is NOT already registered
				idToFd[id] = desc;
				fdToId[desc] = id;
				return true;
			}
			return false;
		}

		inline bool deregisterPeer(const descriptor_pair & desc)
		{
			std::lock_guard<std::mutex> lockWhileInsideScope(mutex);
			bool found = fdToId.count(desc) > 0;
			if (found) {
				const peer_id id = fdToId[desc];
				idToFd.erase(id);
				fdToId.erase(desc);
			}
			return found;
		}

		inline descriptor_pair idToDescriptor(peer_id id)
		{
			std::lock_guard<std::mutex> lockWhileInsideScope(mutex);
			return idToFd[id];
		}

		inline peer_id descriptorToId(descriptor_pair desc)
		{
			std::lock_guard<std::mutex> lockWhileInsideScope(mutex);
			return fdToId[desc];
		}

		inline std::vector<peer_id> getAllIds()
		{
			std::lock_guard<std::mutex> lockWhileInsideScope(mutex);
			std::vector<peer_id> v;
			for (auto & peer : idToFd) {
				v.push_back(peer.first);
			}
			return v;
		}

		inline std::vector<descriptor_pair> getAllDescriptors()
		{
			std::lock_guard<std::mutex> lockWhileInsideScope(mutex);
			std::vector<descriptor_pair> v;
			for (std::pair<const descriptor_pair, int> & peer : fdToId) {
				v.push_back(peer.first);
			}
			return v;
		}

		inline bool idExists(peer_id id)
		{
			std::lock_guard<std::mutex> lockWhileInsideScope(mutex);
			return idToFd.count(id) > 0;
		}

		inline uint size()
		{
			std::lock_guard<std::mutex> lockWhileInsideScope(mutex);
			return idToFd.size();
		}

		std::string toString(const char * sep = "\n");
	};

	// ======================================================
	// ============= SOCKET DESCRIPTORS CLASS ===============
	// ======================================================

	class SocketDescriptors
	{
	public:
		int maxFd;

	private:
		fd_set fdSet;
		std::mutex mutex;

	public:
		SocketDescriptors() {
			FD_ZERO(&fdSet);
			maxFd = 0;
		}

		inline const fd_set & copyDescriptorSet()
		{
			std::lock_guard<std::mutex> lockWhileInsideScope(mutex);
			return fdSet;
		}

		inline void setFd(int fd)
		{
			std::lock_guard<std::mutex> lockWhileInsideScope(mutex);
			FD_SET(fd, &fdSet);
			maxFd = std::max(fd, maxFd);
		}

		inline void unsetFd(int fd)
		{
			std::lock_guard<std::mutex> lockWhileInsideScope(mutex);
			FD_CLR(fd, &fdSet);
			if (fd == maxFd) {
				while (!FD_ISSET(maxFd, &fdSet)) {
					maxFd--;
				}
			}
		}
	};

	// ======================================================
	// =================== STATS CLASS ======================
	// ======================================================

	class Stats			// debug/experiments class
	{
	private:
		ulong nSends;
		ulong nSizeSends;
		ulong nBytesSent;
		ulong nReceives;
		ulong nSizeReceives;
		ulong nBytesReceived;

	public:
		Stats() : nSends(0), nSizeSends(0), nBytesSent(0), nReceives(0), nSizeReceives(0), nBytesReceived(0) {}

		inline ulong getNSends()			{ return nSends; }
		inline ulong getNSizeSends()		{ return nSizeSends; }
		inline ulong getNBytesSent()		{ return nBytesSent; }
		inline ulong getNReceives()		{ return nReceives; }
		inline ulong getNSizeReceives()	{ return nSizeReceives; }
		inline ulong getNBytesReceived()	{ return nBytesReceived; }

		inline void incNSends(ulong inc=1)			{ nSends += inc; }
		inline void incNSizeSends(ulong inc=1)		{ nSizeSends += inc; }
		inline void incNBytesSent(ulong inc)		{ nBytesSent += inc; }
		inline void incNReceives(ulong inc=1)		{ nReceives += inc; }
		inline void incNSizeReceives(ulong inc=1)	{ nSizeReceives += inc; }
		inline void incNBytesReceived(ulong inc)	{ nBytesReceived += inc; }

		std::string toString();
	};

	// ======================================================
	// ================= BUFFERING CLASS ====================
	// ======================================================

	class NBuffering
	{
		uint nJobs, blockSize, nSentJobs, nCompletedJobs;

		uint bufferingDepth;
		std::map< peer_id, std::queue<uint> > sentJobs;
		std::function<void (peer_id, uint)> sendJob;
		std::queue<uint> jobsToRetry;

	public:
		NBuffering (uint bufferingDepth, uint size, uint blockSize, std::function<void (peer_id, uint)> sendJob)
			: nJobs(size/blockSize+(size % blockSize ? 1:0)), blockSize(blockSize), nSentJobs(0), nCompletedJobs(0),
			  bufferingDepth(bufferingDepth), sendJob(sendJob) {}

		inline void setBufferingDepth(uint bufferingDepth) {
			this->bufferingDepth = bufferingDepth;
		}

		inline void addPeer(peer_id id) {
			sentJobs[id] = std::queue<uint>();
		}

		inline void addPeers(const std::vector<peer_id> & peerIds) {
			for (igcl::peer_id id : peerIds) {
				addPeer(id);
			}
		}

		inline void removePeer(peer_id id) {
			auto & q = sentJobs[id];
			uint size = q.size();
			nSentJobs -= size;

			while (!q.empty()) {
				jobsToRetry.push(q.front());
				q.pop();
			}
		}

		inline void bufferToAll() {
			if (allJobsSent())
				return;

			std::list<peer_id> available;
			for (auto & id_queue : sentJobs) {
				if (id_queue.second.size() < bufferingDepth) {
					available.push_back(id_queue.first);
				}
			}

			while (!available.empty())
			{
				std::list<peer_id>::iterator it = available.begin();

				while (it != available.end()) {
					if (allJobsSent())
						return;

					auto & q = sentJobs[*it];

					if (q.size() < bufferingDepth) {
						if (!jobsToRetry.empty()) {
							uint job = jobsToRetry.front();
							jobsToRetry.pop();
							sendJob(*it, job*blockSize);
							q.push(job);
						} else {
							sendJob(*it, nSentJobs*blockSize);
							q.push(nSentJobs);
						}
						++nSentJobs;
						++it;
					} else {
						it = available.erase(it);
					}
				}
			}
		}

		inline void bufferTo(peer_id id) {
			while (sentJobs[id].size() < bufferingDepth and !allJobsSent()) {
				sendJob(id, nSentJobs*blockSize);
				sentJobs[id].push(nSentJobs);
				++nSentJobs;
			}
		}

		inline uint completeJob(peer_id id) {
			uint job = sentJobs[id].front();
			sentJobs[id].pop();
			++nCompletedJobs;
			return job*blockSize;
		}

		inline bool allJobsSent() {
			return nSentJobs >= nJobs;
		}

		inline bool allJobsCompleted() {
			return nCompletedJobs >= nJobs;
		}
	};

	// ======================================================
	// =========== COORDINATOR CALLBACKS CLASS ==============
	// ======================================================

	class CoordinatorCallbacks
	{
		friend class Coordinator;

	protected:
		Coordinator * owner;

	public:
		CoordinatorCallbacks() {}
	protected:
		virtual ~CoordinatorCallbacks() {}

		virtual void cbStart() {}
		virtual void cbTerminate() {}
		virtual void cbNewPeerReady(peer_id id) {}

	private:
		// private -> only the Coordinator (friend class) can set itself as owner
		void setOwner(Coordinator * owner) { this->owner = owner; }
	};
}

#endif /* COMMONCLASSES_HPP_ */
