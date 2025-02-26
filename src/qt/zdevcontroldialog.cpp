// Copyright (c) 2017-2020 The PIVX developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "zdevcontroldialog.h"
#include "ui_zdevcontroldialog.h"

#include "guiutil.h"
#include "main.h"
#include "walletmodel.h"


std::set<std::string> zDEVControlDialog::setSelectedMints;
std::set<CMintMeta> zDEVControlDialog::setMints;

bool CzDEVControlWidgetItem::operator<(const QTreeWidgetItem& other) const
{
    int column = treeWidget()->sortColumn();
    if (column == zDEVControlDialog::COLUMN_DENOMINATION || column == zDEVControlDialog::COLUMN_VERSION || column == zDEVControlDialog::COLUMN_CONFIRMATIONS)
        return data(column, Qt::UserRole).toLongLong() < other.data(column, Qt::UserRole).toLongLong();
    return QTreeWidgetItem::operator<(other);
}


zDEVControlDialog::zDEVControlDialog(QWidget* parent) : QDialog(parent, Qt::WindowSystemMenuHint | Qt::WindowTitleHint | Qt::WindowCloseButtonHint),
                                                        ui(new Ui::zDEVControlDialog),
                                                        model(0)
{
    ui->setupUi(this);
    setMints.clear();

    /* Open CSS when configured */
    this->setStyleSheet(GUIUtil::loadStyleSheet());

    ui->frame->setProperty("cssClass", "container-dialog");

    // Title
    ui->labelTitle->setText(tr("Select zDEV Denominations to Spend"));
    ui->labelTitle->setProperty("cssClass", "text-title-dialog");


    // Label Style
    ui->labelzDEV->setProperty("cssClass", "text-main-purple");
    ui->labelzDEV_int->setProperty("cssClass", "text-main-purple");
    ui->labelQuantity->setProperty("cssClass", "text-main-purple");
    ui->labelQuantity_int->setProperty("cssClass", "text-main-purple");

    ui->layoutAmount->setProperty("cssClass", "container-border-purple");
    ui->layoutQuantity->setProperty("cssClass", "container-border-purple");

    // Buttons

    ui->btnEsc->setText("");
    ui->btnEsc->setProperty("cssClass", "ic-close");
    ui->pushButtonAll->setProperty("cssClass", "btn-check");

    // click on checkbox
    connect(ui->treeWidget, &QTreeWidget::itemChanged, this, &zDEVControlDialog::updateSelection);
    // push select/deselect all button
    connect(ui->pushButtonAll, &QPushButton::clicked, this, &zDEVControlDialog::ButtonAllClicked);
}

zDEVControlDialog::~zDEVControlDialog()
{
    delete ui;
}

void zDEVControlDialog::setModel(WalletModel* model)
{
    this->model = model;
    updateList();
}


