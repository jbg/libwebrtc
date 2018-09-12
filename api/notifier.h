/*
 *  Copyright 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef API_NOTIFIER_H_
#define API_NOTIFIER_H_

#include <list>

#include "api/mediastreaminterface.h"
#include "rtc_base/checks.h"
#include "rtc_base/thread_checker.h"

namespace webrtc {

// Implements a template version of a notifier.
// TODO(deadbeef): This is an implementation detail; move out of api/.
template <class T>
class Notifier : public T {
 public:
  Notifier() { thread_checker_.DetachFromThread(); }

  virtual void RegisterObserver(ObserverInterface* observer) {
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    RTC_DCHECK(observer != nullptr);
    observers_.push_back(observer);
  }

  virtual void UnregisterObserver(ObserverInterface* observer) {
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    for (std::list<ObserverInterface*>::iterator it = observers_.begin();
         it != observers_.end(); it++) {
      if (*it == observer) {
        observers_.erase(it);
        break;
      }
    }
  }

  void FireOnChanged() {
    RTC_DCHECK(thread_checker_.CalledOnValidThread());
    // Copy the list of observers to avoid a crash if the observer object
    // unregisters as a result of the OnChanged() call. If the same list is used
    // UnregisterObserver will affect the list make the iterator invalid.
    // It will still crash in case OnChanged on one observer unregisters and
    // destroys some other observer on the list. So don't do that.
    std::list<ObserverInterface*> observers = observers_;
    for (std::list<ObserverInterface*>::iterator it = observers.begin();
         it != observers.end(); ++it) {
      (*it)->OnChanged();
    }
  }

 protected:
  std::list<ObserverInterface*> observers_;

 private:
  // No locking, so must run on a single thread, typically the signalling
  // thread.
  rtc::ThreadChecker thread_checker_;
};

}  // namespace webrtc

#endif  // API_NOTIFIER_H_
