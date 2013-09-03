#ifndef LIBNICEHELPER_HPP_
#define LIBNICEHELPER_HPP_

#include "CommonDefines.hpp"

#ifndef DISABLE_LIBNICE

#include <iostream>
#include <cstdio>
#include <cstring>
#include <string>
#include <cassert>
#include <map>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <sys/time.h>

#include <nice/agent.h>


namespace igcl
{
	class LibniceHelper
	{
	private:
		static GMainLoop * mainLoop;
		static GMainContext * mainContext;
		static NiceAgent * agent;

		static std::mutex gatheredMutex, connectedMutex, startedMutex, writableMutex;
		static std::condition_variable gatheredCondVar, connectedCondVar, startedCondVar, writableCondVar;
		static int isConnected, hasGathered, hasStarted;
		static char * localInfo;
		static std::map<uint, bool> isWritable;

	public:
		static NiceAgentRecvFunc cb_nice_recv;

	public:
		static void start();
		static void quit();
		static bool startNewStream(guint & stream_id, std::string & info);
		static bool connect(guint stream_id, const std::string & remoteInfo);
		static int send(guint stream_id, const char * data, uint size);
		static void waitWritable(guint stream_id);

	private:
		static void init();

		static void cb_candidate_gathering_done (NiceAgent * agent, guint stream_id, gpointer data_);
		static void cb_component_state_changed (NiceAgent * agent, guint stream_id, guint component_id, guint state, gpointer data);
		static void cb_reliable_transport_writable (NiceAgent * agent, guint stream_id, guint component_id, gpointer user_data);

		static void establishConnection(guint stream_id, const char * remoteInfo);
		static char * getLocalInfo(guint stream_id);
		static NiceCandidate * parseCandidate(guint stream_id, char * str);
	};
}

#endif

#endif /* LIBNICEHELPER_HPP_ */
