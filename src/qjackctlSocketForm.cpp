// qjackctlSocketForm.cpp
//
/****************************************************************************
   Copyright (C) 2003-2007, rncbc aka Rui Nuno Capela. All rights reserved.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

*****************************************************************************/

#include "qjackctlAbout.h"
#include "qjackctlSocketForm.h"

#include "qjackctlPatchbay.h"
#include "qjackctlConnectAlias.h"

#include <QHeaderView>
#include <QButtonGroup>
#include <QRegExp>
#include <QPixmap>
#include <QMenu>


//----------------------------------------------------------------------------
// qjackctlSocketForm -- UI wrapper form.

// Constructor.
qjackctlSocketForm::qjackctlSocketForm (
	QWidget *pParent, Qt::WindowFlags wflags )
	: QDialog(pParent, wflags)
{
	// Setup UI struct...
	m_ui.setupUi(this);

	m_pSocketList  = NULL;
	m_pJackClient  = NULL;
	m_pAlsaSeq     = NULL;
	m_ppPixmaps    = NULL;

	// Setup time-display radio-button group.
	m_pSocketTypeButtonGroup = new QButtonGroup(this);
	m_pSocketTypeButtonGroup->addButton(m_ui.AudioRadioButton, 0);
	m_pSocketTypeButtonGroup->addButton(m_ui.MidiRadioButton,  1);
	m_pSocketTypeButtonGroup->setExclusive(true);

	// Plug list is not sortable.
	//m_ui.PlugListView->setSorting(-1);

	// Plug list view...
	QHeaderView *pHeader = m_ui.PlugListView->header();
//	pHeader->setResizeMode(QHeaderView::Custom);
	pHeader->setDefaultAlignment(Qt::AlignLeft);
//	pHeader->setDefaultSectionSize(300);
	pHeader->setMovable(false);
	pHeader->setStretchLastSection(true);

#ifndef CONFIG_ALSA_SEQ
	m_ui.MidiRadioButton->setEnabled(false);
#endif

	// UI connections...

	QObject::connect(m_ui.PlugAddPushButton,
		SIGNAL(clicked()),
		SLOT(addPlug()));
	QObject::connect(m_ui.PlugRemovePushButton,
		SIGNAL(clicked()),
		SLOT(removePlug()));
	QObject::connect(m_ui.PlugEditPushButton,
		SIGNAL(clicked()),
		SLOT(editPlug()));
	QObject::connect(m_ui.PlugUpPushButton,
		SIGNAL(clicked()),
		SLOT(moveUpPlug()));
	QObject::connect(m_ui.PlugDownPushButton,
		SIGNAL(clicked()),
		SLOT(moveDownPlug()));
	QObject::connect(m_ui.PlugListView,
		SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
		SLOT(selectedPlug()));

	QObject::connect(m_ui.SocketNameLineEdit,
		SIGNAL(textChanged(const QString&)),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.AudioRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(socketTypeChanged()));
	QObject::connect(m_ui.MidiRadioButton,
		SIGNAL(toggled(bool)),
		SLOT(socketTypeChanged()));
	QObject::connect(m_ui.ExclusiveCheckBox,
		SIGNAL(toggled(bool)),
		SLOT(socketTypeChanged()));
	QObject::connect(m_ui.ClientNameComboBox,
		SIGNAL(textChanged(const QString&)),
		SLOT(clientNameChanged()));
	QObject::connect(m_ui.PlugNameComboBox,
		SIGNAL(textChanged(const QString&)),
		SLOT(stabilizeForm()));

	QObject::connect(m_ui.PlugListView,
		SIGNAL(contextMenuRequested(const QPoint&)),
		SLOT(contextMenu(const QPoint&)));
	QObject::connect(m_ui.PlugListView,
		SIGNAL(itemRenamed()),
		SLOT(stabilizeForm()));
	QObject::connect(m_ui.SocketForwardComboBox,
		SIGNAL(activated(int)),
		SLOT(stabilizeForm()));

	QObject::connect(m_ui.OkPushButton,
		SIGNAL(clicked()),
		SLOT(accept()));
	QObject::connect(m_ui.CancelPushButton,
		SIGNAL(clicked()),
		SLOT(reject()));
}