//Update the tree widget
void zDEVControlDialog::updateList()
{
    // need to prevent the slot from being called each time something is changed
    ui->treeWidget->blockSignals(true);
    ui->treeWidget->clear();

    // add a top level item for each denomination
    QFlags<Qt::ItemFlag> flgTristate = Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsTristate;
    std::map<libzerocoin::CoinDenomination, int> mapDenomPosition;
    for (auto denom : libzerocoin::zerocoinDenomList) {
        CzDEVControlWidgetItem* itemDenom(new CzDEVControlWidgetItem);
        ui->treeWidget->addTopLevelItem(itemDenom);

        //keep track of where this is positioned in tree widget
        mapDenomPosition[denom] = ui->treeWidget->indexOfTopLevelItem(itemDenom);

        itemDenom->setFlags(flgTristate);
        itemDenom->setText(COLUMN_DENOMINATION, QString::number(denom));
        itemDenom->setData(COLUMN_DENOMINATION, Qt::UserRole, QVariant((qlonglong)denom));
    }

    // select all unused coins - including not mature and mismatching seed. Update status of coins too.
    std::set<CMintMeta> set;
    model->listZerocoinMints(set, true, false, true, true);
    this->setMints = set;

    //populate rows with mint info
    int nBestHeight = chainActive.Height();
    for (const CMintMeta& mint : setMints) {
        // assign this mint to the correct denomination in the tree view
        libzerocoin::CoinDenomination denom = mint.denom;
        CzDEVControlWidgetItem* itemMint = new CzDEVControlWidgetItem(ui->treeWidget->topLevelItem(mapDenomPosition.at(denom)));

        // if the mint is already selected, then it needs to have the checkbox checked
        std::string strPubCoinHash = mint.hashPubcoin.GetHex();

        if (setSelectedMints.count(strPubCoinHash))
            itemMint->setCheckState(COLUMN_CHECKBOX, Qt::Checked);
        else
            itemMint->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

        itemMint->setText(COLUMN_DENOMINATION, QString::number(mint.denom));
        itemMint->setData(COLUMN_DENOMINATION, Qt::UserRole, QVariant((qlonglong)denom));
        itemMint->setText(COLUMN_PUBCOIN, QString::fromStdString(strPubCoinHash));
        itemMint->setText(COLUMN_VERSION, QString::number(mint.nVersion));
        itemMint->setData(COLUMN_VERSION, Qt::UserRole, QVariant((qlonglong)mint.nVersion));

        int nConfirmations = (mint.nHeight ? nBestHeight - mint.nHeight : 0);
        if (nConfirmations < 0) {
            // Sanity check
            nConfirmations = 0;
        }

        itemMint->setText(COLUMN_CONFIRMATIONS, QString::number(nConfirmations));
        itemMint->setData(COLUMN_CONFIRMATIONS, Qt::UserRole, QVariant((qlonglong)nConfirmations));

        // check for maturity
        // Always mature, public spends doesn't require any new accumulation.
        bool isMature = true;
        //if (mapMaturityHeight.count(mint.denom))
        //    isMature = mint.nHeight < mapMaturityHeight.at(denom);

        // disable selecting this mint if it is not spendable - also display a reason why
        const int nRequiredConfs = Params().GetConsensus().ZC_MinMintConfirmations;
        bool fSpendable = isMature && nConfirmations >= nRequiredConfs && mint.isSeedCorrect;
        if (!fSpendable) {
            itemMint->setDisabled(true);
            itemMint->setCheckState(COLUMN_CHECKBOX, Qt::Unchecked);

            //if this mint is in the selection list, then remove it
            if (setSelectedMints.count(strPubCoinHash))
                setSelectedMints.erase(strPubCoinHash);

            std::string strReason = "";
            if (nConfirmations < nRequiredConfs)
                strReason = strprintf("Needs %d more confirmations", nRequiredConfs - nConfirmations);
            else if (model->getEncryptionStatus() == WalletModel::EncryptionStatus::Locked)
                strReason = "Your wallet is locked. Impossible to spend zDEV.";
            else if (!mint.isSeedCorrect)
                strReason = "The zDEV seed used to mint this zDEV is not the same as currently hold in the wallet";
            else
                strReason = "Needs 1 more mint added to network";

            itemMint->setText(COLUMN_ISSPENDABLE, QString::fromStdString(strReason));
        } else {
            itemMint->setText(COLUMN_ISSPENDABLE, QString("Yes"));
        }
    }

    ui->treeWidget->blockSignals(false);
    updateLabels();
}

// Update the list when a checkbox is clicked
void zDEVControlDialog::updateSelection(QTreeWidgetItem* item, int column)
{
    // only want updates from non top level items that are available to spend
    if (item->parent() && column == COLUMN_CHECKBOX && !item->isDisabled()) {
        // see if this mint is already selected in the selection list
        std::string strPubcoin = item->text(COLUMN_PUBCOIN).toStdString();
        bool fSelected = setSelectedMints.count(strPubcoin);

        // set the checkbox to the proper state and add or remove the mint from the selection list
        if (item->checkState(COLUMN_CHECKBOX) == Qt::Checked) {
            if (fSelected) return;
            setSelectedMints.insert(strPubcoin);
        } else {
            if (!fSelected) return;
            setSelectedMints.erase(strPubcoin);
        }
        updateLabels();
    }
}

// Update the Quantity and Amount display
void zDEVControlDialog::updateLabels()
{
    int64_t nAmount = 0;
    for (const CMintMeta& mint : setMints) {
        if (setSelectedMints.count(mint.hashPubcoin.GetHex()))
            nAmount += mint.denom;
    }

    //update this dialog's labels
    ui->labelzDEV_int->setText(QString::number(nAmount));
    ui->labelQuantity_int->setText(QString::number(setSelectedMints.size()));

    //update PrivacyDialog labels
    //privacyDialog->setzDEVControlLabels(nAmount, setSelectedMints.size());
}

std::vector<CMintMeta> zDEVControlDialog::GetSelectedMints()
{
    std::vector<CMintMeta> listReturn;
    for (const CMintMeta& mint : setMints) {
        if (setSelectedMints.count(mint.hashPubcoin.GetHex()))
            listReturn.emplace_back(mint);
    }

    return listReturn;
}

// select or deselect all of the mints
void zDEVControlDialog::ButtonAllClicked()
{
    ui->treeWidget->blockSignals(true);
    Qt::CheckState state = Qt::Checked;
    for (int i = 0; i < ui->treeWidget->topLevelItemCount(); i++) {
        if (ui->treeWidget->topLevelItem(i)->checkState(COLUMN_CHECKBOX) != Qt::Unchecked) {
            state = Qt::Unchecked;
            break;
        }
    }

    //much quicker to start from scratch than to have QT go through all the objects and update
    ui->treeWidget->clear();

    if (state == Qt::Checked) {
        for (const CMintMeta& mint : setMints)
            setSelectedMints.insert(mint.hashPubcoin.GetHex());
    } else {
        setSelectedMints.clear();
    }

    updateList();
}
