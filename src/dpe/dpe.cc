#include "dpe/dpe.h"

#include <iostream>

#include <windows.h>
#include <winsock2.h>
#include <Shlobj.h>
#pragma comment(lib, "ws2_32")

#include "dpe/dpe_internal.h"
#include "dpe/dpe_master_node.h"
#include "dpe/dpe_worker_node.h"
#include "dpe/http_server.h"

namespace dpe {
static inline std::string GetInterfaceAddress() {
  char hostname[128];
  char localHost[128][32] = {{0}};
  struct hostent* temp;
  gethostname(hostname, 128);
  temp = gethostbyname(hostname);
  for (int i = 0; temp->h_addr_list[i] != NULL && i < 1; ++i) {
    strcpy(localHost[i], inet_ntoa(*(struct in_addr*)temp->h_addr_list[i]));
  }
  return localHost[0];
}

static inline void StartNetwork() {
  WSADATA wsaData;
  WORD sockVersion = MAKEWORD(2, 2);
  if (::WSAStartup(sockVersion, &wsaData) != 0) {
    std::cerr << "Cannot initialize wsa" << std::endl;
    exit(-1);
  }
}

static inline void StopNetwork() { ::WSACleanup(); }

static inline std::string ParseCmd(const std::string& s, int& idx,
                                   std::string& value) {
  idx = -1;
  value = "";

  const int l = static_cast<int>(s.length());
  int i = 0;
  while (i < l && s[i] == '-') ++i;
  int j = i;
  while (j < l && s[j] != '=') ++j;
  if (j < l) {
    idx = j;
    value = s.substr(j + 1);
  }
  return StringToLowerASCII(s.substr(i, j - i));
}

scoped_refptr<DPEMasterNode> dpeMasterNode;
scoped_refptr<DPEWorkerNode> dpeWorkerNode;
http::HttpServer httpServer;
std::string type = "server";
std::string myIP;
std::string serverIP;
int port = 0;
Solver* solver;
int httpPort = 80;

static void ExitDpeImpl() {
  LOG(INFO) << "exitDpeImpl";
  if (dpeMasterNode) {
    dpeMasterNode->Stop();
  }
  dpeMasterNode = NULL;
  httpServer.Stop();
  if (dpeWorkerNode) {
    dpeWorkerNode->Stop();
  }
  dpeWorkerNode = NULL;
  base::will_quit_main_loop();
}

void WillExitDpe() {
  LOG(INFO) << "WillExitDpe";
  httpServer.SetHandler(NULL);
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
                             base::Bind(ExitDpeImpl));
}

Solver* GetSolver() { return solver; }

static inline void run() {
  LOG(INFO) << "running";
  LOG(INFO) << "type = " << type;
  LOG(INFO) << "myIP = " << myIP;
  LOG(INFO) << "serverIP = " << serverIP;

  if (type == "server") {
    dpeMasterNode =
        new DPEMasterNode(myIP, port == 0 ? dpe::kServerPort : port);
    httpServer.SetHandler(dpeMasterNode);
    httpServer.Start(httpPort);
    if (!dpeMasterNode->Start()) {
      LOG(ERROR) << "Failed to start master node";
      WillExitDpe();
    }
  } else if (type == "worker") {
    dpeWorkerNode =
        new DPEWorkerNode(serverIP, port == 0 ? dpe::kServerPort : port);
    if (!dpeWorkerNode->Start()) {
      LOG(ERROR) << "Failed to start worker node";
      WillExitDpe();
    }
  } else {
    LOG(ERROR) << "Unknown type";
    WillExitDpe();
  }
}

void RunDpe(Solver* solver, int argc, char* argv[]) {
  std::cout << "DCFPE (version 2.0.0.0)" << std::endl;
  std::cout << "Author: baihacker" << std::endl;
  std::cout << "HomePage: https://github.com/baihacker/dcfpe" << std::endl;

  dpe::solver = solver;
  StartNetwork();

  type = "server";
  myIP = GetInterfaceAddress();
  serverIP = myIP;

  int loggingLevel = 1;
  for (int i = 1; i < argc;) {
    int idx;
    std::string value;
    const std::string str = ParseCmd(argv[i], idx, value);
    if (str == "t" || str == "type") {
      if (idx == -1) {
        type = argv[i + 1];
        i += 2;
      } else {
        type = value;
        ++i;
      }
    } else if (str == "ip") {
      if (idx == -1) {
        myIP = argv[i + 1];
        i += 2;
      } else {
        myIP = value;
        ++i;
      }
    } else if (str == "server_ip") {
      if (idx == -1) {
        serverIP = argv[i + 1];
        i += 2;
      } else {
        serverIP = value;
        ++i;
      }
    } else if (str == "l" || str == "log") {
      if (idx == -1) {
        loggingLevel = atoi(argv[i + 1]);
        i += 2;
      } else {
        loggingLevel = atoi(value.c_str());
        ++i;
      }
    } else if (str == "p" || str == "port") {
      if (idx == -1) {
        port = atoi(argv[i + 1]);
        i += 2;
      } else {
        port = atoi(value.c_str());
        ++i;
      }
    } else if (str == "hp" || str == "http_port") {
      if (idx == -1) {
        httpPort = atoi(argv[i + 1]);
        i += 2;
      } else {
        httpPort = atoi(value.c_str());
        ++i;
      }
    } else {
      ++i;
    }
  }

  base::dpe_base_main(run, NULL, loggingLevel);

  StopNetwork();
}

static DpeStub __stub_impl = {&dpe::RunDpe};

DPE_EXPORT DpeStub* get_stub() { return &__stub_impl; }
}  // namespace dpe