// Destructor.
qjackctlSocketForm::~qjackctlSocketForm (void)
{
	delete m_pSocketTypeButtonGroup;
}


// Socket caption utility method.
void qjackctlSocketForm::setSocketCaption ( const QString& sSocketCaption )
{
	m_ui.SocketTabWidget->setTabText(0, sSocketCaption);
	(m_ui.PlugListView->headerItem())->setText(0,
		sSocketCaption + ' ' + tr("Plugs / Ports"));
}


// Socket list enablement.
void qjackctlSocketForm::setSocketList ( qjackctlSocketList *pSocketList )
{
	m_pSocketList = pSocketList;
}


// Pixmap utility methods.
void qjackctlSocketForm::setPixmaps ( QPixmap **ppPixmaps )
{
	m_ppPixmaps = ppPixmaps;
}


// JACK client accessor.
void qjackctlSocketForm::setJackClient ( jack_client_t *pJackClient )
{
	m_pJackClient = pJackClient;
}


// ALSA sequencer accessor.
void qjackctlSocketForm::setAlsaSeq ( snd_seq_t *pAlsaSeq )
{
	m_pAlsaSeq = pAlsaSeq;
}


// Socket type and exclusiveness editing enablement.
void qjackctlSocketForm::setConnectCount ( int iConnectCount )
{
//	m_ui.SocketTypeGroupBox->setEnabled(iConnectCount < 1);
	if (iConnectCount) {
		switch (m_pSocketTypeButtonGroup->checkedId()) {
		case 0: // QJACKCTL_SOCKETTYPE_AUDIO
			m_ui.MidiRadioButton->setEnabled(false);
			break;
		case 1: // QJACKCTL_SOCKETTYPE_MIDI
			m_ui.AudioRadioButton->setEnabled(false);
			break;
		}
	}
//	m_ui.ExclusiveCheckBox->setEnabled(iConnectCount < 2);

#ifndef CONFIG_ALSA_SEQ
	m_ui.MidiRadioButton->setEnabled(false);
#endif
}


// Load dialog controls from socket properties.
void qjackctlSocketForm::load ( qjackctlPatchbaySocket *pSocket )
{
	m_ui.SocketNameLineEdit->setText(pSocket->name());

	QRadioButton *pRadioButton
		= static_cast<QRadioButton *> (
			m_pSocketTypeButtonGroup->button(pSocket->type()));
	if (pRadioButton)
		pRadioButton->setChecked(true);

	m_ui.ClientNameComboBox->setEditText(pSocket->clientName());
	m_ui.ExclusiveCheckBox->setChecked(pSocket->isExclusive());

	m_ui.PlugListView->clear();
	QTreeWidgetItem *pPlugItem = NULL;
	QStringListIterator iter(pSocket->pluglist());
	while (iter.hasNext()) {
		const QString& sPlugName = iter.next();
		pPlugItem = new QTreeWidgetItem(m_ui.PlugListView, pPlugItem);
		if (pPlugItem) {
			pPlugItem->setText(0, sPlugName);
			pPlugItem->setFlags(pPlugItem->flags() | Qt::ItemIsEditable);
		}
	}

	socketTypeChanged();

	int iItemIndex = 0;
	if (!pSocket->forward().isEmpty()) {
		int iItem = m_ui.SocketForwardComboBox->findText(pSocket->forward());
		if (iItem >= 0)
			iItemIndex = iItem;
	}
	m_ui.SocketForwardComboBox->setCurrentIndex(iItemIndex);

	stabilizeForm();
}


