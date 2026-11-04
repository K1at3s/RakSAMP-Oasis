#ifndef BACKEND_API_H
#define BACKEND_API_H
#include "BackendHttp.h"
bool QueueBackendRequest(const std::string &method, const std::string &path, const std::string &body,
    BackendHttpQueue::Callback callback = BackendHttpQueue::Callback(), const std::string &key = "", bool sessionBound = true);
BackendResponse GetBackendSnapshot(const std::string &path, unsigned int maxAgeMs = 10000);
bool ReadBackendOnce(const std::string &path, BackendHttpQueue::Callback callback, unsigned int maxAgeMs = 10000);
std::string DescribeBackend();
void ProcessDurableResults();
#endif
