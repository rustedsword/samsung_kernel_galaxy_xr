/*
* Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: BSD-3-Clause-Clear
*/

#include "PowerHintSession.h"
#include "utils.h"
#include "hint-data.h"
#include "performance.h"

#define CPU_BOOST_HINT      0x0000104E
#define THREAD_LOW_LATENCY  0x40CD0000
#define UCLAMP_MIN_FOR_TASK 0x63C04000
#define UCLAMP_MAX_FOR_TASK 0x63C08000
#define MPCTLV3_SCHED_TASK_LOAD_BOOST  0x43C04000
#define MAX_THREADS 32
#define MAX_BOOST 200
#define MIN_BOOST -200
#define PERFLOCK_MAX_THREADS 5
#define PERFLOCK_MAX_BOOST_PCT 80
#define PERFLOCK_MIN_BOOST_PCT -80
#define PERFLOCK_BOOST_INC_PCT 10
#define PERFLOCK_BOOST_DEC_PCT -10

#include <android-base/logging.h>
#include <android-base/stringprintf.h>
#include <cutils/properties.h>
#include <utils/Trace.h>
#include <atomic>

#define LOG_TAG "QTI PowerHAL"
#define ATRACE_TAG (ATRACE_TAG_POWER | ATRACE_TAG_HAL)

std::unordered_map<PowerHintSessionImpl*, int32_t> mPowerHintSessions;
std::mutex mSessionLock;

using ::android::base::StringPrintf;
static std::atomic<int64_t> sSessionIDCounter{0};

static int getPropInt(const char *key, int defaultValue) {
    char value[PROPERTY_VALUE_MAX];
    property_get(key, value, "");

    if (strlen(value) == 0)
        return defaultValue;

    return atoi(value);
}

static int getPropHexToInt(const char *key, int defaultValue) {
    char value[PROPERTY_VALUE_MAX];
    property_get(key, value, "");

    if (strlen(value) == 0)
        return defaultValue;

    return strtol(value, NULL, 16);
}

static double getPropDouble(const char *key, double defaultValue) {
    char value[PROPERTY_VALUE_MAX];
    property_get(key, value, "");

    if (strlen(value) == 0)
        return defaultValue;

    return atof(value);
}

static int64_t ns_to_100us(int64_t ns) {
    return ns / 100000;
}

static int validateBoost(int boostVal, int boostSum) {
    boostSum += boostVal;
    if(boostSum > MAX_BOOST)
        return MAX_BOOST;
    else if(boostSum < MIN_BOOST)
        return MIN_BOOST;
    return boostSum;
}

void PowerHintSessionImpl::resetBoost() {
    if(mHandle > 0)
        release_request(mHandle);
    mHandle = -1;
    mLastAction = LOAD_RESET;
}

static std::string printThreads(std::vector<int32_t>& threadIds) {
    std::string str;
    for(int num : threadIds)
        str = str + std::to_string(num) + " ";
    return str;
}

bool PowerHintSessionImpl::perfBoost(int boostVal, int hintType) {
    int tBoostSum = mBoostSum;
    int mHandlePerfHint = -1;

    if(hintType == LOAD_RESET){
        resetBoost();
        return true;
    }

    if(hintType == LOAD_RESUME && mLastAction != LOAD_RESET) {
        tBoostSum = 0;
    }

    tBoostSum = validateBoost(boostVal, tBoostSum);
    if(tBoostSum != 0) {
        mHandlePerfHint = perf_hint_enable(CPU_BOOST_HINT, tBoostSum);
        if(mHandlePerfHint < 0) {
            LOG(ERROR) << mIdString.c_str() << " Unable to acquire Perf hint for" << CPU_BOOST_HINT;
            return false;
        }
    }

    if(mHandle > 0)
        release_request(mHandle);
    mBoostSum = tBoostSum;
    mHandle = mHandlePerfHint;
    mLastAction = hintType;
    return true;
}

