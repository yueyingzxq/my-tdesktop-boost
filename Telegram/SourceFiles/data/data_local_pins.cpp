/*
This file is part of Telegram Desktop,
the official desktop application for the Telegram messaging service.

For license and copyright information please follow this link:
https://github.com/telegramdesktop/tdesktop/blob/master/LEGAL
*/
#include "data/data_local_pins.h"

#include "base/options.h"
#include "core/application.h"
#include "core/version.h"
#include "data/data_chat_filters.h"
#include "data/data_folder.h"
#include "data/data_forum.h"
#include "data/data_forum_topic.h"
#include "data/data_histories.h"
#include "data/data_peer.h"
#include "data/data_saved_messages.h"
#include "data/data_saved_sublist.h"
#include "data/data_session.h"
#include "dialogs/dialogs_entry.h"
#include "dialogs/dialogs_key.h"
#include "dialogs/dialogs_main_list.h"
#include "dialogs/dialogs_pinned_list.h"
#include "history/history.h"
#include "main/main_session.h"
#include "settings.h"
#include "storage/serialize_common.h"
#include "storage/serialize_peer.h"

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonArray>
#include <QtCore/QJsonDocument>
#include <QtCore/QJsonObject>
#include <QtCore/QSaveFile>

namespace Data {
namespace {

constexpr auto kSaveDelay = crl::time(200);

base::options::toggle OptionUnlimitedLocalPins({
	.id = "unlimited-local-pins",
	.name = "Unlimited Local Pinned Chats",
	.description = "解除本地置顶会话数量限制，允许在所有聊天、归档、分组、收藏与话题中无限置顶（超出云端上限部分自动保存在本地，重启保留）。",
	.defaultValue = true,
});

[[nodiscard]] QString StoragePath() {
	return cWorkingDir() + u"tdata/boost_local_pins.json"_q;
}

[[nodiscard]] PeerId ParsePeerEntryId(const QString &id) {
	if (!id.startsWith(u"p:"_q)) {
		return PeerId();
	}
	auto ok = false;
	const auto raw = id.mid(2).toULongLong(&ok);
	return ok ? PeerId(uint64(raw)) : PeerId();
}

[[nodiscard]] MsgId ParseTopicEntryId(const QString &id) {
	if (!id.startsWith(u"t:"_q)) {
		return MsgId();
	}
	auto ok = false;
	const auto raw = id.mid(2).toLongLong(&ok);
	return (ok && raw > 0) ? MsgId(raw) : MsgId();
}

[[nodiscard]] FolderId ParseFolderEntryId(const QString &id) {
	if (!id.startsWith(u"f:"_q)) {
		return FolderId();
	}
	auto ok = false;
	const auto raw = id.mid(2).toInt(&ok);
	return (ok && raw > 0) ? FolderId(raw) : FolderId();
}

} // namespace

bool UnlimitedLocalPinsEnabled() {
	return OptionUnlimitedLocalPins.value();
}

void SetUnlimitedLocalPinsEnabled(bool enabled) {
	OptionUnlimitedLocalPins.set(enabled);
}

rpl::producer<bool> UnlimitedLocalPinsEnabledValue() {
	return rpl::single(
		OptionUnlimitedLocalPins.value()
	) | rpl::then(OptionUnlimitedLocalPins.changes());
}

LocalPinnedChats::LocalPinnedChats(not_null<Session*> owner)
: _owner(owner)
, _saveTimer([=] { writeToDisk(); }) {
}

LocalPinnedChats::~LocalPinnedChats() {
	if (_dirty || _saveTimer.isActive()) {
		_saveTimer.cancel();
		writeToDisk();
	}
}

QString LocalPinnedChats::ScopeKey(Folder *folder) {
	if (!folder) {
		return u"main"_q;
	} else if (folder->id() == Folder::kId) {
		return u"archive"_q;
	}
	return u"folder:%1"_q.arg(folder->id());
}

QString LocalPinnedChats::ScopeKey(FilterId filterId) {
	return u"filter:%1"_q.arg(filterId);
}

QString LocalPinnedChats::ScopeKey(not_null<Forum*> forum) {
	return u"forum:%1"_q.arg(forum->peer()->id.value);
}

QString LocalPinnedChats::ScopeKey(not_null<SavedMessages*> saved) {
	return saved->parentChat() ? QString() : u"saved"_q;
}

QString LocalPinnedChats::ScopeKeyForEntry(
		Dialogs::Key key,
		FilterId filterId) {
	if (filterId) {
		return ScopeKey(filterId);
	} else if (const auto topic = key.topic()) {
		return ScopeKey(topic->forum());
	} else if (const auto sublist = key.sublist()) {
		return sublist->parentChat() ? QString() : u"saved"_q;
	} else if (key.entry()->folderKnown()) {
		return ScopeKey(key.entry()->folder());
	}
	return ScopeKey(static_cast<Folder*>(nullptr));
}

QString LocalPinnedChats::EntryId(Dialogs::Key key) {
	if (const auto history = key.history()) {
		return u"p:%1"_q.arg(history->peer->id.value);
	} else if (const auto sublist = key.sublist()) {
		return u"p:%1"_q.arg(sublist->sublistPeer()->id.value);
	} else if (const auto topic = key.topic()) {
		return u"t:%1"_q.arg(topic->rootId().bare);
	} else if (const auto folder = key.folder()) {
		return u"f:%1"_q.arg(folder->id());
	}
	return QString();
}

void LocalPinnedChats::ensureLoaded() const {
	if (_loaded) {
		return;
	}
	_loaded = true;
	loadFromDisk();
}

void LocalPinnedChats::loadFromDisk() const {
	_scopes.clear();
	auto file = QFile(StoragePath());
	if (!file.open(QIODevice::ReadOnly)) {
		return;
	}
	auto parseError = QJsonParseError();
	const auto doc = QJsonDocument::fromJson(file.readAll(), &parseError);
	if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
		return;
	}
	const auto accountKey = QString::number(_owner->session().userId().bare);
	const auto root = doc.object();
	const auto accountObj = root.value(accountKey).toObject();
	const auto scopesObj = accountObj.value(u"scopes"_q).toObject();
	for (auto it = scopesObj.begin(); it != scopesObj.end(); ++it) {
		const auto scopeObj = it.value().toObject();
		auto state = ScopeState();
		const auto localArray = scopeObj.value(u"local"_q).toArray();
		state.local.reserve(localArray.size());
		for (const auto &itemVal : localArray) {
			const auto itemObj = itemVal.toObject();
			const auto id = itemObj.value(u"id"_q).toString();
			if (id.isEmpty()) {
				continue;
			}
			auto entry = LocalEntry{
				.id = id,
				.peerVersion = itemObj.value(u"v"_q).toInt(),
			};
			const auto peerB64 = itemObj.value(u"peer"_q).toString();
			if (!peerB64.isEmpty()) {
				entry.peerSerialized = QByteArray::fromBase64(peerB64.toUtf8());
			}
			state.local.push_back(std::move(entry));
		}
		const auto orderArray = scopeObj.value(u"order"_q).toArray();
		state.order.reserve(orderArray.size());
		for (const auto &idVal : orderArray) {
			const auto id = idVal.toString();
			if (!id.isEmpty()) {
				state.order.push_back(id);
			}
		}
		if (!state.local.empty() || !state.order.empty()) {
			_scopes.emplace(it.key(), std::move(state));
		}
	}
}

