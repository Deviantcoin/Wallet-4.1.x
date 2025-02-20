// Copyright (c) 2019-2020 The DEV developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "qt/dev/deviantgui.h"

#ifdef Q_OS_MAC
#include "macdockiconhandler.h"
#endif

#include "clientmodel.h"
#include "guiinterface.h"
#include "networkstyle.h"
#include "notificator.h"
#include "optionsmodel.h"
#include "qt/dev/defaultdialog.h"
#include "qt/dev/qtutils.h"
#include "qt/dev/settings/settingsfaqwidget.h"
#include "qt/guiutil.h"

#include "init.h"
#include "util.h"

#include <QApplication>
#include <QColor>
#include <QDesktopWidget>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QShortcut>
#include <QVBoxLayout>
#include <QWindowStateChangeEvent>


#define BASE_WINDOW_WIDTH 1200
#define BASE_WINDOW_HEIGHT 780
#define BASE_WINDOW_MIN_HEIGHT 620
#define BASE_WINDOW_MIN_WIDTH 1100


const QString DEVGUI::DEFAULT_WALLET = "~Default";

DEVGUI::DEVGUI(const NetworkStyle* networkStyle, QWidget* parent) : QMainWindow(parent),
                                                                    clientModel(0)
{
    /* Open CSS when configured */
    this->setStyleSheet(GUIUtil::loadStyleSheet());
    this->setMinimumSize(BASE_WINDOW_MIN_WIDTH, BASE_WINDOW_MIN_HEIGHT);


    // Adapt screen size
    QRect rec = QApplication::desktop()->screenGeometry();
    int adaptedHeight = (rec.height() < BASE_WINDOW_HEIGHT) ? BASE_WINDOW_MIN_HEIGHT : BASE_WINDOW_HEIGHT;
    int adaptedWidth = (rec.width() < BASE_WINDOW_WIDTH) ? BASE_WINDOW_MIN_WIDTH : BASE_WINDOW_WIDTH;
    GUIUtil::restoreWindowGeometry(
        "nWindow",
        QSize(adaptedWidth, adaptedHeight),
        this);

#ifdef ENABLE_WALLET
    /* if compiled with wallet support, -disablewallet can still disable the wallet */
    enableWallet = !GetBoolArg("-disablewallet", false);
#else
    enableWallet = false;
#endif // ENABLE_WALLET

    QString windowTitle = QString::fromStdString(GetArg("-windowtitle", ""));
    if (windowTitle.isEmpty()) {
        windowTitle = tr("Deviant Core") + " - ";
        windowTitle += ((enableWallet) ? tr("Wallet") : tr("Node"));
    }
    windowTitle += " " + networkStyle->getTitleAddText();
    setWindowTitle(windowTitle);

    QApplication::setWindowIcon(networkStyle->getAppIcon());
    setWindowIcon(networkStyle->getAppIcon());

#ifdef ENABLE_WALLET
    // Create wallet frame
    if (enableWallet) {
        QFrame* centralWidget = new QFrame(this);
        this->setMinimumWidth(BASE_WINDOW_MIN_WIDTH);
        this->setMinimumHeight(BASE_WINDOW_MIN_HEIGHT);
        QHBoxLayout* centralWidgetLayouot = new QHBoxLayout();
        centralWidget->setLayout(centralWidgetLayouot);
        centralWidgetLayouot->setContentsMargins(0, 0, 0, 0);
        centralWidgetLayouot->setSpacing(0);

        centralWidget->setProperty("cssClass", "container");
        centralWidget->setStyleSheet("padding:0px; border:none; margin:0px;");

        // First the nav
        navMenu = new NavMenuWidget(this);
        centralWidgetLayouot->addWidget(navMenu);

        this->setCentralWidget(centralWidget);
        this->setContentsMargins(0, 0, 0, 0);

        QFrame* container = new QFrame(centralWidget);
        container->setContentsMargins(0, 0, 0, 0);
        centralWidgetLayouot->addWidget(container);

        // Then topbar + the stackedWidget
        QVBoxLayout* baseScreensContainer = new QVBoxLayout(this);
        baseScreensContainer->setMargin(0);
        baseScreensContainer->setSpacing(0);
        baseScreensContainer->setContentsMargins(0, 0, 0, 0);
        container->setLayout(baseScreensContainer);

        // Insert the topbar
        topBar = new TopBar(this);
        topBar->setContentsMargins(0, 0, 0, 0);
        baseScreensContainer->addWidget(topBar);

        // Now stacked widget
        stackedContainer = new QStackedWidget(this);
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        stackedContainer->setSizePolicy(sizePolicy);
        stackedContainer->setContentsMargins(0, 0, 0, 0);
        baseScreensContainer->addWidget(stackedContainer);

        // Init
        dashboard = new DashboardWidget(this);
        sendWidget = new SendWidget(this);
        receiveWidget = new ReceiveWidget(this);
        addressesWidget = new AddressesWidget(this);
        masterNodesWidget = new MasterNodesWidget(this);
        coldStakingWidget = new ColdStakingWidget(this);
        governancePage = new GovernancePage(this);
        settingsWidget = new SettingsWidget(this);

        // Add to parent
        stackedContainer->addWidget(dashboard);
        stackedContainer->addWidget(sendWidget);
        stackedContainer->addWidget(receiveWidget);
        stackedContainer->addWidget(addressesWidget);
        stackedContainer->addWidget(masterNodesWidget);
        stackedContainer->addWidget(coldStakingWidget);
        stackedContainer->addWidget(governancePage);
        stackedContainer->addWidget(settingsWidget);
        stackedContainer->setCurrentWidget(dashboard);

    } else
#endif
    {
        // When compiled without wallet or -disablewallet is provided,
        // the central widget is the rpc console.
        rpcConsole = new RPCConsole(enableWallet ? this : 0);
        setCentralWidget(rpcConsole);
    }

    // Create actions for the toolbar, menu bar and tray/dock icon
    createActions(networkStyle);

    // Create system tray icon and notification
    createTrayIcon(networkStyle);

    // Connect events
    connectActions();

    // TODO: Add event filter??
    // // Install event filter to be able to catch status tip events (QEvent::StatusTip)
    //    this->installEventFilter(this);

    // Subscribe to notifications from core
    subscribeToCoreSignals();
}