bool PowerHintSessionImpl::perfLockBoost(int boostVal, int hintType) {
    int tBoostSumPerfLock = mBoostSumPerfLock;
    int handle = -1;

    // calculate boostSum
    if(hintType == LOAD_RESET || hintType == LOAD_RESUME) {
        tBoostSumPerfLock = 0;
    }

    tBoostSumPerfLock += boostVal;
    if(tBoostSumPerfLock > PERFLOCK_MAX_BOOST_PCT) {
        tBoostSumPerfLock = PERFLOCK_MAX_BOOST_PCT;
        if(mHandleFreq < 0) {
            // Change the min freq to 800MHz
            int listFreq[2];
            listFreq[0] = 0x40800000;
            listFreq[1] = 800;
            mHandleFreq = interaction_with_handle(mHandleFreq, 3000, 2, listFreq);
            if(mHandleFreq < 0) {
                LOG(ERROR) << mIdString.c_str() << " Unable to acquire Perf lock.";
            }
        }
    }
    else {
        if(mHandleFreq > 0) {
            release_request(mHandleFreq);
            mHandleFreq = -1;
        }
    }
    if(tBoostSumPerfLock < PERFLOCK_MIN_BOOST_PCT)
        tBoostSumPerfLock = PERFLOCK_MIN_BOOST_PCT;

    // release old boost
    if(mHandlePerfLock > 0)
        release_request(mHandlePerfLock);

    if(tBoostSumPerfLock != 0) {
        int numThreads = (mThreadIds.size() > PERFLOCK_MAX_THREADS) ? PERFLOCK_MAX_THREADS : mThreadIds.size();
        int size = numThreads * 2;
        int list[size];
        for(int i = 0; i < size; i+=2) {
            list[i] = MPCTLV3_SCHED_TASK_LOAD_BOOST;
            list[i + 1] = (std::abs(tBoostSumPerfLock) & 0x7F) | ((tBoostSumPerfLock < 0) ? 0x80 : 0x00) | (mThreadIds[i/2] << 8);
        }

        // acquire new boost
        handle = interaction_with_handle(handle, 0, size, list);
        if(handle < 0) {
            LOG(ERROR) << mIdString.c_str() << " Unable to acquire Perf lock.";
            return false;
        }
    }

    mHandlePerfLock = handle;
    if(hintType != LOAD_RESET)
        mBoostSumPerfLock = tBoostSumPerfLock;
    mLastActionPerfLock = hintType;
    return true;
}

bool isSessionAlive(PowerHintSessionImpl* session) {
    if(mPowerHintSessions.find(session) != mPowerHintSessions.end())
        return true;
    return false;
}

bool isSessionActive(PowerHintSessionImpl* session) {
    if(!isSessionAlive(session))
        return false;
    if(mPowerHintSessions[session] == 1)
        return true;
    return false;
}

static int64_t sessionPreferredRate = -1L;

std::shared_ptr<aidl::android::hardware::power::IPowerHintSession> setPowerHintSession(int32_t tgid, int32_t uid, const std::vector<int32_t>& threadIds, int64_t durationNanos){
    int adpf_enabled = getPropInt("vendor.powerhint.adpf.enable", 0);

    if (!adpf_enabled) {
        LOG(INFO) << "setPowerHintSession : adpf is not enabled.";
        sessionPreferredRate = -1L;
        return nullptr;
    }

    LOG(INFO) << "setPowerHintSession ";
    std::shared_ptr<aidl::android::hardware::power::IPowerHintSession> mPowerSession = ndk::SharedRefBase::make<PowerHintSessionImpl>(tgid, uid, threadIds, durationNanos);

    if(mPowerSession == nullptr) {
        sessionPreferredRate = -1L;
        return nullptr;
    }

    sessionPreferredRate = 13888888L;
    return mPowerSession;
}

int64_t getSessionPreferredRate(){
    return sessionPreferredRate;
}

