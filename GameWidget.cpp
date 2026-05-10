#include "GameWidget.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFont>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QSizeF>
#include <QStringList>
#include <QtMath>

#include <algorithm>

namespace {
constexpr double Gravity = 1750.0;
constexpr double JumpSpeed = -735.0;
constexpr double MoveAcceleration = 1750.0;
constexpr double AirMoveAcceleration = 1120.0;
constexpr double MaxHorizontalSpeed = 480.0;
constexpr double FallBoost = 2600.0;
constexpr double GroundFriction = 0.82;
constexpr double AirFriction = 0.96;
constexpr QSizeF PlayerSize(74.0, 96.0);

double randomRange(double min, double max)
{
    return min + (max - min) * QRandomGenerator::global()->generateDouble();
}

bool loadAsset(QPixmap &pixmap, const QString &fileName)
{
    const QStringList roots{
        QDir::currentPath(),
        QCoreApplication::applicationDirPath(),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("..")),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../..")),
        QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("../../.."))
    };

    for (const QString &root : roots) {
        const QString path = QDir(root).absoluteFilePath(QStringLiteral("assets/%1").arg(fileName));
        if (pixmap.load(path)) {
            return true;
        }
    }
    return false;
}
}

GameWidget::GameWidget(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMinimumSize(1100, 620);
    loadSprites();

    QSettings settings(QStringLiteral("CodexQtGame"), QStringLiteral("JamesRunner"));
    m_highScore = settings.value(QStringLiteral("highScore"), 0).toInt();

    connect(&m_timer, &QTimer::timeout, this, &GameWidget::tick);
    m_clock.start();
    m_timer.start(16);
}

void GameWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    if (m_screen == Screen::MainMenu) {
        drawMainMenu(painter);
        return;
    }

    if (m_screen == Screen::MapSelect) {
        drawMapSelect(painter);
        return;
    }

    drawGame(painter);

    if (m_screen == Screen::Paused) {
        drawPausedOverlay(painter);
    } else if (m_screen == Screen::GameOver) {
        drawGameOver(painter);
    }
}

void GameWidget::keyPressEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        event->ignore();
        return;
    }

    const int key = event->key();
    if (key == Qt::Key_Escape && (m_screen == Screen::Playing || m_screen == Screen::Paused)) {
        m_screen = (m_screen == Screen::Playing) ? Screen::Paused : Screen::Playing;
        update();
        return;
    }

    if (m_screen == Screen::MainMenu) {
        if (key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Space) {
            m_screen = Screen::MapSelect;
            update();
        } else if (key == Qt::Key_Escape) {
            QApplication::quit();
        }
        return;
    }

    if (m_screen == Screen::MapSelect) {
        if (key == Qt::Key_1) {
            startGame(MapKind::Court);
        } else if (key == Qt::Key_2) {
            startGame(MapKind::City);
        } else if (key == Qt::Key_3) {
            startGame(MapKind::Space);
        } else if (key == Qt::Key_B || key == Qt::Key_Escape) {
            m_screen = Screen::MainMenu;
            update();
        }
        return;
    }

    if (m_screen == Screen::GameOver) {
        if (key == Qt::Key_R) {
            startGame(m_map);
        } else if (key == Qt::Key_B) {
            m_screen = Screen::MainMenu;
            update();
        }
        return;
    }

    if (m_screen != Screen::Playing) {
        return;
    }

    if (key == Qt::Key_W && m_jumpsRemaining > 0) {
        m_playerVelocity.setY(JumpSpeed);
        --m_jumpsRemaining;
    } else if (key == Qt::Key_A) {
        m_leftPressed = true;
    } else if (key == Qt::Key_D) {
        m_rightPressed = true;
    } else if (key == Qt::Key_S) {
        m_downPressed = true;
    }
}

void GameWidget::keyReleaseEvent(QKeyEvent *event)
{
    if (event->isAutoRepeat()) {
        event->ignore();
        return;
    }

    if (event->key() == Qt::Key_A) {
        m_leftPressed = false;
    } else if (event->key() == Qt::Key_D) {
        m_rightPressed = false;
    } else if (event->key() == Qt::Key_S) {
        m_downPressed = false;
    }
}

void GameWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_screen == Screen::MainMenu && menuButtonRect(0).contains(event->pos())) {
        m_screen = Screen::MapSelect;
        update();
        return;
    }

    if (m_screen == Screen::MapSelect) {
        for (int i = 0; i < 3; ++i) {
            if (mapCardRect(i).contains(event->pos())) {
                startGame(static_cast<MapKind>(i));
                return;
            }
        }
    }
}

void GameWidget::tick()
{
    const double dt = qMin(m_clock.restart() / 1000.0, 0.033);
    if (m_screen == Screen::Playing) {
        updatePlayer(dt);
        spawnEntities(dt);
        updateEntities(dt);
        handleCollisions();

        m_worldSpeed = qMin(470.0, 250.0 + m_score * 3.2);
        if (m_invincibleTimer > 0.0) {
            m_invincibleTimer = qMax(0.0, m_invincibleTimer - dt);
        }
        if (m_healDelay > 0.0) {
            m_healDelay = qMax(0.0, m_healDelay - dt);
        } else {
            m_health = qMin(100.0, m_health + 5.5 * dt);
        }
    } else {
        m_clock.restart();
    }

    update();
}

void GameWidget::resetGame()
{
    m_entities.clear();
    m_playerPos = QPointF(width() * 0.22, groundRect().top() - PlayerSize.height());
    m_playerVelocity = QPointF(0.0, 0.0);
    m_leftPressed = false;
    m_rightPressed = false;
    m_downPressed = false;
    m_jumpsRemaining = 2;
    m_health = 100.0;
    m_score = 0;
    m_recordBroken = false;
    m_spawnTimer = 0.65;
    m_ballTimer = 0.25;
    m_flyingTimer = 2.0;
    m_rewardTimer = 5.5;
    m_invincibleTimer = 0.0;
    m_healDelay = 0.0;
    m_worldSpeed = 250.0;
}

void GameWidget::startGame(MapKind map)
{
    m_map = map;
    resetGame();
    m_screen = Screen::Playing;
    m_clock.restart();
    update();
}

void GameWidget::loadSprites()
{
    loadAsset(m_playerSprite, QStringLiteral("james.png"));
    loadAsset(m_ballSprite, QStringLiteral("basketball.png"));
    loadAsset(m_groundObstacleSprite, QStringLiteral("obstacle.png"));
    loadAsset(m_flyingObstacleSprite, QStringLiteral("flying_obstacle.png"));
    loadAsset(m_rewardSprite, QStringLiteral("reward.png"));
    loadAsset(m_courtBackground, QStringLiteral("图片4.png"));
    loadAsset(m_cityBackground, QStringLiteral("map2_background.png"));
    loadAsset(m_spaceBackground, QStringLiteral("map3_background.png"));
    loadAsset(m_menuBackground, QStringLiteral("menu_background.jpg"));
    createFallbackSprites();
}