void DEVGUI::createActions(const NetworkStyle* networkStyle)
{
    toggleHideAction = new QAction(networkStyle->getAppIcon(), tr("&Show / Hide"), this);
    toggleHideAction->setStatusTip(tr("Show or hide the main Window"));

    quitAction = new QAction(QIcon(":/icons/quit"), tr("E&xit"), this);
    quitAction->setStatusTip(tr("Quit application"));
    quitAction->setShortcut(QKeySequence(Qt::CTRL + Qt::Key_Q));
    quitAction->setMenuRole(QAction::QuitRole);

    connect(toggleHideAction, &QAction::triggered, this, &DEVGUI::toggleHidden);
    connect(quitAction, &QAction::triggered, qApp, &QApplication::quit);
}

/**
 * Here add every event connection
 */
void DEVGUI::connectActions()
{
    QShortcut* consoleShort = new QShortcut(this);
    consoleShort->setKey(QKeySequence(SHORT_KEY + Qt::Key_C));
    connect(consoleShort, &QShortcut::activated, [this]() {
        navMenu->selectSettings();
        settingsWidget->showDebugConsole();
        goToSettings();
    });
    connect(topBar, &TopBar::showHide, this, &DEVGUI::showHide);
    connect(topBar, &TopBar::themeChanged, this, &DEVGUI::changeTheme);
    connect(topBar, &TopBar::onShowHideColdStakingChanged, navMenu, &NavMenuWidget::onShowHideColdStakingChanged);
    connect(settingsWidget, &SettingsWidget::showHide, this, &DEVGUI::showHide);
    connect(sendWidget, &SendWidget::showHide, this, &DEVGUI::showHide);
    connect(receiveWidget, &ReceiveWidget::showHide, this, &DEVGUI::showHide);
    connect(addressesWidget, &AddressesWidget::showHide, this, &DEVGUI::showHide);
    connect(masterNodesWidget, &MasterNodesWidget::showHide, this, &DEVGUI::showHide);
    connect(masterNodesWidget, &MasterNodesWidget::execDialog, this, &DEVGUI::execDialog);
    connect(coldStakingWidget, &ColdStakingWidget::showHide, this, &DEVGUI::showHide);
    connect(coldStakingWidget, &ColdStakingWidget::execDialog, this, &DEVGUI::execDialog);
    connect(settingsWidget, &SettingsWidget::execDialog, this, &DEVGUI::execDialog);
}


