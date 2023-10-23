#include "GameTimer.h"

using namespace _GameTimer;

GameTimer::GameTimer() : mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseCount(0), mPauseCount(0), mStopCount(0), mPrevCount(0), mCurrCount(0), isStopped(false) {
	// 计算计数器每秒多少count
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);// 获取一count多少秒
	mSecondsPerCount = 1.0 / (double)countsPerSec;
}

// 计算每帧间隔
void GameTimer::Tick() {
	// 如果停止，则每帧间隔为0
	if (isStopped) {
		mDeltaTime = 0.0;
		return;
	}

	// 计算当前时刻count
	__int64 currCount;
	QueryCnt(&currCount);
	mCurrCount = currCount;
	mDeltaTime = Cnt2Sec(mCurrCount - mPrevCount);// 计算时间差
	mPrevCount = mCurrCount;

	// 使时间差为非负值
	if (mDeltaTime < 0.0) 
		mDeltaTime = 0.0;
}

// 获得每帧时间
float GameTimer::DeltaTime() const {
	return (float)mDeltaTime;
}

// 重置计时器
void GameTimer::Reset() {
	__int64 currCount;
	QueryCnt(&currCount);

	mBaseCount = currCount;
	mPrevCount = currCount;
	mStopCount = 0;
	isStopped = false;
}

// 关闭计时器
void GameTimer::Stop() {
	if (!isStopped) {
		__int64 currCount;
		QueryCnt(&currCount);
		mStopCount = currCount;
		isStopped = true;
	}
}

// 打开计时器
void GameTimer::Start() {
	__int64 startCount;
	QueryCnt(&startCount);
	if (isStopped) {
		mPauseCount += (startCount - mStopCount);
		mPrevCount = startCount;
		mStopCount = 0;
		isStopped = false;
	}
}

// 运行时间 不包括暂停
float GameTimer::TotalTime() const {
	if (isStopped)
		return Cnt2Sec(mStopCount - mBaseCount - mPauseCount );
	else
		return Cnt2Sec(mCurrCount - mBaseCount - mPauseCount );
}

// 获取isStopped
bool GameTimer::IsStopped() {
	return isStopped;
}