void GameWidget::createFallbackSprites()
{
    if (m_playerSprite.isNull()) {
        QPixmap pixmap(148, 192);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(96, 46, 22));
        p.drawEllipse(QRectF(52, 8, 44, 44));
        p.setBrush(QColor(92, 38, 132));
        p.drawRoundedRect(QRectF(38, 54, 72, 88), 10, 10);
        p.setBrush(QColor(245, 196, 62));
        p.drawRect(QRectF(40, 77, 68, 10));
        p.setPen(QPen(Qt::white, 6));
        p.setFont(QFont(QStringLiteral("Arial"), 18, QFont::Bold));
        p.drawText(QRectF(38, 66, 72, 50), Qt::AlignCenter, QStringLiteral("23"));
        p.setPen(QPen(QColor(96, 46, 22), 16, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(35, 66), QPointF(12, 116));
        p.drawLine(QPointF(113, 66), QPointF(135, 112));
        p.setPen(QPen(QColor(35, 31, 32), 18, Qt::SolidLine, Qt::RoundCap));
        p.drawLine(QPointF(58, 140), QPointF(46, 184));
        p.drawLine(QPointF(90, 140), QPointF(102, 184));
        p.end();
        m_playerSprite = pixmap;
    }

    if (m_ballSprite.isNull()) {
        QPixmap pixmap(64, 64);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(219, 112, 36));
        p.setPen(QPen(QColor(78, 40, 20), 3));
        p.drawEllipse(QRectF(6, 6, 52, 52));
        p.drawArc(QRectF(14, 6, 36, 52), 90 * 16, 180 * 16);
        p.drawArc(QRectF(14, 6, 36, 52), -90 * 16, 180 * 16);
        p.drawLine(QPointF(8, 32), QPointF(56, 32));
        p.drawArc(QRectF(6, 18, 52, 28), 0, 180 * 16);
        p.drawArc(QRectF(6, 18, 52, 28), 180 * 16, 180 * 16);
        p.end();
        m_ballSprite = pixmap;
    }

    if (m_groundObstacleSprite.isNull()) {
        QPixmap pixmap(80, 80);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        QPainterPath cone;
        cone.moveTo(40, 8);
        cone.lineTo(68, 70);
        cone.lineTo(12, 70);
        cone.closeSubpath();
        p.fillPath(cone, QColor(228, 82, 39));
        p.setPen(QPen(Qt::white, 7));
        p.drawLine(QPointF(28, 44), QPointF(53, 44));
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(44, 44, 44));
        p.drawRoundedRect(QRectF(8, 68, 64, 8), 3, 3);
        p.end();
        m_groundObstacleSprite = pixmap;
    }

    if (m_flyingObstacleSprite.isNull()) {
        QPixmap pixmap(92, 58);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setBrush(QColor(52, 66, 86));
        p.setPen(QPen(QColor(230, 236, 242), 3));
        p.drawRoundedRect(QRectF(10, 14, 72, 30), 10, 10);
        p.drawEllipse(QRectF(5, 9, 18, 18));
        p.drawEllipse(QRectF(69, 31, 18, 18));
        p.setBrush(QColor(255, 207, 82));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QRectF(39, 23, 14, 14));
        p.end();
        m_flyingObstacleSprite = pixmap;
    }

    if (m_rewardSprite.isNull()) {
        QPixmap pixmap(64, 64);
        pixmap.fill(Qt::transparent);
        QPainter p(&pixmap);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(64, 194, 125));
        p.drawEllipse(QRectF(7, 7, 50, 50));
        p.setBrush(Qt::white);
        p.drawRect(QRectF(29, 18, 6, 28));
        p.drawRect(QRectF(18, 29, 28, 6));
        p.end();
        m_rewardSprite = pixmap;
    }
}

void GameWidget::updatePlayer(double dt)
{
    const bool onGround = playerRect().bottom() >= groundRect().top() - 0.5;
    const double acceleration = onGround ? MoveAcceleration : AirMoveAcceleration;

    if (m_leftPressed) {
        m_playerVelocity.rx() -= acceleration * dt;
    }
    if (m_rightPressed) {
        m_playerVelocity.rx() += acceleration * dt;
    }
    if (!m_leftPressed && !m_rightPressed) {
        m_playerVelocity.rx() *= onGround ? GroundFriction : AirFriction;
        if (qAbs(m_playerVelocity.x()) < 5.0) {
            m_playerVelocity.setX(0.0);
        }
    }

    m_playerVelocity.setX(qBound(-MaxHorizontalSpeed, m_playerVelocity.x(), MaxHorizontalSpeed));
    m_playerVelocity.ry() += Gravity * dt;
    if (m_downPressed && !onGround) {
        m_playerVelocity.ry() += FallBoost * dt;
    }

    m_playerPos += m_playerVelocity * dt;

    const QRectF ground = groundRect();
    if (playerRect().bottom() >= ground.top()) {
        m_playerPos.setY(ground.top() - PlayerSize.height());
        m_playerVelocity.setY(0.0);
        m_jumpsRemaining = 2;
    }

    const double minX = 18.0;
    const double maxX = width() - PlayerSize.width() - 18.0;
    m_playerPos.setX(qBound(minX, m_playerPos.x(), maxX));
}

void GameWidget::updateEntities(double dt)
{
    for (Entity &entity : m_entities) {
        entity.rect.translate(entity.velocity * dt);
        if (entity.rect.right() < -120.0) {
            entity.active = false;
        }
    }

    m_entities.erase(std::remove_if(m_entities.begin(), m_entities.end(), [](const Entity &entity) {
        return !entity.active;
    }), m_entities.end());
}