// Save dialog controls into socket properties.
void qjackctlSocketForm::save ( qjackctlPatchbaySocket *pSocket )
{
	pSocket->setName(m_ui.SocketNameLineEdit->text());
	pSocket->setType(m_pSocketTypeButtonGroup->checkedId());
	pSocket->setClientName(m_ui.ClientNameComboBox->currentText());
	pSocket->setExclusive(m_ui.ExclusiveCheckBox->isChecked());

	pSocket->pluglist().clear();
	int iPlugCount = m_ui.PlugListView->topLevelItemCount();
	for (int iPlug = 0; iPlug < iPlugCount; ++iPlug) {
		QTreeWidgetItem *pItem = m_ui.PlugListView->topLevelItem(iPlug);
		pSocket->addPlug(pItem->text(0));
	}

	if (m_ui.SocketForwardComboBox->currentIndex() > 0)
		pSocket->setForward(m_ui.SocketForwardComboBox->currentText());
	else
		pSocket->setForward(QString::null);
}


// Stabilize current state form.
void qjackctlSocketForm::stabilizeForm (void)
{
	m_ui.OkPushButton->setEnabled(validateForm());

	QTreeWidgetItem *pItem = m_ui.PlugListView->currentItem();
	if (pItem) {
		int iItem = m_ui.PlugListView->indexOfTopLevelItem(pItem);
		int iItemCount = m_ui.PlugListView->topLevelItemCount();
		m_ui.PlugEditPushButton->setEnabled(true);
		m_ui.PlugRemovePushButton->setEnabled(true);
		m_ui.PlugUpPushButton->setEnabled(iItem > 0);
		m_ui.PlugDownPushButton->setEnabled(iItem < iItemCount - 1);
	} else {
		m_ui.PlugEditPushButton->setEnabled(false);
		m_ui.PlugRemovePushButton->setEnabled(false);
		m_ui.PlugUpPushButton->setEnabled(false);
		m_ui.PlugDownPushButton->setEnabled(false);
	}

	bool bEnabled = !m_ui.PlugNameComboBox->currentText().isEmpty();
	if (bEnabled) {
		bEnabled = (m_ui.PlugListView->findItems(
			m_ui.PlugNameComboBox->currentText(), Qt::MatchExactly).isEmpty());
	}
	m_ui.PlugAddPushButton->setEnabled(bEnabled);
}


// Validate form fields.
bool qjackctlSocketForm::validateForm (void)
{
	bool bValid = true;

	bValid = bValid && !m_ui.SocketNameLineEdit->text().isEmpty();
	bValid = bValid && !m_ui.ClientNameComboBox->currentText().isEmpty();
	bValid = bValid && (m_ui.PlugListView->topLevelItemCount() > 0);

	return bValid;
}

// Validate form fields and accept it valid.
void qjackctlSocketForm::accept (void)
{
	if (!validateForm())
		return;

	QDialog::accept();
}


void qjackctlSocketForm::reject (void)
{
	QDialog::reject();
}


// Add new Plug to socket list.
void qjackctlSocketForm::addPlug (void)
{
	if (m_ppPixmaps == NULL)
		return;

	QString sPlugName = m_ui.PlugNameComboBox->currentText();
	if (!sPlugName.isEmpty()) {
		QTreeWidgetItem *pItem = m_ui.PlugListView->currentItem();
		if (pItem)
			pItem->setSelected(false);
		pItem = new QTreeWidgetItem(m_ui.PlugListView, pItem);
		if (pItem) {
			pItem->setText(0, sPlugName);
			pItem->setFlags(pItem->flags() | Qt::ItemIsEditable);
			QPixmap *pXpmPlug = NULL;
			switch (m_pSocketTypeButtonGroup->checkedId()) {
			case 0: // QJACKCTL_SOCKETTYPE_AUDIO
				pXpmPlug = m_ppPixmaps[QJACKCTL_XPM_AUDIO_PLUG];
				break;
			case 1: // QJACKCTL_SOCKETTYPE_MIDI
				pXpmPlug = m_ppPixmaps[QJACKCTL_XPM_MIDI_PLUG];
				break;
			}
			if (pXpmPlug)
				pItem->setIcon(0, QIcon(*pXpmPlug));
			pItem->setSelected(true);
			m_ui.PlugListView->setCurrentItem(pItem);
		}
		m_ui.PlugNameComboBox->setEditText(QString::null);
	}

	clientNameChanged();
	stabilizeForm();
}


