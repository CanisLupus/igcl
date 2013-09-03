#include "Common.hpp"

#include <string>
#include <sstream>
#include <algorithm>


namespace igcl		// Internet Group-Communication Library
{
	std::ostream & operator << (std::ostream & os, const address & addr)
	{
		return os << addr.ip << ':' << addr.port;
	}


	std::string PeerTable::toString(const char * sep)
	{
		std::lock_guard<std::mutex> lockWhileInsideScope(mutex);
		std::stringstream ss;

		ss << "known peer list:" << sep;

		std::for_each(idToFd.begin(), idToFd.end(),
			[sep, &ss] (std::pair<const peer_id, descriptor_pair> & peer)
			{
				ss << "peer ID " << peer.first << " with ";
				switch (peer.second.type)
				{
					case DESCRIPTOR_NONE:
						ss << "no direct connection" << sep;
						break;
					case DESCRIPTOR_SOCK:
						ss << "descriptor " << peer.second.desc << sep;
						break;
					case DESCRIPTOR_NICE:
						ss << "stream " << peer.second.desc << sep;
						break;
				}
			});

		return ss.str();
	}

	std::string Stats::toString() {
		std::stringstream ss;
		ss << "# bytes sent:     " << nBytesSent << " (" << nSends << " sends)";
		ss << ", overhead: " << (nSizeSends * sizeof(size_type)) << " bytes" << std::endl;
		ss << "# bytes received: " << nBytesReceived << " (" << nReceives << " receives)";
		ss << ", overhead: " << (nSizeReceives * sizeof(size_type)) << " bytes";
		return ss.str();
	}
}
