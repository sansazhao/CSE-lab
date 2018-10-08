// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cond, NULL);
  granted.clear();
  lock_stat.clear();
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  pthread_mutex_lock(&mutex);
  r = lock_stat[lid];
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
  printf("LOCK want to acquire %llu\n",  lid);
  pthread_mutex_lock(&mutex);

  if(granted.find(lid) != granted.end()){
    printf("LOCK find %llu\n",lid);
    while(granted[lid])
      pthread_cond_wait(&cond, &mutex);
  }
  printf("LOCK find lock free, ready to acquire\n");
  granted[lid] = true;
  lock_stat[lid]++;

  pthread_mutex_unlock(&mutex);
  printf("LOCK acquire succ\n");
  return lock_protocol::OK;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
  //printf("RELEASE want to release %llu\n", clt, lid);
  pthread_mutex_lock(&mutex);
  
  if(granted.find(lid) == granted.end() || granted[lid] == false){
    printf("-----Release ERROR: %llu\n",lid);
    pthread_mutex_unlock(&mutex);
    return lock_protocol::NOENT;
  }  
  
  printf("LOCK release and broadcast\n");
  granted[lid] = false;
 // lock_stat[lid]--;
  pthread_cond_signal(&cond);
  
  pthread_mutex_unlock(&mutex);
  printf("LOCK release succ\n");
  return lock_protocol::OK;
}
