#include "mscclpp.h"
#include "bootstrap.h"
#include "core.h"
#include <map>
#include <sstream>

static uint64_t hashUniqueId(mscclppUniqueId const &id) {
  char const *bytes = (char const*)&id;
  uint64_t h = 0xdeadbeef;
  for(int i=0; i < (int)sizeof(mscclppUniqueId); i++) {
    h ^= h >> 32;
    h *= 0x8db3db47fa2994ad;
    h += bytes[i];
  }
  return h;
}

pthread_mutex_t initLock = PTHREAD_MUTEX_INITIALIZER;
static bool initialized = false;
// static size_t maxLocalSizeBytes = 0;

static mscclppResult_t mscclppInit() {
  if (__atomic_load_n(&initialized, __ATOMIC_ACQUIRE)) return mscclppSuccess;
  pthread_mutex_lock(&initLock);
  if (!initialized) {
    // initEnv();
    // initGdrCopy();
    // maxLocalSizeBytes = mscclppKernMaxLocalSize();
    // int carveout = mscclppParamL1SharedMemoryCarveout();
    // if (carveout) mscclppKernSetSharedMemoryCarveout(carveout);
    // Always initialize bootstrap network
    MSCCLPPCHECK(bootstrapNetInit());
    // MSCCLPPCHECK(mscclppNetPluginInit());

    // initNvtxRegisteredEnums();
    __atomic_store_n(&initialized, true, __ATOMIC_RELEASE);
  }
  pthread_mutex_unlock(&initLock);
  return mscclppSuccess;
}

static std::string mscclppShmFileName(mscclppComm_t comm, int rank)
{
  std::stringstream ss;
  ss << "mscclpp." << std::hex << comm->magic << "." << rank;
  return ss.str();
}

mscclppResult_t mscclppGetUniqueId(mscclppUniqueId* out) {
  MSCCLPPCHECK(mscclppInit());
//   mscclppCHECK(PtrCheck(out, "GetUniqueId", "out"));
  mscclppResult_t res = bootstrapGetUniqueId((struct mscclppBootstrapHandle*)out);
  TRACE_CALL("mscclppGetUniqueId(0x%llx)", (unsigned long long)hashUniqueId(*out));
  return res;
}

MSCCLPP_API(mscclppResult_t, mscclppBootStrapAllGather, mscclppComm_t comm, void* data, int size);
mscclppResult_t mscclppBootStrapAllGather(mscclppComm_t comm, void* data, int size){
  MSCCLPPCHECK(bootstrapAllGather(comm->bootstrap, data, size));
  return mscclppSuccess;
}

MSCCLPP_API(mscclppResult_t, mscclppCommInitRank, mscclppComm_t* comm, int nranks, int rank, const char* ip_port_pair);
mscclppResult_t mscclppCommInitRank(mscclppComm_t* comm, int nranks, int rank, const char* ip_port_pair){
  mscclppResult_t res = mscclppSuccess;
  mscclppComm_t _comm = NULL;
  // uint64_t hash = getHostHash();
  // uint64_t *hashes;
  // std::map<uint64_t, int> hashToNode;

  MSCCLPPCHECKGOTO(mscclppCalloc(&_comm, 1), res, fail);
  _comm->rank = rank;
  _comm->nRanks = nranks;

  MSCCLPPCHECK(bootstrapNetInit(ip_port_pair));
  mscclppBootstrapHandle handle;
  MSCCLPPCHECK(bootstrapGetUniqueId(&handle, rank == 0, ip_port_pair));
  _comm->magic = handle.magic;

  MSCCLPPCHECKGOTO(mscclppCudaHostCalloc((uint32_t **)&_comm->abortFlag, 1), res, fail);
  MSCCLPPCHECK(bootstrapInit(&handle, _comm));

  // _comm->maxLocalRanks = 8;
  // MSCCLPPCHECKGOTO(mscclppCalloc(&_comm->rankToNode, nranks), res, fail);
  // MSCCLPPCHECKGOTO(mscclppCalloc(&_comm->rankToLocalRank, nranks), res, fail);
  // MSCCLPPCHECKGOTO(mscclppCalloc(&_comm->localRankToRank, _comm->maxLocalRanks), res, fail);

  // MSCCLPPCHECKGOTO(mscclppCalloc(&hashes, nranks), res, fail);
  // hashes[rank] = hash;
  // MSCCLPPCHECK(bootstrapAllGather(_comm->bootstrap, hashes, sizeof(uint64_t)));

  // for (int i = 0; i < nranks; ++i) {
  //   auto it = hashToNode.find(hashes[i]);
  //   if (it == hashToNode.end()) {
  //     _comm->nNodes++;
  //     hashToNode[hashes[i]] = _comm->nNodes - 1;
  //     _comm->rankToNode[i] = _comm->nNodes - 1;
  //   } else {
  //     _comm->rankToNode[i] = it->second;
  //   }
  //   if (hashes[i] == hash) {
  //     _comm->rankToLocalRank[i] = _comm->localRanks++;
  //     _comm->localRankToRank[_comm->rankToLocalRank[i]] = i;
  //   }
  // }
  // if (_comm->localRanks > _comm->maxLocalRanks) {
  //   WARN("Too many ranks on the same host: %d", _comm->localRanks);
  //   res = mscclppInvalidUsage;
  //   goto fail;
  // }
  // _comm->node = _comm->rankToNode[rank];
  // _comm->localRank = _comm->rankToLocalRank[rank];

  *comm = _comm;
  return res;
fail:
  if (_comm) {
    if (_comm->abortFlag) mscclppCudaHostFree((void *)_comm->abortFlag);
    free(_comm);
  }
  if (comm) *comm = NULL;
  return res;
}

