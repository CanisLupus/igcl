#include "CommonDefines.hpp"

#ifndef DISABLE_LIBNICE

#include "LibniceHelper.hpp"
#include "Utils.hpp"


namespace igcl		// Internet Group-Communication Library
{
	// VERY ugly but necessary :(
	GMainLoop * LibniceHelper::mainLoop;
	GMainContext * LibniceHelper::mainContext;
	NiceAgent * LibniceHelper::agent;
	std::mutex LibniceHelper::gatheredMutex, LibniceHelper::connectedMutex, LibniceHelper::startedMutex, LibniceHelper::writableMutex;
	std::condition_variable LibniceHelper::gatheredCondVar, LibniceHelper::connectedCondVar, LibniceHelper::startedCondVar, LibniceHelper::writableCondVar;
	int LibniceHelper::isConnected, LibniceHelper::hasGathered, LibniceHelper::hasStarted;
	char * LibniceHelper::localInfo;
	std::map<uint, bool> LibniceHelper::isWritable;
	NiceAgentRecvFunc LibniceHelper::cb_nice_recv;

	// ------------------------------------------------------
	// Public methods
	// ------------------------------------------------------

	void LibniceHelper::start()
	{
		localInfo = NULL;
		hasStarted = 0;
		std::thread th(init);
		th.detach();

		std::unique_lock<std::mutex> lock(startedMutex);
		while (hasStarted == 0) {
			startedCondVar.wait(lock);
		}
		lock.unlock();
	}


	void LibniceHelper::quit()
	{
		g_main_loop_quit(mainLoop);
		g_object_unref(agent);
	}


	bool LibniceHelper::startNewStream(guint & stream_id, std::string & info)
	{
		hasGathered = 0;

		stream_id = nice_agent_add_stream(agent, 1);
		nice_agent_attach_recv(agent, stream_id, 1, mainContext, cb_nice_recv, NULL);	// needed for server reflexive candidates
		nice_agent_gather_candidates(agent, stream_id);

		std::unique_lock<std::mutex> lock(gatheredMutex);
		while (hasGathered == 0) {
			gatheredCondVar.wait(lock);
		}
		lock.unlock();

		if (hasGathered == 1) {
			isWritable[stream_id] = false;
			info.assign(localInfo);
			free(localInfo);
			localInfo = NULL;
			return true;
		}
		return false;
	}


	bool LibniceHelper::connect(guint stream_id, const std::string & remoteInfo)
	{
		isConnected = 0;

		establishConnection(stream_id, remoteInfo.c_str());

		std::unique_lock<std::mutex> lock(connectedMutex);
		while (isConnected == 0) {
			connectedCondVar.wait(lock);
		}
		lock.unlock();

		if (isConnected == 1) {
			return true;
		}
		return false;
	}


	int LibniceHelper::send(guint stream_id, const char * data, uint size)
	{
		int nBytesSent = nice_agent_send(agent, stream_id, 1, size, data);
		if (nBytesSent < 0) {
			std::unique_lock<std::mutex> lock(writableMutex);
			isWritable[stream_id] = false;
		}
		return nBytesSent;
	}


	void LibniceHelper::waitWritable(guint stream_id)
	{
		std::unique_lock<std::mutex> lock(writableMutex);
		while (!isWritable[stream_id]) {
			writableCondVar.wait(lock);
		}
		lock.unlock();
	}

	// ------------------------------------------------------
	// Setup
	// ------------------------------------------------------

	void LibniceHelper::init()
	{
		g_type_init();

		mainContext = g_main_context_new();
		agent = nice_agent_new_reliable(mainContext, NICE_COMPATIBILITY_RFC5245);
		mainLoop = g_main_loop_new(mainContext, FALSE);

		g_object_set(G_OBJECT(agent), "stun-server", "192.198.87.70", "stun-server-port", 3478, NULL);	// stun.3cx.com

		g_signal_connect(G_OBJECT(agent), "candidate-gathering-done",	 G_CALLBACK(cb_candidate_gathering_done), NULL);
		g_signal_connect(G_OBJECT(agent), "component-state-changed",	 G_CALLBACK(cb_component_state_changed), NULL);
		g_signal_connect(G_OBJECT(agent), "reliable-transport-writable", G_CALLBACK(cb_reliable_transport_writable), NULL);

		hasStarted = 1;
		startedMutex.lock();
		startedCondVar.notify_one();
		startedMutex.unlock();

		g_main_loop_run (mainLoop);
	}


	// ------------------------------------------------------
	// Callbacks
	// ------------------------------------------------------

	void LibniceHelper::cb_candidate_gathering_done (NiceAgent * agent, guint stream_id, gpointer data_)
	{
		//std::cout << "cb_candidate_gathering_done" << std::endl;

		char * tmp = getLocalInfo(stream_id);
		localInfo = tmp;

		//std::cout << "local info:  " << localInfo << std::endl;

		std::lock_guard<std::mutex> scoped_lock(gatheredMutex);
		hasGathered = 1;
		gatheredCondVar.notify_one();
	}