void DEVGUI::createTrayIcon(const NetworkStyle* networkStyle)
{
#ifndef Q_OS_MAC
    trayIcon = new QSystemTrayIcon(this);
    QString toolTip = tr("Deviant Core client") + " " + networkStyle->getTitleAddText();
    trayIcon->setToolTip(toolTip);
    trayIcon->setIcon(networkStyle->getAppIcon());
    trayIcon->hide();
#endif
    notificator = new Notificator(QApplication::applicationName(), trayIcon, this);
}

//
DEVGUI::~DEVGUI()
{
    // Unsubscribe from notifications from core
    unsubscribeFromCoreSignals();

    GUIUtil::saveWindowGeometry("nWindow", this);
    if (trayIcon) // Hide tray icon, as deleting will let it linger until quit (on Ubuntu)
        trayIcon->hide();
#ifdef Q_OS_MAC
    MacDockIconHandler::cleanup();
#endif
}


/** Get restart command-line parameters and request restart */
void DEVGUI::handleRestart(QStringList args)
{
    if (!ShutdownRequested())
        Q_EMIT requestedRestart(args);
}


void DEVGUI::setClientModel(ClientModel* clientModel)
{
    this->clientModel = clientModel;
    if (this->clientModel) {
        // Create system tray menu (or setup the dock menu) that late to prevent users from calling actions,
        // while the client has not yet fully loaded
        createTrayIconMenu();

        topBar->setClientModel(clientModel);
        dashboard->setClientModel(clientModel);
        sendWidget->setClientModel(clientModel);
        settingsWidget->setClientModel(clientModel);

        // Receive and report messages from client model
        connect(clientModel, &ClientModel::message, this, &DEVGUI::message);
        connect(topBar, &TopBar::walletSynced, dashboard, &DashboardWidget::walletSynced);
        connect(topBar, &TopBar::walletSynced, coldStakingWidget, &ColdStakingWidget::walletSynced);

        // Get restart command-line parameters and handle restart
        connect(settingsWidget, &SettingsWidget::handleRestart, [this](QStringList arg) { handleRestart(arg); });

        if (rpcConsole) {
            rpcConsole->setClientModel(clientModel);
        }

        if (trayIcon) {
            trayIcon->show();
        }
    } else {
        // Disable possibility to show main window via action
        toggleHideAction->setEnabled(false);
        if (trayIconMenu) {
            // Disable context menu on tray icon
            trayIconMenu->clear();
        }
    }
}

void DEVGUI::createTrayIconMenu()
{
#ifndef Q_OS_MAC
    // return if trayIcon is unset (only on non-macOSes)
    if (!trayIcon)
        return;

    trayIconMenu = new QMenu(this);
    trayIcon->setContextMenu(trayIconMenu);

    connect(trayIcon, &QSystemTrayIcon::activated, this, &DEVGUI::trayIconActivated);
#else
    // Note: On macOS, the Dock icon is used to provide the tray's functionality.
    MacDockIconHandler* dockIconHandler = MacDockIconHandler::instance();
    connect(dockIconHandler, &MacDockIconHandler::dockIconClicked, this, &DEVGUI::macosDockIconActivated);

    trayIconMenu = new QMenu(this);
    trayIconMenu->setAsDockMenu();
#endif

    // Configuration of the tray icon (or Dock icon) icon menu
    trayIconMenu->addAction(toggleHideAction);
    trayIconMenu->addSeparator();

#ifndef Q_OS_MAC // This is built-in on macOS
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(quitAction);
#endif
}