void GameWidget::spawnEntities(double dt)
{
    m_spawnTimer -= dt;
    m_ballTimer -= dt;
    m_flyingTimer -= dt;
    m_rewardTimer -= dt;

    const QRectF ground = groundRect();
    if (m_spawnTimer <= 0.0) {
        const int variant = QRandomGenerator::global()->bounded(4);
        double obstacleWidth = 64.0;
        double obstacleHeight = 64.0;
        double obstacleBottomOffset = 0.0;

        if (variant == 0) {
            obstacleWidth = randomRange(38.0, 56.0);
            obstacleHeight = randomRange(38.0, 58.0);
        } else if (variant == 1) {
            obstacleWidth = randomRange(48.0, 70.0);
            obstacleHeight = randomRange(68.0, 96.0);
        } else if (variant == 2) {
            obstacleWidth = randomRange(70.0, 104.0);
            obstacleHeight = randomRange(34.0, 52.0);
        } else {
            obstacleWidth = randomRange(40.0, 62.0);
            obstacleHeight = randomRange(40.0, 66.0);
            obstacleBottomOffset = randomRange(46.0, 118.0);
        }

        Entity obstacle;
        obstacle.kind = EntityKind::GroundObstacle;
        obstacle.rect = QRectF(width() + 40.0,
                               ground.top() - obstacleBottomOffset - obstacleHeight,
                               obstacleWidth,
                               obstacleHeight);
        obstacle.velocity = QPointF(-m_worldSpeed, 0.0);
        m_entities.push_back(obstacle);
        m_spawnTimer = randomRange(0.45, 0.95);
    }

    if (m_ballTimer <= 0.0) {
        Entity ball;
        ball.kind = EntityKind::Ball;
        const double y = randomRange(ground.top() - 245.0, ground.top() - 90.0);
        ball.rect = QRectF(width() + randomRange(30.0, 180.0), y, 38.0, 38.0);
        ball.velocity = QPointF(-m_worldSpeed * 0.93, 0.0);
        m_entities.push_back(ball);
        m_ballTimer = randomRange(0.75, 1.25);
    }

    if (m_flyingTimer <= 0.0) {
        Entity flying;
        flying.kind = EntityKind::FlyingObstacle;
        const double flyingWidth = randomRange(48.0, 86.0);
        const double flyingHeight = randomRange(28.0, 54.0);
        const double flyingY = randomRange(90.0, ground.top() - 210.0);
        flying.rect = QRectF(width() + 70.0, flyingY, flyingWidth, flyingHeight);
        flying.velocity = QPointF(-m_worldSpeed * randomRange(1.16, 1.42), randomRange(-30.0, 30.0));
        m_entities.push_back(flying);
        m_flyingTimer = randomRange(1.05, 2.15);
    }

    if (m_rewardTimer <= 0.0) {
        Entity reward;
        reward.kind = EntityKind::Reward;
        const int rewardLane = QRandomGenerator::global()->bounded(4);
        const double rewardSize = randomRange(34.0, 58.0);
        double rewardY = ground.top() - 130.0;
        if (rewardLane == 0) {
            rewardY = ground.top() - randomRange(105.0, 150.0);
        } else if (rewardLane == 1) {
            rewardY = ground.top() - randomRange(185.0, 250.0);
        } else if (rewardLane == 2) {
            rewardY = ground.top() - randomRange(285.0, 360.0);
        } else {
            rewardY = randomRange(92.0, ground.top() - 390.0);
        }
        reward.rect = QRectF(width() + randomRange(90.0, 250.0), rewardY, rewardSize, rewardSize);
        reward.velocity = QPointF(-m_worldSpeed * randomRange(0.78, 0.98), 0.0);
        m_entities.push_back(reward);
        m_rewardTimer = randomRange(6.0, 9.0);
    }
}

void GameWidget::handleCollisions()
{
    const QRectF player = playerRect().adjusted(10.0, 8.0, -10.0, -4.0);
    for (Entity &entity : m_entities) {
        if (!entity.active || !player.intersects(entity.rect.adjusted(4.0, 4.0, -4.0, -4.0))) {
            continue;
        }

        if (entity.kind == EntityKind::Ball) {
            m_score += 2;
            entity.active = false;
        } else if (entity.kind == EntityKind::Reward) {
            m_score += 5;
            m_health = qMin(100.0, m_health + 18.0);
            entity.active = false;
        } else if (m_invincibleTimer <= 0.0) {
            addDamage(entity.kind == EntityKind::FlyingObstacle ? 20 : 14);
            m_invincibleTimer = 0.75;
            entity.active = false;
        }
    }
}

