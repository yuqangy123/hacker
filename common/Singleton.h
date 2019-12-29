#ifndef __SINGLETON_H__
#define __SINGLETON_H__

#include "stdio.h"
//单例
template <class T>
class Singleton
{
public:

	static T* getInstance()
	{
		if (!m_this)
		{
			m_this = new T();
		}

		return (T*)m_this;
	}

	static T* release()
	{
		if (m_this)
		{
			delete m_this;
			m_this = NULL;
		}
	}

	Singleton()
	{
		m_this = 0;
	}
private:
	static T* m_this;
};
template <class T>
T* Singleton<T>::m_this = 0;

#endif  // __SINGLETON_H__

