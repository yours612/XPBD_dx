#pragma once
#include <windows.h>

namespace _GameTimer {

	class GameTimer {
	public:
		GameTimer();

		float TotalTime()const;	// 游戏运行的总时间(s)（不包括暂停）
		float DeltaTime()const;	// 获取mDeltaTime(s)变量
		bool IsStopped();	// 获取isStoped变量

		void Reset();	// 重置计时器
		void Start();	// 打开计时器
		void Stop();	// 关闭计时器
		void Tick();	// 计算每帧时间间隔

	private:

		double mSecondsPerCount;	// 计数器每一次需要多少秒
		double mDeltaTime;			// 每帧时间（前一帧和当前帧的时间差）

		__int64 mBaseCount;		// 重置后的基准时间
		__int64 mPauseCount;		// 暂停的总时间
		__int64 mStopCount;		// 停止那一刻的时间
		__int64 mPrevCount;		// 上一帧count
		__int64 mCurrCount;	// 本帧count

		bool isStopped;		// 是否为停止的状态

		inline void QueryCnt(__int64* p) { QueryPerformanceCounter((LARGE_INTEGER*)p); }// 查询count
		inline double Cnt2Sec(const __int64 &count) const { return count * mSecondsPerCount; };// count转换为Second
	};
}