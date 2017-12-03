#include "remote_shell/local_server_node.h"
#include "remote_shell/proto/rs.pb.h"
#include "dpe_base/dpe_base.h"
#include "remote_shell/listener_node.h"

namespace rs
{
LocalServerNode::LocalServerNode(
    const std::string& myIP):
    ServerNode(myIP),
    runningRequestId(-1),
    sessionId(-1),
    weakptr_factory_(this)
  {

  }

LocalServerNode::~LocalServerNode()
{
}

static void stopImpl(LocalServerNode* node) {
  node->stop();
  base::will_quit_main_loop();
}

static void willStop(LocalServerNode* node) {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(stopImpl, node));
}

static const int kLocalServerStartPort = 3331;

bool LocalServerNode::start() {
  int offset = rand() % 1000;
  for (int i = kLocalServerStartPort + offset; i < 5000; ++i) {
    if (ServerNode::start(i)) {
      return true;
    }
  }
  return false;
}

void LocalServerNode::connectToTarget(const std::string& target) {
  targetAddress = base::AddressHelper::MakeZMQTCPAddress(target, kRSListenerPort);
  CreateSessionRequest* csRequest = new CreateSessionRequest();
  csRequest->set_address(zserver->GetServerAddress());
  
  Request req;
  req.set_name("CreateSession");
  req.set_allocated_create_session(csRequest);

  msgSender = new MessageSender(targetAddress);
  printf("Connecting...\n");
  runningRequestId = msgSender->sendRequest(req, [=](int32_t zmqError, const Response& reply){
    this->handleCreateSessionResponse(zmqError, reply);
  }, 10*1000);
}

void LocalServerNode::handleCreateSessionResponse(int32_t zmqError, const Response& reply) {
  if (zmqError != 0 || reply.error_code() != 0) {
    std::string info = "Cannot connect to " + targetAddress + "!\n";
    printf(info.c_str());
    willStop(this);
    return;
  }
  sessionId = reply.session_id();
  msgSender = NULL;
}

static void runNextCommandImpl(LocalServerNode* node) {
  node->executeCommand();
}

static void willRunNextCommand(LocalServerNode* node) {
  base::ThreadPool::PostTask(base::ThreadPool::UI, FROM_HERE,
    base::Bind(runNextCommandImpl, node));
}

char line[1<<16];
void LocalServerNode::executeCommand() {
  for (;;) {
    printf("rs>");
    gets(line);
    std::vector<std::string> cmds;
    int now = 0;
    const int n = strlen(line);
    while (now < n) {
      while (now < n && line[now] == ' ') ++now;
      if (now == n) break;
      
      if (line[now] == '"') {
        std::string token;
        ++now;
        for (;now < n;) {
          if (line[now] == '"') {
            ++now;
            cmds.push_back(token);
            break;
          } else if (now == '\\') {
            token += line[now++];
            if (now < n) {
              token += line[now++];
            }
          } else {
            token += line[now++];
          }
        }
      } else {
        std::string token;
        while (now < n && line[now] != ' ') {
          token += line[now++];
        }
        cmds.push_back(token);
      }
    }
    if (cmds.empty()) {
      printf("Command is empty!\n");
      continue;
    }

    if (handleInternalCommand(cmds)) {
      break;
    }
    
    std::string myAddress = base::AddressHelper::MakeZMQTCPAddress(myIP, port);

    ExecuteCommandRequest* ecRequest = new ExecuteCommandRequest();
    ecRequest->set_address(myAddress);
    
    ecRequest->set_cmd(cmds[0]);
    cmds.erase(cmds.begin());
    for (auto& a: cmds) ecRequest->add_args(a);
    
    Request req;
    req.set_name("ExecuteCommand");
    req.set_session_id(sessionId);
    req.set_allocated_execute_command(ecRequest);
    runningRequestId = msgSender->sendRequest(req, [=](int32_t zmqError, const Response& reply){
      this->handleExecuteCommandResponse(zmqError, reply);
    }, 10*1000);
    break;
  }
}