// Rename current selected Plug.
void qjackctlSocketForm::editPlug (void)
{
	QTreeWidgetItem *pItem = m_ui.PlugListView->currentItem();
	if (pItem)
		m_ui.PlugListView->editItem(pItem, 0);

	clientNameChanged();
	stabilizeForm();
}


// Remove current selected Plug.
void qjackctlSocketForm::removePlug (void)
{
	QTreeWidgetItem *pItem = m_ui.PlugListView->currentItem();
	if (pItem)
		delete pItem;

	clientNameChanged();
	stabilizeForm();
}


// Move current selected Plug one position up.
void qjackctlSocketForm::moveUpPlug (void)
{
	QTreeWidgetItem *pItem = m_ui.PlugListView->currentItem();
	if (pItem) {
		int iItem = m_ui.PlugListView->indexOfTopLevelItem(pItem);
		if (iItem > 0) {
			pItem->setSelected(false);
			pItem = m_ui.PlugListView->takeTopLevelItem(iItem);
			m_ui.PlugListView->insertTopLevelItem(iItem - 1, pItem);
			pItem->setSelected(true);
			m_ui.PlugListView->setCurrentItem(pItem);
		}
	}

	stabilizeForm();
}


// Move current selected Plug one position down
void qjackctlSocketForm::moveDownPlug (void)
{
	QTreeWidgetItem *pItem = m_ui.PlugListView->currentItem();
	if (pItem) {
		int iItem = m_ui.PlugListView->indexOfTopLevelItem(pItem);
		int iItemCount = m_ui.PlugListView->topLevelItemCount();
		if (iItem < iItemCount - 1) {
			pItem->setSelected(false);
			pItem = m_ui.PlugListView->takeTopLevelItem(iItem);
			m_ui.PlugListView->insertTopLevelItem(iItem + 1, pItem);
			pItem->setSelected(true);
			m_ui.PlugListView->setCurrentItem(pItem);
		}
	}

	stabilizeForm();
}

// Update selected plug one position down
void qjackctlSocketForm::selectedPlug (void)
{
	QTreeWidgetItem *pItem = m_ui.PlugListView->currentItem();
	if (pItem)
		m_ui.PlugNameComboBox->setEditText(pItem->text(0));

	stabilizeForm();
}


// Add new Plug from context menu.
void qjackctlSocketForm::activateAddPlugMenu ( QAction *pAction )
{
	int iIndex = pAction->data().toInt();
	if (iIndex >= 0 && iIndex < m_ui.PlugNameComboBox->count()) {
		m_ui.PlugNameComboBox->setCurrentIndex(iIndex);
		addPlug();
	}
}


// Plug list context menu handler.
void qjackctlSocketForm::contextMenu ( const QPoint& pos )
{
	int iItem = 0;
	int iItemCount = 0;
	QTreeWidgetItem *pItem = m_ui.PlugListView->itemAt(pos);
	if (pItem == NULL)
		pItem = m_ui.PlugListView->currentItem();
	if (pItem) {
		iItem = m_ui.PlugListView->indexOfTopLevelItem(pItem);
		iItemCount = m_ui.PlugListView->topLevelItemCount();
	}

	QMenu menu(this);
	QAction *pAction;

	// Build the add plug sub-menu...
	QMenu *pAddPlugMenu = menu.addMenu(
		QIcon(":/icons/add1.png"), tr("Add Plug"));
	for (int iIndex = 0; iIndex < m_ui.PlugNameComboBox->count(); iIndex++) {
		pAction = pAddPlugMenu->addAction(
			m_ui.PlugNameComboBox->itemText(iIndex));
		pAction->setData(iIndex);
	}
	QObject::connect(pAddPlugMenu,
		SIGNAL(triggered(QAction*)),
		SLOT(activateAddPlugMenu(QAction*)));
	// Build the plug context menu...
	bool bEnabled = (pItem != NULL);
	pAction = menu.addAction(QIcon(":/icons/remove1.png"),
		tr("Remove"), this, SLOT(removePlug()));
	pAction->setEnabled(bEnabled);
	pAction = menu.addAction(QIcon(":/icons/edit1.png"),
		tr("Edit"), this, SLOT(editPlug()));
	pAction->setEnabled(bEnabled);
	menu.addSeparator();
	pAction = menu.addAction(QIcon(":/icons/up1.png"),
		tr("Move Up"), this, SLOT(moveUpPlug()));
	pAction->setEnabled(bEnabled && iItem > 0);
	pAction = menu.addAction(QIcon(":/icons/down1.png"),
		tr("Move Down"), this, SLOT(moveDownPlug()));
	pAction->setEnabled(bEnabled && iItem < iItemCount - 1);

	menu.exec(pos);
}