#ifndef Q_OS_MAC
void DEVGUI::trayIconActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::Trigger) {
        // Click on system tray icon triggers show/hide of the main window
        toggleHidden();
    }
}
#else
void DEVGUI::macosDockIconActivated()
{
    show();
    activateWindow();
}
#endif

void DEVGUI::changeEvent(QEvent* e)
{
    QMainWindow::changeEvent(e);
#ifndef Q_OS_MAC // Ignored on Mac
    if (e->type() == QEvent::WindowStateChange) {
        if (clientModel && clientModel->getOptionsModel() && clientModel->getOptionsModel()->getMinimizeToTray()) {
            QWindowStateChangeEvent* wsevt = static_cast<QWindowStateChangeEvent*>(e);
            if (!(wsevt->oldState() & Qt::WindowMinimized) && isMinimized()) {
                QTimer::singleShot(0, this, &DEVGUI::hide);
                e->ignore();
            }
        }
    }
#endif
}

void DEVGUI::closeEvent(QCloseEvent* event)
{
#ifndef Q_OS_MAC // Ignored on Mac
    if (clientModel && clientModel->getOptionsModel()) {
        if (!clientModel->getOptionsModel()->getMinimizeOnClose()) {
            QApplication::quit();
        }
    }
#endif
    QMainWindow::closeEvent(event);
}


void DEVGUI::messageInfo(const QString& text)
{
    if (!this->snackBar) this->snackBar = new SnackBar(this, this);
    this->snackBar->setText(text);
    this->snackBar->resize(this->width(), snackBar->height());
    openDialog(this->snackBar, this);
}


void DEVGUI::message(const QString& title, const QString& message, unsigned int style, bool* ret)
{
    QString strTitle = tr("Deviant Core"); // default title
    // Default to information icon
    int nNotifyIcon = Notificator::Information;

    QString msgType;

    // Prefer supplied title over style based title
    if (!title.isEmpty()) {
        msgType = title;
    } else {
        switch (style) {
        case CClientUIInterface::MSG_ERROR:
            msgType = tr("Error");
            break;
        case CClientUIInterface::MSG_WARNING:
            msgType = tr("Warning");
            break;
        case CClientUIInterface::MSG_INFORMATION:
            msgType = tr("Information");
            break;
        default:
            msgType = tr("System Message");
            break;
        }
    }

    // Check for error/warning icon
    if (style & CClientUIInterface::ICON_ERROR) {
        nNotifyIcon = Notificator::Critical;
    } else if (style & CClientUIInterface::ICON_WARNING) {
        nNotifyIcon = Notificator::Warning;
    }

    // Display message
    if (style & CClientUIInterface::MODAL) {
        // Check for buttons, use OK as default, if none was supplied
        int r = 0;
        showNormalIfMinimized();
        if (style & CClientUIInterface::BTN_MASK) {
            r = openStandardDialog(
                (title.isEmpty() ? strTitle : title), message, "OK", "CANCEL");
        } else {
            r = openStandardDialog((title.isEmpty() ? strTitle : title), message, "OK");
        }
        if (ret != NULL)
            *ret = r;
    } else if (style & CClientUIInterface::MSG_INFORMATION_SNACK) {
        messageInfo(message);
    } else {
        // Append title to "DEV - "
        if (!msgType.isEmpty())
            strTitle += " - " + msgType;
        notificator->notify((Notificator::Class)nNotifyIcon, strTitle, message);
    }
}

bool DEVGUI::openStandardDialog(QString title, QString body, QString okBtn, QString cancelBtn)
{
    DefaultDialog* dialog;
    if (isVisible()) {
        showHide(true);
        dialog = new DefaultDialog(this);
        dialog->setText(title, body, okBtn, cancelBtn);
        dialog->adjustSize();
        openDialogWithOpaqueBackground(dialog, this);
    } else {
        dialog = new DefaultDialog();
        dialog->setText(title, body, okBtn);
        dialog->setWindowTitle(tr("Deviant Core"));
        dialog->adjustSize();
        dialog->raise();
        dialog->exec();
    }
    bool ret = dialog->isOk;
    dialog->deleteLater();
    return ret;
}


