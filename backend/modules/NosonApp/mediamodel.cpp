/*
 *      Copyright (C) 2015-2016 Jean-Luc Barriere
 *
 *  This library is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published
 *  by the Free Software Foundation; either version 3, or (at your option)
 *  any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this library; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301 USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "mediamodel.h"
#include "sonos.h"
#include "tools.h"
#include "../../lib/noson/noson/src/private/cppdef.h"

#include <cstdio> // for strncpy
#include <cctype> // for isdigit

#define LOAD_BULKSIZE 100

MediaItem::MediaItem(const SONOS::SMAPIItem& data)
: m_ptr(data.uriMetadata)
, m_valid(false)
, m_canQueue(false)
, m_canPlay(false)
{
  m_id = QString::fromUtf8(data.item->GetObjectID().c_str());
  m_parent = QString::fromUtf8(data.item->GetParentID().c_str());
  m_title = QString::fromUtf8(data.item->GetValue("dc:title").c_str());
  m_normalized = normalizedString(m_title);
  m_description = QString::fromUtf8(data.item->GetValue("r:description").c_str());
  m_art = QString::fromUtf8(data.item->GetValue("upnp:albumArtURI").c_str());

  if (data.uriMetadata)
  {
    m_canPlay = true;
    m_canQueue = SONOS::System::CanQueueItem(data.uriMetadata);
  }
  switch (data.item->subType())
  {
  case SONOS::DigitalItem::SubType_album:
    m_type = MediaType::album;
    m_artist = QString::fromUtf8(data.item->GetValue("dc:creator").c_str());
    m_album = QString::fromUtf8(data.item->GetValue("dc:title").c_str());
    break;
  case SONOS::DigitalItem::SubType_person:
    m_type = MediaType::person;
    m_artist = QString::fromUtf8(data.item->GetValue("dc:title").c_str());
    break;
  case SONOS::DigitalItem::SubType_genre:
    m_type = MediaType::genre;
    break;
  case SONOS::DigitalItem::SubType_playlistContainer:
    m_type = MediaType::playlist;
    break;
  case SONOS::DigitalItem::SubType_audioItem:
    m_type = MediaType::audioItem;
    m_artist = QString::fromUtf8(data.item->GetValue("dc:creator").c_str());
    m_album = QString::fromUtf8(data.item->GetValue("upnp:album").c_str());
    break;
  case SONOS::DigitalItem::SubType_storageFolder:
    m_type = MediaType::folder;
    break;
  default:
    m_type = MediaType::unknown;
  }
  m_displayType = data.displayType;
  m_valid = true;
}

QVariant MediaItem::payload() const
{
  QVariant var;
  var.setValue<SONOS::DigitalItemPtr>(m_ptr);
  return var;
}

MediaModel::MediaModel(QObject* parent)
: QAbstractListModel(parent)
, m_smapi(0)
, m_nextIndex(0)
, m_totalCount(0)
{
}

MediaModel::~MediaModel()
{
  clear();
  SAFE_DELETE(m_smapi);
}

void MediaModel::addItem(MediaItem* item)
{
  {
    SONOS::LockGuard lock(m_lock);
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    m_items << item;
    endInsertRows();
  }
  emit countChanged();
}

int MediaModel::rowCount(const QModelIndex& parent) const
{
  Q_UNUSED(parent);
  SONOS::LockGuard lock(m_lock);
  return m_items.count();
}

QVariant MediaModel::data(const QModelIndex& index, int role) const
{
  SONOS::LockGuard lock(m_lock);
  if (index.row() < 0 || index.row() >= m_items.count())
      return QVariant();

  const MediaItem* item = m_items[index.row()];
  switch (role)
  {
  case PayloadRole:
    return item->payload();
  case IdRole:
    return item->id();
  case TitleRole:
    return item->title();
  case DescriptionRole:
    return item->description();
  case ArtRole:
    return item->art();
  case NormalizedRole:
    return item->normalized();
  case TypeRole:
    return item->type();
  case CanQueueRole:
    return item->canQueue();
  case CanPlayRole:
    return item->canPlay();
  case ArtistRole:
    return item->artist();
  case AlbumRole:
    return item->album();
  case ParentRole:
    return item->parent();
  case DisplayTypeRole:
    return item->displayType();
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> MediaModel::roleNames() const
{
  QHash<int, QByteArray> roles;
  roles[PayloadRole] = "payload";
  roles[IdRole] = "id";
  roles[TitleRole] = "title";
  roles[DescriptionRole] = "description";
  roles[ArtRole] = "art";
  roles[NormalizedRole] = "normalized";
  roles[TypeRole] = "type";
  roles[CanQueueRole] = "canQueue";
  roles[CanPlayRole] = "canPlay";
  roles[ArtistRole] = "artist";
  roles[AlbumRole] = "album";
  roles[ParentRole] = "parent";
  roles[DisplayTypeRole] = "displayType";
  return roles;
}

QVariantMap MediaModel::get(int row)
{
  SONOS::LockGuard lock(m_lock);
  if (row < 0 || row >= m_items.count())
    return QVariantMap();
  const MediaItem* item = m_items[row];
  QVariantMap model;
  QHash<int, QByteArray> roles = roleNames();
  model[roles[PayloadRole]] = item->payload();
  model[roles[IdRole]] = item->id();
  model[roles[TitleRole]] = item->title();
  model[roles[DescriptionRole]] = item->description();
  model[roles[ArtRole]] = item->art();
  model[roles[NormalizedRole]] = item->normalized();
  model[roles[TypeRole]] = item->type();
  model[roles[CanQueueRole]] = item->canQueue();
  model[roles[CanPlayRole]] = item->canPlay();
  model[roles[ArtistRole]] = item->artist();
  model[roles[AlbumRole]] = item->album();
  model[roles[ParentRole]] = item->parent();
  model[roles[DisplayTypeRole]] = item->displayType();
  return model;
}

bool MediaModel::init(QObject* sonos, const QVariant& service, bool fill)
{
  Sonos* _sonos = reinterpret_cast<Sonos*> (sonos);
  if (!_sonos)
    return false;
  SONOS::PlayerPtr player = _sonos->getPlayer();
  if (!player)
    return false;
  SAFE_DELETE(m_smapi);
  m_smapi = new SONOS::SMAPI(player);
  if (!m_smapi || !m_smapi->Init(service.value<SONOS::SMServicePtr>()))
    return false;
  m_path.clear();
  return ListModel::init(sonos, "", fill);
}

void MediaModel::clear()
{
  {
    SONOS::LockGuard lock(m_lock);
    beginRemoveRows(QModelIndex(), 0, m_items.count());
    qDeleteAll(m_items);
    m_items.clear();
    endRemoveRows();
  }
  emit countChanged();
}

bool MediaModel::load()
{
  setUpdateSignaled(false);
  SONOS::LockGuard lock(m_lock);
  if (!m_smapi)
    return false;

  clear();
  m_nextIndex = m_totalCount = 0;
  SONOS::SMAPIMetadata meta;
  if (!m_smapi->GetMetadata(pathId().toUtf8().constData(), m_nextIndex, LOAD_BULKSIZE, false, meta))
  {
    emit totalCountChanged();
    return false;
  }
  m_totalCount = meta.TotalCount();
  m_nextIndex = meta.ItemCount();
  emit totalCountChanged();

  SONOS::SMAPIItemList list = meta.GetItems();
  for (SONOS::SMAPIItemList::const_iterator it = list.begin(); it != list.end(); ++it)
  {
    MediaItem* item = new MediaItem(*it);
    if (item->isValid())
      addItem(item);
    else
      delete item;
  }
  return m_loaded = true;
}

bool MediaModel::loadMore()
{
  SONOS::LockGuard lock(m_lock);
  if (!m_smapi)
    return false;
  // At end return false
  if (m_nextIndex >= m_totalCount)
    return false;

  SONOS::SMAPIMetadata meta;
  if (!m_smapi->GetMetadata(pathId().toUtf8().constData(), m_nextIndex, LOAD_BULKSIZE, false, meta))
    return false;
  if (m_totalCount != meta.TotalCount())
  {
    m_totalCount = meta.TotalCount();
    emit totalCountChanged();
  }
  m_nextIndex += meta.ItemCount();

  SONOS::SMAPIItemList list= meta.GetItems();
  for (SONOS::SMAPIItemList::const_iterator it = list.begin(); it != list.end(); ++it)
  {
    MediaItem* item = new MediaItem(*it);
    if (item->isValid())
      addItem(item);
    else
      delete item;
  }
  return true;
}

bool MediaModel::loadChild(const QString& id, const QString& title, int displayType)
{
  if (id.isEmpty())
    return false;
  SONOS::LockGuard lock(m_lock);
  m_path.push(Path(id, title, displayType));
  emit isRootChanged();
  emit displayTypeChanged();
  return load();
}

bool MediaModel::loadParent()
{
  SONOS::LockGuard lock(m_lock);
  if (m_path.empty())
    return false;
  m_path.pop();
  emit isRootChanged();
  emit displayTypeChanged();
  return load();
}

QString MediaModel::path() const
{
  SONOS::LockGuard lock(m_lock);
  if (m_path.empty())
    return QString("root");
  else
    return m_path.top().title;
}

QString MediaModel::pathId() const
{
  SONOS::LockGuard lock(m_lock);
  if (m_path.empty())
    return QString("root");
  else
    return m_path.top().id;
}

int MediaModel::displayType() const
{
  SONOS::LockGuard lock(m_lock);
  if (m_path.empty())
    return 0; // Grid
  else
    return m_path.top().displayType;
}

bool MediaModel::asyncLoad()
{
  if (m_provider)
    m_provider->runModelLoader(this);
}

void MediaModel::handleDataUpdate()
{
  if (!updateSignaled())
  {
    setUpdateSignaled(true);
    dataUpdated();
  }
}