// Socket type change slot.
void qjackctlSocketForm::socketTypeChanged (void)
{
	if (m_ppPixmaps == NULL)
		return;
	if (m_pSocketList == NULL)
		return;

	QString sOldClientName = m_ui.ClientNameComboBox->currentText();

	m_ui.ClientNameComboBox->clear();

	QPixmap *pXpmSocket = NULL;
	QPixmap *pXpmPlug   = NULL;

	bool bReadable = m_pSocketList->isReadable();
	int iSocketType = m_pSocketTypeButtonGroup->checkedId();
	switch (iSocketType) {
	case 0: // QJACKCTL_SOCKETTYPE_AUDIO
		if (m_ui.ExclusiveCheckBox->isChecked())
			pXpmSocket = m_ppPixmaps[QJACKCTL_XPM_AUDIO_SOCKET_X];
		else
			pXpmSocket = m_ppPixmaps[QJACKCTL_XPM_AUDIO_SOCKET];
		m_ui.SocketTabWidget->setTabIcon(0, QIcon(*pXpmSocket));
		pXpmPlug = m_ppPixmaps[QJACKCTL_XPM_AUDIO_PLUG];
		if (m_pJackClient) {
			// Grab all client ports.
			const char **ppszClientPorts = jack_get_ports(m_pJackClient,
				0, JACK_DEFAULT_AUDIO_TYPE,
				(bReadable ? JackPortIsOutput : JackPortIsInput));
			if (ppszClientPorts) {
				int iClientPort = 0;
				while (ppszClientPorts[iClientPort]) {
					QString sClientPort = ppszClientPorts[iClientPort];
					int iColon = sClientPort.indexOf(':');
					if (iColon >= 0) {
						QString sClientName
							= qjackctlClientAlias::escapeRegExpDigits(
								sClientPort.left(iColon));
						bool bExists = false;
						for (int i = 0;
							i < m_ui.ClientNameComboBox->count() && !bExists; i++)
							bExists = (sClientName == m_ui.ClientNameComboBox->itemText(i));
						if (!bExists) {
							m_ui.ClientNameComboBox->addItem(
								QIcon(*m_ppPixmaps[QJACKCTL_XPM_AUDIO_CLIENT]),
								sClientName);
						}
					}
					iClientPort++;
				}
				::free(ppszClientPorts);
			}
		}
		break;
	case 1: // QJACKCTL_SOCKETTYPE_MIDI
		if (m_ui.ExclusiveCheckBox->isChecked())
			pXpmSocket = m_ppPixmaps[QJACKCTL_XPM_MIDI_SOCKET_X];
		else
			pXpmSocket = m_ppPixmaps[QJACKCTL_XPM_MIDI_SOCKET];
		m_ui.SocketTabWidget->setTabIcon(0, QIcon(*pXpmSocket));
		pXpmPlug = m_ppPixmaps[QJACKCTL_XPM_MIDI_PLUG];
#ifdef CONFIG_ALSA_SEQ
		if (m_pAlsaSeq) {
			// Readd all subscribers...
			snd_seq_client_info_t *pClientInfo;
			snd_seq_port_info_t   *pPortInfo;
			unsigned int uiAlsaFlags;
			if (bReadable)
				uiAlsaFlags = SND_SEQ_PORT_CAP_READ  | SND_SEQ_PORT_CAP_SUBS_READ;
			else
				uiAlsaFlags = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
			snd_seq_client_info_alloca(&pClientInfo);
			snd_seq_port_info_alloca(&pPortInfo);
			snd_seq_client_info_set_client(pClientInfo, -1);
			while (snd_seq_query_next_client(m_pAlsaSeq, pClientInfo) >= 0) {
				int iAlsaClient = snd_seq_client_info_get_client(pClientInfo);
				QString sClient
					= qjackctlClientAlias::escapeRegExpDigits(
						snd_seq_client_info_get_name(pClientInfo));
				if (iAlsaClient > 0) {
					bool bExists = false;
					snd_seq_port_info_set_client(pPortInfo, iAlsaClient);
					snd_seq_port_info_set_port(pPortInfo, -1);
					while (!bExists
						&& snd_seq_query_next_port(m_pAlsaSeq, pPortInfo) >= 0) {
						unsigned int uiPortCapability
							= snd_seq_port_info_get_capability(pPortInfo);
						if (((uiPortCapability & uiAlsaFlags) == uiAlsaFlags) &&
							((uiPortCapability & SND_SEQ_PORT_CAP_NO_EXPORT) == 0)) {
							for (int i = 0;
								i < m_ui.ClientNameComboBox->count() && !bExists; i++)
								bExists = (sClient == m_ui.ClientNameComboBox->itemText(i));
							if (!bExists) {
								m_ui.ClientNameComboBox->addItem(
									QIcon(*m_ppPixmaps[QJACKCTL_XPM_MIDI_CLIENT]),
									sClient);
								bExists = true;
							}
						}
					}
				}
			}
		}
#endif	// CONFIG_ALSA_SEQ
		break;
	}

	m_ui.ClientNameComboBox->setEditText(sOldClientName);
	clientNameChanged();

	if (pXpmPlug) {
		int iItemCount = m_ui.PlugListView->topLevelItemCount();
		for (int iItem = 0; iItem < iItemCount; ++iItem) {
			QTreeWidgetItem *pItem = m_ui.PlugListView->topLevelItem(iItem);
			pItem->setIcon(0, QIcon(*pXpmPlug));
		}
	}

	// Now the socket forward list...
	m_ui.SocketForwardComboBox->clear();
	m_ui.SocketForwardComboBox->addItem(tr("(None)"));
	if (!bReadable) {
		QListIterator<qjackctlSocketItem *> iter(m_pSocketList->sockets());
		while (iter.hasNext()) {
			qjackctlSocketItem *pSocketItem = iter.next();
			if (pSocketItem->socketType() == iSocketType
				&& pSocketItem->socketName() != m_ui.SocketNameLineEdit->text()) {
				switch (iSocketType) {
				case 0: // QJACKCTL_SOCKETTYPE_AUDIO
					if (pSocketItem->isExclusive())
						pXpmSocket = m_ppPixmaps[QJACKCTL_XPM_AUDIO_SOCKET_X];
					else
						pXpmSocket = m_ppPixmaps[QJACKCTL_XPM_AUDIO_SOCKET];
					break;
				case 1: // QJACKCTL_SOCKETTYPE_MIDI
					if (pSocketItem->isExclusive())
						pXpmSocket = m_ppPixmaps[QJACKCTL_XPM_MIDI_SOCKET_X];
					else
						pXpmSocket = m_ppPixmaps[QJACKCTL_XPM_MIDI_SOCKET];
					break;
				}
				m_ui.SocketForwardComboBox->addItem(
					QIcon(*pXpmSocket), pSocketItem->socketName());
			}
		}
	}

	bool bEnabled = (m_ui.SocketForwardComboBox->count() > 1);
	m_ui.SocketForwardTextLabel->setEnabled(bEnabled);
	m_ui.SocketForwardComboBox->setEnabled(bEnabled);
}


