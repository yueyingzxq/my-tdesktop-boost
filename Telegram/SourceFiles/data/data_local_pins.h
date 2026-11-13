/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#pragma once

#include "base/flat_map.h"
#include "base/flat_set.h"
#include "base/timer.h"

namespace Dialogs {
class Entry;
class Key;
class PinnedList;
} // namespace Dialogs

namespace Data {

class Folder;
class Forum;
class SavedMessages;
class Session;

inline constexpr auto kUnlimitedLocalPinsLimit = 10000;

[[nodiscard]] bool UnlimitedLocalPinsEnabled();
void SetUnlimitedLocalPinsEnabled(bool enabled);
[[nodiscard]] rpl::producer<bool> UnlimitedLocalPinsEnabledValue();

class LocalPinnedChats final {
public:
	explicit LocalPinnedChats(not_null<Session*> owner);
	~LocalPinnedChats();

	[[nodiscard]] static QString ScopeKey(Folder *folder);
	[[nodiscard]] static QString ScopeKey(FilterId filterId);
	[[nodiscard]] static QString ScopeKey(not_null<Forum*> forum);
	[[nodiscard]] static QString ScopeKey(not_null<SavedMessages*> saved);
	[[nodiscard]] static QString ScopeKeyForEntry(
		Dialogs::Key key,
		FilterId filterId);
	[[nodiscard]] static QString EntryId(Dialogs::Key key);

	void recordPinned(
		const QString &scopeKey,
		Dialogs::Key key,
		const std::vector<Dialogs::Key> &fullOrder);
	void removePin(
		const QString &scopeKey,
		Dialogs::Key key,
		const std::vector<Dialogs::Key> &fullOrder);
	void markCloudSynced(const QString &scopeKey, Dialogs::Key key);
	void markCloudSynced(Dialogs::Key key);
	void updateOrder(
		const QString &scopeKey,
		const std::vector<Dialogs::Key> &fullOrder);

	[[nodiscard]] bool isLocallyPinned(
		const QString &scopeKey,
		Dialogs::Key key) const;
	[[nodiscard]] bool isLocallyPinned(Dialogs::Key key) const;
	[[nodiscard]] bool hasLocallyPinned(const QString &scopeKey) const;

	[[nodiscard]] std::vector<Dialogs::Key> cloudOrder(
		const QString &scopeKey,
		const std::vector<Dialogs::Key> &fullOrder,
		int cloudLimit) const;

	void restore(Folder *folder);
	void restore(FilterId filterId);
	void restore(not_null<Forum*> forum);
	void restore(not_null<SavedMessages*> saved);

	void applyPreferredOrder(
		const QString &scopeKey,
		not_null<Dialogs::PinnedList*> list) const;

	void applyPreferredOrderForEntry(Dialogs::Key key) const;

private:
	struct LocalEntry {
		QString id;
		int peerVersion = 0;
		QByteArray peerSerialized;
	};
	struct ScopeState {
		std::vector<LocalEntry> local;
		std::vector<QString> order;
	};

	void ensureLoaded() const;
	void loadFromDisk() const;
	void saveDelayed();
	void saveNow();
	void writeToDisk() const;

	void setOrder(
		ScopeState &scope,
		const std::vector<Dialogs::Key> &fullOrder);
	[[nodiscard]] PeerData *resolvePeer(const LocalEntry &entry) const;
	[[nodiscard]] LocalEntry serializeEntry(
		const QString &id,
		Dialogs::Key key) const;

	const not_null<Session*> _owner;
	mutable bool _loaded = false;
	mutable base::flat_map<QString, ScopeState> _scopes;
	base::flat_set<QString> _pendingRequests;
	base::flat_set<QString> _requestedOnce;
	mutable bool _dirty = false;
	base::Timer _saveTimer;

};

} // namespace Data