void setSessionActivity(PowerHintSessionImpl* session, bool flag) {
    std::lock_guard<std::mutex> mLockGuard(mSessionLock);
    if(flag)
        mPowerHintSessions[session] = 1;
    else
        mPowerHintSessions[session] = 0;
}

PowerHintSessionImpl::PowerHintSessionImpl(int32_t tgid, int32_t uid, const std::vector<int32_t>& threadIds, int64_t durationNanos)
    : mSessionId(++sSessionIDCounter),
      mIdString(StringPrintf("%" PRId32 "-%" PRId32 "-%" PRId64, tgid, uid, mSessionId)),
      mAdpfTargetDurStr(StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRId64 "-target", tgid, uid, mSessionId)),
      mAdpfAdjTargetDurStr(StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRId64 "-Adj.target", tgid, uid, mSessionId)),
      mAdpfActualDurStr(StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRId64 "-actual", tgid, uid, mSessionId)),
      mAdpfUclampMinStr(StringPrintf("adpf.%" PRId32 "-%" PRId32 "-%" PRId64 "-uclamp.min", tgid, uid, mSessionId)) {
    mUid = uid;
    mTgid = tgid;
    mBoostSum = 0;
    mHandle = -1;
    mLastAction = -1;
    setSessionActivity(this, true);
    mThreadIds = threadIds;

    mBoostSumPerfLock = 0;
    mHandlePerfLock = -1;
    mLastActionPerfLock = -1;
    mHandleFreq = -1;

    mForceTargetDuration = getPropInt("vendor.powerhint.adpf.forcetargetduration", -1);
    mTargetWorkDurationNanos = durationNanos;
    
    mUclampCurrentMin = 0;
    mUclampMinInit = getPropInt("vendor.powerhint.adpf.uclampmininit", 693);   // 1920000 KHz
    mUclampMinLow = getPropInt("vendor.powerhint.adpf.uclampminlow", 395);     // 1094400 KHz
    mUclampMinHigh = getPropInt("vendor.powerhint.adpf.uclampminhigh", 693);   // 1920000 KHz
    mUclampMinDownThreshold = getPropInt("vendor.powerhint.adpf.uclampmindownthreshold", 10);   // 10% over current uclamp min
    mLowLatency = getPropInt("vendor.powerhint.adpf.lowlatency", 1);
    mThreadHandle = setThreadPipelining(mThreadIds);
    mThreadIdsSize = mThreadIds.size();
    if (mThreadIdsSize > 0)
        mUclampReqArgs = (int *)malloc(mThreadIdsSize * 4 * sizeof(int));
    else
        mUclampReqArgs = NULL;

    int offset = 0;
    offset = snprintf(mAdpfTraceBuffer, TRACE_BUF_SIZE, "%s tid:", mIdString.c_str());
    for (int i = 0; i < mThreadIdsSize; i++) {
        offset += snprintf(mAdpfTraceBuffer + offset, TRACE_BUF_SIZE - offset, " %d", mThreadIds[i]);
    }
    applyUclampMin(mUclampMinInit);
    
    LOG(INFO) << mIdString.c_str() << " PowerHintSessionImpl create ";
    LOG(INFO) << mIdString.c_str() << " low_latency : " << mLowLatency;
    LOG(INFO) << mIdString.c_str() << " targetduration input : " << durationNanos << " force : " << mForceTargetDuration;
    LOG(INFO) << mIdString.c_str() << " uclamp.min init : " << mUclampMinInit << " low : " << mUclampMinLow << " high : " << mUclampMinHigh;
    LOG(INFO) << mIdString.c_str() << " down threshold : " << mUclampMinDownThreshold;
}

PowerHintSessionImpl::~PowerHintSessionImpl(){
     close();
}