void LocalPinnedChats::saveDelayed() {
	_dirty = true;
	if (!_saveTimer.isActive()) {
		_saveTimer.callOnce(kSaveDelay);
	}
}

void LocalPinnedChats::saveNow() {
	_dirty = true;
	_saveTimer.cancel();
	writeToDisk();
}

void LocalPinnedChats::writeToDisk() const {
	if (!_dirty) {
		return;
	}
	const auto path = StoragePath();
	QDir().mkpath(QFileInfo(path).absolutePath());

	auto root = QJsonObject();
	{
		auto existing = QFile(path);
		if (existing.open(QIODevice::ReadOnly)) {
			const auto doc = QJsonDocument::fromJson(existing.readAll());
			if (doc.isObject()) {
				root = doc.object();
			}
		}
	}

	const auto accountKey = QString::number(_owner->session().userId().bare);
	auto scopesObj = QJsonObject();
	for (const auto &[scopeKey, state] : _scopes) {
		if (state.local.empty() && state.order.empty()) {
			continue;
		}
		auto scopeObj = QJsonObject();
		auto localArray = QJsonArray();
		for (const auto &entry : state.local) {
			auto itemObj = QJsonObject();
			itemObj.insert(u"id"_q, entry.id);
			if (!entry.peerSerialized.isEmpty()) {
				itemObj.insert(u"v"_q, entry.peerVersion);
				itemObj.insert(
					u"peer"_q,
					QString::fromUtf8(entry.peerSerialized.toBase64()));
			}
			localArray.push_back(itemObj);
		}
		auto orderArray = QJsonArray();
		for (const auto &id : state.order) {
			orderArray.push_back(id);
		}
		scopeObj.insert(u"local"_q, localArray);
		scopeObj.insert(u"order"_q, orderArray);
		scopesObj.insert(scopeKey, scopeObj);
	}

	if (scopesObj.isEmpty()) {
		root.remove(accountKey);
	} else {
		auto accountObj = QJsonObject();
		accountObj.insert(u"scopes"_q, scopesObj);
		root.insert(accountKey, accountObj);
	}

	auto saveFile = QSaveFile(path);
	if (saveFile.open(QIODevice::WriteOnly)) {
		saveFile.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
		saveFile.commit();
	}
	_dirty = false;
}

