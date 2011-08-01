/* $Id$ */
/*
 * Copyright (C) 2011-2011 Teluu Inc. (http://www.teluu.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
#ifndef VIDGUI_H_
#define VIDGUI_H_

#include <QApplication>
#include <QFont>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QObject>
#include <QPushButton>
#include <QStatusBar>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QWidget>

#include <pjsua.h>

class VidWin;

class MainWin : public QWidget
{
    Q_OBJECT

public:
    MainWin(QWidget *parent = 0);
    virtual ~MainWin();

    static MainWin *instance();

    bool initStack();
    void showError(const char *title, pj_status_t status);
    void showStatus(const char *);

public:
    void on_reg_state(pjsua_acc_id acc_id);
    void on_call_state(pjsua_call_id call_id, pjsip_event *e);
    void on_incoming_call(pjsua_acc_id acc_id, pjsua_call_id call_id, pjsip_rx_data *rdata);
    void on_call_media_state(pjsua_call_id call_id);

public slots:
    void preview();
    void call();
    void hangup();
    void quit();

private:
    static MainWin *theInstance_;
    pjsua_acc_id accountId_;
    pjsua_call_id currentCall_;
    bool preview_on;

    void onNewCall(pjsua_call_id cid, bool incoming);
    void onCallReleased();

private:
    QPushButton *callButton_,
		*hangupButton_,
		*quitButton_,
		*previewButton_;
    QLineEdit *url_;
    VidWin *video_;
    VidWin *video_prev_;
    //QStatusBar *statusBar_;
    QLabel *statusBar_;
    QLabel *localUri_;

    QVBoxLayout *vbox_left;

    void initLayout();
    void init_video_window();
};



#endif /* VIDGUI_H_ */
