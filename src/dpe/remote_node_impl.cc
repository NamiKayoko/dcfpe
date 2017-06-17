#include "dpe/remote_node_impl.h"
#include "dpe/dpe_node_base.h"

namespace dpe
{
RemoteNodeImpl::RemoteNodeImpl(
RemoteNodeHandler* handler, const std::string myAddress, int connectionId) :
    isReady(false),
    nextRequestId(0),
    handler(handler),
    myAddress(myAddress),
    connectionId(connectionId),
    zmqClient(base::zmq_client()),
    weakptr_factory_(this)
{
}

RemoteNodeImpl::~RemoteNodeImpl()
{
}
void RemoteNodeImpl::connectTo(const std::string& remoteAddress)
{
  this->remoteAddress = remoteAddress;
  
  Request req;
  req.set_name("connect");
  req.set_timestamp(base::Time::Now().ToInternalValue());
  
  ConnectRequest* cr = new ConnectRequest();
  cr->set_address(myAddress);
  req.set_allocated_connect(cr);

  std::string val;
  req.SerializeToString(&val);
  
  zmqClient->SendRequest(
    remoteAddress,
    val.c_str(),
    static_cast<int>(val.size()),
    base::Bind(&RemoteNodeImpl::handleConnect, weakptr_factory_.GetWeakPtr()),
    0);
}

void  RemoteNodeImpl::handleConnect(base::WeakPtr<RemoteNodeImpl> self,
            scoped_refptr<base::ZMQResponse> rep)
{
  if (RemoteNodeImpl* pThis = self.get())
  {
    pThis->handleConnectImpl(rep);
  }
}

void  RemoteNodeImpl::handleConnectImpl(scoped_refptr<base::ZMQResponse> rep)
{
  if (rep->error_code_ != base::ZMQResponse::ZMQ_REP_OK)
  {
    handler->onConnectionFinished(this, false);
    return;
  }
  Response data;
  data.ParseFromString(rep->data_);

  remoteConnectionId = data.connect().connection_id();
  
  isReady = true;
  handler->onConnectionFinished(this, true);
}

void RemoteNodeImpl::disconnect()
{
  int requestId = ++nextRequestId;

  isReady = false;

  DisconnectRequest* dr = new DisconnectRequest();
  dr->set_address(myAddress);

  Request req;
  req.set_connection_id(remoteConnectionId);
  req.set_request_id(requestId);
  req.set_timestamp(base::Time::Now().ToInternalValue());
  req.set_allocated_disconnect(dr);
  
  std::string val;
  req.SerializeToString(&val);
  
  zmqClient->SendRequest(
    remoteAddress,
    val.c_str(),
    static_cast<int>(val.size()),
    base::Bind(&RemoteNodeImpl::handleDisconnect, weakptr_factory_.GetWeakPtr()),
    0);
}
  
void  RemoteNodeImpl::handleDisconnect(base::WeakPtr<RemoteNodeImpl> self,
            scoped_refptr<base::ZMQResponse> rep)
{
  // no need to handle it
}
  
int RemoteNodeImpl::sendRequest(Request& req, base::ZMQCallBack callback)
{
  int requestId = ++nextRequestId;
  req.set_connection_id(remoteConnectionId);
  req.set_request_id(requestId);
  req.set_timestamp(base::Time::Now().ToInternalValue());
  
  std::string val;
  req.SerializeToString(&val);

  zmqClient->SendRequest(
    remoteAddress,
    val.c_str(),
    static_cast<int>(val.size()),
    base::Bind(&RemoteNodeImpl::handleResponse, weakptr_factory_.GetWeakPtr(), callback),
    0);
  
  return requestId;
}
  
void  RemoteNodeImpl::handleResponse(base::WeakPtr<RemoteNodeImpl> self,
            base::ZMQCallBack callback,
            scoped_refptr<base::ZMQResponse> rep)
{
  if (RemoteNodeImpl* pThis = self.get())
  {
    callback.Run(rep);
  }
}

RemoteNodeControllerImpl::RemoteNodeControllerImpl(
  base::WeakPtr<DPENodeBase> pNode, base::WeakPtr<RemoteNodeImpl> pRemoteNode) :
  pNode(pNode), pRemoteNode(pRemoteNode), refCount(0), weakptr_factory_(this)
{
  id = pRemoteNode->getId();
}

void RemoteNodeControllerImpl::addRef()
{
  ++refCount;
}

void RemoteNodeControllerImpl::release()
{
  if (--refCount == 0)
  {
    delete this;
  }
}

void RemoteNodeControllerImpl::removeNode()
{
  if (auto* local = pNode.get())
  {
    local->removeNode(id);
  }
}

int RemoteNodeControllerImpl::getId() const
{
  return id;
}

int RemoteNodeControllerImpl::addTask(int taskId, const std::string& data,
  std::function<void (int, bool, const std::string&)> callback)
{
  Request req;
  req.set_name("compute");
  
  ComputeRequest* cr = new ComputeRequest();
  cr->set_task_id(taskId);
  req.set_allocated_compute(cr);

  LOG(INFO) << "Send request:\n" << req.DebugString();

  if (auto* remote = pRemoteNode.get())
  {
    remote->sendRequest(req,
        base::Bind(&RemoteNodeControllerImpl::handleAddTask, weakptr_factory_.GetWeakPtr(),
        taskId, data, callback));
  }
  return 0;
}

void RemoteNodeControllerImpl::handleAddTask(
    base::WeakPtr<RemoteNodeControllerImpl> self,
    int taskId, const std::string& data,
    std::function<void (int, bool, const std::string&)> callback,
    scoped_refptr<base::ZMQResponse> rep)
{
  if (auto* pThis = self.get())
  {
    pThis->handleAddTaskImpl(taskId, data, callback, rep);
  }
}

void RemoteNodeControllerImpl::handleAddTaskImpl(
    int taskId, const std::string& data,
    std::function<void (int, bool, const std::string&)> callback,
    scoped_refptr<base::ZMQResponse> rep)
{
  if (rep->error_code_ != base::ZMQResponse::ZMQ_REP_OK)
  {
    callback(taskId, false, "");
  }
  else
  {
    callback(id, true, "");
  }
}

int RemoteNodeControllerImpl::finishTask(int taskId, const std::string& result)
{
  Request req;
  req.set_name("finishCompute");
  
  FinishComputeRequest* cr = new FinishComputeRequest();
  cr->set_task_id(taskId);
  cr->set_result(result);

  req.set_allocated_finish_compute(cr);

  LOG(INFO) << "Send request:\n" << req.DebugString();

  if (auto* remote = pRemoteNode.get())
  {
    remote->sendRequest(req,
      base::Bind(&RemoteNodeControllerImpl::handleFinishTask, weakptr_factory_.GetWeakPtr()));
  }
  return 0;
}
void RemoteNodeControllerImpl::handleFinishTask(base::WeakPtr<RemoteNodeControllerImpl> self,
    scoped_refptr<base::ZMQResponse> rep)
{
}
}