LocalPinnedChats::LocalEntry LocalPinnedChats::serializeEntry(
		const QString &id,
		Dialogs::Key key) const {
	auto result = LocalEntry{ .id = id };
	PeerData *peer = nullptr;
	if (const auto history = key.history()) {
		peer = history->peer;
	} else if (const auto sublist = key.sublist()) {
		peer = sublist->sublistPeer();
	}
	if (peer) {
		auto writer = Serialize::ByteArrayWriter(int(Serialize::peerSize(peer)));
		Serialize::writePeer(writer, peer);
		result.peerVersion = AppVersion;
		result.peerSerialized = std::move(writer).result();
	}
	return result;
}

PeerData *LocalPinnedChats::resolvePeer(const LocalEntry &entry) const {
	const auto peerId = ParsePeerEntryId(entry.id);
	if (!peerId) {
		return nullptr;
	}
	if (const auto existing = _owner->peerLoaded(peerId)) {
		return existing;
	}
	if (entry.peerSerialized.isEmpty() || entry.peerVersion <= 0) {
		return nullptr;
	}
	auto reader = Serialize::ByteArrayReader(entry.peerSerialized);
	return Serialize::readPeer(&_owner->session(), entry.peerVersion, reader);
}

void LocalPinnedChats::setOrder(
		ScopeState &scope,
		const std::vector<Dialogs::Key> &fullOrder) {
	scope.order.clear();
	scope.order.reserve(fullOrder.size());
	for (const auto &item : fullOrder) {
		const auto id = EntryId(item);
		if (!id.isEmpty()) {
			scope.order.push_back(id);
		}
	}
}

void LocalPinnedChats::recordPinned(
		const QString &scopeKey,
		Dialogs::Key key,
		const std::vector<Dialogs::Key> &fullOrder) {
	if (scopeKey.isEmpty()) {
		return;
	}
	ensureLoaded();
	const auto id = EntryId(key);
	if (id.isEmpty()) {
		return;
	}
	auto &scope = _scopes[scopeKey];
	const auto existing = ranges::find(scope.local, id, &LocalEntry::id);
	if (existing == end(scope.local)) {
		scope.local.push_back(serializeEntry(id, key));
	} else if (existing->peerSerialized.isEmpty()) {
		*existing = serializeEntry(id, key);
	}
	setOrder(scope, fullOrder);
	saveNow();
}

void LocalPinnedChats::removePin(
		const QString &scopeKey,
		Dialogs::Key key,
		const std::vector<Dialogs::Key> &fullOrder) {
	if (scopeKey.isEmpty()) {
		return;
	}
	ensureLoaded();
	const auto id = EntryId(key);
	if (id.isEmpty()) {
		return;
	}
	const auto it = _scopes.find(scopeKey);
	if (it == end(_scopes)) {
		return;
	}
	auto &scope = it->second;
	scope.local.erase(
		ranges::remove(scope.local, id, &LocalEntry::id),
		end(scope.local));
	setOrder(scope, fullOrder);
	if (scope.local.empty() && scope.order.empty()) {
		_scopes.erase(it);
	}
	saveNow();
}

void LocalPinnedChats::markCloudSynced(
		const QString &scopeKey,
		Dialogs::Key key) {
	if (scopeKey.isEmpty()) {
		return;
	}
	ensureLoaded();
	const auto it = _scopes.find(scopeKey);
	if (it == end(_scopes)) {
		return;
	}
	const auto id = EntryId(key);
	auto &scope = it->second;
	const auto prevSize = scope.local.size();
	scope.local.erase(
		ranges::remove(scope.local, id, &LocalEntry::id),
		end(scope.local));
	if (scope.local.size() != prevSize) {
		if (scope.local.empty() && scope.order.empty()) {
			_scopes.erase(it);
		}
		saveDelayed();
	}
}

void LocalPinnedChats::markCloudSynced(Dialogs::Key key) {
	markCloudSynced(ScopeKeyForEntry(key, FilterId()), key);
}

