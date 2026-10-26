/*
* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#ifndef __POWERHINTSESSION__
#define __POWERHINTSESSION__

#include <unordered_map>
#include <set>
#include <mutex>
#include <aidl/android/hardware/power/WorkDuration.h>
#include <aidl/android/hardware/power/SessionHint.h>
#include <aidl/android/hardware/power/BnPowerHintSession.h>

#define TRACE_BUF_SIZE (256)

enum LOAD_TYPE {
    LOAD_UP,
    LOAD_DOWN,
    LOAD_RESET,
    LOAD_RESUME
};

std::shared_ptr<aidl::android::hardware::power::IPowerHintSession> setPowerHintSession(int32_t tgid, int32_t uid, const std::vector<int32_t>& threadIds, int64_t durationNanos);
int64_t getSessionPreferredRate();

class PowerHintSessionImpl : public aidl::android::hardware::power::BnPowerHintSession{
public:
    explicit PowerHintSessionImpl(int32_t tgid, int32_t uid, const std::vector<int32_t>& threadIds, int64_t durationNanos);
    ~PowerHintSessionImpl();
    ndk::ScopedAStatus updateTargetWorkDuration(int64_t targetDurationNanos) override;
    ndk::ScopedAStatus reportActualWorkDuration(
            const std::vector<aidl::android::hardware::power::WorkDuration>& actualDurations) override;
    ndk::ScopedAStatus pause() override;
    ndk::ScopedAStatus resume() override;
    ndk::ScopedAStatus close() override;
    ndk::ScopedAStatus sendHint(aidl::android::hardware::power::SessionHint hint) override;
    ndk::ScopedAStatus setThreads(const std::vector<int32_t>& threadIds) override;
    ndk::ScopedAStatus sendHintPerfLock(aidl::android::hardware::power::SessionHint hint);
    bool perfBoost(int boostVal, int hintType);
    bool perfLockBoost(int boostVal, int hintType);
    int setThreadPipelining(std::vector<int32_t>& threadIds);
    void removePipelining();
    void resumeThreadPipelining();
    void resetBoost();
private:
    int32_t mUid;
    int32_t mTgid;
    int mHandle;    // CPU_BOOST_HINT
    int mBoostSum;
    int mLastAction;
    int mHandleFreq;    // MIN_FREQ
    int mThreadHandle;  // LOW_LATENCY
    std::vector<int32_t> mThreadIds;
    int64_t mForceTargetDuration;
    int64_t mTargetWorkDurationNanos;
    int mHandlePerfLock;    // SCHED_TASK_LOAD_BOOST
    int mBoostSumPerfLock;
    int mLastActionPerfLock;
    const int64_t mSessionId = 0;
    const std::string mIdString;
    const std::string mAdpfTargetDurStr;
    const std::string mAdpfAdjTargetDurStr;
    const std::string mAdpfActualDurStr;
    const std::string mAdpfUclampMinStr;
    char mAdpfTraceBuffer[TRACE_BUF_SIZE];
    int32_t mLowLatency;
    int32_t mUclampCurrentMin;
    int32_t mUclampMinInit;
    int32_t mUclampMinLow;
    int32_t mUclampMinHigh;
    int32_t mUclampMinDownThreshold;
    int mUclampMinHandle;
    int mThreadIdsSize;
    int *mUclampReqArgs;
    void removeUclampMin();
    int applyUclampMin(int newUclampMin);
    int32_t convertWorkDurationToBoost(const std::vector<::aidl::android::hardware::power::WorkDuration> &actualDurations);
};
#endif /* __POWERHINTSESSION__ */
