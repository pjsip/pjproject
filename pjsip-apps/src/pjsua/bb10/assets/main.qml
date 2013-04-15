// Default empty project template
import bb.cascades 1.0

// creates one page with a label
Page {
    Container {
        layout: StackLayout {
        }
        background: Color.Black
        Label {
            text: qsTr("")
            horizontalAlignment: HorizontalAlignment.Center
        }
        ImageView {
            imageSource: "asset:///images/teluu-logo.png"
            topMargin: 200
            preferredWidth: 500
            preferredHeight: 500
            verticalAlignment: VerticalAlignment.Center
            horizontalAlignment: HorizontalAlignment.Center
        }
        Label {
            text: qsTr("pjsua BB10")
            textStyle.base: SystemDefaults.TextStyles.BigText
            horizontalAlignment: HorizontalAlignment.Center
            textStyle {
                color: Color.White
            }
        }
        Label {
            objectName: "telnetMsg"
            text: qsTr("Starting..")
            topMargin: 200
            horizontalAlignment: HorizontalAlignment.Center
            textStyle {
                color: Color.White
            }
        }
    }
}