void DEVGUI::showNormalIfMinimized(bool fToggleHidden)
{
    if (!clientModel)
        return;
    if (!isHidden() && !isMinimized() && !GUIUtil::isObscured(this) && fToggleHidden) {
        hide();
    } else {
        GUIUtil::bringToFront(this);
    }
}

void DEVGUI::toggleHidden()
{
    showNormalIfMinimized(true);
}

void DEVGUI::detectShutdown()
{
    if (ShutdownRequested()) {
        if (rpcConsole)
            rpcConsole->hide();
        qApp->quit();
    }
}

void DEVGUI::goToDashboard()
{
    if (stackedContainer->currentWidget() != dashboard) {
        stackedContainer->setCurrentWidget(dashboard);
        topBar->showBottom();
    }
}

void DEVGUI::goToSend()
{
    showTop(sendWidget);
}

void DEVGUI::goToAddresses()
{
    showTop(addressesWidget);
}

void DEVGUI::goToPrivacy()
{
    if (privacyWidget) showTop(privacyWidget);
}

void DEVGUI::goToMasterNodes()
{
    showTop(masterNodesWidget);
}

void DEVGUI::goToColdStaking()
{
    showTop(coldStakingWidget);
}

void DEVGUI::goToGovernance()
{
    showTop(governancePage);
}

void DEVGUI::goToSettings()
{
    showTop(settingsWidget);
}

void DEVGUI::goToSettingsInfo()
{
    navMenu->selectSettings();
    settingsWidget->showInformation();
    goToSettings();
}

void DEVGUI::goToReceive()
{
    showTop(receiveWidget);
}

void DEVGUI::openNetworkMonitor()
{
    settingsWidget->openNetworkMonitor();
}

void DEVGUI::showTop(QWidget* view)
{
    if (stackedContainer->currentWidget() != view) {
        stackedContainer->setCurrentWidget(view);
        topBar->showTop();
    }
}

void DEVGUI::changeTheme(bool isLightTheme)
{
    QString css = GUIUtil::loadStyleSheet();
    this->setStyleSheet(css);

    // Notify
    Q_EMIT themeChanged(isLightTheme, css);

    // Update style
    updateStyle(this);
}

void DEVGUI::resizeEvent(QResizeEvent* event)
{
    // Parent..
    QMainWindow::resizeEvent(event);
    // background
    showHide(opEnabled);
    // Notify
    Q_EMIT windowResizeEvent(event);
}

bool DEVGUI::execDialog(QDialog* dialog, int xDiv, int yDiv)
{
    return openDialogWithOpaqueBackgroundY(dialog, this);
}

void DEVGUI::showHide(bool show)
{
    if (!op) op = new QLabel(this);
    if (!show) {
        op->setVisible(false);
        opEnabled = false;
    } else {
        QColor bg("#000000");
        bg.setAlpha(200);
        if (!isLightTheme()) {
            bg = QColor("#00000000");
            bg.setAlpha(150);
        }

        QPalette palette;
        palette.setColor(QPalette::Window, bg);
        op->setAutoFillBackground(true);
        op->setPalette(palette);
        op->setWindowFlags(Qt::CustomizeWindowHint);
        op->move(0, 0);
        op->show();
        op->activateWindow();
        op->resize(width(), height());
        op->setVisible(true);
        opEnabled = true;
    }
}

int DEVGUI::getNavWidth()
{
    return this->navMenu->width();
}

void DEVGUI::openFAQ(int section)
{
    showHide(true);
    SettingsFaqWidget* dialog = new SettingsFaqWidget(this);
    if (section > 0) dialog->setSection(section);
    openDialogWithOpaqueBackgroundFullScreen(dialog, this);
    dialog->deleteLater();
}


