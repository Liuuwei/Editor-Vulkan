#pragma once

#include "../include/Timer.h"

#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <functional>
#include <queue>

template<typename T>
class Animation {
public:
    Animation(const T&begin, const T&end, long long fullTime) : begin_(begin), end_(end), prevTime_(0), currTime_(0), fullTime_(fullTime), time_(0) {
        
    }

    T genCurrentState() {
        auto& animation = animations_.front();

        float coefficient = animation.time_ / static_cast<float>(animation.fullTime_);

        T result(animation.begin_.size());
        for (size_t i = 0; i < result.size(); i++) {
            result[i] = animation.begin_[i] * (1.0 - coefficient) + animation.end_[i] * coefficient;
        }

        return result;
    }

    void step() {
        assert(!animations_.empty());

        auto& animation = animations_.front();

        if (animation.firstStep_) {
            animation.firstStep_ = false;
            animation.prevTime_ = Timer::nowMilliseconds();
        }

        animation.currTime_ = Timer::nowMilliseconds();
        
        animation.time_ += animation.currTime_ - animation.prevTime_;
        if (animation.finish()) {
            if (animation.callback_) {
                animation.callback_();
            }
            animations_.pop();
        }

        animation.prevTime_ = animation.currTime_;
    }

    bool finish() {
        return animations_.empty();
    }

    void callback() {
        if (callback_) {
            callback_();
        }
    }

    void setFinishCallback(const std::function<void()>& cb) {
        callback_ = cb;
    }

    void addStage(const T& begin, const T& end, long long fullTime, const std::function<void()>& cb) {
        animations_.push({begin, end, fullTime, cb});
    }

private:
    struct OneTimeAnimation {
        OneTimeAnimation(const T& begin, const T& end, long long fullTime, const std::function<void()>& cb) : begin_(begin), end_(end), fullTime_(fullTime), callback_(cb) {

        }

        T begin_;
        T end_;
        long long time_ = 0;
        long long fullTime_;
        std::function<void()> callback_;

        long long prevTime_ = 0;
        long long currTime_ = 0;

        bool firstStep_ = true;

        bool finish() const {
            return time_ >= fullTime_;
        }
    };

    T begin_;
    T end_;
    long long prevTime_;
    long long currTime_;
    long long fullTime_;
    long long time_;
    bool firstStep_ = true;

    std::queue<OneTimeAnimation> animations_;

    std::function<void()> callback_;
};