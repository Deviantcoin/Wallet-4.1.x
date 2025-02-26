// Copyright (c) 2019-2020 The PIVX developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef TOPBAR_H
#define TOPBAR_H

#include "amount.h"
#include "qt/dev/lockunlock.h"
#include "qt/dev/pwidget.h"
#include <QProgressBar>
#include <QTimer>
#include <QWidget>

class DEVGUI;
class WalletModel;
class ClientModel;

namespace Ui
{
class TopBar;
}

class TopBar : public PWidget
{
    Q_OBJECT

public:
    explicit TopBar(DEVGUI* _mainWindow, QWidget* parent = nullptr);
    ~TopBar();

    void showTop();
    void showBottom();

    void loadWalletModel() override;
    void loadClientModel() override;

    void openPassPhraseDialog(AskPassphraseDialog::Mode mode, AskPassphraseDialog::Context ctx);
    void encryptWallet();
    void showUpgradeDialog();

    void run(int type) override;
    void onError(QString error, int type) override;
    void unlockWallet();

public Q_SLOTS:
    void updateBalances(const CAmount& balance, const CAmount& unconfirmedBalance, const CAmount& immatureBalance, const CAmount& zerocoinBalance, const CAmount& unconfirmedZerocoinBalance, const CAmount& immatureZerocoinBalance, const CAmount& watchOnlyBalance, const CAmount& watchUnconfBalance, const CAmount& watchImmatureBalance, const CAmount& delegatedBalance, const CAmount& coldStakedBalance);
    void updateDisplayUnit();

    void setNumConnections(int count);
    void setNumBlocks(int count);
    void setStakingStatusActive(bool fActive);
    void updateStakingStatus();
    void updateHDState(const bool& upgraded, const QString& upgradeError);

Q_SIGNALS:
    void themeChanged(bool isLight);
    void walletSynced(bool isSync);
    void onShowHideColdStakingChanged(bool show);

protected:
    void resizeEvent(QResizeEvent* event) override;
private Q_SLOTS:
    void onBtnReceiveClicked();
    void onThemeClicked();
    void onBtnLockClicked();
    void lockDropdownMouseLeave();
    void lockDropdownClicked(const StateClicked&);
    void refreshStatus();
    void openLockUnlock();
    void onColdStakingClicked();
    void refreshProgressBarSize();
    void expandSync();

private:
    Ui::TopBar* ui;
    LockUnlock* lockUnlockWidget = nullptr;
    QProgressBar* progressBar = nullptr;

    int nDisplayUnit = -1;
    QTimer* timerStakingIcon = nullptr;
    bool isInitializing = true;

    void updateTorIcon();
};

#endif // TOPBAR_H