int PowerHintSessionImpl::setThreadPipelining(std::vector<int32_t>& threadIds) {
    int mHandleTid = -1;
    int size = threadIds.size() * 2;
    int args[MAX_THREADS];

    if(mThreadHandle > 0)
        return mThreadHandle;

    if(mLowLatency != 1)
        return -1;

    if(size > MAX_THREADS)
        return -1;

    for(int i=0; i<size; i+=2) {
        args[i] = THREAD_LOW_LATENCY;
        args[i+1] = threadIds[i/2];
    }

    mHandleTid = interaction_with_handle(0, 0, size, args);
    if(mHandleTid < 0) {
            LOG(ERROR) << mIdString.c_str() << " Unable to put these threads into pipeline";
            return -1;
    }

    LOG(INFO) << mIdString.c_str() << " Thread Low Latency Handle " << mHandleTid << " for threads " << printThreads(threadIds);
    return mHandleTid;
}

void PowerHintSessionImpl::removePipelining() {
    if(mThreadHandle > 0) {
        release_request(mThreadHandle);
        LOG(INFO) << mIdString.c_str() << " Handle " << mThreadHandle << " released";
    }
    mThreadHandle = -1;
}

void PowerHintSessionImpl::resumeThreadPipelining() {
    mThreadHandle = setThreadPipelining(mThreadIds);
}

