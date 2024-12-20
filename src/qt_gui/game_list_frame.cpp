// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/path_util.h"
#include "common/string_util.h"
#include "game_list_frame.h"

GameListFrame::GameListFrame(std::shared_ptr<GameInfoClass> game_info_get, QWidget* parent)
    : QTableWidget(parent), m_game_info(game_info_get) {
    icon_size = Config::getIconSize();
    this->setShowGrid(false);
    this->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->setSelectionMode(QAbstractItemView::SingleSelection);
    this->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    this->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    this->verticalScrollBar()->installEventFilter(this);
    this->verticalScrollBar()->setSingleStep(20);
    this->horizontalScrollBar()->setSingleStep(20);
    this->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    this->verticalHeader()->setVisible(false);
    this->horizontalHeader()->setContextMenuPolicy(Qt::CustomContextMenu);
    this->horizontalHeader()->setHighlightSections(false);
    this->horizontalHeader()->setSortIndicatorShown(true);
    this->horizontalHeader()->setStretchLastSection(true);
    this->setContextMenuPolicy(Qt::CustomContextMenu);
    this->setColumnCount(9);
    this->setColumnWidth(1, 300); // Name
    this->setColumnWidth(2, 120); // Serial
    this->setColumnWidth(3, 90);  // Region
    this->setColumnWidth(4, 90);  // Firmware
    this->setColumnWidth(5, 90);  // Size
    this->setColumnWidth(6, 90);  // Version
    this->setColumnWidth(7, 120); // Play Time
    QStringList headers;
    headers << tr("Icon") << tr("Name") << tr("Serial") << tr("Region") << tr("Firmware")
            << tr("Size") << tr("Version") << tr("Play Time") << tr("Path");
    this->setHorizontalHeaderLabels(headers);
    this->horizontalHeader()->setSortIndicatorShown(true);
    this->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    this->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    this->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    PopulateGameList();

    connect(this, &QTableWidget::currentCellChanged, this, &GameListFrame::onCurrentCellChanged);
    connect(this->verticalScrollBar(), &QScrollBar::valueChanged, this,
            &GameListFrame::RefreshListBackgroundImage);
    connect(this->horizontalScrollBar(), &QScrollBar::valueChanged, this,
            &GameListFrame::RefreshListBackgroundImage);

    this->horizontalHeader()->setSortIndicatorShown(true);
    this->horizontalHeader()->setSectionsClickable(true);
    QObject::connect(
        this->horizontalHeader(), &QHeaderView::sectionClicked, this, [this](int columnIndex) {
            if (ListSortedAsc) {
                SortNameDescending(columnIndex);
                this->horizontalHeader()->setSortIndicator(columnIndex, Qt::DescendingOrder);
                ListSortedAsc = false;
            } else {
                SortNameAscending(columnIndex);
                this->horizontalHeader()->setSortIndicator(columnIndex, Qt::AscendingOrder);
                ListSortedAsc = true;
            }
            this->clearContents();
            PopulateGameList();
        });

    connect(this, &QTableWidget::customContextMenuRequested, this, [=, this](const QPoint& pos) {
        m_gui_context_menus.RequestGameMenu(pos, m_game_info->m_games, this, true);
    });
}

void GameListFrame::onCurrentCellChanged(int currentRow, int currentColumn, int previousRow,
                                         int previousColumn) {
    QTableWidgetItem* item = this->item(currentRow, currentColumn);
    if (!item) {
        return;
    }
    SetListBackgroundImage(item);
    PlayBackgroundMusic(item);
}

void GameListFrame::PlayBackgroundMusic(QTableWidgetItem* item) {
    if (!item || !Config::getPlayBGM()) {
        BackgroundMusicPlayer::getInstance().stopMusic();
        return;
    }
    QString snd0path;
    Common::FS::PathToQString(snd0path, m_game_info->m_games[item->row()].snd0_path);
    BackgroundMusicPlayer::getInstance().playMusic(snd0path);
}