void GameWidget::addDamage(int amount)
{
    m_health = qMax(0.0, m_health - amount);
    m_score = qMax(0, m_score - (amount >= 20 ? 3 : 1));
    m_healDelay = 2.0;
    if (m_health <= 0.0) {
        finishGame();
    }
}

void GameWidget::finishGame()
{
    if (m_score > m_highScore) {
        m_highScore = m_score;
        m_recordBroken = true;
        QSettings settings(QStringLiteral("CodexQtGame"), QStringLiteral("JamesRunner"));
        settings.setValue(QStringLiteral("highScore"), m_highScore);
    }
    m_screen = Screen::GameOver;
}

void GameWidget::drawBackground(QPainter &painter)
{
    QLinearGradient gradient(rect().topLeft(), rect().bottomLeft());
    gradient.setColorAt(0.0, mapColorA());
    gradient.setColorAt(1.0, mapColorB());
    painter.fillRect(rect(), gradient);

    painter.setPen(Qt::NoPen);
    const QRectF ground = groundRect();

    if (m_map == MapKind::Court) {
        if (!m_courtBackground.isNull()) {
            painter.drawPixmap(rect(), m_courtBackground);
            return;
        }

        painter.setBrush(QColor(246, 184, 84));
        painter.drawRect(ground);
        painter.setPen(QPen(QColor(120, 68, 28, 130), 4));
        painter.drawLine(QPointF(0, ground.top() + 18), QPointF(width(), ground.top() + 18));
        painter.setPen(QPen(Qt::white, 3));
        painter.drawEllipse(QRectF(width() * 0.58, ground.top() - 38, 86, 86));
    } else if (m_map == MapKind::City) {
        if (!m_cityBackground.isNull()) {
            painter.drawPixmap(rect(), m_cityBackground);
            return;
        }

        painter.setBrush(QColor(58, 64, 72));
        painter.drawRect(ground);
        painter.setBrush(QColor(35, 41, 48, 170));
        for (int i = 0; i < 9; ++i) {
            const double w = width() / 9.0;
            const double h = 80.0 + (i % 4) * 35.0;
            painter.drawRect(QRectF(i * w, ground.top() - h, w - 8.0, h));
        }
        painter.setPen(QPen(QColor(236, 203, 91), 4, Qt::DashLine));
        painter.drawLine(QPointF(0, ground.center().y()), QPointF(width(), ground.center().y()));
    } else {
        if (!m_spaceBackground.isNull()) {
            painter.drawPixmap(rect(), m_spaceBackground);
            return;
        }

        painter.setBrush(QColor(36, 36, 64));
        painter.drawRect(ground);
        painter.setPen(QPen(QColor(114, 230, 215, 130), 2));
        for (int i = 0; i < 14; ++i) {
            painter.drawPoint(QPointF(randomRange(0, width()), randomRange(0, ground.top() - 40)));
        }
        painter.setBrush(QColor(118, 101, 180));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(width() * 0.72, 70, 84, 84));
    }
}

void GameWidget::drawMainMenu(QPainter &painter)
{
    m_map = MapKind::Court;
    if (m_menuBackground.isNull()) {
        drawBackground(painter);
    } else {
        painter.drawPixmap(rect(), m_menuBackground);
    }
    painter.fillRect(rect(), QColor(10, 12, 18, 95));

    painter.setPen(Qt::white);
    painter.setFont(QFont(QStringLiteral("Arial"), 48, QFont::Black));
    painter.drawText(QRectF(0, height() * 0.15, width(), 70), Qt::AlignCenter, QStringLiteral("James Runner"));

    painter.setFont(QFont(QStringLiteral("Arial"), 17, QFont::Medium));
    painter.drawText(QRectF(width() * 0.18, height() * 0.31, width() * 0.64, 72),
                     Qt::AlignCenter | Qt::TextWordWrap,
                     QStringLiteral("W 二段跳  S 快速下落  A/D 加速移动  ESC 暂停"));

    const QRectF button = menuButtonRect(0);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(245, 196, 62));
    painter.drawRoundedRect(button, 8, 8);
    painter.setPen(QColor(28, 24, 20));
    drawCenteredText(painter, button, QStringLiteral("开始游戏"), 22, true);

    painter.setPen(QColor(245, 245, 245));
    painter.setFont(QFont(QStringLiteral("Arial"), 15, QFont::Normal));
    painter.drawText(QRectF(0, button.bottom() + 28, width(), 32), Qt::AlignCenter,
                     QStringLiteral("最高纪录：%1").arg(m_highScore));
}