bool LocalServerNode::handleInternalCommand(const std::vector<std::string>& cmds) {
  const int size = cmds.size();
  if (cmds[0] == "exit" || cmds[0] == "q") {
    DeleteSessionRequest* dsRequest = new DeleteSessionRequest();
    Request req;
    req.set_name("DeleteSession");
    req.set_session_id(sessionId);
    req.set_allocated_delete_session(dsRequest);
    
    runningRequestId = msgSender->sendRequest(req, 0);

    willStop(this);
    return true;
  } else if (cmds[0] == "l") {
    int now = 0;
    while (line[now] != 'l') ++now;
    ++now;
    system(line + now);
    willRunNextCommand(this);
    return true;
  } else if (cmds[0] == "fs") {
    if (cmds.size() == 1) {
      printf("No file specified!\n");
      willRunNextCommand(this);
      return true;
    }
    for (int i = 1; i < size; ++i) {
      if (!base::PathExists(base::FilePath(base::UTF8ToNative(cmds[i])))) {
        printf("%s doesn't exist\n", cmds[i].c_str());
        willRunNextCommand(this);
        return true;
      }
    }

    FileOperationRequest* foRequest = new FileOperationRequest();
    foRequest->set_cmd("fs");
    for (int i = 1; i < size; ++i) {
      foRequest->add_args(cmds[i]);
      std::string data;
      if (!base::ReadFileToString(base::FilePath(base::UTF8ToNative(cmds[i])), &data)) {
        printf("Cannot read %s\n", cmds[i].c_str());
        willRunNextCommand(this);
        return true;
      }
      foRequest->add_args(data);
    }

    Request req;
    req.set_name("FileOperation");
    req.set_allocated_file_operation(foRequest);
    msgSender->sendRequest(req, [=](int32_t zmqError, const Response& reply){
      this->handleFileOperationResponse(zmqError, req, reply);
    }, 0);
    return true;
  } else if (cmds[0] == "fg") {
    if (cmds.size() == 1) {
      printf("No file specified!\n");
      willRunNextCommand(this);
      return true;
    }

    FileOperationRequest* foRequest = new FileOperationRequest();
    foRequest->set_cmd("fg");
    for (int i = 1; i < size; ++i) {
      foRequest->add_args(cmds[i]);
    }

    Request req;
    req.set_name("FileOperation");
    req.set_allocated_file_operation(foRequest);
    msgSender->sendRequest(req, [=](int32_t zmqError, const Response& reply){
      this->handleFileOperationResponse(zmqError, req, reply);
    }, 0);
    return true;
  }
  return false;
}

void LocalServerNode::handleFileOperationResponse(int32_t zmqError, const Request& req, const Response& reply) {
  if (zmqError != 0 || reply.error_code() != 0) {
    printf("Failed!\n");
    willRunNextCommand(this);
    return;
  }
  const auto& foRequest = req.file_operation();
  const auto& detail = reply.file_operation();
  if (foRequest.cmd() == "fs") {
    printf(reply.error_code() == 0 ? "Succeed\n" : "Failed\n");
    willRunNextCommand(this);
  } else if (foRequest.cmd() == "fg") {
    const auto& args = detail.args();
    const int size = args.size();
    for (int i = 0; i + 1 < size; i += 2) {
      auto path = base::FilePath(base::UTF8ToNative(args[i]));
      auto dest = path.BaseName();
      if (base::WriteFile(dest, args[i+1].c_str(), args[i+1].size()) == -1) {
        willRunNextCommand(this);
        return;
      }
    }
    willRunNextCommand(this);
  }
}

void LocalServerNode::handleRequest(const Request& req, Response& reply)
{
  reply.set_session_id(sessionId);
  reply.set_error_code(-1);

  if (req.session_id() != sessionId) {
    return;
  }

  if (req.has_execute_output()) {
    const auto& detail = req.execute_output();
    if (detail.original_request_id() != runningRequestId) {
      // Unexpected output request.
      return;
    }

    if (detail.is_exit()) {
      printf("exit code: %d\n", detail.exit_code());
      willRunNextCommand(this);
    } else {
      printf(detail.output().c_str());
    }
    reply.set_error_code(0);
  } else if (req.has_create_session()) {
    const auto& detail = req.create_session();
    printf("Connected!\n");
    executorAddress = detail.address();
    msgSender = new MessageSender(executorAddress);
    willRunNextCommand(this);
    reply.set_error_code(0);
  }
}

bool LocalServerNode::preHandleRequest(const Request& req, Response& reply) {
  reply.set_session_id(sessionId);
  reply.set_error_code(-1);

  if (req.session_id() != sessionId) {
    return true;
  }

  if (req.has_session_heart_beat()) {
    const auto& detail = req.session_heart_beat();
    reply.set_error_code(0);
    return true;
  }
  return false;
}

void LocalServerNode::handleExecuteCommandResponse(int32_t zmqError, const Response& reply) {
  if (zmqError != 0 || reply.error_code() != 0) {
    printf("Failed to execute command remotely!\n");
    runningRequestId = -1;
    willRunNextCommand(this);
    return;
  }
  printf("Command: %s\n", reply.execute_command().command().c_str());
}
}
