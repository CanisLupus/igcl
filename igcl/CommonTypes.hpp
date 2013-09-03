#ifndef COMMONTYPES_HPP_
#define COMMONTYPES_HPP_

#include <climits>


namespace igcl		// Internet Group-Communication Library
{
	// ======================================================
	// ====================== TYPES =========================
	// ======================================================

	typedef unsigned int uint;

	typedef int peer_id;
	typedef char msg_type;
	typedef int flag_type;
	typedef char result_type;
	typedef uint size_type;
	typedef char descriptor_type;

	// ======================================================
	// ==================== CONSTANTS =======================
	// ======================================================

	const descriptor_type DESCRIPTOR_NONE = 0;
	const descriptor_type DESCRIPTOR_SOCK = 1;
	const descriptor_type DESCRIPTOR_NICE = 2;

	const size_type SIZE_TYPE_MAX = UINT_MAX;

	const result_type FAILURE = 0;
	const result_type SUCCESS = 1;
	const result_type NOTHING = 2;

	//const peer_id SEND_TO_ALL_FILTER = 0;

	// possible message types (headers)
	const msg_type NONE = 0;
	const msg_type REGISTER = 1;
	const msg_type DEREGISTER = 2;
	const msg_type DEREGISTER_PEER = 3;
	// peer to coordinator:
	const msg_type REQUEST_PEER_CREDENTIALS = 101;
	const msg_type PROVIDE_PEER_CREDENTIALS = 102;
	const msg_type REQUEST_NICE_PEER_CREDENTIALS = 103;
	const msg_type PROVIDE_NICE_PEER_CREDENTIALS = 104;
	const msg_type SET_RELAYED_CONNECTION = 109;
	// coordinator to peers:
	const msg_type GET_PEER_CREDENTIALS = 105;
	const msg_type GIVE_PEER_CREDENTIALS = 106;
	const msg_type GET_NICE_PEER_CREDENTIALS = 107;
	const msg_type GIVE_NICE_PEER_CREDENTIALS = 108;
	// other:
	const msg_type BARRIER = 10;
	const msg_type BARRIER_REPLY = 11;
	const msg_type READY = 22;
	const msg_type SHUTDOWN = 25;
	const msg_type SEND_TO_PEER = 34;
	const msg_type SEND_TO_PEER_RELAYED = 35;
	const msg_type SEND_TO_ALL_RELAYED = 36;
}


#endif /* COMMONTYPES_HPP_ */
