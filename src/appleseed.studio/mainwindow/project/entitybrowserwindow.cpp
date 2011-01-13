
//
// This source file is part of appleseed.
// Visit http://appleseedhq.net/ for additional information and resources.
//
// This software is released under the MIT license.
//
// Copyright (c) 2010-2011 Francois Beaune
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//

// Interface header.
#include "entitybrowserwindow.h"

// UI definition header.
#include "ui_entitybrowserwindow.h"

// appleseed.studio headers.
#include "utility/interop.h"
#include "utility/tweaks.h"

// appleseed.foundation headers.
#include "foundation/utility/containers/dictionary.h"
#include "foundation/utility/foreach.h"

// Qt headers.
#include <QDialogButtonBox>
#include <QList>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QShortcut>
#include <Qt>

using namespace foundation;
using namespace std;

namespace appleseed {
namespace studio {

EntityBrowserWindow::EntityBrowserWindow(
    QWidget*                parent,
    const string&           window_title)
  : QWidget(parent)
  , m_ui(new Ui::EntityBrowserWindow())
{
    m_ui->setupUi(this);

    setAttribute(Qt::WA_DeleteOnClose);
    setWindowFlags(Qt::Tool);
    setWindowTitle(QString::fromStdString(window_title));

    resize(400, 300);

    m_ui->buttonbox->button(QDialogButtonBox::Ok)->setEnabled(false);

    connect(
        m_ui->tab_widget, SIGNAL(currentChanged(int)),
        this, SLOT(slot_current_tab_changed(int)));

    connect(
        m_ui->buttonbox->button(QDialogButtonBox::Ok), SIGNAL(clicked()),
        this, SLOT(slot_accept()));

    connect(
        m_ui->buttonbox->button(QDialogButtonBox::Cancel), SIGNAL(clicked()),
        this, SLOT(close()));

    connect(
        create_window_local_shortcut(this, Qt::Key_Return), SIGNAL(activated()),
        this, SLOT(slot_accept()));

    connect(
        create_window_local_shortcut(this, Qt::Key_Escape), SIGNAL(activated()),
        this, SLOT(close()));
}

EntityBrowserWindow::~EntityBrowserWindow()
{
    delete m_ui;
}

namespace
{
    void add_items_to_list_widget(QListWidget* list_widget, const StringDictionary& items)
    {
        for (const_each<StringDictionary> i = items; i; ++i)
        {
            const QString item_label = i->name();
            const QString item_value = i->value<QString>();

            QListWidgetItem* item = new QListWidgetItem(item_label, list_widget);
            item->setData(0, item_value);
        }
    }
}

void EntityBrowserWindow::add_items_page(
    const string&           page_name,
    const string&           page_label,
    const StringDictionary& items)
{
    QWidget* tab = new QWidget(m_ui->tab_widget);
    QGridLayout* layout = new QGridLayout(tab);

    QListWidget* list_widget = new QListWidget(tab);
    layout->addWidget(list_widget, 0, 0, 1, 1);

    disable_mac_focus_rect(*list_widget);

    add_items_to_list_widget(list_widget, items);

    connect(
        list_widget, SIGNAL(itemSelectionChanged()),
        this, SLOT(slot_item_selection_changed()));

    connect(
        list_widget, SIGNAL(itemActivated(QListWidgetItem*)),
        this, SLOT(slot_item_activated(QListWidgetItem*)));

    const bool were_signals_blocked = m_ui->tab_widget->blockSignals(true);
    const int tab_index = m_ui->tab_widget->addTab(tab, QString::fromStdString(page_label));
    m_ui->tab_widget->blockSignals(were_signals_blocked);

    Page page;
    page.m_page_name = page_name;
    page.m_list_widget = list_widget;
    m_pages[tab_index] = page;
}

void EntityBrowserWindow::slot_current_tab_changed(int tab_index)
{
    slot_item_selection_changed();
}

void EntityBrowserWindow::slot_item_selection_changed()
{
    const Page& page = m_pages[m_ui->tab_widget->currentIndex()];
    const bool item_selected = page.m_list_widget->selectedItems().size() == 1;

    m_ui->buttonbox->button(QDialogButtonBox::Ok)->setEnabled(item_selected);
}

void EntityBrowserWindow::slot_item_activated(QListWidgetItem* item)
{
    const Page& page = m_pages[m_ui->tab_widget->currentIndex()];

    emit signal_accepted(QString::fromStdString(page.m_page_name), item->data(0).toString());
}

void EntityBrowserWindow::slot_accept()
{
    const Page& page = m_pages[m_ui->tab_widget->currentIndex()];
    const QList<QListWidgetItem*> selected_items = page.m_list_widget->selectedItems();

    if (selected_items.size() == 1)
        slot_item_activated(selected_items.first());
}

}   // namespace studio
}   // namespace appleseed
