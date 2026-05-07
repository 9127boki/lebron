#pragma once

#include <QElapsedTimer>
#include <QColor>
#include <QKeyEvent>
#include <QPixmap>
#include <QPointF>
#include <QRectF>
#include <QSettings>
#include <QString>
#include <QTimer>
#include <QVector>
#include <QWidget>

class QMouseEvent;
class QPainter;

class GameWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit GameWidget(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;

private slots:
    void tick();

private:
    enum class Screen {
        MainMenu,
        MapSelect,
        Playing,
        Paused,
        GameOver
    };

    enum class MapKind {
        Court,
        City,
        Space
    };

    enum class EntityKind {
        Ball,
        GroundObstacle,
        FlyingObstacle,
        Reward
    };

    struct Entity {
        EntityKind kind = EntityKind::Ball;
        QRectF rect;
        QPointF velocity;
        bool active = true;
    };

    void resetGame();
    void startGame(MapKind map);
    void loadSprites();
    void createFallbackSprites();
    void updatePlayer(double dt);
    void updateEntities(double dt);
    void spawnEntities(double dt);
    void handleCollisions();
    void addDamage(int amount);
    void finishGame();
    void drawBackground(QPainter &painter);
    void drawMainMenu(QPainter &painter);
    void drawMapSelect(QPainter &painter);
    void drawGame(QPainter &painter);
    void drawPausedOverlay(QPainter &painter);
    void drawGameOver(QPainter &painter);
    void drawCenteredText(QPainter &painter, const QRectF &rect, const QString &text, int size, bool bold = false);
    QRectF groundRect() const;
    QRectF playerRect() const;
    QRectF menuButtonRect(int index) const;
    QRectF mapCardRect(int index) const;
    QString mapName(MapKind map) const;
    QColor mapColorA() const;
    QColor mapColorB() const;

    QTimer m_timer;
    QElapsedTimer m_clock;
    Screen m_screen = Screen::MainMenu;
    MapKind m_map = MapKind::Court;
    QVector<Entity> m_entities;

    QPixmap m_playerSprite;
    QPixmap m_ballSprite;
    QPixmap m_groundObstacleSprite;
    QPixmap m_flyingObstacleSprite;
    QPixmap m_rewardSprite;
    QPixmap m_courtBackground;

    QPointF m_playerPos;
    QPointF m_playerVelocity;
    bool m_leftPressed = false;
    bool m_rightPressed = false;
    bool m_downPressed = false;
    int m_jumpsRemaining = 2;
    double m_health = 100.0;
    int m_score = 0;
    int m_highScore = 0;
    bool m_recordBroken = false;
    double m_spawnTimer = 0.0;
    double m_ballTimer = 0.0;
    double m_flyingTimer = 0.0;
    double m_rewardTimer = 0.0;
    double m_invincibleTimer = 0.0;
    double m_healDelay = 0.0;
    double m_worldSpeed = 250.0;
};
