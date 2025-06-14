#pragma once
#include <mutex>
#include <string>
#include <chrono>
#include "modal_popup.h"

using namespace std;

namespace Status {
    inline mutex _mtx;
    inline string _currentText = "Idle";
    inline chrono::steady_clock::time_point _expireTime = chrono::steady_clock::now();

    inline void Set(const string &s) {
        lock_guard<mutex> lock(_mtx);
        _currentText = s;
        _expireTime = chrono::steady_clock::now() + chrono::seconds(5);
    }

    inline void Error(const string &s) {
        Set(s);
        ModalPopup::Add(s);
    }

    inline string Get() {
        lock_guard<mutex> lock(_mtx);
        auto now = chrono::steady_clock::now();
        if (now >= _expireTime) {
            _currentText = "Idle";
            return "Idle";
        }
        auto remaining = chrono::duration_cast<chrono::seconds>(_expireTime - now).count();
        return _currentText + " (" + to_string(remaining) + ")";
    }
}