void LocalPinnedChats::updateOrder(
		const QString &scopeKey,
		const std::vector<Dialogs::Key> &fullOrder) {
	if (scopeKey.isEmpty()) {
		return;
	}
	ensureLoaded();
	auto &scope = _scopes[scopeKey];
	setOrder(scope, fullOrder);
	if (scope.local.empty() && scope.order.empty()) {
		_scopes.erase(scopeKey);
	}
	saveNow();
}

bool LocalPinnedChats::isLocallyPinned(
		const QString &scopeKey,
		Dialogs::Key key) const {
	if (scopeKey.isEmpty()) {
		return false;
	}
	ensureLoaded();
	const auto it = _scopes.find(scopeKey);
	if (it == end(_scopes)) {
		return false;
	}
	const auto id = EntryId(key);
	return !id.isEmpty()
		&& ranges::contains(it->second.local, id, &LocalEntry::id);
}

bool LocalPinnedChats::isLocallyPinned(Dialogs::Key key) const {
	return isLocallyPinned(ScopeKeyForEntry(key, FilterId()), key);
}

bool LocalPinnedChats::hasLocallyPinned(const QString &scopeKey) const {
	if (scopeKey.isEmpty()) {
		return false;
	}
	ensureLoaded();
	const auto it = _scopes.find(scopeKey);
	return (it != end(_scopes)) && !it->second.local.empty();
}

std::vector<Dialogs::Key> LocalPinnedChats::cloudOrder(
		const QString &scopeKey,
		const std::vector<Dialogs::Key> &fullOrder,
		int cloudLimit) const {
	ensureLoaded();
	auto result = std::vector<Dialogs::Key>();
	result.reserve(fullOrder.size());
	for (const auto &key : fullOrder) {
		if (!isLocallyPinned(scopeKey, key)) {
			result.push_back(key);
			if (cloudLimit > 0 && int(result.size()) >= cloudLimit) {
				break;
			}
		}
	}
	return result;
}

void LocalPinnedChats::applyPreferredOrder(
		const QString &scopeKey,
		not_null<Dialogs::PinnedList*> list) const {
	if (scopeKey.isEmpty()) {
		return;
	}
	ensureLoaded();
	const auto it = _scopes.find(scopeKey);
	if (it == end(_scopes) || it->second.order.empty()) {
		return;
	}
	const auto &order = it->second.order;
	auto rankById = base::flat_map<QString, int>();
	rankById.reserve(order.size());
	for (auto i = 0; i < int(order.size()); ++i) {
		rankById.emplace(order[i], i);
	}
	list->applyOrderByRank([&](Dialogs::Key key) {
		const auto id = EntryId(key);
		const auto found = rankById.find(id);
		return (found != end(rankById)) ? found->second : -1;
	});
}

void LocalPinnedChats::applyPreferredOrderForEntry(Dialogs::Key key) const {
	const auto scopeKey = ScopeKeyForEntry(key, FilterId());
	if (scopeKey.isEmpty()) {
		return;
	}
	applyPreferredOrder(scopeKey, _owner->chatsListFor(key.entry())->pinned());
}

void LocalPinnedChats::restore(Folder *folder) {
	if (!UnlimitedLocalPinsEnabled()) {
		return;
	}
	ensureLoaded();
	const auto scopeKey = ScopeKey(folder);
	const auto it = _scopes.find(scopeKey);
	if (it == end(_scopes)) {
		return;
	}
	const auto list = _owner->chatsList(folder)->pinned();
	const auto entries = it->second.local;
	auto staleIds = std::vector<QString>();
	for (const auto &entry : entries) {
		if (const auto folderId = ParseFolderEntryId(entry.id)) {
			if (!folder) {
				list->addPinned(_owner->folder(folderId));
			}
			continue;
		}
		const auto peer = resolvePeer(entry);
		if (!peer) {
			continue;
		}
		const auto history = _owner->history(peer);
		if (history->folderKnown()) {
			if (history->folder() == folder) {
				list->addPinned(history);
			} else {
				staleIds.push_back(entry.id);
			}
		} else {
			const auto reqKey = scopeKey + u":"_q + entry.id;
			if (!_pendingRequests.contains(reqKey)
				&& !_requestedOnce.contains(reqKey)) {
				_pendingRequests.emplace(reqKey);
				_requestedOnce.emplace(reqKey);
				_owner->histories().requestDialogEntry(
					history,
					crl::guard(&_owner->session(), [=] {
						_pendingRequests.remove(reqKey);
						if (!UnlimitedLocalPinsEnabled()) {
							return;
						}
						if (history->folderKnown() && history->folder() == folder) {
							restore(folder);
							_owner->chatsListChanged(folder);
							_owner->notifyPinnedDialogsOrderUpdated();
						}
					}));
			}
		}
	}
	if (!staleIds.empty()) {
		if (const auto scopeIt = _scopes.find(scopeKey); scopeIt != end(_scopes)) {
			for (const auto &stale : staleIds) {
				scopeIt->second.local.erase(
					ranges::remove(scopeIt->second.local, stale, &LocalEntry::id),
					end(scopeIt->second.local));
				scopeIt->second.order.erase(
					ranges::remove(scopeIt->second.order, stale),
					end(scopeIt->second.order));
			}
			saveDelayed();
		}
	}
	applyPreferredOrder(scopeKey, list);
}

