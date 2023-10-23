#pragma once
#include <windows.h>

namespace _GameTimer {

	class GameTimer {
	public:
		GameTimer();

		float TotalTime()const;	// ��Ϸ���е���ʱ��(s)����������ͣ��
		float DeltaTime()const;	// ��ȡmDeltaTime(s)����
		bool IsStopped();	// ��ȡisStoped����

		void Reset();	// ���ü�ʱ��
		void Start();	// �򿪼�ʱ��
		void Stop();	// �رռ�ʱ��
		void Tick();	// ����ÿ֡ʱ����

	private:

		double mSecondsPerCount;	// ������ÿһ����Ҫ������
		double mDeltaTime;			// ÿ֡ʱ�䣨ǰһ֡�͵�ǰ֡��ʱ��

		__int64 mBaseCount;		// ���ú�Ļ�׼ʱ��
		__int64 mPauseCount;		// ��ͣ����ʱ��
		__int64 mStopCount;		// ֹͣ��һ�̵�ʱ��
		__int64 mPrevCount;		// ��һ֡count
		__int64 mCurrCount;	// ��֡count

		bool isStopped;		// �Ƿ�Ϊֹͣ��״̬

		inline void QueryCnt(__int64* p) { QueryPerformanceCounter((LARGE_INTEGER*)p); }// ��ѯcount
		inline double Cnt2Sec(const __int64 &count) const { return count * mSecondsPerCount; };// countת��ΪSecond
	};
}