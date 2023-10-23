#include "GameTimer.h"

using namespace _GameTimer;

GameTimer::GameTimer() : mSecondsPerCount(0.0), mDeltaTime(-1.0), mBaseCount(0), mPauseCount(0), mStopCount(0), mPrevCount(0), mCurrCount(0), isStopped(false) {
	// ���������ÿ�����count
	__int64 countsPerSec;
	QueryPerformanceFrequency((LARGE_INTEGER*)&countsPerSec);// ��ȡһcount������
	mSecondsPerCount = 1.0 / (double)countsPerSec;
}

// ����ÿ֡���
void GameTimer::Tick() {
	// ���ֹͣ����ÿ֡���Ϊ0
	if (isStopped) {
		mDeltaTime = 0.0;
		return;
	}

	// ���㵱ǰʱ��count
	__int64 currCount;
	QueryCnt(&currCount);
	mCurrCount = currCount;
	mDeltaTime = Cnt2Sec(mCurrCount - mPrevCount);// ����ʱ���
	mPrevCount = mCurrCount;

	// ʹʱ���Ϊ�Ǹ�ֵ
	if (mDeltaTime < 0.0) 
		mDeltaTime = 0.0;
}

// ���ÿ֡ʱ��
float GameTimer::DeltaTime() const {
	return (float)mDeltaTime;
}

// ���ü�ʱ��
void GameTimer::Reset() {
	__int64 currCount;
	QueryCnt(&currCount);

	mBaseCount = currCount;
	mPrevCount = currCount;
	mStopCount = 0;
	isStopped = false;
}

// �رռ�ʱ��
void GameTimer::Stop() {
	if (!isStopped) {
		__int64 currCount;
		QueryCnt(&currCount);
		mStopCount = currCount;
		isStopped = true;
	}
}

// �򿪼�ʱ��
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

// ����ʱ�� ��������ͣ
float GameTimer::TotalTime() const {
	if (isStopped)
		return Cnt2Sec(mStopCount - mBaseCount - mPauseCount );
	else
		return Cnt2Sec(mCurrCount - mBaseCount - mPauseCount );
}

// ��ȡisStopped
bool GameTimer::IsStopped() {
	return isStopped;
}