	void LibniceHelper::cb_component_state_changed (NiceAgent * agent, guint stream_id, guint component_id, guint state, gpointer data)
	{
		if (state == NICE_COMPONENT_STATE_READY)
		{
			//std::cout << "Component state is READY" << std::endl;

			NiceCandidate *local, *remote;

			if (nice_agent_get_selected_pair (agent, stream_id, component_id, &local, &remote))
			{
				/*gchar localIp [INET6_ADDRSTRLEN];
				gchar remoteIp[INET6_ADDRSTRLEN];
				int localPort, remotePort;

				nice_address_to_string(&local->addr, localIp);
				localPort = nice_address_get_port(&local->addr);
				printf("Negotiation complete: (local:[%s]:%d,", localIp, localPort);

				nice_address_to_string(&remote->addr, remoteIp);
				remotePort = nice_address_get_port(&remote->addr);
				printf(" remote:[%s]:%d)\n", remoteIp, remotePort);*/

				std::lock_guard<std::mutex> scoped_lock(connectedMutex);
				isConnected = 1;
				connectedCondVar.notify_one();
			} else {
				std::lock_guard<std::mutex> scoped_lock(connectedMutex);
				isConnected = -1;
				connectedCondVar.notify_one();
			}
		}
	}


	void LibniceHelper::cb_reliable_transport_writable (NiceAgent * agent, guint stream_id, guint component_id, gpointer user_data)
	{
		//std::cout << "cb_reliable_transport_writable " << stream_id << std::endl;
		std::lock_guard<std::mutex> scoped_lock(writableMutex);
		isWritable[stream_id] = true;
		writableCondVar.notify_all();
	}


	// ------------------------------------------------------
	// Parsing and connecting
	// ------------------------------------------------------

	void LibniceHelper::establishConnection(guint stream_id, const char * remoteInfo)
	{
		GSList * remote_candidates = NULL;
		gchar * ufrag = NULL;
		gchar * passwd = NULL;

		gchar ** line_argv = g_strsplit_set (remoteInfo, " \t\n", 0);

		for (int i = 0;  line_argv && line_argv[i];  i++) {
			if (strlen (line_argv[i]) == 0)
				continue;

			if (!ufrag) {
				ufrag = line_argv[i];
			} else if (!passwd) {
				passwd = line_argv[i];
			} else {
				NiceCandidate * cand = parseCandidate(stream_id, line_argv[i]);
				//if (cand->type != 0)
					remote_candidates = g_slist_prepend(remote_candidates, cand);
			}
		}

		nice_agent_set_remote_credentials(agent, stream_id, ufrag, passwd);
		nice_agent_set_remote_candidates(agent, stream_id, 1, remote_candidates);	// tests candidate pairs to establish connection

		if (line_argv != NULL)
			g_strfreev(line_argv);
		if (remote_candidates != NULL)
			g_slist_free_full(remote_candidates, (GDestroyNotify) &nice_candidate_free);
	}


	char * LibniceHelper::getLocalInfo(guint stream_id)
	{
		gchar * local_ufrag = NULL, * local_password = NULL;
		GSList * cands = NULL;
		char * data = NULL;

		if (nice_agent_get_local_credentials(agent, stream_id, &local_ufrag, &local_password))
		{
			cands = nice_agent_get_local_candidates(agent, stream_id, 1);
			if (cands != NULL)
			{
				const gchar * candidate_type_to_name[] =  {"host", "srflx", "prflx", "relay"};
				data = (char *) malloc(sizeof(char)*1000);

				int len;
				len = sprintf(data, "%s %s", local_ufrag, local_password);

				for (GSList * item = cands;  item != NULL;  item = item->next)
				{
					NiceCandidate * c = (NiceCandidate *) item->data;
					gchar ip[INET6_ADDRSTRLEN];
					nice_address_to_string(&c->addr, ip);
					len += sprintf(data+len, " %s,%u,%s,%u,%s",
						c->foundation, c->priority, ip, nice_address_get_port(&c->addr), candidate_type_to_name[c->type]);
				}
			}
		}

		if (local_ufrag)    g_free(local_ufrag);
		if (local_password) g_free(local_password);
		if (cands)          g_slist_free_full(cands, (GDestroyNotify) &nice_candidate_free);

		return data;
	}


	NiceCandidate * LibniceHelper::parseCandidate(guint stream_id, char * str)
	{
		NiceCandidate * cand = NULL;

		gchar ** tokens = g_strsplit (str, ",", 5);
		std::map<std::string, int> candidate_name_to_type = {{"host",0}, {"srflx",1}, {"prflx",2}, {"relay",3}};
		NiceCandidateType ntype = (NiceCandidateType) candidate_name_to_type[tokens[4]];

		cand = nice_candidate_new(ntype);
		cand->component_id = 1;
		cand->stream_id = stream_id;
		cand->transport = NICE_CANDIDATE_TRANSPORT_UDP;

		strncpy(cand->foundation, tokens[0], NICE_CANDIDATE_MAX_FOUNDATION);
		cand->priority = atoi(tokens[1]);
		nice_address_set_from_string(&cand->addr, tokens[2]);
		nice_address_set_port(&cand->addr, atoi(tokens[3]));

		g_strfreev(tokens);
		return cand;
	}
}

#endif