void LocalPinnedChats::restore(FilterId filterId) {
	if (!filterId || !UnlimitedLocalPinsEnabled()) {
		return;
	}
	ensureLoaded();
	const auto scopeKey = ScopeKey(filterId);
	const auto it = _scopes.find(scopeKey);
	if (it == end(_scopes)) {
		return;
	}
	if (!ranges::contains(
			_owner->chatsFilters().list(),
			filterId,
			&ChatFilter::id)) {
		return;
	}
	const auto filterList = _owner->chatsFilters().chatsList(filterId);
	const auto pinnedList = filterList->pinned();
	const auto entries = it->second.local;
	for (const auto &entry : entries) {
		const auto peer = resolvePeer(entry);
		if (!peer) {
			continue;
		}
		const auto history = _owner->history(peer);
		if (!history->inChatList(filterId)) {
			history->addToChatList(filterId, filterList);
		}
		if (history->folderKnown()) {
			pinnedList->addPinned(history);
		} else {
			pinnedList->setPinned(history, true);
			const auto reqKey = scopeKey + u":"_q + entry.id;
			if (!_pendingRequests.contains(reqKey)
				&& !_requestedOnce.contains(reqKey)) {
				_pendingRequests.emplace(reqKey);
				_requestedOnce.emplace(reqKey);
				_owner->histories().requestDialogEntry(
					history,
					crl::guard(&_owner->session(), [=] {
						_pendingRequests.remove(reqKey);
						if (!UnlimitedLocalPinsEnabled()) {
							return;
						}
						restore(filterId);
						_owner->notifyPinnedDialogsOrderUpdated();
					}));
			}
		}
	}
	applyPreferredOrder(scopeKey, pinnedList);
}

void LocalPinnedChats::restore(not_null<Forum*> forum) {
	if (!UnlimitedLocalPinsEnabled()) {
		return;
	}
	ensureLoaded();
	const auto scopeKey = ScopeKey(forum);
	const auto it = _scopes.find(scopeKey);
	if (it == end(_scopes)) {
		return;
	}
	const auto pinnedList = forum->topicsList()->pinned();
	for (const auto &entry : it->second.local) {
		const auto rootId = ParseTopicEntryId(entry.id);
		if (!rootId) {
			continue;
		}
		if (const auto topic = forum->topicFor(rootId)) {
			if (!forum->creating(rootId)) {
				pinnedList->addPinned(topic);
			}
		} else {
			const auto reqKey = scopeKey + u":"_q + entry.id;
			if (!_pendingRequests.contains(reqKey)
				&& !_requestedOnce.contains(reqKey)) {
				_pendingRequests.emplace(reqKey);
				_requestedOnce.emplace(reqKey);
				forum->requestTopic(
					rootId,
					crl::guard(&_owner->session(), [
						this,
						peer = forum->peer(),
						reqKey
					] {
						_pendingRequests.remove(reqKey);
						if (!UnlimitedLocalPinsEnabled()) {
							return;
						}
						if (const auto f = peer->forum()) {
							restore(f);
							_owner->notifyPinnedDialogsOrderUpdated();
						}
					}));
			}
		}
	}
	applyPreferredOrder(scopeKey, pinnedList);
}

void LocalPinnedChats::restore(not_null<SavedMessages*> saved) {
	if (!UnlimitedLocalPinsEnabled() || saved->parentChat()) {
		return;
	}
	ensureLoaded();
	const auto scopeKey = ScopeKey(saved);
	const auto it = _scopes.find(scopeKey);
	if (it == end(_scopes)) {
		return;
	}
	const auto pinnedList = saved->chatsList()->pinned();
	for (const auto &entry : it->second.local) {
		const auto peer = resolvePeer(entry);
		if (!peer) {
			continue;
		}
		const auto sublist = saved->sublist(peer);
		if (sublist->shouldBeInChatList()) {
			pinnedList->addPinned(sublist);
		}
	}
	applyPreferredOrder(scopeKey, pinnedList);
}

} // namespace Data
