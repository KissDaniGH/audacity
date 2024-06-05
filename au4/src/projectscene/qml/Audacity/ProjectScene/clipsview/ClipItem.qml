import QtQuick

import Muse.Ui
import Muse.UiComponents

import Audacity.ProjectScene

Rectangle {

    id: root

    property alias context: waveView.context
    property alias clipKey: waveView.clipKey
    property alias title: titleLabel.text
    property color clipColor: "#677CE4"
    property bool clipActive: false

    signal positionChanged(x : double)
    signal requestAboutActive()
    signal titleEdited(var newTitle)

    radius: 4
    color: "#ffffff"
    border.width: 1
    border.color: ui.theme.strokeColor
    clip: true

    Drag.active: dragArea.drag.active

    Rectangle {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        height: 20

        color: root.clipActive ? ui.blendColors(root.clipColor, "#ffffff", 0.7) : root.clipColor

        StyledTextLabel {
            id: titleLabel
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: menuBtn.left
            anchors.leftMargin: 4
            anchors.rightMargin: 2
            horizontalAlignment: Qt.AlignLeft
            color: root.clipActive ? "#000000" : "#ffffff"
        }

        MouseArea {
            id: dragArea
            anchors.fill: parent

            cursorShape: Qt.OpenHandCursor
            drag.target: root
            drag.axis: Drag.XAxis

            onReleased: {
                if (drag.active) {
                    root.positionChanged(root.x)
                }
            }

            onClicked: root.requestAboutActive()

            onDoubleClicked: {
                titleEdit.visible = true
            }
        }

        TextInputField {
            id: titleEdit
            anchors.fill: titleLabel
            currentText: titleLabel.text
            visible: false
            onTextEditingFinished: function (newTitle) {
                titleEdit.visible = false
                root.titleEdited(newTitle)
            }
        }

        MenuButton {
            id: menuBtn
            width: 16
            height: 16
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter

            menuModel: [
                {"id": 1, "title": "Item 1"},
                {"id": 2, "title": "Item 2"}
            ]

            onHandleMenuItem: function(itemId) {
                console.log("onHandleMenuItem: " + itemId)
            }
        }
    }

    WaveView {
        id: waveView
        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom

        clipColor: root.clipColor
        clipLeft: root.x
        clipActive: root.clipActive
    }
}