void GameListFrame::PopulateGameList() {
    this->setRowCount(m_game_info->m_games.size());
    ResizeIcons(icon_size);

    for (int i = 0; i < m_game_info->m_games.size(); i++) {
        SetTableItem(i, 1, QString::fromStdString(m_game_info->m_games[i].name));
        SetTableItem(i, 2, QString::fromStdString(m_game_info->m_games[i].serial));
        SetRegionFlag(i, 3, QString::fromStdString(m_game_info->m_games[i].region));
        SetTableItem(i, 4, QString::fromStdString(m_game_info->m_games[i].fw));
        SetTableItem(i, 5, QString::fromStdString(m_game_info->m_games[i].size));
        SetTableItem(i, 6, QString::fromStdString(m_game_info->m_games[i].version));

        QString playTime = GetPlayTime(m_game_info->m_games[i].serial);
        if (playTime.isEmpty()) {
            m_game_info->m_games[i].play_time = "0:00:00";
            SetTableItem(i, 7, tr("Never Played"));
        } else {
            QStringList timeParts = playTime.split(':');
            int hours = timeParts[0].toInt();
            int minutes = timeParts[1].toInt();
            int seconds = timeParts[2].toInt();

            QString formattedPlayTime;
            if (hours > 0) {
                formattedPlayTime += QString("%1h ").arg(hours);
            }
            if (minutes > 0) {
                formattedPlayTime += QString("%1m ").arg(minutes);
            }

            formattedPlayTime = formattedPlayTime.trimmed();
            m_game_info->m_games[i].play_time = playTime.toStdString();
            if (formattedPlayTime.isEmpty()) {
                SetTableItem(i, 7, QString("%1s").arg(seconds));
            } else {
                SetTableItem(i, 7, formattedPlayTime);
            }
        }

        QString path;
        Common::FS::PathToQString(path, m_game_info->m_games[i].path);
        SetTableItem(i, 8, path);
    }
}

void GameListFrame::SetListBackgroundImage(QTableWidgetItem* item) {
    if (!item) {
        // handle case where no item was clicked
        return;
    }

    QString pic1Path;
    Common::FS::PathToQString(pic1Path, m_game_info->m_games[item->row()].pic_path);
    const auto blurredPic1Path = Common::FS::GetUserPath(Common::FS::PathType::MetaDataDir) /
                                 m_game_info->m_games[item->row()].serial / "pic1.png";
    QString blurredPic1PathQt;
    Common::FS::PathToQString(blurredPic1PathQt, blurredPic1Path);

    backgroundImage = QImage(blurredPic1PathQt);
    if (backgroundImage.isNull()) {
        QImage image(pic1Path);
        backgroundImage = m_game_list_utils.BlurImage(image, image.rect(), 16);

        std::filesystem::path img_path =
            Common::FS::GetUserPath(Common::FS::PathType::MetaDataDir) /
            m_game_info->m_games[item->row()].serial;
        std::filesystem::create_directories(img_path);
        if (!backgroundImage.save(blurredPic1PathQt, "PNG")) {
            // qDebug() << "Error: Unable to save image.";
        }
    }
    RefreshListBackgroundImage();
}

void GameListFrame::RefreshListBackgroundImage() {
    if (!backgroundImage.isNull()) {
        QPalette palette;
        palette.setBrush(QPalette::Base,
                         QBrush(backgroundImage.scaled(size(), Qt::IgnoreAspectRatio)));
        QColor transparentColor = QColor(135, 206, 235, 40);
        palette.setColor(QPalette::Highlight, transparentColor);
        this->setPalette(palette);
    }
}

void GameListFrame::SortNameAscending(int columnIndex) {
    std::sort(m_game_info->m_games.begin(), m_game_info->m_games.end(),
              [columnIndex](const GameInfo& a, const GameInfo& b) {
                  return CompareStringsAscending(a, b, columnIndex);
              });
}