MSCCLPP_API(mscclppResult_t, mscclppCommDestroy, mscclppComm_t comm);
mscclppResult_t mscclppCommDestroy(mscclppComm_t comm){
  if (comm == NULL)
    return mscclppSuccess;

  for (int i = 0; i < MSCCLPP_IB_MAX_DEVS; ++i) {
    if (comm->ibContext[i]) {
      MSCCLPPCHECK(mscclppIbContextDestroy(comm->ibContext[i]));
    }
  }

  if (comm->bootstrap)
    MSCCLPPCHECK(bootstrapClose(comm->bootstrap));

  mscclppCudaHostFree((void *)comm->abortFlag);
  free(comm);
  return mscclppSuccess;
}

MSCCLPP_API(mscclppResult_t, mscclppConnect, mscclppComm_t comm, mscclppDevConn* devConnOut, int remoteRank,
            void* localBuff, size_t buffSize, int* localFlag, int tag, mscclppTransport_t transportType, const char *ibDev);
mscclppResult_t mscclppConnect(mscclppComm_t comm, mscclppDevConn* devConnOut, int remoteRank, void* localBuff, size_t buffSize,
                               int* localFlag, int tag, mscclppTransport_t transportType, const char *ibDev/*=NULL*/)
{
  if (comm->nConns == MAXCONNECTIONS) {
    WARN("Too many connections made");
    return mscclppInternalError;
  }
  if (devConnOut == NULL) {
    WARN("devConnOut is the output of this function and needs to be allocated by the user");
    return mscclppInvalidUsage;
  }
  struct mscclppConn *conn = &comm->conns[comm->nConns++];
  conn->transport = transportType;
  conn->remoteRank = remoteRank;
  conn->buffSize = buffSize;
  conn->devConn = devConnOut;
  conn->devConn->localBuff = localBuff;
  conn->devConn->localFlag = localFlag;
  conn->devConn->tag = tag;
  MSCCLPPCHECK(mscclppCudaHostCalloc(&conn->devConn->trigger, 1));

  conn->ibCtx = NULL;
  conn->ibQp = NULL;
  if (ibDev != NULL) {
    // Check if an IB context exists
    int ibDevIdx = -1;
    int firstNullIdx = -1;
    for (int i = 0; i < MSCCLPP_IB_MAX_DEVS; ++i) {
      if (comm->ibContext[i] == NULL) {
        if (firstNullIdx == -1) {
          firstNullIdx = i;
        }
      } else if (strncmp(comm->ibContext[i]->ctx->device->name, ibDev, IBV_SYSFS_NAME_MAX) == 0) {
        ibDevIdx = i;
        break;
      }
    }
    if (ibDevIdx == -1) {
      // Create a new context.
      if (firstNullIdx == -1) {
        WARN("Too many IB devices");
        return mscclppInvalidUsage;
      }
      ibDevIdx = firstNullIdx;
      if (mscclppIbContextCreate(&comm->ibContext[ibDevIdx], ibDev) != mscclppSuccess) {
        WARN("Failed to create IB context");
        return mscclppInternalError;
      }
    }
    conn->ibCtx = comm->ibContext[ibDevIdx];
  }
  return mscclppSuccess;
}

struct connInfo {
  cudaIpcMemHandle_t handleBuff;
  cudaIpcMemHandle_t handleFlag;
  mscclppIbQpInfo infoQp;
  mscclppIbMrInfo infoBuffMr;
  mscclppIbMrInfo infoLocalFlagMr;
  mscclppIbMrInfo infoRemoteFlagMr;
};

mscclppResult_t mscclppP2pConnectionSetupStart(struct connInfo* connInfo /*output*/, struct mscclppConn* conn /*input*/){
  if (connInfo == NULL || conn == NULL){
    WARN("connInfo or connection cannot be null");
    return mscclppInternalError;
  }
  CUDACHECK(cudaIpcGetMemHandle(&connInfo->handleBuff, conn->devConn->localBuff));
  CUDACHECK(cudaIpcGetMemHandle(&connInfo->handleFlag, conn->devConn->localFlag));
  return mscclppSuccess;
}

