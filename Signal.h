//
// Signal.h
//
// Implements a c++11 style signal

#pragma once

#include <functional>
#include <list>
#include <memory>


template <typename... T>
class SignalConnection;

template <typename... T>
class ScopedSignalConnection;

template <typename... T>
class SignalConnectionItem
{
public:
	typedef std::function<void(T...)> Callback;

	SignalConnectionItem(const Callback& cb, bool connected = true)
		: m_callback(cb)
		, m_connected(connected)
	{
	}

	void operator()(T... args)
	{
		if (m_connected && m_callback)
			m_callback(args...);
	}

	bool IsConnected() const
	{
		return m_connected;
	}

	void Disconnect()
	{
		m_connected = false;
	}

private:
	Callback m_callback;
	bool m_connected;
};

template <typename... T>
class Signal
{
public:
	typedef std::function<void(T...)> Callback;
	typedef SignalConnection<T...> Connection;
	typedef ScopedSignalConnection<T...> ScopedConnection;

private:
	typedef SignalConnectionItem<T...> ConnectionItem;
	typedef std::list<std::shared_ptr<ConnectionItem>> ConnectionList;

	ConnectionList m_list;
	uint32_t m_recurseCount = 0;

	void ClearDisconnected()
	{
		m_list.erase(std::remove_if(m_list.begin(), m_list.end(),
			[](std::shared_ptr<ConnectionItem>& item)
		{
			return !item->IsConnected();
		}), m_list.end());
	}
	
public:
	Signal() {}

	~Signal()
	{
		for (auto& item : m_list)
			item->Disconnect();
	}

	void operator()(T... args)
	{
		std::list<std::shared_ptr<ConnectionItem>> list;
		for (auto& item : m_list)
		{
			if (item->IsConnected())
				list.push_back(item);
		}

		++m_recurseCount;

		for (auto& item : list)
		{
			(*item)(args...);
		}

		if (--m_recurseCount == 0)
			ClearDisconnected();
	}

	Connection Connect(const Callback& callback)
	{
		auto item = std::make_shared<ConnectionItem>(callback, true);
		m_list.push_back(item);

		return Connection(*this, item);
	}

	bool Disconnect(const Connection& connection)
	{
		bool found = false;
		for (auto& item : m_list)
		{
			if (connection.HasItem(*item) && item->IsConnected())
			{
				found = true;
				item->Disconnect();
			}
		}

		if (found)
		{
			ClearDisconnected();
		}

		return found;
	}

	bool DisconnectAll()
	{
		for (auto& item : m_list)
			item->Disconnect();

		ClearDisconnected();
	}

	friend class Connection;
};

template <typename... T>
class SignalConnection
{
private:
	typedef SignalConnectionItem<T...> Item;

	Signal<T...>* m_signal = nullptr;
	std::shared_ptr<Item> m_item;

public:
	SignalConnection() {}

	SignalConnection(Signal<T...>& signal, const std::shared_ptr<Item>& item)
		: m_signal(signal)
		, m_item(item)
	{}

	void operator=(const SignalConnection& other)
	{
		m_signal = other.m_signal;
		m_item = other.m_item;
	}

	bool HasItem(const Item& item) const
	{
		return m_item.get() == &item;
	}

	bool IsConnected() const
	{
		return m_item->IsConnected();
	}

	bool Disconnect()
	{
		if (m_signal && m_item && m_item->IsConnected())
			return m_signal->Disconnect(*this);

		return false;
	}
};

template <typename... T>
class ScopedSignalConnection : public SignalConnection<T...>
{
public:
	ScopedSignalConnection() {}

	ScopedSignalConnection(Signal<T...>* signal, void* callback)
		: SignalConnection<T...>(signal, callback)
	{}

	ScopedSignalConnection(const SignalConnection<T...>& other)
		: SignalConnection<T...>(other)
	{}

	~ScopedSignalConnection()
	{
		Disconnect();
	}

	ScopedSignalConnection& operator=(const SignalConnection<T...>& connection)
	{
		Disconnect();
		SignalConnection<T...>::operator=(connection);
		return *this;
	}
};
