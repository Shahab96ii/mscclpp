#include "mscclpp.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>


void print_usage(const char *prog)
{
  printf("usage: %s IP:PORT rank nranks\n", prog);
}

int main(int argc, const char *argv[])
{
  if (argc != 4) {
    print_usage(argv[0]);
    return -1;
  }

  mscclppComm_t comm;
  const char *ip_port = argv[1];
  int rank = atoi(argv[2]);
  int world_size = atoi(argv[3]);

  // sleep(10);

  mscclppCommInitRank(&comm, world_size, rank, ip_port);

  int *buf = (int *)calloc(world_size, sizeof(int));
  if (buf == nullptr) {
    printf("calloc failed\n");
    return -1;
  }
  buf[rank] = rank;
  mscclppResult_t res = mscclppBootStrapAllGather(comm, buf, sizeof(int));
  if (res != mscclppSuccess) {
    printf("bootstrapAllGather failed\n");
    return -1;
  }

  for (int i = 0; i < world_size; ++i) {
    if (buf[i] != i) {
      printf("wrong data: %d, expected %d\n", buf[i], i);
      return -1;
    }
  }

  res = mscclppCommDestroy(comm);
  if (res != mscclppSuccess) {
    printf("mscclppDestroy failed\n");
    return -1;
  }

  printf("Succeeded! %d\n", rank);
  return 0;
}
