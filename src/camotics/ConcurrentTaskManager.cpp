/******************************************************************************\

    CAMotics is an Open-Source CAM software.
    Copyright (C) 2011-2015 Joseph Coffland <joseph@cauldrondevelopment.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.

\******************************************************************************/

#include "ConcurrentTaskManager.h"

#include <cbang/util/SmartLock.h>
#include <cbang/util/SmartUnlock.h>
#include <cbang/util/DefaultCatch.h>

using namespace std;
using namespace cb;
using namespace CAMotics;


ConcurrentTaskManager::ConcurrentTaskManager() {
  start();
}


ConcurrentTaskManager::~ConcurrentTaskManager() {
  stop();
  signal();
  join();
}


double ConcurrentTaskManager::getProgress() const {
  SmartLock lock(this);
  return current.isNull() ? 0 : current->getProgress();
}


double ConcurrentTaskManager::getETA() const {
  SmartLock lock(this);
  return current.isNull() ? 0 : current->getETA();
}


string ConcurrentTaskManager::getStatus() const {
  SmartLock lock(this);
  return current.isNull() ? "" : current->getStatus();
}


void ConcurrentTaskManager::add(const SmartPointer<Task> &task, bool priority) {
  SmartLock lock(this);

  if (shouldShutdown()) complete(task);

  else {
    if (priority) interrupt();
    waiting.push_back(task);
    signal();
  }
}


bool ConcurrentTaskManager::hasMore() const {
  SmartLock lock(this);
  return !done.empty();
}


SmartPointer<Task> ConcurrentTaskManager::remove() {
  SmartLock lock(this);

  if (done.empty()) return 0;
  cb::SmartPointer<Task> task = done.front();
  done.pop_front();

  return task;
}


void ConcurrentTaskManager::add(TaskObserver *observer) {
  SmartLock lock(this);
  observers.insert(observer);
}


void ConcurrentTaskManager::interrupt() {
  if (!current.isNull()) current->interrupt();

  for (queue_t::iterator it = waiting.begin(); it != waiting.end(); it++)
    (*it)->interrupt();
}


void ConcurrentTaskManager::run() {
  SmartLock lock(this);

  while (!shouldShutdown() || !waiting.empty()) {
    if (waiting.empty()) Condition::wait();

    current = waiting.front();
    waiting.pop_front();

    if (!shouldShutdown() && !current->shouldQuit()) {
      SmartUnlock unlock(this);
      try {
        current->begin();
        current->run();
      } CATCH_ERROR;

      try {current->end();} CATCH_ERROR;
    }

    complete(current);
    current.release();
  }
}


void ConcurrentTaskManager::stop() {
  SmartLock lock(this);
  Thread::stop();
  interrupt();
}


void ConcurrentTaskManager::complete(const cb::SmartPointer<Task> &task) {
  if (task->shouldQuit()) return;

  done.push_back(task);

  observers_t::iterator it;
  for (it = observers.begin(); it != observers.end(); it++)
    (*it)->taskCompleted();
}
