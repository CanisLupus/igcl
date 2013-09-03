#ifndef COMMONDEFINES_HPP_
#define COMMONDEFINES_HPP_

//#define DEBUG_MODE
#define TEST() if (true)

//#define DISABLE_LIBNICE
#define FORCE_LIBNICE
//#define FORCE_RELAYED
//#define USE_SEND_QUEUE

#define QUIT_IF_UNSUCCESSFUL(res) if ((res) != igcl::SUCCESS) return (res);
#define QUIT_IF_FAILURE(res) if ((res) == igcl::FAILURE) return (res);

#endif /* COMMONDEFINES_HPP_ */