// Update client list if available.
void qjackctlSocketForm::clientNameChanged (void)
{
	if (m_ppPixmaps == NULL)
		return;
	if (m_pSocketList == NULL)
		return;

	m_ui.PlugNameComboBox->clear();

	QString sClientName = m_ui.ClientNameComboBox->currentText();
	if (sClientName.isEmpty())
		return;
	QRegExp rxClientName(sClientName);

	bool bReadable = m_pSocketList->isReadable();
	int iSocketType = m_pSocketTypeButtonGroup->checkedId();
	switch (iSocketType) {
	case 0: // QJACKCTL_SOCKETTYPE_AUDIO
		if (m_pJackClient) {
			const char **ppszClientPorts = jack_get_ports(m_pJackClient,
				0, JACK_DEFAULT_AUDIO_TYPE,
				(bReadable ? JackPortIsOutput : JackPortIsInput));
			if (ppszClientPorts) {
				int iClientPort = 0;
				while (ppszClientPorts[iClientPort]) {
					QString sClientPort = ppszClientPorts[iClientPort];
					int iColon = sClientPort.indexOf(':');
					if (iColon >= 0 && rxClientName.exactMatch(sClientPort.left(iColon))) {
						QString sPort
							= qjackctlClientAlias::escapeRegExpDigits(
								sClientPort.right(sClientPort.length() - iColon - 1));
						if (m_ui.PlugListView->findItems(sPort, Qt::MatchExactly).isEmpty())
							m_ui.PlugNameComboBox->addItem(
								QIcon(*m_ppPixmaps[QJACKCTL_XPM_AUDIO_PLUG]),
								sPort);
					}
					iClientPort++;
				}
				::free(ppszClientPorts);
			}
		}
		break;
	case 1: // QJACKCTL_SOCKETTYPE_MIDI
#ifdef CONFIG_ALSA_SEQ
		if (m_pAlsaSeq) {
			// Fill sequencer plugs...
			snd_seq_client_info_t *pClientInfo;
			snd_seq_port_info_t   *pPortInfo;
			unsigned int uiAlsaFlags;
			if (bReadable)
				uiAlsaFlags = SND_SEQ_PORT_CAP_READ  | SND_SEQ_PORT_CAP_SUBS_READ;
			else
				uiAlsaFlags = SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE;
			snd_seq_client_info_alloca(&pClientInfo);
			snd_seq_port_info_alloca(&pPortInfo);
			snd_seq_client_info_set_client(pClientInfo, -1);
			while (snd_seq_query_next_client(m_pAlsaSeq, pClientInfo) >= 0) {
				int iAlsaClient = snd_seq_client_info_get_client(pClientInfo);
				QString sClient = snd_seq_client_info_get_name(pClientInfo);
				if (iAlsaClient > 0 && rxClientName.exactMatch(sClient)) {
					snd_seq_port_info_set_client(pPortInfo, iAlsaClient);
					snd_seq_port_info_set_port(pPortInfo, -1);
					while (snd_seq_query_next_port(m_pAlsaSeq, pPortInfo) >= 0) {
						unsigned int uiPortCapability
							= snd_seq_port_info_get_capability(pPortInfo);
						if (((uiPortCapability & uiAlsaFlags) == uiAlsaFlags) &&
							((uiPortCapability & SND_SEQ_PORT_CAP_NO_EXPORT) == 0)) {
							QString sPort
								= qjackctlClientAlias::escapeRegExpDigits(
									snd_seq_port_info_get_name(pPortInfo));
							if (m_ui.PlugListView->findItems(sPort, Qt::MatchExactly).isEmpty())
								m_ui.PlugNameComboBox->addItem(
									QIcon(*m_ppPixmaps[QJACKCTL_XPM_MIDI_PLUG]),
									sPort);
						}
					}
				}
			}
		}
#endif	// CONFIG_ALSA_SEQ
		break;
	}

	stabilizeForm();
}


// end of qjackctlSocketForm.cpp