void GameListFrame::SortNameDescending(int columnIndex) {
    std::sort(m_game_info->m_games.begin(), m_game_info->m_games.end(),
              [columnIndex](const GameInfo& a, const GameInfo& b) {
                  return CompareStringsDescending(a, b, columnIndex);
              });
}

void GameListFrame::ResizeIcons(int iconSize) {
    for (int index = 0; auto& game : m_game_info->m_games) {
        QImage scaledPixmap = game.icon.scaled(QSize(iconSize, iconSize), Qt::KeepAspectRatio,
                                               Qt::SmoothTransformation);
        QTableWidgetItem* iconItem = new QTableWidgetItem();
        this->verticalHeader()->resizeSection(index, scaledPixmap.height());
        this->horizontalHeader()->resizeSection(0, scaledPixmap.width());
        iconItem->setData(Qt::DecorationRole, scaledPixmap);
        this->setItem(index, 0, iconItem);
        index++;
    }
    this->horizontalHeader()->setSectionResizeMode(8, QHeaderView::ResizeToContents);
}

void GameListFrame::SetTableItem(int row, int column, QString itemStr) {
    QTableWidgetItem* item = new QTableWidgetItem();
    QWidget* widget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(widget);
    QLabel* label = new QLabel(itemStr, widget);

    label->setStyleSheet("color: white; font-size: 16px; font-weight: bold;");

    // Create shadow effect
    QGraphicsDropShadowEffect* shadowEffect = new QGraphicsDropShadowEffect();
    shadowEffect->setBlurRadius(5);               // Set the blur radius of the shadow
    shadowEffect->setColor(QColor(0, 0, 0, 160)); // Set the color and opacity of the shadow
    shadowEffect->setOffset(2, 2);                // Set the offset of the shadow

    label->setGraphicsEffect(shadowEffect); // Apply shadow effect to the QLabel

    layout->addWidget(label);
    if (column != 8 && column != 1)
        layout->setAlignment(Qt::AlignCenter);
    widget->setLayout(layout);
    this->setItem(row, column, item);
    this->setCellWidget(row, column, widget);
}

void GameListFrame::SetRegionFlag(int row, int column, QString itemStr) {
    QTableWidgetItem* item = new QTableWidgetItem();
    QImage scaledPixmap;
    if (itemStr == "Japan") {
        scaledPixmap = QImage(":images/flag_jp.png");
    } else if (itemStr == "Europe") {
        scaledPixmap = QImage(":images/flag_eu.png");
    } else if (itemStr == "USA") {
        scaledPixmap = QImage(":images/flag_us.png");
    } else if (itemStr == "Asia") {
        scaledPixmap = QImage(":images/flag_china.png");
    } else if (itemStr == "World") {
        scaledPixmap = QImage(":images/flag_world.png");
    } else {
        scaledPixmap = QImage(":images/flag_unk.png");
    }
    QWidget* widget = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(widget);
    QLabel* label = new QLabel(widget);
    label->setPixmap(QPixmap::fromImage(scaledPixmap));
    layout->setAlignment(Qt::AlignCenter);
    layout->addWidget(label);
    widget->setLayout(layout);
    this->setItem(row, column, item);
    this->setCellWidget(row, column, widget);
}

QString GameListFrame::GetPlayTime(const std::string& serial) {
    QString playTime;
    const auto user_dir = Common::FS::GetUserPath(Common::FS::PathType::UserDir);
    QString filePath = QString::fromStdString((user_dir / "play_time.txt").string());

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return playTime;
    }

    while (!file.atEnd()) {
        QByteArray line = file.readLine();
        QString lineStr = QString::fromUtf8(line).trimmed();

        QStringList parts = lineStr.split(' ');
        if (parts.size() >= 2) {
            QString fileSerial = parts[0];
            QString time = parts[1];

            if (fileSerial == QString::fromStdString(serial)) {
                playTime = time;
                break;
            }
        }
    }

    file.close();
    return playTime;
}