void GameWidget::drawMapSelect(QPainter &painter)
{
    painter.fillRect(rect(), QColor(83, 35, 150));

    painter.setPen(QColor(24, 27, 34));
    painter.setFont(QFont(QStringLiteral("Arial"), 32, QFont::Black));
    painter.drawText(QRectF(0, 52, width(), 52), Qt::AlignCenter, QStringLiteral("选择地图"));

    const QStringList descriptions{
        QStringLiteral("主场作战"),
        QStringLiteral("宿命对决"),
        QStringLiteral("对阵家乡")
    };
    for (int i = 0; i < 3; ++i) {
        const QRectF card = mapCardRect(i);
        const MapKind map = static_cast<MapKind>(i);
        QLinearGradient fill(card.topLeft(), card.bottomRight());
        const MapKind oldMap = m_map;
        m_map = map;
        fill.setColorAt(0.0, mapColorA());
        fill.setColorAt(1.0, mapColorB());
        m_map = oldMap;

        painter.setPen(QPen(QColor(255, 255, 255, 210), 2));
        painter.setBrush(fill);
        painter.drawRoundedRect(card, 8, 8);

        painter.setPen(Qt::white);
        drawCenteredText(painter, QRectF(card.left(), card.top() + 32, card.width(), 42),
                         QStringLiteral("%1").arg(i + 1), 28, true);
        drawCenteredText(painter, QRectF(card.left(), card.top() + 96, card.width(), 42), mapName(map), 24, true);
        painter.setFont(QFont(QStringLiteral("Arial"), 14, QFont::Normal));
        painter.drawText(QRectF(card.left() + 14, card.bottom() - 58, card.width() - 28, 38),
                         Qt::AlignCenter | Qt::TextWordWrap, descriptions.value(i));
    }

    painter.setPen(QColor(24, 27, 34));
    painter.setFont(QFont(QStringLiteral("Arial"), 14));
    painter.drawText(QRectF(0, height() - 48, width(), 24), Qt::AlignCenter,
                     QStringLiteral("按 1/2/3 或点击地图进入，B 返回主菜单"));
}

void GameWidget::drawGame(QPainter &painter)
{
    drawBackground(painter);

    for (const Entity &entity : m_entities) {
        const QPixmap *sprite = nullptr;
        if (entity.kind == EntityKind::Ball) {
            sprite = &m_ballSprite;
        } else if (entity.kind == EntityKind::GroundObstacle) {
            sprite = &m_groundObstacleSprite;
        } else if (entity.kind == EntityKind::FlyingObstacle) {
            sprite = &m_flyingObstacleSprite;
        } else {
            sprite = &m_rewardSprite;
        }
        painter.drawPixmap(entity.rect.toRect(), *sprite);
    }

    if (m_invincibleTimer <= 0.0 || qSin(m_invincibleTimer * 30.0) > 0.0) {
        painter.drawPixmap(playerRect().toRect(), m_playerSprite);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(25, 28, 34, 190));
    painter.drawRoundedRect(QRectF(18, 16, 258, 86), 8, 8);

    painter.setPen(Qt::white);
    painter.setFont(QFont(QStringLiteral("Arial"), 15, QFont::Bold));
    painter.drawText(QRectF(34, 26, 210, 24), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("分数 %1").arg(m_score));
    painter.drawText(QRectF(34, 54, 210, 24), Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("最高 %1").arg(m_highScore));

    const QRectF hpBack(34, 82, 206, 10);
    painter.setBrush(QColor(85, 38, 44));
    painter.drawRoundedRect(hpBack, 5, 5);
    painter.setBrush(QColor(72, 207, 119));
    painter.drawRoundedRect(QRectF(hpBack.left(), hpBack.top(), hpBack.width() * (m_health / 100.0), hpBack.height()), 5, 5);

    painter.setPen(QColor(255, 255, 255, 210));
    painter.setFont(QFont(QStringLiteral("Arial"), 13));
    painter.drawText(QRectF(width() - 270, 20, 252, 26), Qt::AlignRight, QStringLiteral("ESC 暂停"));
}

