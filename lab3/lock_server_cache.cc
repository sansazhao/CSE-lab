/// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"


lock_server_cache::lock_server_cache():
  nacquire (0)
{
  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cond, NULL);
}


int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&mutex);
  // std::cout <<"acquire id:"<< id <<std::endl;
  //free return OK 
  if(lock[lid].empty()){
    lock[lid] = id;
    // std::cout <<"grant lid:"<< lid << " id:"<< id <<std::endl;
  }
  // else if(wait_set1[lid].count(id) == 0){
    
  //}
  else{
    // printf("insert wait %s\n",id.c_str());
    // if(id != lock[lid])
    wait_set[lid].push(id);
    // wait_set1[lid].insert(id);
    if(wait_set[lid].size() > 1){//has others
      ret = lock_protocol::RETRY;
      // printf("RETRY1\n");
    }
    else{//revoke the holder and return OK
      // printf("holder: %s\n", lock[lid].c_str());
      handle h(lock[lid]);
      rpcc* cl = h.safebind();
      int revoke = 0;
      // printf("start revoke %d\n", lid);
      pthread_mutex_unlock(&mutex);
      revoke = cl->call(rlock_protocol::revoke, lid, r);
      pthread_mutex_lock(&mutex);
      if(revoke == lock_protocol::RETRY){
        // printf("RETRY2\n");
        ret = lock_protocol::RETRY;
      }
      else{
        ret = rlock_protocol::REVOKE;
        // printf("REVOKE\n");
      }
      pthread_mutex_unlock(&mutex);
      return ret;
    }
  }
  // else{
    // r = lock_protocol::OK;
  // }
  pthread_mutex_unlock(&mutex);
  return ret;
}

int 
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  pthread_mutex_lock(&mutex);
  if(lock[lid] == id){
    // std::cout <<"release lid:"<< lid << " id:"<< id <<std::endl;
  }
  else{
    pthread_mutex_unlock(&mutex);
    return ret;
  }
  if(wait_set[lid].size() > 0){
    std::string next = wait_set[lid].front();
    // printf("retry: %s\n", next.c_str());
    handle h(next);
    rpcc *cl = h.safebind();
    lock[lid] = next;
    // wait_set[lid].pop();
    pthread_mutex_unlock(&mutex);
    cl->call(rlock_protocol::retry, lid, r);
    pthread_mutex_lock(&mutex);
    lock[lid] = next;
    wait_set[lid].pop();
    wait_set1[lid].erase(id);
    // printf("NOW lock[lid]:%s\n",lock[lid].c_str());
    // printf("NOW queue front:%s\n",wait_set[lid].front().c_str());
    if(wait_set[lid].size() > 0){
      // printf("revoke %s still wait %d\n",lock[lid].c_str(), wait_set[lid].size());
      pthread_mutex_unlock(&mutex);
      cl->call(rlock_protocol::revoke, lid, r);
      return ret;
    }
    pthread_mutex_unlock(&mutex);
    return ret;
  }
  pthread_mutex_unlock(&mutex);
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

