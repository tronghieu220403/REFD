// PushLock.h
#pragma once
extern "C" {
#include <ntddk.h>   // or <wdm.h>
}

class PushLock {
public:
	enum class Mode : UCHAR { Shared = 0, Exclusive = 1 };

	PushLock() noexcept { ExInitializePushLock(&m_lock); }
	~PushLock() { }

	// Explicit initialization (optional style)
	_IRQL_requires_max_(APC_LEVEL)
		NTSTATUS Create() noexcept {
		ExInitializePushLock(&m_lock);
		return STATUS_SUCCESS;
	}

	// Acquire exclusive (write) lock
	_IRQL_requires_max_(APC_LEVEL)
		_Acquires_exclusive_lock_(m_lock)
		VOID LockExclusive() noexcept {
		KeEnterCriticalRegion();
		ExAcquirePushLockExclusive(&m_lock);
		m_mode = Mode::Exclusive;
	}

	// Acquire shared (read) lock
	_IRQL_requires_max_(APC_LEVEL)
		_Acquires_shared_lock_(m_lock)
		VOID LockShared() noexcept {
		KeEnterCriticalRegion();
		ExAcquirePushLockShared(&m_lock);
		m_mode = Mode::Shared;
	}

	// Release lock (based on last acquired mode)
	_IRQL_requires_max_(APC_LEVEL)
		_Releases_lock_(m_lock)
		VOID Unlock() noexcept {
		if (m_mode == Mode::Exclusive) {
			ExReleasePushLockExclusive(&m_lock);
		}
		else {
			ExReleasePushLockShared(&m_lock);
		}
		m_mode = Mode::Shared; // reset default
		KeLeaveCriticalRegion();
	}

	// Release lock with explicit mode
	_IRQL_requires_max_(APC_LEVEL)
		_Releases_lock_(m_lock)
		VOID Unlock(Mode mode) noexcept {
		if (mode == Mode::Exclusive) {
			ExReleasePushLockExclusive(&m_lock);
		}
		else {
			ExReleasePushLockShared(&m_lock);
		}
		KeLeaveCriticalRegion();
	}

	// Non-copyable
	PushLock(const PushLock&) = delete;
	PushLock& operator=(const PushLock&) = delete;

	// RAII helper for exclusive lock
	class AutoExclusive {
	public:
		explicit AutoExclusive(PushLock& l) : lock(l) { lock.LockExclusive(); }
		~AutoExclusive() { lock.Unlock(PushLock::Mode::Exclusive); }
	private:
		PushLock& lock;
	};

	// RAII helper for shared lock
	class AutoShared {
	public:
		explicit AutoShared(PushLock& l) : lock(l) { lock.LockShared(); }
		~AutoShared() { lock.Unlock(PushLock::Mode::Shared); }
	private:
		PushLock& lock;
	};

private:
	// This mode value is only a per-thread hint to decide which release API to call.
	// Push locks are not recursive; do not acquire the same lock multiple times in the same thread.
	EX_PUSH_LOCK m_lock{};
	Mode m_mode{ Mode::Shared };
};