ndk::ScopedAStatus PowerHintSessionImpl::updateTargetWorkDuration(int64_t in_targetDurationNanos){
    if(in_targetDurationNanos <= 0) {
        LOG(ERROR) << mIdString.c_str() << " Invalid target work duration";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    int64_t prevTargetDuration = mTargetWorkDurationNanos;
    mTargetWorkDurationNanos = in_targetDurationNanos;
    if (prevTargetDuration != mTargetWorkDurationNanos) {
        LOG(INFO) << mIdString.c_str() << " updateTargetWorkDuration " << mTargetWorkDurationNanos;
    }

    ATRACE_INT(mAdpfTargetDurStr.c_str(), mTargetWorkDurationNanos);

    return ndk::ScopedAStatus::ok();
}

void PowerHintSessionImpl::removeUclampMin()
{
    if (mUclampMinHandle > 0) {
        release_request(mUclampMinHandle);
        mUclampCurrentMin = 0;
        mUclampMinHandle = -1;
    }
}

int PowerHintSessionImpl::applyUclampMin(int in_uclampMin)
{
    int size = mThreadIds.size();
    int uclampMin;
    int *args;
    int handle = -1;

    ATRACE_BEGIN(mAdpfTraceBuffer);

    if (mUclampCurrentMin == in_uclampMin) {
        handle = mUclampMinHandle;
        goto out;
    } else if (in_uclampMin < 0)
        uclampMin = 0;
    else if (in_uclampMin > 1024)
        uclampMin = 1024;
    else
        uclampMin = in_uclampMin;

    removeUclampMin();

    if (size > 0) {
        if (!mUclampReqArgs || mThreadIdsSize < size) {
            mThreadIdsSize = size;
            free(mUclampReqArgs);
            mUclampReqArgs = (int *)malloc(mThreadIdsSize * 4 * sizeof(int));
            if(!mUclampReqArgs) {
                LOG(ERROR) << mIdString.c_str() << " Unable to malloc for Perf lock.";
                handle = mUclampMinHandle;
                goto out;
            }
        }
        size = size * 4;
        args = mUclampReqArgs;
        for(int i = 0; i < size; i += 4) {
            args[i] = UCLAMP_MIN_FOR_TASK;
            args[i + 1] = 2;
            args[i + 2] = mThreadIds[i / 4];
            args[i + 3] = uclampMin;
            LOG(DEBUG) << mIdString.c_str() << " applyUclampMin " << args[i] << " " << args[i + 1] << " " << args[i + 2] << " " << args[i + 3];
        }
        handle = interaction_with_handle(handle, 0, size, args);
        if(handle < 0) {
            LOG(ERROR) << mIdString.c_str() << " Unable to acquire Perf lock.";
            goto out;
        }
        mUclampMinHandle = handle;
        mUclampCurrentMin = uclampMin;
    }

out:
    ATRACE_INT(mAdpfUclampMinStr.c_str(), mUclampCurrentMin);
    ATRACE_END();
    return handle;
}

int32_t PowerHintSessionImpl::convertWorkDurationToBoost(
        const std::vector<::aidl::android::hardware::power::WorkDuration> &actualDurations) {
    int32_t length = actualDurations.size();
    int64_t actualDurationNanos, adjTargetDurationNanos;
    int32_t output;

    LOG(DEBUG) << mIdString.c_str() << " convertWorkDurationToBoost length : " << length;
    LOG(DEBUG) << mIdString.c_str() << " mTargetWorkDurationNanos : " << mTargetWorkDurationNanos;
    ATRACE_INT(mAdpfTargetDurStr.c_str(), mTargetWorkDurationNanos);

    if (mForceTargetDuration > 0)
        adjTargetDurationNanos = mForceTargetDuration;
    else if (mTargetWorkDurationNanos < getSessionPreferredRate())
        adjTargetDurationNanos = getSessionPreferredRate();
    else
        adjTargetDurationNanos = mTargetWorkDurationNanos;
    ATRACE_INT(mAdpfAdjTargetDurStr.c_str(), adjTargetDurationNanos);

    actualDurationNanos = adjTargetDurationNanos;
    for (int32_t i = length - 1; i < length; i++) {
        actualDurationNanos = actualDurations[i].durationNanos;
        LOG(DEBUG) << mIdString.c_str() << " i : " << i << ", actualDurationNanos : " << actualDurationNanos;
        ATRACE_INT(mAdpfActualDurStr.c_str(), actualDurationNanos);
    }

    output = ((actualDurationNanos - adjTargetDurationNanos) * mUclampCurrentMin / actualDurationNanos) ;
    if (output < 0) {
        if (output >= -1 * mUclampMinDownThreshold * mUclampCurrentMin / 100)
            output = 0;
        else
            output = -1 * mUclampMinDownThreshold * mUclampCurrentMin / 100;
    }
    
    LOG(DEBUG) << mIdString.c_str() << " convertWorkDurationToBoost output : " << output;

    return output;
}

ndk::ScopedAStatus PowerHintSessionImpl::reportActualWorkDuration(const std::vector<::aidl::android::hardware::power::WorkDuration>& in_durations){
    if(!isSessionAlive(this)) {
        LOG(ERROR) << mIdString.c_str() << " session is not alive";
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    }
    int64_t actualDurations = in_durations.back().durationNanos;
    int64_t targetDurations = mTargetWorkDurationNanos;
    int boostSum = mBoostSumPerfLock;
    int lastAction = mLastActionPerfLock;
    LOG(DEBUG) << mIdString.c_str() << " reportActualWorkDuration " << actualDurations;

    int32_t output = convertWorkDurationToBoost(in_durations);
    int32_t next_min = std::max(mUclampMinLow, (mUclampCurrentMin + output));
    next_min = std::min(mUclampMinHigh, next_min);
    
    applyUclampMin(static_cast<int>(next_min));

    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSessionImpl::pause(){
    LOG(INFO) << mIdString.c_str() << " PowerHintSessionImpl::pause ";
    if(isSessionAlive(this)) {
        setSessionActivity(this, false);
        sendHint(aidl::android::hardware::power::SessionHint::CPU_LOAD_RESET);
        sendHintPerfLock(aidl::android::hardware::power::SessionHint::CPU_LOAD_RESET);
        if(mHandleFreq > 0) {
            release_request(mHandleFreq);
            mHandleFreq = -1;
        }
        removeUclampMin();
        removePipelining();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSessionImpl::resume(){
    LOG(INFO) << mIdString.c_str() << " PowerHintSessionImpl::resume ";
    if(isSessionAlive(this)) {
        sendHint(aidl::android::hardware::power::SessionHint::CPU_LOAD_RESUME);
        sendHintPerfLock(aidl::android::hardware::power::SessionHint::CPU_LOAD_RESUME);
        resumeThreadPipelining();
        setSessionActivity(this, true);
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSessionImpl::close(){
    LOG(INFO) << mIdString.c_str() << " PowerHintSessionImpl::close ";

    if(isSessionAlive(this)) {
        sendHint(aidl::android::hardware::power::SessionHint::CPU_LOAD_RESET);
        sendHintPerfLock(aidl::android::hardware::power::SessionHint::CPU_LOAD_RESET);
        if(mHandleFreq > 0) {
            release_request(mHandleFreq);
            mHandleFreq = -1;
        }
        if (mUclampReqArgs)
            free(mUclampReqArgs);
        removeUclampMin();
        removePipelining();
        mThreadIds.clear();

        mSessionLock.lock();
        mPowerHintSessions.erase(this);
        mSessionLock.unlock();
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSessionImpl::sendHintPerfLock(aidl::android::hardware::power::SessionHint hint){
    LOG(INFO) << mIdString.c_str() << " PowerHintSessionImpl::sendHintPerfLock ";
    if(hint == aidl::android::hardware::power::SessionHint::CPU_LOAD_RESET)
        perfLockBoost(0, LOAD_RESET);
    else if(hint == aidl::android::hardware::power::SessionHint::CPU_LOAD_RESUME)
        perfLockBoost(0, LOAD_RESUME);
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSessionImpl::sendHint(aidl::android::hardware::power::SessionHint hint){
    LOG(INFO) << mIdString.c_str() << " PowerHintSessionImpl::sendHint ";
    if(!isSessionActive(this))
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_STATE);
    switch(hint)
    {
        case aidl::android::hardware::power::SessionHint::CPU_LOAD_UP:
            perfBoost(20, LOAD_UP);
            break;
        case aidl::android::hardware::power::SessionHint::CPU_LOAD_DOWN:
            perfBoost(-20, LOAD_DOWN);
            break;
        case aidl::android::hardware::power::SessionHint::CPU_LOAD_RESET:
            perfBoost(0, LOAD_RESET);
            break;
        case aidl::android::hardware::power::SessionHint::CPU_LOAD_RESUME:
            perfBoost(0, LOAD_RESUME);
            break;
        case aidl::android::hardware::power::SessionHint::POWER_EFFICIENCY:
            perfBoost(-20, LOAD_DOWN);
            break;
    }
    return ndk::ScopedAStatus::ok();
}

ndk::ScopedAStatus PowerHintSessionImpl::setThreads(const std::vector<int32_t>& threadIds){
    LOG(INFO) << mIdString.c_str() << " PowerHintSessionImpl::setThreads ";
    if (threadIds.size() == 0) {
        LOG(ERROR) << mIdString.c_str() << " Error: threadIds.size() shouldn't be " << threadIds.size();
        return ndk::ScopedAStatus::fromExceptionCode(EX_ILLEGAL_ARGUMENT);
    }

    if(mThreadHandle > 0) {
        release_request(mThreadHandle);
        mThreadIds.clear();
        mThreadHandle = -1;
    }

    mThreadIds = threadIds;
    mThreadHandle = setThreadPipelining(mThreadIds);

    if (!mUclampReqArgs || mThreadIdsSize < threadIds.size()) {
        mThreadIdsSize = threadIds.size();
        free(mUclampReqArgs);
        mUclampReqArgs = (int *)malloc(mThreadIdsSize * 4 * sizeof(int));
        if(!mUclampReqArgs) {
            LOG(ERROR) << mIdString.c_str() << " Unable to malloc for Perf lock.";
        }
    }

    int offset = 0;
    offset = snprintf(mAdpfTraceBuffer, TRACE_BUF_SIZE, "%s tid:", mIdString.c_str());
    for (int i = 0; i < mThreadIdsSize; i++) {
        offset += snprintf(mAdpfTraceBuffer + offset, TRACE_BUF_SIZE - offset, " %d", mThreadIds[i]);
    }

    return ndk::ScopedAStatus::ok();
}