void GameWidget::drawPausedOverlay(QPainter &painter)
{
    painter.fillRect(rect(), QColor(0, 0, 0, 145));
    painter.setPen(Qt::white);
    drawCenteredText(painter, QRectF(0, height() * 0.36, width(), 58), QStringLiteral("游戏暂停"), 34, true);
    painter.setFont(QFont(QStringLiteral("Arial"), 16));
    painter.drawText(QRectF(0, height() * 0.48, width(), 36), Qt::AlignCenter, QStringLiteral("再次按 ESC 继续"));
}

void GameWidget::drawGameOver(QPainter &painter)
{
    painter.fillRect(rect(), QColor(0, 0, 0, 165));
    painter.setPen(Qt::white);
    drawCenteredText(painter, QRectF(0, height() * 0.25, width(), 62), QStringLiteral("游戏结束"), 36, true);

    painter.setFont(QFont(QStringLiteral("Arial"), 18, QFont::Bold));
    painter.drawText(QRectF(0, height() * 0.40, width(), 36), Qt::AlignCenter,
                     QStringLiteral("本局分数：%1   最高纪录：%2").arg(m_score).arg(m_highScore));
    if (m_recordBroken) {
        painter.setPen(QColor(245, 196, 62));
        painter.drawText(QRectF(0, height() * 0.48, width(), 34), Qt::AlignCenter, QStringLiteral("新纪录已自动保存"));
        painter.setPen(Qt::white);
    }
    painter.setFont(QFont(QStringLiteral("Arial"), 16));
    painter.drawText(QRectF(0, height() * 0.60, width(), 38), Qt::AlignCenter,
                     QStringLiteral("按 R 重新开始，按 B 返回主菜单"));
}

void GameWidget::drawCenteredText(QPainter &painter, const QRectF &rect, const QString &text, int size, bool bold)
{
    QFont font(QStringLiteral("Arial"), size, bold ? QFont::Bold : QFont::Normal);
    painter.setFont(font);
    painter.drawText(rect, Qt::AlignCenter | Qt::TextWordWrap, text);
}

QRectF GameWidget::groundRect() const
{
    return QRectF(0.0, height() - 96.0, width(), 96.0);
}

QRectF GameWidget::playerRect() const
{
    return QRectF(m_playerPos, PlayerSize);
}

QRectF GameWidget::menuButtonRect(int) const
{
    return QRectF(width() / 2.0 - 116.0, height() * 0.52, 232.0, 56.0);
}

QRectF GameWidget::mapCardRect(int index) const
{
    const double cardWidth = qMin(240.0, width() * 0.26);
    const double gap = qMax(20.0, width() * 0.035);
    const double total = cardWidth * 3.0 + gap * 2.0;
    const double left = (width() - total) / 2.0 + index * (cardWidth + gap);
    return QRectF(left, height() * 0.28, cardWidth, height() * 0.42);
}

QString GameWidget::mapName(MapKind map) const
{
    switch (map) {
    case MapKind::Court:
        return QStringLiteral("洛杉矶湖人");
    case MapKind::City:
        return QStringLiteral("金州勇士");
    case MapKind::Space:
        return QStringLiteral("克利夫兰骑士队");
    }
    return {};
}

QColor GameWidget::mapColorA() const
{
    switch (m_map) {
    case MapKind::Court:
        return QColor(83, 55, 136);
    case MapKind::City:
        return QColor(54, 132, 166);
    case MapKind::Space:
        return QColor(22, 24, 54);
    }
    return QColor(83, 55, 136);
}

QColor GameWidget::mapColorB() const
{
    switch (m_map) {
    case MapKind::Court:
        return QColor(246, 196, 82);
    case MapKind::City:
        return QColor(230, 225, 203);
    case MapKind::Space:
        return QColor(86, 42, 118);
    }
    return QColor(246, 196, 82);
}