#ifdef ENABLE_WALLET
bool DEVGUI::addWallet(const QString& name, WalletModel* walletModel)
{
    // Single wallet supported for now..
    if (!stackedContainer || !clientModel || !walletModel)
        return false;

    // set the model for every view
    navMenu->setWalletModel(walletModel);
    dashboard->setWalletModel(walletModel);
    topBar->setWalletModel(walletModel);
    receiveWidget->setWalletModel(walletModel);
    sendWidget->setWalletModel(walletModel);
    addressesWidget->setWalletModel(walletModel);
    masterNodesWidget->setWalletModel(walletModel);
    coldStakingWidget->setWalletModel(walletModel);
    governancePage->setWalletModel(walletModel);
    settingsWidget->setWalletModel(walletModel);

    // Privacy screen
    if (walletModel->getZerocoinBalance() > 0) {
        privacyWidget = new PrivacyWidget(this);
        stackedContainer->addWidget(privacyWidget);

        privacyWidget->setWalletModel(walletModel);
        connect(privacyWidget, &PrivacyWidget::message, this, &DEVGUI::message);
        connect(privacyWidget, &PrivacyWidget::showHide, this, &DEVGUI::showHide);
    }

    // Connect actions..
    connect(masterNodesWidget, &MasterNodesWidget::message, this, &DEVGUI::message);
    connect(coldStakingWidget, &ColdStakingWidget::message, this, &DEVGUI::message);
    connect(topBar, &TopBar::message, this, &DEVGUI::message);
    connect(sendWidget, &SendWidget::message, this, &DEVGUI::message);
    connect(receiveWidget, &ReceiveWidget::message, this, &DEVGUI::message);
    connect(addressesWidget, &AddressesWidget::message, this, &DEVGUI::message);
    connect(settingsWidget, &SettingsWidget::message, this, &DEVGUI::message);

    // Pass through transaction notifications
    connect(dashboard, &DashboardWidget::incomingTransaction, this, &DEVGUI::incomingTransaction);

    return true;
}

bool DEVGUI::setCurrentWallet(const QString& name)
{
    // Single wallet supported.
    return true;
}

void DEVGUI::removeAllWallets()
{
    // Single wallet supported.
}

void DEVGUI::incomingTransaction(const QString& date, int unit, const CAmount& amount, const QString& type, const QString& address)
{
    // Only send notifications when not disabled
    if (!bdisableSystemnotifications) {
        // On new transaction, make an info balloon
        message((amount) < 0 ? (pwalletMain->fMultiSendNotify == true ? tr("Sent MultiSend transaction") : tr("Sent transaction")) : tr("Incoming transaction"),
            tr("Date: %1\n"
               "Amount: %2\n"
               "Type: %3\n"
               "Address: %4\n")
                .arg(date)
                .arg(BitcoinUnits::formatWithUnit(unit, amount, true))
                .arg(type)
                .arg(address),
            CClientUIInterface::MSG_INFORMATION);

        pwalletMain->fMultiSendNotify = false;
    }
}

#endif // ENABLE_WALLET


static bool ThreadSafeMessageBox(DEVGUI* gui, const std::string& message, const std::string& caption, unsigned int style)
{
    bool modal = (style & CClientUIInterface::MODAL);
    // The SECURE flag has no effect in the Qt GUI.
    // bool secure = (style & CClientUIInterface::SECURE);
    style &= ~CClientUIInterface::SECURE;
    bool ret = false;
    std::cout << "thread safe box: " << message << std::endl;
    // In case of modal message, use blocking connection to wait for user to click a button
    QMetaObject::invokeMethod(gui, "message",
        modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
        Q_ARG(QString, QString::fromStdString(caption)),
        Q_ARG(QString, QString::fromStdString(message)),
        Q_ARG(unsigned int, style),
        Q_ARG(bool*, &ret));
    return ret;
}


void DEVGUI::subscribeToCoreSignals()
{
    // Connect signals to client
    uiInterface.ThreadSafeMessageBox.connect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}

void DEVGUI::unsubscribeFromCoreSignals()
{
    // Disconnect signals from client
    uiInterface.ThreadSafeMessageBox.disconnect(boost::bind(ThreadSafeMessageBox, this, _1, _2, _3));
}