mscclppResult_t mscclppP2pConnectionSetupEnd(struct connInfo* connInfo /*input*/, struct mscclppConn* conn /*output*/){
  if (connInfo == NULL || conn == NULL){
    WARN("ipcHandles or connection cannot be null");
    return mscclppInternalError;
  }
  CUDACHECK(cudaIpcOpenMemHandle((void**)&conn->devConn->remoteBuff, connInfo->handleBuff, cudaIpcMemLazyEnablePeerAccess));
  CUDACHECK(cudaIpcOpenMemHandle((void**)&conn->devConn->remoteFlag, connInfo->handleFlag, cudaIpcMemLazyEnablePeerAccess));
  return mscclppSuccess;
}

mscclppResult_t mscclppIbConnectionSetupStart(struct connInfo* connInfo /*output*/, struct mscclppConn* conn /*input*/){
  if (connInfo == NULL || conn == NULL){
    WARN("connInfo or connection cannot be null");
    return mscclppInternalError;
  }
  struct mscclppDevConn *devConn = conn->devConn;
  devConn->remoteBuff = NULL;
  MSCCLPPCHECK(mscclppCudaCalloc(&devConn->remoteFlag, 1));

  struct mscclppIbContext *ibCtx = conn->ibCtx;
  if (conn->ibQp == NULL) {
    MSCCLPPCHECK(mscclppIbContextCreateQp(ibCtx, &conn->ibQp));
  }
  MSCCLPPCHECK(mscclppIbContextRegisterMr(ibCtx, devConn->localBuff, conn->buffSize, &conn->ibBuffMr));
  MSCCLPPCHECK(mscclppIbContextRegisterMr(ibCtx, devConn->localFlag, sizeof(int), &conn->ibLocalFlagMr));
  MSCCLPPCHECK(mscclppIbContextRegisterMr(ibCtx, devConn->remoteFlag, sizeof(int), &conn->ibRemoteFlagMr));
  connInfo->infoQp = conn->ibQp->info;
  connInfo->infoBuffMr = conn->ibBuffMr->info;
  connInfo->infoLocalFlagMr = conn->ibLocalFlagMr->info;
  connInfo->infoRemoteFlagMr = conn->ibRemoteFlagMr->info;
  return mscclppSuccess;
}

mscclppResult_t mscclppIbConnectionSetupEnd(struct connInfo* connInfo /*input*/, struct mscclppConn* conn /*output*/){
  if (connInfo == NULL || conn == NULL){
    WARN("ipcHandles or connection cannot be null");
    return mscclppInternalError;
  }
  if (conn->ibQp->rtr(&connInfo->infoQp) != 0) {
    WARN("Failed to transition QP to RTR");
    return mscclppInvalidUsage;
  }
  if (conn->ibQp->rts() != 0) {
    WARN("Failed to transition QP to RTS");
    return mscclppInvalidUsage;
  }
  conn->ibBuffMrInfo = connInfo->infoBuffMr;
  conn->ibLocalFlagMrInfo = connInfo->infoLocalFlagMr;
  conn->ibRemoteFlagMrInfo = connInfo->infoRemoteFlagMr;
  return mscclppSuccess;
}

MSCCLPP_API(mscclppResult_t, mscclppConnectionSetup, mscclppComm_t comm);
mscclppResult_t mscclppConnectionSetup(mscclppComm_t comm)
{
  // Send info to peers
  for (int i = 0; i < comm->nConns; ++i) {
    struct mscclppConn *conn = &comm->conns[i];

    struct connInfo cInfo;
    if (conn->transport == mscclppTransportP2P) {
      MSCCLPPCHECK(mscclppP2pConnectionSetupStart(&cInfo, conn));
    } else if (conn->transport == mscclppTransportIB) {
      MSCCLPPCHECK(mscclppIbConnectionSetupStart(&cInfo, conn));
    }
    MSCCLPPCHECK(bootstrapSend(comm->bootstrap, conn->remoteRank, conn->devConn->tag, &cInfo, sizeof(cInfo)));
  }

  // Recv info from peers
  for (int i = 0; i < comm->nConns; ++i) {
    struct mscclppConn *conn = &comm->conns[i];
    struct connInfo cInfo;
    MSCCLPPCHECK(bootstrapRecv(comm->bootstrap, conn->remoteRank, conn->devConn->tag, &cInfo, sizeof(cInfo)));
    if (conn->transport == mscclppTransportP2P) {
      MSCCLPPCHECK(mscclppP2pConnectionSetupEnd(&cInfo, conn));
    } else if (conn->transport == mscclppTransportIB) {
      MSCCLPPCHECK(mscclppIbConnectionSetupEnd(&cInfo, conn));
    }
  }
  return mscclppSuccess;
}

MSCCLPP_API(mscclppResult_t, mscclppProxyLaunch, mscclppComm_t comm);
mscclppResult_t mscclppProxyLaunch(mscclppComm_t comm)
{
  MSCCLPPCHECK(mscclppProxyCreate(comm));
  return mscclppSuccess;
}

MSCCLPP_API(mscclppResult_t, mscclppProxyStop, mscclppComm_t comm);
mscclppResult_t mscclppProxyStop(mscclppComm_t comm)
{
  MSCCLPPCHECK(mscclppProxyDestroy(comm));
  return mscclppSuccess;
}
