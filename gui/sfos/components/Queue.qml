/*
 * Copyright (C) 2013-2019
 *      Jean-Luc Barriere <jlbarriere68@gmail.com>
 *      Adam Pigg <adam@piggz.co.uk>
 *      Andrew Hayzen <ahayzen@gmail.com>
 *      Daniel Holm <d.holmen@gmail.com>
 *      Victor Thompson <victor.thompson@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

import QtQuick 2.2
import Sailfish.Silica 1.0
import QtQml.Models 2.2
import "Delegates"
import "Flickables"
import "ListItemActions"
import "Dialog"

SilicaFlickable {
    id: queue
    property alias listview: queueList
    property alias header: queueList.header
    property alias headerItem: queueList.headerItem
    clip: true

    Component {
        id: dragDelegate

        MusicListItem {
            id: listItem
            color: "transparent"
            highlightedColor: styleMusic.view.highlightedColor
            highlighted: (player.currentIndex === index)

            onClicked: dialogSongInfo.open(model, [{art: imageSource}],
                                           "qrc:/sfos/pages/ArtistView.qml",
                                           {
                                               "artistSearch": "A:ARTIST/" + model.author,
                                               "artist": model.author,
                                               "covers": makeCoverSource(undefined, model.author, undefined),
                                               "pageTitle": qsTr("Artist")
                                           })

            imageSources: makeCoverSource(model.art, model.author, model.album)
            description: qsTr("Song")

            onImageError: model.art = "" // reset invalid url from model
            onActionPressed: indexQueueClicked(model.index)
            actionVisible: true
            actionIconSource: (player.isPlaying && player.currentIndex === index ? "image://theme/icon-m-pause" : "image://theme/icon-m-play")

            menu: ContextMenu {
                AddToFavorites {
                    enabled: parent.enabled && !model.isService
                    visible: enabled
                    description: listItem.description
                    art: model.art ? model.art : ""
                }
                AddToPlaylist {
                    enabled: parent.enabled && !model.isService
                    visible: enabled
                }
                Remove {
                    onClicked: {
                        listview.focusIndex = index > 0 ? index - 1 : 0;
                        removeTrackFromQueue(model);
                        listItem.color = "red";
                    }
                }
            }

            coverSize: units.gu(5)

            column: Column {
                Label {
                    id: trackTitle
                    color: styleMusic.view.primaryColor
                    font.pixelSize: units.fx("medium")
                    text: model.title
                }

                Label {
                    id: trackArtist
                    color: styleMusic.view.secondaryColor
                    font.pixelSize: units.fx("x-small")
                    text: model.author
                }

            }

        }

    }

    MusicListView {
        id: queueList
        objectName: "queueList"
        anchors.fill: parent

        footer: Item {
            height: mainView.height - (styleMusic.view.expandHeight + queueList.currentHeight) + units.gu(8)
        }

        model: DelegateModel {
            id: visualModel
            model: player.trackQueue.model
            delegate: dragDelegate
        }

        property int focusIndex: 0

        Connections {
            target: player.trackQueue.model
            onLoaded: {
                if (queueList.focusIndex > 0) {
                    queueList.positionViewAtIndex(queueList.focusIndex, ListView.Center);
                    queueList.focusIndex = 0;
                } else {
                    queueList.positionViewAtIndex(player.currentIndex > 0 ? player.currentIndex - 1 : 0, ListView.Beginning);
                }
            }
        }

        signal reorder(int from, int to)

        onReorder: {
            customdebug("Reorder queue item " + from + " to " + to);
            queueList.focusIndex = to;
            reorderTrackInQueue(from, to);
        }

    }
}
